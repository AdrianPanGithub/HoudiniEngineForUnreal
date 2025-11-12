// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineCommands.h"

#include "Tasks/Task.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"

#include "DesktopPlatformModule.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ISettingsModule.h"

#include "HAPI/HAPI_Version.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommon.h"
#include "HoudiniEngineSettings.h"

#include "HoudiniEngineStyle.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

FHoudiniEngineCommands::FHoudiniEngineCommands()
	: TCommands<FHoudiniEngineCommands>(TEXT("HoudiniEngine"), NSLOCTEXT("Contexts", "HoudiniEngine", "Houdini Engine Plugin"), NAME_None, FHoudiniEngineStyle::GetStyleSetName())
{
}


void FHoudiniEngineCommands::RegisterCommands()
{
	UI_COMMAND(RestartSession, "Restart Houdini Session...", "Restart Houdini Engine session.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenSessionSync, "Open Houdini Session Sync...", "Opens Houdini with Session Sync and connect to it.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopSession, "Stop Houdini Session...", "Stops the current Houdini Engine session.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SaveHoudiniScene, "Save Houdini Scene...", "Save current level to houdini .hip file", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OpenSceneInHoudini, "Open Scene In Houdini...", "Opens the current Houdini scene in Houdini.", EUserInterfaceActionType::Button, FInputChord(EKeys::O, EModifierKey::Control | EModifierKey::Alt));
	UI_COMMAND(CleanUpCookFolder, "Clean Up Cook Folder...", "Cleanup the legacy assets in cook folder.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Settings, "Settings", "Houdini Engine Plugin Settings", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(About, "About", "About Houdini Engine", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(IncreasePointSize, "Increase Point Size", "Increase Point Size.", EUserInterfaceActionType::Button, FInputChord(EKeys::Equals));
	UI_COMMAND(DecreasePointSize, "Decrease Point Size", "Decrease Point Size.", EUserInterfaceActionType::Button, FInputChord(EKeys::Hyphen));
	UI_COMMAND(TogglePointCoordinateShown, "Show/Hide Point Coordinate", "Show/Hide Point Coordinate.", EUserInterfaceActionType::Button, FInputChord(EKeys::O));
	UI_COMMAND(ToggleEdgeLengthShown, "Show/Hide Edge Length", "Show/Hide Edge Length.", EUserInterfaceActionType::Button, FInputChord(EKeys::L));
	UI_COMMAND(ToggleDistanceCulling, "Enable/Disable Distance Culling", "Enable/Disable Distance Culling.", EUserInterfaceActionType::Button, FInputChord(EKeys::Slash, EModifierKey::Control | EModifierKey::Alt));
	UI_COMMAND(IncreaseCullDistance, "Increase Cull Distance", "Increase Cull Distance.", EUserInterfaceActionType::Button, FInputChord(EKeys::Period));
	UI_COMMAND(DecreaseCullDistance, "Decrease Cull Distance", "Decrease Cull Distance.", EUserInterfaceActionType::Button, FInputChord(EKeys::Comma));
}

void FHoudiniEngineCommands::AsyncOpenSessionSync()
{
	FHoudiniEngine::Get().StartHoudiniTask();
	if (FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.IsBound())
		FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(LOCTEXT("OpenSessionSync", HAPI_MESSAGE_START_SESSION_SYNC));

	UE::Tasks::Launch(UE_SOURCE_LOCATION, []
		{
			if (!FHoudiniEngine::Get().HapiStartSessionSync())
			{
				AsyncTask(ENamedThreads::GameThread, []()
					{
						FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(LOCTEXT("SessionSyncFailed", "Houdini Session Sync Failed"));
					});
				FHoudiniEngine::Get().InvalidateSessionData();
			}
			else
			{
				FHoudiniEngine::Get().FinishHoudiniTask();
				AsyncTask(ENamedThreads::GameThread, []()
					{
						FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(LOCTEXT("SessionSyncStarted", "Houdini Session Sync Started"));
						FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();
					});
			}
		});
}

FORCEINLINE static void NotifyCommandExecuted(const FText& Message, const FSlateBrush* Icon)
{
	FNotificationInfo Info(Message);
	Info.FadeInDuration = 0.1f;
	Info.ExpireDuration = 1.5f;
	Info.FadeOutDuration = 0.4f;
	Info.Image = Icon;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void FHoudiniEngineCommands::SyncStopSession()
{
	if (!FHoudiniEngine::Get().IsNullSession())  // Do NOT check IsSessionValid
		FHoudiniEngine::Get().HapiStopSession();
	FHoudiniEngine::Get().InvalidateSessionData();

	NotifyCommandExecuted(LOCTEXT("HoudiniEngineCommand", "Session Stopped"),
		FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.StopSession"));
}

void FHoudiniEngineCommands::AsyncRestartSession()
{
	FHoudiniEngine::Get().StartHoudiniTask();
	FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FHoudiniEngine::Get().IsSessionValid() ?
		LOCTEXT("RestartSession", HAPI_MESSAGE_RESTART_SESSION) : LOCTEXT("CreateSession", HAPI_MESSAGE_START_SESSION));

	UE::Tasks::Launch(UE_SOURCE_LOCATION, []
		{
			if (FHoudiniEngine::Get().IsSessionValid())  // If Session is running, then we should stop and invalidate it first
				FHoudiniEngine::Get().HapiStopSession();
			FHoudiniEngine::Get().InvalidateSessionData();
			for (int32 Iter = 0; Iter < 5; ++Iter)
			{
				if (FHoudiniEngine::Get().HapiStartSession())
					break;

				UE_LOG(LogHoudiniEngine, Warning, TEXT("Failed to start Session: %d"), Iter);
			}
			
			AsyncTask(ENamedThreads::GameThread, []
				{
					FHoudiniEngine::Get().FinishHoudiniTask();
					FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();
				});
		});
}

void FHoudiniEngineCommands::AsyncOpenSceneInHoudini()
{
	const bool bIsSessionValid = FHoudiniEngine::Get().IsSessionValid();

	NotifyCommandExecuted(bIsSessionValid ?
		LOCTEXT("OpenSceneInHoudini", "Open Scene In Houdini...") : LOCTEXT("HoudiniEngineCommand", "Session Have NOT Started Yet"),
		FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.OpenSceneInHoudini"));

	if (bIsSessionValid)
	{
		FHoudiniEngine::Get().StartHoudiniTask();
		UE::Tasks::Launch(UE_SOURCE_LOCATION, []
			{
				FHoudiniEngine::Get().HapiOpenSceneInHoudini();
				AsyncTask(ENamedThreads::GameThread, [] { FHoudiniEngine::Get().FinishHoudiniTask(); });
			});
	}
}

void FHoudiniEngineCommands::AsyncSaveHoudiniScene()
{
	if (!FHoudiniEngine::Get().IsSessionValid())
		return;

	TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	TArray<FString> FilePaths;
	if (DesktopPlatform->SaveFileDialog(ParentWindowHandle, TEXT("Save Houdini Scene:"), TEXT(""), TEXT(""),
		TEXT("HIP|*.hip"), EFileDialogFlags::None, FilePaths))
	{
		if (FilePaths.IsValidIndex(0) && !FilePaths[0].IsEmpty())
		{
			FHoudiniEngine::Get().StartHoudiniTask();

			FString FilePath = FilePaths[0];
			if (FPaths::IsRelative(FilePath))
				FilePath = FPaths::ConvertRelativePathToFull(FilePath);
			if (!FilePath.EndsWith(TEXT(".hip")))
				FilePath += TEXT(".hip");

			FNotificationInfo Info(LOCTEXT("SavingHoudiniScene", "Saving Houdini Scene..."));
			Info.bFireAndForget = false;
			Info.FadeInDuration = 0.1f;
			Info.ExpireDuration = 0.5f;
			Info.FadeOutDuration = 2.0f;
			Info.Image = FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.SaveHoudiniScene");

			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);

			UE::Tasks::Launch(UE_SOURCE_LOCATION, [FilePath, Notification]
				{
					bool bSuccess = false;
					HOUDINI_FAIL_INVALIDATE(FHoudiniEngine::Get().HapiSaveSceneToFile(FilePath, bSuccess));

					AsyncTask(ENamedThreads::GameThread, [bSuccess, FilePath, Notification]
						{
							if (bSuccess)
								UE_LOG(LogHoudiniEngine, Log, TEXT("Saved Houdini Scene: %s"), *FilePath)
							else
								UE_LOG(LogHoudiniEngine, Error, TEXT("Failed To Save Houdini Scene!"))

							if (Notification.IsValid())
							{
								if (bSuccess)
								{
									Notification->SetText(LOCTEXT("SavedHoudiniScene", "Saved Houdini Scene:"));
									Notification->SetHyperlink(FSimpleDelegate::CreateStatic(
										[](FString SourceFilePath) { FPlatformProcess::ExploreFolder(*(FPaths::GetPath(SourceFilePath))); }, FilePath),
										FText::FromString(FilePath));
								}
								else
									Notification->SetText(LOCTEXT("FailedToSaveHoudiniScene", "Failed To Save Houdini Scene!"));

								Notification->ExpireAndFadeout();
							}

							FHoudiniEngine::Get().FinishHoudiniTask();
						});
				});
		}
	}
}

static int32 DeleteUnreferencedAssets(const IAssetRegistry& AssetRegistry, const TArray<FAssetData>& Assets, FScopedSlowTask& CleanupTask, const float SubProgressFrame)
{
	TArray<FAssetData> AssetsToDelete;
	for (const FAssetData& Asset : Assets)
	{
		if (CleanupTask.ShouldCancel())
			break;

		// See Editor/UnrealEd/Private/AssetDeleteModel.cpp - FPendingDelete::CheckForReferences()
		TArray<FName> DiskReferencers;
		AssetRegistry.GetReferencers(Asset.PackageName, DiskReferencers, UE::AssetRegistry::EDependencyCategory::All);
		if (DiskReferencers.IsEmpty())
		{
			UObject* Object = Asset.GetAsset();
			if (IsValid(Object))
			{
				bool bIsReferencedInMemory = false;
				bool bIsReferencedByUndo = false;
				ObjectTools::GatherObjectReferencersForDeletion(Object, bIsReferencedInMemory, bIsReferencedByUndo);
				if (!bIsReferencedInMemory)
					AssetsToDelete.Add(Asset);
			}
		}

		CleanupTask.EnterProgressFrame(SubProgressFrame);
	}

	if (!AssetsToDelete.IsEmpty())
		return ObjectTools::DeleteAssets(AssetsToDelete);

	return 0;
}

void FHoudiniEngineCommands::SyncCleanupCookFolder()
{
	FScopedSlowTask CleanupTask(1.0f, LOCTEXT("Cleanup", "Cleaning Up Houdini Engine Cook Folder..."));
	CleanupTask.MakeDialog(true);

	TArray<FAssetData> AssetDatas;
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	AssetRegistry.GetAssetsByPath(*(GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder + TEXT("Cook")), AssetDatas, true);

	TArray<FAssetData> CollisionMeshAssetDatas;
	for (int32 AssetIdx = AssetDatas.Num() - 1; AssetIdx >= 0; --AssetIdx)
	{
		if (AssetDatas[AssetIdx].AssetClassPath.GetAssetName() == UStaticMesh::StaticClass()->GetFName() &&
			AssetDatas[AssetIdx].AssetName.ToString().EndsWith(TEXT("ComplexCollision")))
		{
			CollisionMeshAssetDatas.Add(AssetDatas[AssetIdx]);
			AssetDatas.RemoveAt(AssetIdx);
		}
		else if (AssetDatas[AssetIdx].AssetClassPath.GetAssetName() == FName("DataLayerAsset"))  // We should skip DataLayerAssets, as they are soft referenced by actors
			AssetDatas.RemoveAt(AssetIdx);
	}

	// As collision mesh always referred by a staticmesh, we should delete static mesh first, then delete collision meshes
	int32 NumDeletedAssets = DeleteUnreferencedAssets(AssetRegistry, AssetDatas, CleanupTask, 1.0f / AssetDatas.Num());
	NumDeletedAssets += DeleteUnreferencedAssets(AssetRegistry, CollisionMeshAssetDatas, CleanupTask, 1.0f / AssetDatas.Num());

	NotifyCommandExecuted(FText::Format(LOCTEXT("Cleanup", "{0} Assets Cleaned Up "), NumDeletedAssets),
		FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.CleanupCookFolder"));
}

void FHoudiniEngineCommands::NavigateToSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer(FName("Project"), FName("Plugins"), FName("HoudiniEngine"));
}

void FHoudiniEngineCommands::ShowInfo()
{
	// See "Editor/MainFrame/Private/Frame/MainFrameActions.cpp" FMainFrameActionCallbacks::AboutUnrealEd_Execute

	FSlateApplication::Get().AddWindow(SNew(SWindow)
		.Title(LOCTEXT("AboutHoudiniEngine", "About Houdini Engine"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		//.ClientSize(FVector2D(400.0, 240.0))
		.Content()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16.0f, 16.0f, 16.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Unreal"))
					.DesiredSizeOverride(FVector2D(32.0, 32.0))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+ SHorizontalBox::Slot()
				.Padding(4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HoudiniEngine", "Houdini Engine For Unreal"))
				]
			]

			+ SVerticalBox::Slot()
			.Padding(16.0f)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("HoudiniEngineInfo", R"""(Copyright Yuzhe Pan (childadrianpan@gmail.com)
Version: {0}

Built With:
Houdini {1}.{2}.{3}

Found Houdini Installation:
{4})"""),           LOCTEXT("HoudiniEngineVersion", __DATE__),
					HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD,
					FText::FromString(FHoudiniEngine::Get().GetHoudiniDir())))
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			]
		]);
}

#undef LOCTEXT_NAMESPACE
