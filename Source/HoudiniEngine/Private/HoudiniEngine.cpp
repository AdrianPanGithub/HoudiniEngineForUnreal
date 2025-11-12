// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngine.h"

#include "Tasks/Task.h"
#include "WorldPartition/WorldPartition.h"
#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"

#include "HAPI/HAPI_Version.h"
#include "HoudiniEngineCommon.h"
#include "HoudiniEngineSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"
#include "HoudiniNode.h"
#include "HoudiniInputs.h"
#include "HoudiniOutputs.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FHoudiniEngine* FHoudiniEngine::HoudiniEngineInstance = nullptr;

void FHoudiniEngine::StartupModule()
{
	FHoudiniEngine::HoudiniEngineInstance = this;
	
	ResetSession();

	InitializeHAPI();
	
	RegisterIntrinsicBuilders();
}

void FHoudiniEngine::ShutdownModule()
{
	UnregisterActorInputDelegates();
	
	UnregisterWorldDestroyedDelegate();

	UnregisterNodeMovedDelegate();

	if (!IsNullSession())
		FHoudiniApi::CloseSession(&Session);

	FHoudiniApi::FinalizeHAPI();

	FHoudiniEngine::HoudiniEngineInstance = nullptr;
}

void FHoudiniEngine::RegisterIntrinsicBuilders()
{
	RegisterInputBuilder(MakeShared<FHoudiniSkeletalMeshInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniDataAssetInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniTextureInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniFoliageTypeInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniBlueprintInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniDataTableInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniStaticMeshInputBuilder>());

	RegisterInputBuilder(MakeShared<FHoudiniActorComponentInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniStaticMeshComponentInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniSkeletalMeshComponentInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniSplineComponentInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniBrushComponentInputBuilder>());
	RegisterInputBuilder(MakeShared<FHoudiniDynamicMeshComponentInputBuilder>());

	RegisterOutputBuilder(MakeShared<FHoudiniLandscapeOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniInstancerOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniAssetOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniCurveOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniMeshOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniSkeletalMeshOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniMaterialInstanceOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniTextureOutputBuilder>());
	RegisterOutputBuilder(MakeShared<FHoudiniDataTableOutputBuilder>());
}

void FHoudiniEngine::CacheNameActorMap()
{
	CachedNameActorMap.Empty();

	ULevel* CurrLevel = CurrWorld->GetCurrentLevel();
	if (!IsValid(CurrLevel))
		return;

	for (AActor* Actor : CurrLevel->Actors)
	{
		if (IsValid(Actor))
			CachedNameActorMap.Add(Actor->GetFName(), Actor);
	}
}

void FHoudiniEngine::RegisterActor(AActor* Actor)
{
	CachedNameActorMap.FindOrAdd(Actor->GetFName(), Actor);
}

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
static AActor* ForceLoadActorFromDescInst(const FWorldPartitionActorDescInstance* ActorDescInstPtr)
{
	AActor* Actor = ActorDescInstPtr->GetActor();
	// Copy from UE5.3 WorldPartition/WorldPartitionActorDesc.cpp: FWorldPartitionActorDesc::Load()
	if (!IsValid(Actor))
	{
		const FLinkerInstancingContext* InstancingContext = ActorDescInstPtr->GetContainerInstance() ? ActorDescInstPtr->GetContainerInstance()->GetInstancingContext() : nullptr;

		UPackage* Package = nullptr;

		if (InstancingContext)
		{
			FName RemappedPackageName = InstancingContext->RemapPackage(ActorDescInstPtr->GetActorPackage());
			check(RemappedPackageName != ActorDescInstPtr->GetActorSoftPath().GetLongPackageFName());

			Package = CreatePackage(*RemappedPackageName.ToString());
		}

		Package = LoadPackage(Package, *ActorDescInstPtr->GetActorPackage().ToString(), LOAD_None, nullptr, InstancingContext);

		if (Package)
			Actor = ActorDescInstPtr->GetActor();  // Set FWorldPartitionActorDescInstance::ActorPtr
	}

	return Actor;
}
#endif

AActor* FHoudiniEngine::GetActorByName(const FName& ActorName, const bool& bForceLoad)
{
	// Try find in cache
	if (const TWeakObjectPtr<AActor>* FoundActorPtr = CachedNameActorMap.Find(ActorName))
	{
		if (FoundActorPtr->IsValid())  // As cook is async, user may unload some actors while cooking
			return FoundActorPtr->Get();
	}

	if (bForceLoad)  // If not found, try load in world partiton
	{
		const UWorldPartition* WP = CurrWorld->GetWorldPartition();
		if (!WP)
			return nullptr;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
		const FWorldPartitionActorDescInstance* ActorDescInstPtr = WP->GetActorDescInstanceByName(ActorName);
		if (!ActorDescInstPtr)
			return nullptr;

		AActor* LoadedActor = ForceLoadActorFromDescInst(ActorDescInstPtr);
#else
		const FWorldPartitionActorDesc* ActorDescPtr = WP->GetActorDescByName(ActorName.ToString());
		if (!ActorDescPtr)
			return nullptr;

		AActor* LoadedActor = ActorDescPtr->Load();
#endif
		if (!IsValid(LoadedActor))
			return nullptr;

		CachedNameActorMap.FindOrAdd(ActorName) = LoadedActor;
		CurrWorld->GetCurrentLevel()->AddLoadedActor(LoadedActor);

		return LoadedActor;
	}
	
	return nullptr;
}

void FHoudiniEngine::DestroyActorByName(const FName& ActorName)
{
	if (const TWeakObjectPtr<AActor>* FoundActorPtr = CachedNameActorMap.Find(ActorName))
	{
		AActor* FoundActor = FoundActorPtr->IsValid() ? FoundActorPtr->Get() : nullptr;
		CachedNameActorMap.Remove(ActorName);
		if (FoundActor)
		{
			CurrWorld->DestroyActor(FoundActor);
			return;
		}
	}

	const UWorldPartition* WP = CurrWorld->GetWorldPartition();
	if (!WP)
		return;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
	const FWorldPartitionActorDescInstance* ActorDescInstPtr = WP->GetActorDescInstanceByName(ActorName);
	if (!ActorDescInstPtr)
		return;

	if (ActorDescInstPtr->IsLoaded())
		CurrWorld->DestroyActor(ActorDescInstPtr->GetActor());
	else
	{
		AActor* Actor = ForceLoadActorFromDescInst(ActorDescInstPtr);
#else
	const FWorldPartitionActorDesc* ActorDescPtr = WP->GetActorDescByName(ActorName.ToString());
	if (!ActorDescPtr)
		return;

	if (ActorDescPtr->IsLoaded())
		CurrWorld->DestroyActor(ActorDescPtr->GetActor());
	else
	{
		AActor* Actor = ActorDescPtr->Load();
#endif
		if (IsValid(Actor))
			CurrWorld->DestroyActor(Actor);  // TODO: Find a way to destroy actor without shown in outliner anymore, just like what unreal editor does
	}
}

bool FHoudiniEngine::IsSessionValid() const
{
	if (IsNullSession())
		return false;

	return HAPI_RESULT_SUCCESS == FHoudiniApi::IsSessionValid(GetSession());
}

void FHoudiniEngine::PreStartSession()
{
	FPlatformMisc::SetEnvironmentVar(TEXT(HAPI_ENV_CLIENT_NAME), TEXT("unreal"));
	FPlatformMisc::SetEnvironmentVar(TEXT(HAPI_ENV_CLIENT_PROJECT_DIR), *FPaths::ProjectDir());

	FFileHelper::SaveStringToFile(
		FString::Printf(TEXT("{ \"hpath\": \"%s/Resources/houdini\" }"), *FPaths::ConvertRelativePathToFull(GetPluginDir())),
#if PLATFORM_WINDOWS
		*(FString::Printf(TEXT("%shoudini%d.%d/packages/houdini_engine_utils.json"), FPlatformProcess::UserDir(),
#elif PLATFORM_MAC
		*(FString::Printf(TEXT("%s/Library/Preferences/houdini/%d.%d/packages/houdini_engine_utils.json"), FPlatformProcess::UserHomeDir(),
#elif PLATFORM_LINUX
		*(FString::Printf(TEXT("%s/houdini%d.%d/packages/houdini_engine_utils.json"), FPlatformProcess::UserHomeDir(),
#endif
		HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR)));

	if (FHoudiniEngine::Get().OnHoudiniSessionPreStartEvent.IsBound())
		FHoudiniEngine::Get().OnHoudiniSessionPreStartEvent.Broadcast();
}

bool FHoudiniEngine::HapiStartSession()
{
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	// Modify our PATH so that HARC will find HARS.exe
	{
		const FString HoudiniBinDir = GetHoudiniDir() + TEXT("/" HAPI_HOUDINI_BIN_DIR) + FPlatformMisc::GetPathVarDelimiter();
		const FString OrigPathVar = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
		if (!OrigPathVar.StartsWith(HoudiniBinDir))
			FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *(HoudiniBinDir + OrigPathVar));
	}

	PreStartSession();

	HAPI_ThriftServerOptions ServerOptions;
	FHoudiniApi::ThriftServerOptions_Init(&ServerOptions);
	ServerOptions.autoClose = true;
	ServerOptions.timeoutMs = 3000.0f;

	// Ensure each time we start a brand new session.
	const std::string HARSName = TCHAR_TO_UTF8(*FString::Printf(TEXT("hapi_%08X"), FPlatformTime::Cycles()));

	// Start session(HARS) first
	const UHoudiniEngineSettings* Settings = GetDefault<UHoudiniEngineSettings>();
	switch (Settings->SessionType)
	{
	case EHoudiniSessionType::SharedMemory:
	{
		ServerOptions.sharedMemoryBufferType = HAPI_THRIFT_SHARED_MEMORY_FIXED_LENGTH_BUFFER;
		ServerOptions.sharedMemoryBufferSize = HAPI_Int64(Settings->SharedMemoryBufferSize);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::StartThriftSharedMemoryServer(&ServerOptions, HARSName.c_str(), nullptr, nullptr))
			return false;
	}
	break;
	default:
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::StartThriftNamedPipeServer(&ServerOptions, HARSName.c_str(), nullptr, nullptr))
			return false;
	}
	break;
	}

	// Then create session
	HAPI_SessionInfo SessionInfo;
	FHoudiniApi::SessionInfo_Init(&SessionInfo);

	switch (Settings->SessionType)
	{
	case EHoudiniSessionType::SharedMemory:
	{
		SessionInfo.sharedMemoryBufferType = HAPI_THRIFT_SHARED_MEMORY_FIXED_LENGTH_BUFFER;
		SessionInfo.sharedMemoryBufferSize = HAPI_Int64(Settings->SharedMemoryBufferSize);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::CreateThriftSharedMemorySession(&Session, HARSName.c_str(), &SessionInfo))
			return false;
	}
	break;
	default:
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::CreateThriftNamedPipeSession(&Session, HARSName.c_str(), &SessionInfo))
			return false;
	}
	break;
	}

	bSessionSync = false;

	return HapiInitializeSession();
}

FORCEINLINE static bool StartHoudiniProcess(const FString& HoudiniDir, const TCHAR* HoudiniArg)
{
	if (!FPlatformProcess::CreateProc(
		*(HoudiniDir + TEXT("/" HAPI_HOUDINI_BIN_DIR "/houdini")),
		HoudiniArg,
		true, false, false,
		nullptr, 0,
		*FPlatformProcess::GetCurrentWorkingDirectory(),
		nullptr, nullptr).IsValid())
	{
		if (!FPlatformProcess::CreateProc(
			*(HoudiniDir + TEXT("/" HAPI_HOUDINI_BIN_DIR "/hindie.steam")),
			HoudiniArg,
			true, false, false,
			nullptr, 0,
			*FPlatformProcess::GetCurrentWorkingDirectory(),
			nullptr, nullptr).IsValid())
			return false;
	}

	return true;
}

bool FHoudiniEngine::HapiStartSessionSync()
{
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	if (IsSessionValid())  // Already has a working session
	{
		if (bSessionSync)  // Session Sync already opened
			return true;

		// has a HARS running in the background, so we need invalidate it first
		FHoudiniApi::CloseSession(&Session);

		InvalidateSessionData();  // will reset FHoudiniEngine::NumWorkingTasks = 0

		StartHoudiniTask();  // Mark a new task again, as InvalidateSessionData() will reset it
	}
	
	PreStartSession();

	bSessionSync = false;

	HAPI_SessionInfo SessionInfo;
	FHoudiniApi::SessionInfo_Init(&SessionInfo);

	const UHoudiniEngineSettings* Settings = GetDefault<UHoudiniEngineSettings>();
	switch (Settings->SessionType)
	{
	case EHoudiniSessionType::SharedMemory:
	{
		SessionInfo.sharedMemoryBufferType = HAPI_THRIFT_SHARED_MEMORY_FIXED_LENGTH_BUFFER;
		SessionInfo.sharedMemoryBufferSize = HAPI_Int64(Settings->SharedMemoryBufferSize);

		// First we should check whether there is already an open houdini
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::CreateThriftSharedMemorySession(&Session, "hapi_session_sync", &SessionInfo))
			bSessionSync = true;
		else if (StartHoudiniProcess(GetHoudiniDir(), *FString::Printf(TEXT("-hess=shared:fixed:%d:hapi_session_sync"), int32(Settings->SharedMemoryBufferSize))))
		{
			for (int32 CheckIter = 0; CheckIter < 300; ++CheckIter)  // We just wait for 5 mins, if not finished, just treat it as failed
			{
				FPlatformProcess::SleepNoStats(1.0f);

				if (HAPI_RESULT_SUCCESS == FHoudiniApi::CreateThriftSharedMemorySession(&Session, "hapi_session_sync", &SessionInfo))
				{
					bSessionSync = true;
					break;
				}
			}
		}
	}
	break;
	default:
	{
		// First we should check whether there is already an open houdini
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::CreateThriftNamedPipeSession(&Session, "hapi_session_sync", &SessionInfo))
			bSessionSync = true;
		else if (StartHoudiniProcess(GetHoudiniDir(), TEXT("-hess=pipe:hapi_session_sync")))
		{
			for (int32 CheckIter = 0; CheckIter < 300; ++CheckIter)  // We just wait for 5 mins, if not finished, just treat it as failed
			{
				FPlatformProcess::SleepNoStats(1.0f);

				if (HAPI_RESULT_SUCCESS == FHoudiniApi::CreateThriftNamedPipeSession(&Session, "hapi_session_sync", &SessionInfo))
				{
					bSessionSync = true;
					break;
				}
			}
		}
	}
	break;
	}
	
	return bSessionSync ? HapiInitializeSession() : false;
}


static bool LoadHoudiniSceneAndSaveFile(const FString& FilePath)
{
	// Start a new session
	HAPI_ThriftServerOptions ServerOptions;
	FHoudiniApi::ThriftServerOptions_Init(&ServerOptions);
	ServerOptions.autoClose = true;
	ServerOptions.timeoutMs = 3000.0f;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::StartThriftNamedPipeServer(&ServerOptions, "hapi_file_save", nullptr, nullptr))
		return false;

	HAPI_SessionInfo SessionInfo;
	FHoudiniApi::SessionInfo_Init(&SessionInfo);
	HAPI_Session Session;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::CreateThriftNamedPipeSession(&Session, "hapi_file_save", &SessionInfo))
		return false;

	HAPI_CookOptions CookOptions;
	FHoudiniApi::CookOptions_Init(&CookOptions);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::Initialize(&Session, &CookOptions, true, -1, nullptr, nullptr, nullptr, nullptr, nullptr))
		return false;

	// Must load this hip file again, then sharedmemory nodes will be locked
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::LoadHIPFile(&Session, TCHAR_TO_UTF8(*FilePath), false))
		return false;

	HAPI_NodeId PrevGlobalNodeId = -1;
	FHoudiniApi::GetNodeFromPath(&Session, -1, "/obj/GlobalNodes1", &PrevGlobalNodeId);
	if (PrevGlobalNodeId >= 0)
		FHoudiniApi::DeleteNode(&Session, PrevGlobalNodeId);

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::SaveHIPFile(&Session, TCHAR_TO_UTF8(*FilePath), false))
		return false;

	FHoudiniApi::CloseSession(&Session);

	return true;
}

bool FHoudiniEngine::HapiSaveSceneToFile(const FString& FilePath, bool& bOutSuccess)
{
	bOutSuccess = false;

	// Save HIP file through Engine.
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SaveHIPFile(FHoudiniEngine::Get().GetSession(),
		TCHAR_TO_UTF8(*FilePath), false));

	bOutSuccess = LoadHoudiniSceneAndSaveFile(FilePath);
	return true;
}

bool FHoudiniEngine::HapiOpenSceneInHoudini()
{
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(&Session))
		return false;

	// First, saves the current scene as a hip file, creates a proper temporary file name
	FString TempHipFilePath = FPaths::CreateTempFilename(FPlatformProcess::UserTempDir(),
		TEXT("HoudiniEngine"), TEXT(".hip"));

	// Save HIP file through Engine.
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SaveHIPFile(&Session,
		TCHAR_TO_UTF8(*TempHipFilePath), false));

	if (!FPaths::FileExists(TempHipFilePath))
		return true;

	// Set env var that can be read after houdini start
	if (const ULevel* Level = FHoudiniEngineUtils::GetCurrentLevel())
		FPlatformMisc::SetEnvironmentVar(TEXT(HAPI_ENV_CLIENT_SCENE_PATH), *Level->GetPathName());
	FPlatformMisc::SetEnvironmentVar(TEXT(HAPI_ENV_HOUDINI_ENGINE_FOLDER), *GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder);

	StartHoudiniProcess(GetHoudiniDir(), *(TEXT("\"") + TempHipFilePath + TEXT("\"")));

	return true;
}

bool FHoudiniEngine::HapiInitializeSession()
{
	HAPI_CookOptions CookOptions;
	FHoudiniApi::CookOptions_Init(&CookOptions);

	CookOptions.splitGeosByGroup = false;
	CookOptions.splitGroupSH = 0;
	CookOptions.splitGeosByAttribute = false;
	CookOptions.splitAttrSH = 0;
	CookOptions.maxVerticesPerPrimitive = 3;
	CookOptions.refineCurveToLinear = false;
	CookOptions.curveRefineLOD = 8.0f;
	CookOptions.clearErrorsAndWarnings = false;
	CookOptions.cookTemplatedGeos = true;
	CookOptions.splitPointsByVertexAttributes = false;
	CookOptions.packedPrimInstancingMode = HAPI_PACKEDPRIM_INSTANCING_MODE_FLAT;
	CookOptions.handleBoxPartTypes = false;
	CookOptions.handleSpherePartTypes = false;
	
	HAPI_Result Result = FHoudiniApi::Initialize(
		&Session,
		&CookOptions,
		GetDefault<UHoudiniEngineSettings>()->bVerbose,  // Use cook thread if need display detailed cook and instantiate progress
		-1,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr);

	if ((Result != HAPI_RESULT_SUCCESS) && (Result != HAPI_RESULT_ALREADY_INITIALIZED))
		return false;

	// Set enviroment variables
	ULevel* Level = FHoudiniEngineUtils::GetCurrentLevel();
	if (IsValid(Level))
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetServerEnvString(&Session,
			HAPI_ENV_CLIENT_SCENE_PATH, TCHAR_TO_UTF8(*Level->GetPathName())));
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetServerEnvString(&Session,
		HAPI_ENV_HOUDINI_ENGINE_FOLDER, TCHAR_TO_UTF8(*GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder)));

	return true;
}

bool FHoudiniEngine::HapiStopSession() const
{
	return (HAPI_RESULT_SUCCESS == FHoudiniApi::CloseSession(&Session));
}

void FHoudiniEngine::UnregisterActorInputDelegates()
{
	if (GEngine)
	{
		if (OnActorChangedHandle.IsValid())
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnActorChangedHandle);
			OnActorChangedHandle.Reset();
		}
		
		if (OnActorFolderChangedHandle.IsValid())
		{
			GEngine->OnLevelActorFolderChanged().Remove(OnActorFolderChangedHandle);
			OnActorFolderChangedHandle.Reset();
		}

		if (OnActorDeletedHandle.IsValid())
		{
			GEngine->OnLevelActorDeleted().Remove(OnActorDeletedHandle);
			OnActorDeletedHandle.Reset();
		}

		if (OnActorAddedHandle.IsValid())
		{
			GEngine->OnLevelActorDeleted().Remove(OnActorAddedHandle);
			OnActorAddedHandle.Reset();
		}
	}
}

void FHoudiniEngine::InvalidateSessionData()
{
	if (IsInGameThread())
	{
		RecoverEngineFrameRate();
		FinishHoudiniMainTaskMessage();
		FinishHoudiniAsyncTaskMessage();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, []
			{
				FHoudiniEngine::Get().RecoverEngineFrameRate();
				FHoudiniEngine::Get().FinishHoudiniMainTaskMessage();
				FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();
			});
	}

	bSessionSync = false;
	ResetSession();
	LoadedAssets.Empty();

	for (const TWeakObjectPtr<AHoudiniNode>& Node : CurrNodes)
	{
		if (Node.IsValid())
			Node->Invalidate();
	}

	NumWorkingTasks = 0;
}

bool FHoudiniEngine::AllowEdit() const
{
	return (NumWorkingTasks == 0) && FHoudiniApi::IsHAPIInitialized();
}

bool FHoudiniEngine::Tick(float DeltaTime)
{
	if (!AllowEdit())  // Pause Tick when async houdini task running
		return true;

	// Check whether NameActorLoadedMap should update
	if (CachedNameActorMap.IsEmpty() && CurrWorld.IsValid() && !CurrNodes.IsEmpty())
		CacheNameActorMap();

	// Check is session valid once per second
	SessionCheckDeltaTime += DeltaTime;
	if (SessionCheckDeltaTime >= 1.0f)
	{
		SessionCheckDeltaTime = 0.0f;
		// Check Not IsNullSession()
		// For the situation that when first node dropped, but we just mark all node invalidate, so if No check, then this node will not start cook
		if (!IsNullSession() && !IsSessionValid())  // If is a null session, then we should NOT invalidate
			InvalidateSessionData();
	}

	// Check has node changed
	for (const TWeakObjectPtr<AHoudiniNode>& CurrNode : CurrNodes)
	{
		if (!CurrNode.IsValid())
			continue;

		AHoudiniNode* Node = CurrNode.Get();
		if (Node->NeedCook())
		{
			if (!IsSessionValid())  // We should create a session first
			{
				StartHoudiniTask();
				HoudiniAsyncTaskMessageEvent.Broadcast(LOCTEXT("StartSession", HAPI_MESSAGE_START_SESSION));
				UE::Tasks::Launch(UE_SOURCE_LOCATION, []
					{
						FHoudiniEngine::Get().HapiStartSession();
						AsyncTask(ENamedThreads::GameThread, []()
							{
								FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FText::GetEmpty());
								FHoudiniEngine::Get().FinishHoudiniTask();  // Finish task, so that tick will trigger the actual cook process
							});
					});
			}
			else  // Actual cook process
			{
				CacheNameActorMap();
				HOUDINI_FAIL_INVALIDATE(Node->HapiAsyncProcess(false));
			}

			return true;  // Do NOT start multi nodes to cook at same time
		}
	}

	return true;
}

void FHoudiniEngine::StartTick()
{
	if (!TickerHandle.IsValid())
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FHoudiniEngine::Tick));
}

void FHoudiniEngine::StopTick()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

void FHoudiniEngine::LimitEngineFrameRate()
{
	if (!bEngineFPSLimited)
	{
		EngineMaxFPS = GEngine->GetMaxFPS();
		GEngine->SetMaxFPS(GetDefault<UHoudiniEngineSettings>()->MaxFPSWhileCooking);
		bEngineFPSLimited = true;
	}
}

void FHoudiniEngine::RecoverEngineFrameRate()
{
	if (bEngineFPSLimited)
	{
		GEngine->SetMaxFPS(EngineMaxFPS);
		bEngineFPSLimited = false;
		EngineMaxFPS = 0.0f;
	}
}

const FString& FHoudiniEngine::GetPluginDir()
{
	static FString PluginDir;

	if (PluginDir.IsEmpty())
		PluginDir = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir();

	return PluginDir;
}

const FString& FHoudiniEngine::GetProcessIdentifier()
{
	static FString ProcessIdentifier;

	if (ProcessIdentifier.IsEmpty())
		ProcessIdentifier = FString::Printf(TEXT("_%d_"), FPlatformProcess::GetCurrentProcessId());

	return ProcessIdentifier;
}

void FHoudiniEngine::RegisterWorldDestroyedDelegate()
{
	if (!OnWorldDestroyedHandle.IsValid())
	{
		OnWorldDestroyedHandle = GEngine->OnWorldDestroyed().AddLambda([](UWorld* DestroyedWorld)
			{
				if (!GIsRunning)
					return;

				if (DestroyedWorld != FHoudiniEngine::Get().CurrWorld)
					return;

				// Destroy nodes in previous world
				const TArray<TWeakObjectPtr<AHoudiniNode>> LegacyNodes = FHoudiniEngine::Get().GetCurrentNodes();
				for (const TWeakObjectPtr<AHoudiniNode>& LegacyNode : LegacyNodes)
				{
					if (LegacyNode.IsValid())
						HOUDINI_FAIL_INVALIDATE(LegacyNode->HapiDestroy());  // We could only destroy node data here, when world destroyed
				}
				FHoudiniEngine::Get().CurrNodes.Empty();
				
				// Clear World exclusively data, see FHoudiniEngine::RegisterNode below
				FHoudiniEngine::Get().CurrWorld.Reset();
				FHoudiniEngine::Get().CachedNameActorMap.Empty();
			});
	}
}

void FHoudiniEngine::UnregisterWorldDestroyedDelegate()
{
	if (GEngine && OnWorldDestroyedHandle.IsValid())
		GEngine->OnWorldDestroyed().Remove(OnWorldDestroyedHandle);
}

void FHoudiniEngine::RegisterNode(AHoudiniNode* Node)
{
	// Force update node LastTransform
	class FHoudiniUpdateNodeLastTransform : public AHoudiniNode
	{
	public:
		FORCEINLINE void Update() { LastTransform = GetActorTransform(); }
	};

	((FHoudiniUpdateNodeLastTransform*)Node)->Update();

	if (CurrWorld != Node->GetWorld())  // World changed
	{
		RegisterWorldDestroyedDelegate();
		RegisterActorInputDelegates();
		RegisterNodeMovedDelegate();

		// Destroy nodes in previous world
		const TArray<TWeakObjectPtr<AHoudiniNode>> LegacyNodes = CurrNodes;
		for (const TWeakObjectPtr<AHoudiniNode>& LegacyNode : LegacyNodes)
		{
			if (LegacyNode.IsValid())
				HOUDINI_FAIL_INVALIDATE(LegacyNode->HapiDestroy());  // We could only destroy node data here, when world destroyed
		}
		CurrNodes.Empty();
		CurrWorld = Node->GetWorld();
		CachedNameActorMap.Empty();  // CachedNameActorLoadedMap should Update in next tick, as this tick does NOT load actors completely
	}

	if (!CurrNodes.Contains(Node))
	{
		CurrNodes.Add(Node);

		if (OnHoudiniNodeRegisteredEvent.IsBound())
			OnHoudiniNodeRegisteredEvent.Broadcast(Node);
	}
}

void FHoudiniEngine::RegisterWorld(UWorld* World)
{
	CurrNodes.Empty();
	for (TActorIterator<AHoudiniNode> NodeIter(World); NodeIter; ++NodeIter)
	{
		AHoudiniNode* Node = *NodeIter;
		if (IsValid(Node))
			RegisterNode(Node);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHoudiniEngine, HoudiniEngine)
