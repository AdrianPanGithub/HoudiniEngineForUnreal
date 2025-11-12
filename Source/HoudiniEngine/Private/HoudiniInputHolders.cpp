// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputs.h"

#include "EngineUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "JsonObjectConverter.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniAttribute.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniNode.h"
#include "HoudiniCurvesComponent.h"


void UHoudiniInputHolder::RequestReimport()
{
	bHasChanged = true;
	((AHoudiniNode*)(GetOuter()->GetOuter()))->TriggerCookByInput((const UHoudiniInput*)GetOuter());
}

// -------- InputCurves --------
UHoudiniInputCurves* UHoudiniInputCurves::Create(UHoudiniInput* Input)
{
	AHoudiniNode* Node = Cast<AHoudiniNode>(Input->GetOuter());

	UHoudiniCurvesComponent* NewCurvesComponent = NewObject<UHoudiniCurvesComponent>(Node, NAME_None, RF_Public | RF_Transactional);

	NewCurvesComponent->AttachToComponent(Node->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	NewCurvesComponent->OnComponentCreated();
	NewCurvesComponent->RegisterComponent();

	NewCurvesComponent->InputUpdate(Node, Input->GetInputName(),
		Input->GetSettings().bDefaultCurveClosed, Input->GetSettings().DefaultCurveType, Input->GetSettings().DefaultCurveColor);

	UHoudiniInputCurves* InputCurves = NewObject<UHoudiniInputCurves>(Input, NAME_None, RF_Public | RF_Transactional);
	InputCurves->CurvesComponent = NewCurvesComponent;

	return InputCurves;
}

void UHoudiniInputCurves::Invalidate()
{
	NodeId = -1;

	if (Handle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(Handle);
		Handle = 0;
	}
}

void UHoudiniInputCurves::Destroy()
{
	Invalidate();

	Modify();  // Otherwise "CurvesComponent" property will not recover after undo

	CurvesComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	CurvesComponent->UnregisterComponent();
	CurvesComponent->DestroyComponent();
	CurvesComponent = nullptr;
}

bool UHoudiniInputCurves::HapiUpload()
{
	if (NodeId < 0)
	{
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GetGeoNodeId(), "", NodeId));
		HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(NodeId));  // set merge node id, HAPI does NOT call
	}

	HOUDINI_FAIL_RETURN(CurvesComponent->HapiInputUpload(NodeId, ShouldImportRotAndScale(), ShouldImportCollisionInfo(), Handle));
	CurvesComponent->ResetDeltaInfo();

	bHasChanged = false;

	return true;
}

bool UHoudiniInputCurves::HapiDestroy()
{
	if (NodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeId));

	Invalidate();

	return true;
}

bool UHoudiniInputCurves::HasChanged() const
{
	return bHasChanged || CurvesComponent->HasChanged();
}

// -------- InputNode --------
UHoudiniInputNode* UHoudiniInputNode::Create(UHoudiniInput* Input, AHoudiniNode* Node)
{
	UHoudiniInputNode* InputNode = NewObject<UHoudiniInputNode>(Input, NAME_None, RF_Public | RF_Transactional);

	InputNode->SetNode(Node);

	return InputNode;
}

void UHoudiniInputNode::SetNode(AHoudiniNode* NewNode)
{
	if (NodeActorName != NewNode->GetFName())
	{
		Node = NewNode;
		NodeActorName = NewNode->GetFName();
		RequestReimport();

		if (NewNode->GetNodeId() < 0)  // If Upstream node instantiated, then we need to trigger instantiated it
			NewNode->RequestCook();
	}
}

TSoftObjectPtr<UObject> UHoudiniInputNode::GetObject() const
{
	return GetNode();
}

AHoudiniNode* UHoudiniInputNode::GetNode() const
{
	if (Node.IsValid())
		return Node.Get();

	Node = Cast<AHoudiniNode>(FHoudiniEngine::Get().GetActorByName(NodeActorName, false));
	return Node.IsValid() ? Node.Get() : nullptr;
}

// -------- InputBlueprint --------
UHoudiniInputHolder* FHoudiniBlueprintInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<UBlueprint, UHoudiniInputBlueprint>(Input, Asset, OldHolder);
}

bool FHoudiniBlueprintInputBuilder::GetInfo(const UObject* Asset, FString& OutInfoStr)
{
	if (const UBlueprint* BP = Cast<UBlueprint>(Asset))
	{
		TArray<const UActorComponent*> Components;
		TArray<FTransform> Transforms;
		UHoudiniInputBlueprint::GetComponents(BP, Components, Transforms);

		UHoudiniInputComponents::GetComponentsInfo(nullptr, Components, Transforms, OutInfoStr);

		return true;
	}
	return false;
}

void UHoudiniInputBlueprint::SetAsset(UBlueprint* NewBlueprint)
{
	if (Blueprint != NewBlueprint)
	{
		Blueprint = NewBlueprint;
		RequestReimport();
	}
}

bool UHoudiniInputBlueprint::IsObjectExists() const
{
	return IsValid(Blueprint.LoadSynchronous());
}

void UHoudiniInputBlueprint::GetComponents(const UBlueprint* BP, TArray<const UActorComponent*>& OutComponents, TArray<FTransform>& OutTransforms)
{
	if (!IsValid(BP->SimpleConstructionScript))
		return;

	TMap<const USceneComponent*, const USceneComponent*> ComponentParentMap;  // Then will accumulate parent relative transforms recursively
	for (const USCS_Node* ScriptNode : BP->SimpleConstructionScript->GetAllNodes())
	{
		if (IsValid(ScriptNode->ComponentTemplate))
		{
			OutComponents.Add(ScriptNode->ComponentTemplate);
			if (const USceneComponent* SC = Cast<USceneComponent>(ScriptNode->ComponentTemplate))
			{
				for (const USCS_Node* ChildScriptNode : ScriptNode->ChildNodes)
				{
					if (const USceneComponent* ChildSC = Cast<USceneComponent>(ChildScriptNode->ComponentTemplate))
						ComponentParentMap.Add(ChildSC, SC);
				}
			}
		}
	}

	for (const UActorComponent* Component : OutComponents)
	{
		if (const USceneComponent* SC = Cast<USceneComponent>(Component))
		{
			FTransform Transform = SC->GetRelativeTransform();
			for (int32 Iter = 0; Iter <= OutComponents.Num(); ++Iter)
			{
				if (const USceneComponent** FoundParentSCPtr = ComponentParentMap.Find(SC))  // Find parent component recursively
				{
					SC = *FoundParentSCPtr;
					Transform *= SC->GetRelativeTransform();
				}
				else
					break;
			}
			OutTransforms.Add(Transform);
		}
		else
			OutTransforms.Add(FTransform::Identity);
	}
}

bool UHoudiniInputBlueprint::HapiUpload()
{
	UBlueprint* BP = Blueprint.LoadSynchronous();
	if (!IsValid(BP))
		return HapiDestroy();

	TArray<const UActorComponent*> Components;
	TArray<FTransform> Transforms;
	GetComponents(BP, Components, Transforms);

	HOUDINI_FAIL_RETURN(UHoudiniInputComponents::HapiUploadComponents(BP, Components, Transforms));

	bHasChanged = false;

	return true;
}

// -------- InputActor --------
UHoudiniInputActor* UHoudiniInputActor::Create(UHoudiniInput* Input, AActor* Actor)
{
	UHoudiniInputActor* ActorInput = NewObject<UHoudiniInputActor>(Input, NAME_None, RF_Public | RF_Transactional);

	ActorInput->SetActor(Actor);

	return ActorInput;
}

void UHoudiniInputActor::SetActor(AActor* NewActor)
{
	if (ActorName != NewActor->GetFName())
	{
		Actor = NewActor;
		ActorName = NewActor->GetFName();
		RequestReimport();
	}
}

AActor* UHoudiniInputActor::GetActor() const
{
	if (Actor.IsValid())
		return Actor.Get();

	Actor = FHoudiniEngine::Get().GetActorByName(ActorName, false);  // If this actor does NOT loaded, then we just skip it
	return Actor.IsValid() ? Actor.Get() : nullptr;
}

bool UHoudiniInputActor::HapiUpload()
{
	const AActor* A = GetActor();
	if (!IsValid(A))
		HapiDestroy();

	TArray<const UClass*> AllowClasses, DisallowClasses;
	GetSettings().GetFilterClasses(AllowClasses, DisallowClasses, UActorComponent::StaticClass());

	TArray<const UActorComponent*> Components;
	TArray<FTransform> Transforms;
	if (const ALevelInstance* LevelInstance = Cast<ALevelInstance>(A))
	{
		const UWorld* LevelInstanceWorld = LevelInstance->GetWorldAsset().LoadSynchronous();
		if (IsValid(LevelInstanceWorld))
		{
			const FTransform& ActorTransform = A->GetActorTransform();
			for (TActorIterator<AActor> ActorIter(LevelInstanceWorld); ActorIter; ++ActorIter)
			{
				AActor* const LevelActor = *ActorIter;
				if (!IsValid(LevelActor))
					continue;

				for (const UActorComponent* Component : LevelActor->GetComponents())
				{
					if (IsValid(Component) && FHoudiniEngineUtils::FilterClass(AllowClasses, DisallowClasses, Component->GetClass()))
					{
						Components.Add(Component);
						if (const USceneComponent* SC = Cast<USceneComponent>(Component))
							Transforms.Add(SC->GetComponentTransform() * ActorTransform);
						else
							Transforms.Add(LevelActor->GetActorTransform() * ActorTransform);
					}
				}
			}
		}
	}
	else
	{
		for (const UActorComponent* Component : A->GetComponents())
		{
			if (IsValid(Component) && FHoudiniEngineUtils::FilterClass(AllowClasses, DisallowClasses, Component->GetClass()))
			{
				Components.Add(Component);
				if (const USceneComponent* SC = Cast<USceneComponent>(Component))
					Transforms.Add(SC->GetComponentTransform());
				else
					Transforms.Add(A->GetActorTransform());
			}
		}
	}

	HOUDINI_FAIL_RETURN(UHoudiniInputComponents::HapiUploadComponents(A, Components, Transforms));

	bHasChanged = false;

	return true;
}

// -------- InputDataTable --------
UHoudiniInputHolder* FHoudiniDataTableInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<UDataTable, UHoudiniInputDataTable>(Input, Asset, OldHolder);
}

void UHoudiniInputDataTable::SetAsset(UDataTable* NewDataTable)
{
	if (DataTable != NewDataTable)
	{
		DataTable = NewDataTable;
		RequestReimport();
	}
}

bool UHoudiniInputDataTable::IsObjectExists() const
{
	return IsValid(DataTable.LoadSynchronous());
}

bool UHoudiniInputDataTable::HapiUpload()
{
	const UDataTable* DT = DataTable.LoadSynchronous();
	if (!IsValid(DT))
		return HapiDestroy();

	const int32 NumRows = DT->GetRowMap().Num();
	const UScriptStruct* RowStruct = DT->GetRowStruct();
	if (!IsValid(RowStruct) || NumRows <= 0)  // Import a "null" reference node
	{
		if (Handle)  // Previous Node is shm input, we should create a null reference node, so destroy the previous node 
			HOUDINI_FAIL_RETURN(HapiDestroy());

		const bool bCreateNewNode = (NodeId < 0);
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("reference_%s_%08X"), *DT->GetName(), FPlatformTime::Cycles()), NodeId));
		
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(NodeId, FVector3f::ZeroVector));
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(NodeId, HAPI_ATTRIB_UNREAL_OBJECT_PATH, FHoudiniEngineUtils::GetAssetReference(DT)));
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), NodeId));

		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(NodeId));

		bHasChanged = false;

		return true;
	}

	if (NodeId >= 0 && !Handle)  // Means previous is a "null" reference node
		HOUDINI_FAIL_RETURN(HapiDestroy());


	// -------- Import data --------
	size_t NumRowNamesChars = 0;
	TArray<std::string> RowNames;
	TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
	{
		TArray<const FProperty*> Properties;
		for (TFieldIterator<FProperty> PropIter(RowStruct); PropIter; ++PropIter)
			Properties.Add(*PropIter);

		TArray<const uint8*> Containers;
		for (const auto& Row : DT->GetRowMap())
		{
			Containers.Add(Row.Value);

			const std::string RowName = TCHAR_TO_UTF8(*Row.Key.ToString());
			NumRowNamesChars += RowName.length() + 1;
			RowNames.Add(RowName);
		}

		FHoudiniAttribute::RetrieveAttributes(Properties, Containers, HAPI_ATTROWNER_POINT, PropAttribs);
	}

	const size_t RowNamesLength32 = NumRowNamesChars / 4 + 1;

	const std::string DataTableRef = TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(DT));
	const size_t DataTableRefLength32 = (DataTableRef.length() + 1) / 4 + 1;

	const std::string RowStructPath = TCHAR_TO_UTF8(*RowStruct->GetPathName());
	const size_t RowStructPathLength32 = (RowStructPath.length() + 1) / 4 + 1;

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumRows, 0, 0);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_OBJECT_PATH,  // s@unreal_object_path
		EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, DataTableRefLength32, EHoudiniInputAttributeCompression::UniqueValue);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWSTRUCT,  // s@unreal_data_table_rowstruct
		EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, RowStructPathLength32, EHoudiniInputAttributeCompression::UniqueValue);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWNAME,  // s@unreal_data_table_rowname
		EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, RowNamesLength32);

	for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
		PropAttrib->UploadInfo(SHMGeoInput, HAPI_ATTRIB_PREFIX_UNREAL_DATA_TABLE);

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)DT), Handle);

	float* DataTableRefDataPtr = SHM + NumRows * 3;  // Skip position data
	FMemory::Memcpy(DataTableRefDataPtr, DataTableRef.c_str(), DataTableRef.length());
	float* RowStructPathDataPtr = DataTableRefDataPtr + DataTableRefLength32;
	FMemory::Memcpy(RowStructPathDataPtr, RowStructPath.c_str(), RowStructPath.length());
	char* RowNamesDataPtr = (char*)(RowStructPathDataPtr + RowStructPathLength32);
	float* RowDataPtr = ((float*)RowNamesDataPtr) + RowNamesLength32;

	for (const std::string& RowName : RowNames)
	{
		FMemory::Memcpy(RowNamesDataPtr, RowName.c_str(), RowName.length());
		RowNamesDataPtr += RowName.length() + 1;  // Data split by '\0'
	}

	for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
		PropAttrib->UploadData(RowDataPtr);

	const bool bCreateNewNode = (NodeId < 0);
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GetGeoNodeId(),
			FString::Printf(TEXT("%s_%08X"), *DT->GetName(), FPlatformTime::Cycles()), NodeId));
	
	HOUDINI_FAIL_RETURN(SHMGeoInput.HapiUpload(NodeId, SHM));
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(NodeId));

	bHasChanged = false;
	return true;
}

bool UHoudiniInputDataTable::HapiDestroy()
{
	if (NodeId >= 0)
	{
		GetInput()->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeId));
	}

	Invalidate();

	return true;
}

void UHoudiniInputDataTable::Invalidate()
{
	NodeId = -1;
	bHasChanged = false;
	if (Handle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(Handle);
		Handle = 0;
	}
}


// -------- InputDataAsset --------
UHoudiniInputHolder* FHoudiniDataAssetInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<UDataAsset, UHoudiniInputDataAsset>(Input, Asset, OldHolder);
}

bool FHoudiniDataAssetInputBuilder::GetInfo(const UObject* Asset, FString& OutInfoStr)
{
	if (const UDataAsset* DA = Cast<UDataAsset>(Asset))
	{
		FString JsonStr;
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
		if (FJsonObjectConverter::UStructToJsonObject(DA->GetClass(), DA, JsonObject, 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase))
#else
		if (FJsonObjectConverter::UStructToJsonObject(DA->GetClass(), DA, JsonObject))
#endif
		{
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr, 2);
			FJsonSerializer::Serialize(JsonObject, JsonWriter);
			JsonWriter->Close();

			OutInfoStr += JsonStr;
			return true;
		}
	}
	return false;
}

void UHoudiniInputDataAsset::SetAsset(UDataAsset* NewDataAsset)
{
	if (DataAsset != NewDataAsset)
	{
		DataAsset = NewDataAsset;
		RequestReimport();
	}
}

bool UHoudiniInputDataAsset::IsObjectExists() const
{
	return IsValid(DataAsset.LoadSynchronous());
}

bool UHoudiniInputDataAsset::HapiUpload()
{
	const UDataAsset* DA = DataAsset.LoadSynchronous();
	if (!IsValid(DA))
		return HapiDestroy();

	const bool bCreateNewNode = (NodeId < 0);
	if (bCreateNewNode)
	{
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiCreateNode(GetGeoNodeId(),
			FString::Printf(TEXT("DataAsset_%s_%08X"), *DA->GetName(), FPlatformTime::Cycles()), NodeId));

		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(NodeId, FVector3f::ZeroVector));
	}
	
	HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(NodeId, HAPI_ATTRIB_UNREAL_OBJECT_PATH, FHoudiniEngineUtils::GetAssetReference(DA)));
	
	FString JsonStr;
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
	if (FJsonObjectConverter::UStructToJsonObject(DA->GetClass(), DA, JsonObject, 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase))
#else
	if (FJsonObjectConverter::UStructToJsonObject(DA->GetClass(), DA, JsonObject))
#endif
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr, 2);
		FJsonSerializer::Serialize(JsonObject, JsonWriter);
		JsonWriter->Close();
	}

	HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddDictAttribute(NodeId, HAPI_ATTRIB_UNREAL_OBJECT_METADATA, JsonStr));

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), NodeId));

	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(NodeId));

	bHasChanged = false;
	return true;
}

bool UHoudiniInputDataAsset::HapiDestroy()
{
	if (NodeId >= 0)
	{
		GetInput()->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeId));
	}

	Invalidate();

	return true;
}

void UHoudiniInputDataAsset::Invalidate()
{
	NodeId = -1;
	bHasChanged = false;
}
