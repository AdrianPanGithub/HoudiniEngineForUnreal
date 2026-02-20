// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineEditorUtils.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Engine/Selection.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "SAssetView.h"
#include "PropertyCustomizationHelpers.h"
#include "ContentBrowserModule.h"

#include "HoudiniEngine.h"
#include "HoudiniNode.h"
#include "HoudiniOutput.h"
#include "HoudiniEditableGeometry.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniAttributeParameterHolder.h"
#include "HoudiniEditableGeometryEditorUtils.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void FHoudiniEngineEditorUtils::RefreshNodeEditor(const AHoudiniNode* Node)
{
	// Reselect selected actors, for refresh component visualizers, and refresh detail panel component view
	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	Selection->GetSelectedObjects(SelectedActors);
	if (SelectedActors.Contains(Node))
	{
		FHoudiniEditableGeometryEditingScope EditingScope;  // As reselect actors will deactivate editing comp vis, so we need this scope to recover it

		GEditor->SelectNone(false, false, false);
		for (AActor* NextSelected : SelectedActors)
			GEditor->SelectActor(NextSelected, true, true, true, true);
	}
	
	// Check if the state of UHoudiniEditableGeometry::CanDropAssets changed, we may need register FHoudiniEditableGeometryAssetDrop
	if (UHoudiniEditableGeometry* EditGeo = FHoudiniEngineEditor::Get().GetEditingGeometry())
	{
		if (EditGeo->CanDropAssets())
			FHoudiniEditableGeometryAssetDrop::Register();
		else
			FHoudiniEditableGeometryAssetDrop::Unregister();
	}
	else
		FHoudiniEditableGeometryAssetDrop::Unregister();

	// Refresh node's parameter panel
	FPropertyEditorModule& PropertyModule = FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");
	static const FName DetailsViewIdentifiers[] =
	{
		"LevelEditorSelectionDetails",
		"LevelEditorSelectionDetails2",
		"LevelEditorSelectionDetails3",
		"LevelEditorSelectionDetails4"
	};

	for (const FName& DetailsViewIdentifier : DetailsViewIdentifiers)
	{
		TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsViewIdentifier);
		if (DetailsView.IsValid() && DetailsView->GetSelectedObjects().Contains(Node))
			DetailsView->ForceRefresh();
	}
	
	bool bOutputPostProcessed = false;
	// Refresh parameter attribute panel
	UHoudiniAttributeParameterHolder* AttribParmHolder = UHoudiniAttributeParameterHolder::Get();
	if (TSharedPtr<IDetailsView> DetailsView = AttribParmHolder->DetailsView.Pin())
	{
		if (DetailsView->GetSelectedObjects().Contains(UHoudiniAttributeParameterHolder::Get()))
		{
			if (AttribParmHolder->RefreshParameters(Node))
			{
				DetailsView->ForceRefresh();

				if (AttribParmHolder->IsRightClickPanelOpen())
				{
					// Do not allow right-click detail view to disappear
					FWidgetPath AttribPanelPath;
					FSlateApplication::Get().FindPathToWidget(DetailsView.ToSharedRef(), AttribPanelPath);
					FSlateApplication::Get().SetUserFocus(0, AttribPanelPath, EFocusCause::WindowActivate);
				}
			}
		}

		if (AttribParmHolder->IsRightClickPanelOpen())
		{
			for (const TSharedPtr<IHoudiniOutputBuilder>& OutputBuilder : FHoudiniEngine::Get().GetOutputBuilders())
				OutputBuilder->PostProcess(Node, true);

			bOutputPostProcessed = true;
		}
	}

	if (!bOutputPostProcessed)
	{
		for (const TSharedPtr<IHoudiniOutputBuilder>& OutputBuilder : FHoudiniEngine::Get().GetOutputBuilders())
			OutputBuilder->PostProcess(Node, false);
	}

	// Allow viewport to show new landscape layers and meshes
	GEditor->RedrawLevelEditingViewports(true);
}

void FHoudiniEngineEditorUtils::ReselectSelectedActors()
{
	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	Selection->GetSelectedObjects(SelectedActors);

	GEditor->SelectNone(false, false, false);

	for (AActor* NextSelected : SelectedActors)
		GEditor->SelectActor(NextSelected, true, true, true, true);
}

FEditorViewportClient* FHoudiniEngineEditorUtils::FindEditorViewportClient(const UWorld* CurrWorld)
{
	for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
	{
		if (ViewportClient->GetWorld() == CurrWorld)
			return ViewportClient;
	}

	return nullptr;
}

void FHoudiniEngineEditorUtils::DisableOverrideEngineShowFlags()
{
	// See "Editor/Experimental/EditorInteractiveToolsFramework/Private/EdModeInteractiveToolsContext.cpp" Line 772
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();
	if (LevelEditor.IsValid())
	{
		TArray<TSharedPtr<SLevelViewport>> Viewports = LevelEditor->GetViewports();
		for (const TSharedPtr<SLevelViewport>& ViewportWindow : Viewports)
		{
			if (ViewportWindow.IsValid())
			{
				FEditorViewportClient& Viewport = ViewportWindow->GetAssetViewportClient();
				Viewport.DisableOverrideEngineShowFlags();
			}
		}
	}
}

TSharedPtr<SWidget> FHoudiniEngineEditorUtils::GetAttributePanel(UHoudiniEditableGeometry* EditGeo)
{
	UHoudiniAttributeParameterHolder* AttribParmHolder = UHoudiniAttributeParameterHolder::Get(EditGeo);

	// Create detail panel
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bAllowSearch = AttribParmHolder->GetParameters().Num() >= 20;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = UHoudiniAttributeParameterHolder::DetailsName;

	TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(AttribParmHolder);  // Set AttribParmHolder as EditingObject, which will generate widgets and cal DisplayHeight

	// Get the distance between cursor - bottom, we should consider multi-displays that have different resolutions
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	float WorkAreaHeight = DisplayMetrics.GetMonitorWorkAreaFromPoint(CursorPos).Bottom;
	if (WorkAreaHeight == 0.0) WorkAreaHeight = DisplayMetrics.PrimaryDisplayHeight - 80.0;
	
	const float DistToBottom = abs((WorkAreaHeight - CursorPos.Y) /
		FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(CursorPos.X, CursorPos.Y) - 10.0);

	// Wrap detail panel if too short to bottom, so that the scroll-bar will display
	return (DistToBottom < AttribParmHolder->GetDisplayHeight() + (DetailsViewArgs.bAllowSearch ? 44.0f : 0.0f)) ? 
		TSharedPtr<SWidget>(SNew(SBox).HeightOverride(DistToBottom) [ DetailsPanel ]) : TSharedPtr<SWidget>(DetailsPanel);
}

TSharedRef<SWidget> FHoudiniEngineEditorUtils::CreateTitleSeparatorWidget(const FText& Text)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(SSeparator)
			.SeparatorImage(FCoreStyle::Get().GetBrush("Header.Pre"))
			.Orientation(Orient_Horizontal)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Padding(-10.0f, 10.0f)
		[
			SNew(STextBlock)
			.Text(Text)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(SSeparator)
			.SeparatorImage(FCoreStyle::Get().GetBrush("Header.Post"))
			.Orientation(Orient_Horizontal)
		];
}

TSharedRef<SWidget> FHoudiniEngineEditorUtils::MakePresetWidget(const TWeakInterfacePtr<IHoudiniPresetHandler> PresetHandler)
{
	TSharedRef<SComboButton> PresetButton = SNew(SComboButton)
		.HasDownArrow(false)
		.ContentPadding(0)
		.ForegroundColor(FSlateColor::UseForeground())
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
		.ButtonContent()
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
			.DesiredSizeOverride(FVector2D(16.0, 16.0))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		.ToolTipText(LOCTEXT("Preset", "Preset"));;

	PresetButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([PresetButton, PresetHandler]()
		{
			FMenuBuilder MenuBuilder(true, nullptr);

			MenuBuilder.BeginSection(NAME_None, LOCTEXT("SavePreset", "Save Preset"));
			MenuBuilder.AddWidget(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(12.0f, 2.0f, 4.0f, 2.0f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("PresetName", "Preset Name"))
				]

				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text_Lambda([PresetHandler]() { return PresetHandler.IsValid() ? FText::FromString(PresetHandler->GetPresetName()) : FText::GetEmpty(); })
					.OnTextCommitted_Lambda([PresetHandler](const FText& NewText, ETextCommit::Type TextCommitType) { PresetHandler->SetPresetName(NewText.ToString()); })
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.Padding(2.0f)
				.AutoWidth()
				[
					PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateLambda([PresetHandler]()
						{
							TArray<FAssetData> Selections;
							GEditor->GetContentBrowserSelections(Selections);

							for (const FAssetData& Selection : Selections)
							{
								if (Selection.IsValid() && Selection.AssetClassPath.GetAssetName() == UDataTable::StaticClass()->GetFName())
								{
									UDataTable* Preset = Cast<UDataTable>(Selection.GetAsset());
									if (IsValid(Preset) && (Preset->RowStruct == FHoudiniGenericParameter::StaticStruct()))
									{
										PresetHandler->LoadPreset(Preset);
										break;
									}
								}
							}
						}),
						LOCTEXT("PickPresetFromContentBrowser", "Pick Preset From Content Browser"))
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.Padding(2.0f)
				.AutoWidth()
				[
					PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateLambda([PresetHandler]()
						{
							const FString PresetFolderPath = PresetHandler->GetPresetFolder();
							if (PresetHandler->GetPresetName().IsEmpty())
							{
								FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(
									FSoftObjectPath(PresetFolderPath / PresetHandler->GetPresetName() + TEXT(".") + PresetHandler->GetPresetName()));
								if (AssetData.IsValid())
								{
									GEditor->SyncBrowserToObjects(TArray<FAssetData>{ AssetData });
									return;
								}
							}

							FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
							ContentBrowserModule.Get().SyncBrowserToFolders(TArray<FString>{ PresetFolderPath });
						}),
						LOCTEXT("BrowsePresetFolderInContentBrowser", "Browse Preset Folder in Content Browser"))
				]

				+ SHorizontalBox::Slot()
				.Padding(0.0f, 2.0f, 8.0f, 2.0f)
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.IsFocusable(false)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Save"))
						.DesiredSizeOverride(FVector2D(16.0, 16.0))
					]
					.OnClicked_Lambda([PresetHandler]() { PresetHandler->SavePreset(PresetHandler->GetPresetName()); return FReply::Handled(); })
				],
				FText::GetEmpty(), false);
			MenuBuilder.EndSection();

			const FString PresetPathFilter = PresetHandler->GetPresetPathFilter();
			MenuBuilder.BeginSection(NAME_None, LOCTEXT("LoadPreset", "Load Preset"));
			{
				TSharedPtr<SAssetView> PresetView;

				MenuBuilder.AddWidget(SNew(SBox)
				.WidthOverride(300.0f)
				.HeightOverride(240.0f)
				[
					SAssignNew(PresetView, SAssetView)  //ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					.ShowViewOptions(false)
					.ShowBottomToolbar(false)
					.InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAssets)
					.InitialViewType(EAssetViewType::List)
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 6)) || (ENGINE_MAJOR_VERSION < 5)
					.InitialThumbnailSize(EThumbnailSize::Small)
#endif
					.AssetShowWarningText(FText::GetEmpty())
					.FilterRecursivelyWithBackendFilter(false)
					.AllowFocusOnSync(false)
					.SelectionMode(ESelectionMode::Single)
					.OnShouldFilterAsset_Lambda([PresetPathFilter](const FAssetData& AssetData)
						{
							if (AssetData.AssetClassPath.GetAssetName() != UDataTable::StaticClass()->GetFName())
								return true;
							return !AssetData.PackagePath.ToString().EndsWith(PresetPathFilter);
						})
					.OnItemSelectionChanged_Lambda([PresetButton, PresetHandler](const FContentBrowserItem& InSelectedItem, ESelectInfo::Type InSelectInfo)
						{
							FAssetData ItemAssetData;
							InSelectedItem.Legacy_TryGetAssetData(ItemAssetData);
							const UDataTable* Preset = Cast<UDataTable>(ItemAssetData.GetAsset());
							if (IsValid(Preset))
							{
								PresetHandler->SetPresetName(Preset->GetName());
								PresetHandler->LoadPreset(Preset);
							}

							if (PresetButton->IsOpen())
								PresetButton->SetIsOpen(false);
						})
				],
				FText::GetEmpty(), false);

				PresetView->RequestSlowFullListRefresh();
			}
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}));

	return PresetButton;
}

TSharedRef<SWidget> FHoudiniEngineEditorUtils::MakeCopyButton(const TWeakInterfacePtr<IHoudiniPresetHandler> Object)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsFocusable(false)
		.Content()
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("GenericCommands.Copy"))
			.DesiredSizeOverride(FVector2D(12.0, 12.0))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		.OnClicked_Lambda([Object]()
			{
				FJsonDataBag DataBag;
				DataBag.JsonObject = Object->ConvertToPresetJson();
				if (DataBag.JsonObject)
					FPlatformApplicationMisc::ClipboardCopy(*DataBag.ToJson());

				return FReply::Handled();
			})
		.ToolTipText(LOCTEXT("CopyAllParameters", "Copy All Parameters"));
}

TSharedRef<SWidget> FHoudiniEngineEditorUtils::MakePasteButton(const TWeakInterfacePtr<IHoudiniPresetHandler> Object)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsFocusable(false)
		.Content()
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("GenericCommands.Paste"))
			.DesiredSizeOverride(FVector2D(12.0, 12.0))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		.OnClicked_Lambda([Object]()
			{
				FString JsonStr;
				FPlatformApplicationMisc::ClipboardPaste(JsonStr);

				TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonStr);
				TSharedPtr<FJsonObject> JsonParms;
				if (FJsonSerializer::Deserialize(JsonReader, JsonParms))
					Object->SetFromPresetJson(JsonParms);

				return FReply::Handled();
			})
		.ToolTipText(LOCTEXT("PasteParameters", "Paste Parameters"));
}

#undef LOCTEXT_NAMESPACE
