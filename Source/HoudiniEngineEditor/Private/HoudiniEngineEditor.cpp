// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineEditor.h"

#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniNode.h"
#include "HoudiniCurvesComponent.h"
#include "HoudiniMeshComponent.h"

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEngineCommands.h"
#include "HoudiniAssetTypeActions.h"
#include "HoudiniAttributeParameterHolder.h"
#include "HoudiniNodeFactory.h"
#include "HoudiniNodeDetails.h"
#include "HoudiniAttributeParameterDetails.h"
#include "HoudiniToolsDetails.h"
#include "HoudiniEditableGeometryEditorUtils.h"
#include "HoudiniCurvesComponentVisualizer.h"
#include "HoudiniMeshComponentVisualizer.h"
#include "HoudiniEngineModeCommands.h"


#define LOCTEXT_NAMESPACE "HoudiniEngineEditor"

FHoudiniEngineEditor* FHoudiniEngineEditor::HoudiniEngineEditorInstance = nullptr;

void FHoudiniEngineEditor::StartupModule()
{
	HoudiniEngineEditorInstance = this;

	FHoudiniEngineStyle::Initialize();

	RegisterAssetTypeActions();

	RegisterActorFactories();

	RegisterDetails();

	RegisterEditorDelegates();

	RegisterCommands();

	FHoudiniEngineModeCommands::Register();

	//RegisterComponentVisualizers();  // We should NOT register comp-vis here because GUnrealEd haven't been initialized yet.
}

void FHoudiniEngineEditor::ShutdownModule()
{
	UnregisterComponentVisualizers();

	FHoudiniEngineModeCommands::Unregister();

	UnregisterCommands();

	UnregisterEditorDelegates();

	UnregisterDetails();

	UnregisterActorFactories();

	UnregisterAssetTypeActions();

	FHoudiniEngineStyle::Shutdown();

	FHoudiniEditableGeometryVisualSettings::Save();

	HoudiniEngineEditorInstance = nullptr;
}

void FHoudiniEngineEditor::RegisterAssetTypeActions()
{
	HoudiniAssetTypeActions = new FHoudiniAssetTypeActions();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShareable(HoudiniAssetTypeActions));
}

void FHoudiniEngineEditor::UnregisterAssetTypeActions()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(HoudiniAssetTypeActions->AsShared());
}

void FHoudiniEngineEditor::RegisterActorFactories()
{
	if (GEditor)
		GEditor->ActorFactories.Add(NewObject<UHoudiniNodeFactory>());
}

void FHoudiniEngineEditor::UnregisterActorFactories()
{
	if (GEditor)
		GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return (ActorFactory->GetClass() == UHoudiniNodeFactory::StaticClass()); });
}

void FHoudiniEngineEditor::RegisterEditorDelegates()
{
	HoudiniNodeEventsHandle = FHoudiniEngine::Get().HoudiniNodeEvents.AddLambda([](AHoudiniNode* Node, const EHoudiniNodeEvent Event)
		{
			if (Event == EHoudiniNodeEvent::FinishCook || Event == EHoudiniNodeEvent::FinishInstantiate)
				FHoudiniEngineEditorUtils::RefreshNodeEditor(Node);
			else if (Event == EHoudiniNodeEvent::StartCook || Event == EHoudiniNodeEvent::Destroy)
				FHoudiniEngineEditor::Get().GetCurvesComponentVisualizer()->TryEndDrawingForParentNode(Node);
		});

	// Register notification
	HoudiniAsyncTaskMessageHandle = FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.AddLambda([](const FText& Message)
		{
			if (Message.IsEmpty())  // End Message
			{
				if (FHoudiniEngineEditor::Get().Notification.IsValid())
				{
					FHoudiniEngineEditor::Get().Notification.Pin()->ExpireAndFadeout();
					FHoudiniEngineEditor::Get().Notification.Reset();
				}
			}
			else
			{
				if (FHoudiniEngineEditor::Get().Notification.IsValid())
					FHoudiniEngineEditor::Get().Notification.Pin()->SetText(Message);
				else
				{
					FNotificationInfo Info(Message);
					Info.bFireAndForget = false;
					Info.FadeInDuration = 0.1f;
					Info.ExpireDuration = 0.3f;
					Info.FadeOutDuration = 0.5f;

					// We could specify notification icon base on Message string
					const FString& MessageStr = Message.ToString();
					if (MessageStr.Equals(TEXT(HAPI_MESSAGE_START_SESSION)))
						Info.Image = FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngine");
					else if (MessageStr.Equals(TEXT(HAPI_MESSAGE_RESTART_SESSION)))
						Info.Image = FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.RestartSession");
					else if (MessageStr.Equals(TEXT(HAPI_MESSAGE_START_SESSION_SYNC)))
						Info.Image = FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.OpenSessionSync");
					else  // When node Instantiate and Cook
					{
						FString NodeLabel;
						FString NodeMessage;
						if (MessageStr.Split(TEXT(": "), &NodeLabel, &NodeMessage))
						{
							// TODO: Grab HoudiniAsset thumbnail if it specified
							Info.Image = FHoudiniEngineStyle::Get()->GetBrush("ClassIcon.HoudiniNode");
						}
						else
							Info.Image = FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.HoudiniEngine");
					}

					FHoudiniEngineEditor::Get().Notification = FSlateNotificationManager::Get().AddNotification(Info);
				}
			}
		});

	// Register progress bar
	HoudiniMainTaskMessageHandle = FHoudiniEngine::Get().HoudiniMainTaskMessageEvent.AddLambda([](const float& Progress, const FText& Message)
		{
			if (Progress >= 0.0f)
			{
				if (FHoudiniEngineEditor::Get().ScopedSlowTask)
					FHoudiniEngineEditor::Get().ScopedSlowTask->EnterProgressFrame(Progress, Message);
				else
				{
					FHoudiniEngineEditor::Get().ScopedSlowTask = new FScopedSlowTask(1.0f, Message);
					FHoudiniEngineEditor::Get().ScopedSlowTask->MakeDialogDelayed(1.0f);
					FHoudiniEngineEditor::Get().ScopedSlowTask->EnterProgressFrame(Progress, Message);
				}
			}
			else  // End Message
			{
				if (FHoudiniEngineEditor::Get().ScopedSlowTask)
				{
					delete FHoudiniEngineEditor::Get().ScopedSlowTask;
					FHoudiniEngineEditor::Get().ScopedSlowTask = nullptr;
				}
			}
		});

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddLambda([](const FString& Filename, bool /*bAsTemplate*/)
		{
			FHoudiniEngineEditor::Get().RegisterComponentVisualizers();

			FHoudiniEngineEditor::Get().RegisterLandscapeEditorDelegate();

			if (!FHoudiniEngine::Get().IsNullSession())  // Update level path folder env var
			{
				HAPI_Result Result = FHoudiniApi::SetServerEnvString(FHoudiniEngine::Get().GetSession(),
					HAPI_ENV_CLIENT_SCENE_PATH, TCHAR_TO_UTF8(*GEditor->GetEditorWorldContext().World()->GetCurrentLevel()->GetPathName()));
				if (HAPI_SESSION_INVALID_RESULT(Result))
					FHoudiniEngine::Get().InvalidateSessionData();
			}
		});

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddLambda([](const bool)
		{
			FHoudiniEngine::Get().RegisterWorld(GEditor->GetEditorWorldContext().World());
		});
}

void FHoudiniEngineEditor::UnregisterEditorDelegates()
{
	if (OnEndPIEHandle.IsValid())
		FEditorDelegates::EndPIE.Remove(OnEndPIEHandle);

	if (OnMapOpenedHandle.IsValid())
		FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);

	if (HoudiniNodeEventsHandle.IsValid())
		FHoudiniEngine::Get().HoudiniNodeEvents.Remove(HoudiniNodeEventsHandle);

	if (HoudiniAsyncTaskMessageHandle.IsValid())
		FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Remove(HoudiniAsyncTaskMessageHandle);

	if (Notification.IsValid())
	{
		FHoudiniEngineEditor::Get().Notification.Pin()->ExpireAndFadeout();
		FHoudiniEngineEditor::Get().Notification.Reset();
	}

	if (HoudiniMainTaskMessageHandle.IsValid())
		FHoudiniEngine::Get().HoudiniMainTaskMessageEvent.Remove(HoudiniMainTaskMessageHandle);

	if (ScopedSlowTask)
	{
		delete ScopedSlowTask;
		ScopedSlowTask = nullptr;
	}
}

void FHoudiniEngineEditor::RegisterDetails()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(AHoudiniNode::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniNodeDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(AHoudiniNodeProxy::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniNodeDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(UHoudiniAttributeParameterHolder::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniAttributeParameterDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout("HoudiniManageTool",
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniManageToolDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout("HoudiniMaskTool",
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniMaskToolDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout("HoudiniEditTool",
		FOnGetDetailCustomizationInstance::CreateStatic(&FHoudiniEditToolDetails::MakeInstance));

	// Houdini Category Section
	TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(
		AHoudiniNode::StaticClass()->GetFName(), "Houdini", LOCTEXT("Houdini", "Houdini"));
	Section->AddCategory(FHoudiniNodeDetails::NodeCategoryName);
	Section->AddCategory(FHoudiniNodeDetails::PDGCategoryName);
	Section->AddCategory(FHoudiniNodeDetails::ParametersCategoryName);
	Section->AddCategory(FHoudiniNodeDetails::InputsCategoryName);
}

void FHoudiniEngineEditor::UnregisterDetails()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.UnregisterCustomClassLayout(AHoudiniNode::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(AHoudiniNodeProxy::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UHoudiniAttributeParameterHolder::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout("HoudiniManageTool");
		PropertyModule.UnregisterCustomClassLayout("HoudiniMaskTool");

		PropertyModule.RemoveSection(AHoudiniNode::StaticClass()->GetFName(), "Houdini");
	}
}

void FHoudiniEngineEditor::RegisterCommands()
{
	FHoudiniEngineCommands::Register();

	HoudiniEngineCommandList = MakeShareable(new FUICommandList);
	
	// Register menu commands
	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().RestartSession,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::AsyncRestartSession),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngine::Get().AllowEdit(); }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().OpenSessionSync,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::AsyncOpenSessionSync),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngine::Get().AllowEdit(); }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().StopSession,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::SyncStopSession),
		FCanExecuteAction::CreateLambda([]() { return !FHoudiniEngine::Get().IsNullSession(); }));
	
	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().SaveHoudiniScene,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::AsyncSaveHoudiniScene),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngine::Get().AllowEdit() && !FHoudiniEngine::Get().IsNullSession(); }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().OpenSceneInHoudini,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::AsyncOpenSceneInHoudini),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngine::Get().AllowEdit() && !FHoudiniEngine::Get().IsNullSession(); }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().CleanUpCookFolder,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::SyncCleanupCookFolder),
		FCanExecuteAction::CreateLambda([]() { return FHoudiniEngine::Get().AllowEdit(); }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().Settings,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::NavigateToSettings),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().About,
		FExecuteAction::CreateStatic(&FHoudiniEngineCommands::ShowInfo),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(HoudiniEngineCommandList.ToSharedRef());

	// -------- Houdini Engine Menu --------
	MainMenuExtender = MakeShareable(new FExtender);
	MainMenuExtender->AddMenuBarExtension(
		"Edit",
		EExtensionHook::After,
		HoudiniEngineCommandList,
		FMenuBarExtensionDelegate::CreateLambda([](FMenuBarBuilder& MenuBarBuilder)
			{
				MenuBarBuilder.AddPullDownMenu(
					LOCTEXT("HoudiniLabel", "Houdini Engine"),
					LOCTEXT("HoudiniMenu_ToolTip", "Open the Houdini Engine menu"),
					FNewMenuDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
						{
							MenuBuilder.BeginSection("Session", LOCTEXT("SessionLabel", "Session"));
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().RestartSession);
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().OpenSessionSync);
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().StopSession);
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().SaveHoudiniScene);
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().OpenSceneInHoudini);
							MenuBuilder.EndSection();

							MenuBuilder.BeginSection("Asset", LOCTEXT("AssetLabel", "Asset"));
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().CleanUpCookFolder);
							MenuBuilder.EndSection();

							MenuBuilder.BeginSection("Plugin", LOCTEXT("PluginLabel", "Plugin"));
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().Settings);
							MenuBuilder.AddMenuEntry(FHoudiniEngineCommands::Get().About);
							MenuBuilder.EndSection();
						}),
					"View");
			}));

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MainMenuExtender);


	// -------- ComponentVisualizer shortcut --------
	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().IncreasePointSize,
		FExecuteAction::CreateLambda([]()
			{
				float& PointSize = FHoudiniEditableGeometryVisualSettings::Load()->PointSize;
				PointSize = FMath::Clamp(PointSize + 3.0f, 3.0f, 60.0f);
			}),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().DecreasePointSize,
		FExecuteAction::CreateLambda([]()
			{
				float& PointSize = FHoudiniEditableGeometryVisualSettings::Load()->PointSize;
				PointSize = FMath::Clamp(PointSize - 3.0f, 3.0f, 60.0f);
			}),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().TogglePointCoordinateShown,
		FExecuteAction::CreateLambda([]() { FHoudiniEditableGeometryVisualSettings::Load()->bShowPointCoordinate = !FHoudiniEditableGeometryVisualSettings::Load()->bShowPointCoordinate; }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().ToggleEdgeLengthShown,
		FExecuteAction::CreateLambda([]() { FHoudiniEditableGeometryVisualSettings::Load()->bShowEdgeLength = !FHoudiniEditableGeometryVisualSettings::Load()->bShowEdgeLength; }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().ToggleDistanceCulling,
		FExecuteAction::CreateLambda([]() { FHoudiniEditableGeometryVisualSettings::Load()->bDistanceCulling = !FHoudiniEditableGeometryVisualSettings::Load()->bDistanceCulling; }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().IncreaseCullDistance,
		FExecuteAction::CreateLambda([]() { FHoudiniEditableGeometryVisualSettings::Load()->CullDistance *= FMath::Sqrt(2.0f); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));

	HoudiniEngineCommandList->MapAction(FHoudiniEngineCommands::Get().DecreaseCullDistance,
		FExecuteAction::CreateLambda([]() { FHoudiniEditableGeometryVisualSettings::Load()->CullDistance *= FMath::Sqrt(0.5f); }),
		FCanExecuteAction::CreateLambda([]() { return true; }));



	// -------- Houdini Node Menu --------
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	MenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateLambda(
		[](const TSharedRef<FUICommandList> CommandList, const TArray<AActor*>& SelectedActors)
		{
			TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

			TArray<TWeakObjectPtr<AHoudiniNode>> SelectedNodes;
			for (AActor* SelectedActor : SelectedActors)
			{
				AHoudiniNode* SelectedNode = Cast<AHoudiniNode>(SelectedActor);
				if (IsValid(SelectedNode))
					SelectedNodes.Add(SelectedNode);
			}

			if (SelectedNodes.IsEmpty())
				return Extender;
			
			Extender->AddMenuExtension(
				"ActorControl",
				EExtensionHook::After,
				CommandList,
				FMenuExtensionDelegate::CreateLambda([SelectedNodes](FMenuBuilder& MenuBuilder)
					{
						MenuBuilder.BeginSection("HoudiniNode", LOCTEXT("HoudiniNodeActions", "Houdini Node"));

						MenuBuilder.AddMenuEntry(
							LOCTEXT("Cook", "Cook"),
							FText::GetEmpty(),
							FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.Cook"),
							FUIAction(
								FExecuteAction::CreateLambda([SelectedNodes]()
									{
										for (const TWeakObjectPtr<AHoudiniNode>& SelectedNode : SelectedNodes)
										{
											if (SelectedNode.IsValid())
												SelectedNode->RequestCook();
										}
									}),
								FCanExecuteAction::CreateLambda([] { return true; })
							));

						MenuBuilder.AddMenuEntry(
							LOCTEXT("Rebuild", "Rebuild"),
							FText::GetEmpty(),
							FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.Rebuild"),
							FUIAction(
								FExecuteAction::CreateLambda([SelectedNodes]()
									{
										for (const TWeakObjectPtr<AHoudiniNode>& SelectedNode : SelectedNodes)
										{
											if (SelectedNode.IsValid())
												SelectedNode->RequestRebuild();
										}
									}),
								FCanExecuteAction::CreateLambda([] { return true; })
							));

						MenuBuilder.EndSection();
					}));

			return Extender;
		}));

	LevelViewportExtenderHandle = MenuExtenders.Last().GetHandle();
}

void FHoudiniEngineEditor::UnregisterCommands()
{
	if (LevelViewportExtenderHandle.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor");
			LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll(
				[this](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate)
				{
					return Delegate.GetHandle() == LevelViewportExtenderHandle;
				});
		}
		LevelViewportExtenderHandle.Reset();
	}
}

void FHoudiniEngineEditor::RegisterComponentVisualizers()
{
	if (GUnrealEd)
	{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
		// See "Editor/UnrealEd/Private/ComponentVisualizerManager.cpp" Line 18
		// Set to false, so that edit curve and mesh can be selected directly when select another component
		if (!CurvesComponentVisualizer.IsValid() || !MeshComponentVisualizer.IsValid())
			IConsoleManager::Get().FindConsoleVariable(TEXT("Editor.ComponentVisualizer.AutoSelectComponent"))->Set(false);
#endif

		if (!CurvesComponentVisualizer.IsValid())
		{
			CurvesComponentVisualizer = MakeShareable<FHoudiniCurvesComponentVisualizer>(new FHoudiniCurvesComponentVisualizer);
			GUnrealEd->RegisterComponentVisualizer(UHoudiniCurvesComponent::StaticClass()->GetFName(), CurvesComponentVisualizer);
			CurvesComponentVisualizer->OnRegister();
		}

		if (!MeshComponentVisualizer.IsValid())
		{
			MeshComponentVisualizer = MakeShareable<FHoudiniMeshComponentVisualizer>(new FHoudiniMeshComponentVisualizer);
			GUnrealEd->RegisterComponentVisualizer(UHoudiniMeshComponent::StaticClass()->GetFName(), MeshComponentVisualizer);
			MeshComponentVisualizer->OnRegister();
		}
	}
}

void FHoudiniEngineEditor::UnregisterComponentVisualizers()
{
	UHoudiniAttributeParameterHolder::Reset();

	if (GUnrealEd)
	{
		if (CurvesComponentVisualizer.IsValid())
			GUnrealEd->UnregisterComponentVisualizer(UHoudiniCurvesComponent::StaticClass()->GetFName());

		if (MeshComponentVisualizer.IsValid())
			GUnrealEd->UnregisterComponentVisualizer(UHoudiniMeshComponent::StaticClass()->GetFName());
	}
}

UHoudiniEditableGeometry* FHoudiniEngineEditor::GetSelectedGeometry() const
{
	UHoudiniEditableGeometry* EditGeo = CurvesComponentVisualizer.IsValid() ? CurvesComponentVisualizer->GetSelectedGeometry<UHoudiniEditableGeometry>() : nullptr;
	if (!EditGeo)
		EditGeo = MeshComponentVisualizer ? MeshComponentVisualizer->GetSelectedGeometry<UHoudiniEditableGeometry>() : nullptr;
	return (IsValid(EditGeo) ? EditGeo : nullptr);
}

UHoudiniEditableGeometry* FHoudiniEngineEditor::GetEditingGeometry() const
{
	UHoudiniEditableGeometry* EditGeo = CurvesComponentVisualizer.IsValid() ? CurvesComponentVisualizer->GetEditingGeometry<UHoudiniEditableGeometry>() : nullptr;
	if (!EditGeo)
		EditGeo = MeshComponentVisualizer ? MeshComponentVisualizer->GetEditingGeometry<UHoudiniEditableGeometry>() : nullptr;
	return (IsValid(EditGeo) ? EditGeo : nullptr);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHoudiniEngineEditor, HoudiniEngineEditor)
