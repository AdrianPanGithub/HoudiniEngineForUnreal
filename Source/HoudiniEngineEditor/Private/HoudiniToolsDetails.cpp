// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniToolsDetails.h"

#include "EngineUtils.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SExpandableArea.h"

#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "SAssetDropTarget.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetThumbnail.h"
#include "EditorActorFolders.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniNode.h"
#include "HoudiniInputs.h"
#include "HoudiniParameter.h"
#include "HoudiniCurvesComponent.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniTools.h"
#include "HoudiniMaskGizmoActiveActor.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniCurvesComponentVisualizer.h"
#include "HoudiniAttributeParameterHolder.h"
#include "HoudiniEditableGeometryEditorUtils.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE


#define NODE_ICON_SIZE 32
#define CURVE_INPUT_ICON_SIZE 24
#define ADD_CURVE_ICON_SIZE 16


static TSharedRef<SButton> CreateDrawButton(const TWeakObjectPtr<UHoudiniInput>& Input,
	const FSlateBrush* Icon, const EHoudiniCurveDrawingType& DrawingType)
{
	return SNew(SButton)
		//.ToolTipText(FText::FromString("Add A " + MainInput->GetLabel()))
		.ButtonColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
		.ContentPadding(FMargin(-4.0, 2.0))
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(ADD_CURVE_ICON_SIZE, ADD_CURVE_ICON_SIZE))
				.ColorAndOpacity(Input->GetSettings().DefaultCurveColor)
				.Image(Icon)
			]
		]
		.OnClicked(FOnClicked::CreateLambda([Input, DrawingType]()
			{
				if (!Input.IsValid())
					return FReply::Handled();

				TArray<TObjectPtr<UHoudiniInputHolder>>& Holders = Input->Holders;
				if (!Holders.IsValidIndex(0))
					Holders.Add(UHoudiniInputCurves::Create(Input.Get()));

				FHoudiniEngineEditor::Get().GetCurvesComponentVisualizer()->AddCurve(
					Cast<UHoudiniInputCurves>(Holders[0])->GetCurvesComponent(), DrawingType, true);

				return FReply::Handled();
			}));
}

static const FSlateBrush* GetEngineVisibilityIconBrush(const bool bShown)
{
	static const FSlateBrush* ShowIcon = nullptr;
	static const FSlateBrush* HideIcon = nullptr;
	if (bShown)
	{
		if (!ShowIcon)
			ShowIcon = FAppStyle::Get().GetBrush("Icons.Visible");
		return ShowIcon;
	}

	if (!HideIcon)
		HideIcon = FAppStyle::Get().GetBrush("Icons.Hidden");
	return HideIcon;
}

void ConstructHoudiniNodeList(IDetailCategoryBuilder& HoudiniNodeListCategoryBuilder,
	const TArray<TPair<FString, TWeakObjectPtr<AHoudiniNode>>>& DisplayNodes)
{
	for (const TPair<FString, TWeakObjectPtr<AHoudiniNode>>& DisplayNode : DisplayNodes)
	{
		const FString& DisplayStr = DisplayNode.Key;
		const TWeakObjectPtr<AHoudiniNode>& Node = DisplayNode.Value;

		TSharedPtr<SWidget> IconWidget;
		if (UPackage* HoudiniAssetPackage = Cast<UPackage>(Node->GetAsset()->GetOuter()))  // Try use the HoudiniAsset Thumbnail if specified
		{
			if (HoudiniAssetPackage->HasThumbnailMap() && !HoudiniAssetPackage->GetThumbnailMap().IsEmpty())
			{
				TSharedPtr<FAssetThumbnail> AssetThumbnail =
					MakeShareable(new FAssetThumbnail(FAssetData(Node->GetAsset()), NODE_ICON_SIZE, NODE_ICON_SIZE,
					HoudiniNodeListCategoryBuilder.GetParentLayout().GetThumbnailPool()));
				IconWidget = SNew(SBox)
					.WidthOverride(NODE_ICON_SIZE)
					.HeightOverride(NODE_ICON_SIZE)
					[
						AssetThumbnail->MakeThumbnailWidget()
					];
			}
		}

		if (!IconWidget.IsValid())  // If no thumbnail specified on HoudiniAsset, then fallback to use "HoudiniNode" icon
		{
			IconWidget = SNew(SImage)
				.DesiredSizeOverride(FVector2D(NODE_ICON_SIZE, NODE_ICON_SIZE))
				.Image(FHoudiniEngineStyle::Get()->GetBrush("ClassIcon.HoudiniNode"));
		}

		TSharedPtr<SHorizontalBox> NodeButtonContentBox;

		TSharedRef<SButton> NodeButton = SNew(SButton)
			.OnClicked_Lambda([Node]()
				{
					if (Node.IsValid())
					{
						if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
							GEditor->SelectActor(Node.Get(), !Node->IsSelectedInEditor(), true, true, true);
						else
						{
							GEditor->SelectNone(false, false, false);
							GEditor->SelectActor(Node.Get(), true, true, true, true);
						}
					}
					
					return FReply::Handled();
				})
			.ButtonColorAndOpacity_Lambda([Node]() { return (Node.IsValid() && Node->IsSelectedInEditor()) ? FLinearColor(1.2f, 2.0f, 3.0f) : FLinearColor::White; })
			.Content()
			[
				SAssignNew(NodeButtonContentBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					IconWidget.ToSharedRef()
				]

				+ SHorizontalBox::Slot()
				.Padding(8, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
					.Text(FText::FromString(DisplayStr))
				]
			];
		
		if (Node->DeferredCook())  // Add a cook shortcut
		{
			NodeButtonContentBox->AddSlot()
				.Padding(0, 0, 4, 0)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
					.ContentPadding(FMargin(-2.0f, 1.0f))
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(2.0f)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Cook"))
							.DesiredSizeOverride(FVector2D(12.0, 12.0))
						]
						+ SHorizontalBox::Slot()
						.Padding(2.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Cook", "Cook"))
							.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]
					]
					.OnClicked_Lambda([Node](){ Node->ForceCook(); return FReply::Handled(); })
				];
		}

		NodeButtonContentBox->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.ToolTipText(LOCTEXT("HoudiniNodeVisibilityHelp", "Toggle Houdini Node Hide/Show"))
				.Visibility(EVisibility::Visible)
				[
					SNew(SImage)
					.Image_Lambda([Node]()
						{
							return GetEngineVisibilityIconBrush(Node.IsValid() && !Node->IsHiddenEd());
						})
				]
				.OnClicked_Lambda([Node]()
					{
						if (Node.IsValid())
						{
							const bool bHidden = !Node->IsHiddenEd();
							Node->SetIsTemporarilyHiddenInEditor(bHidden);

							FString FolderPath = TEXT(HOUDINI_NODE_OUTLINER_FOLDER) / Node->GetActorLabel();
							const FName CookActorFolder(FolderPath);
							FolderPath += TEXT("/");
							for (TActorIterator<AActor> ActorIter(Node->GetWorld()); ActorIter; ++ActorIter)
							{
								AActor* Actor = *ActorIter;
								if (IsValid(Actor))
								{
									const FName Folder = Actor->GetFolderPath();
									if (Folder == CookActorFolder)
										Actor->SetIsTemporarilyHiddenInEditor(bHidden);
									else if (Folder.ToString().StartsWith(FolderPath))  // Contains in the folder
										Actor->SetIsTemporarilyHiddenInEditor(bHidden);
								}
							}
						}

						return FReply::Handled();
					})
			];

		// Curve inputs widget
		TArray<TWeakObjectPtr<UHoudiniInput>> CurveInputs;
		for (UHoudiniInput* Input : Node->GetInputs())
		{
			if (Input->GetType() == EHoudiniInputType::Curves)
				CurveInputs.Add(Input);
		}

		if (CurveInputs.IsEmpty())  // Show all curve inputs
		{
			HoudiniNodeListCategoryBuilder.AddCustomRow(FText::FromString(Node->GetActorLabel(false)))
			[
				NodeButton
			];
		}
		else
		{
			TSharedPtr<SVerticalBox> CurveInputsBox;
			HoudiniNodeListCategoryBuilder.AddCustomRow(FText::FromString(Node->GetActorLabel(false)))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					NodeButton
				]
				
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SExpandableArea)
					.AreaTitle(FText::FromString(Node->GetHelpText().IsEmpty() ? Node->GetActorLabel(false) : Node->GetHelpText()))
					.InitiallyCollapsed(false)
					.BodyContent()
					[
						SAssignNew(CurveInputsBox, SVerticalBox)
					]
					.OnAreaExpansionChanged_Lambda([Node](const bool& bIsExpanded)
					{
						
					})
				]
			];

			for (const TWeakObjectPtr<UHoudiniInput>& Input : CurveInputs)
			{
				CurveInputsBox->AddSlot().Padding(2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(24.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "NoBorder")
						.ClickMethod(EButtonClickMethod::MouseDown)
						.ToolTipText(LOCTEXT("HoudiniCurvesVisibilityHelp", "Toggle Curves Hide/Show"))
						[
							SNew(SImage)
							.DesiredSizeOverride(FVector2D(CURVE_INPUT_ICON_SIZE, CURVE_INPUT_ICON_SIZE))
							.Image(FHoudiniEngineStyle::Get()->GetBrush("ClassIcon.HoudiniCurvesComponent"))
							.ColorAndOpacity_Lambda([Input]()
								{
									const UHoudiniCurvesComponent* HCC = (Input->GetType() == EHoudiniInputType::Curves) ? (Input->Holders.IsValidIndex(0) ?
											Cast<UHoudiniInputCurves>(Input->Holders[0])->GetCurvesComponent() : nullptr) : nullptr;

									return (!HCC || HCC->GetVisibleFlag()) ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
								})
						]
						.OnClicked_Lambda([Input]()
							{
								if (UHoudiniCurvesComponent* HCC = (Input->Holders.IsValidIndex(0) ?
									Cast<UHoudiniInputCurves>(Input->Holders[0])->GetCurvesComponent() : nullptr))
									HCC->SetVisibleFlag(!HCC->GetVisibleFlag());

								return FReply::Handled();
							})
					]

					+ SHorizontalBox::Slot()
					.Padding(8, 0, 0, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.LargeFont"))
						.Text(FText::FromString(FName::NameToDisplayString(Input->GetInputName(), false)))
					]
#if 1
					+ SHorizontalBox::Slot().Padding(8, 0, 0, 0)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						CreateDrawButton(Input, FAppStyle::Get().GetBrush("Icons.Plus"), EHoudiniCurveDrawingType::Click)
					]
#else
					+ SHorizontalBox::Slot().Padding(8, 0, 0, 0)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						CreateDrawButton(Input, FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.PlusRect"), EHoudiniCurveDrawingType::Rectange)
					]

					+ SHorizontalBox::Slot().Padding(2, 0)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						CreateDrawButton(Input, FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.PlusCircle"), EHoudiniCurveDrawingType::Circle)
					]

					+ SHorizontalBox::Slot().Padding(2, 0)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						CreateDrawButton(Input, FAppStyle::Get().GetBrush("Icons.Plus"), EHoudiniCurveDrawingType::Click)
					]
#endif
				];
			}
		}
	}
}

void FHoudiniManageToolDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& HoudiniNodeListCategoryBuilder = DetailBuilder.EditCategory(FName("Houdini Node List"));
	HoudiniNodeListCategoryBuilder.AddCustomRow(LOCTEXT("AddHoudiniAsset", "Add Houdini Asset"))
	[
		SNew(SBox)
		.Padding(0, 4, 0, 4)
		.HeightOverride(36)
		.VAlign(VAlign_Fill)
		[
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
				{
					for (const FAssetData& AssetData : InAssets)
					{
						if (AssetData.AssetClassPath.GetAssetName() == UHoudiniAsset::StaticClass()->GetFName())
							return true;
					}
					return false;
				})
			.OnAssetsDropped_Lambda([&DetailBuilder](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					UWorld* CurrWorld = GEditor->GetEditorWorldContext().World();
					if (!IsValid(CurrWorld))
						return;

					TArray<AHoudiniNode*> NewNodes;
					for (const FAssetData& AssetData : InAssets)
					{
						if (AssetData.AssetClassPath.GetAssetName() == UHoudiniAsset::StaticClass()->GetFName())
							NewNodes.Add(AHoudiniNode::Create(CurrWorld, Cast<UHoudiniAsset>(AssetData.GetAsset())));
					}

					if (!NewNodes.IsEmpty())
					{
						GEditor->SelectNone(false, false, false);

						for (AHoudiniNode* NewNode : NewNodes)
							GEditor->SelectActor(NewNode, true, true, true, true);

						DetailBuilder.ForceRefreshDetails();
					}
				})
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(4, 2, 0, 2)
					.AutoWidth()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
							.ColorAndOpacity(FStyleColors::AccentGreen)
						]
					]
					+ SHorizontalBox::Slot()
					.Padding(6, 2, 0, 0)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
						.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.LargeFont"))))
						.Text(LOCTEXT("HoudiniAsset", "Houdini Asset"))
						.ToolTipText(LOCTEXT("HoudiniAssetPickerHelp", "Spawn a new Houdini Node to current level"))
						.ColorAndOpacity_Lambda([]() { return FHoudiniEngine::Get().IsSessionSync() ? FStyleColors::AccentGreen : FStyleColors::White; })
					]
				]
				.OnGetMenuContent(FOnGetContent::CreateLambda([&DetailBuilder]()
					{
						return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
							FAssetData(nullptr),
							false,
							false,
							TArray<const UClass*>({ UHoudiniAsset::StaticClass() }),
							TArray<UFactory*>{},
							FOnShouldFilterAsset(),
							FOnAssetSelected::CreateLambda(
								[&DetailBuilder](const FAssetData& AssetData)
								{
									UWorld* CurrWorld = GEditor->GetEditorWorldContext().World();
									if (!IsValid(CurrWorld))
										return;

									GEditor->SelectNone(false, false, false);
									GEditor->SelectActor(AHoudiniNode::Create(CurrWorld, Cast<UHoudiniAsset>(AssetData.GetAsset())),
										true, true, true, true);

									DetailBuilder.ForceRefreshDetails();
								}
							),
							FSimpleDelegate::CreateLambda([]() {}));
					}))
			]
		]
	];

	TArray<TPair<FString, TWeakObjectPtr<AHoudiniNode>>> DisplayNodes;
	TMap<FName, TArray<TPair<FString, TWeakObjectPtr<AHoudiniNode>>>> FolderDisplayNodesMap;
	for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())
	{
		if (!Node.IsValid())
			continue;

		const FName NodeFolderPath = Node->GetFolderPath();
		if ((NodeFolderPath == HoudiniNodeDefaultFolderPath) || NodeFolderPath.IsNone())
			DisplayNodes.Add(TPair<FString, TWeakObjectPtr<AHoudiniNode>>(
				Node->GetHelpText().IsEmpty() ? Node->GetActorLabel(false) : Node->GetHelpText(), Node));
		else
			FolderDisplayNodesMap.FindOrAdd(NodeFolderPath).Add(TPair<FString, TWeakObjectPtr<AHoudiniNode>>(
				Node->GetHelpText().IsEmpty() ? Node->GetActorLabel(false) : Node->GetHelpText(), Node));
	}

	if (!DisplayNodes.IsEmpty())
	{
		HoudiniNodeEventsHandle = FHoudiniEngine::Get().HoudiniNodeEvents.AddLambda([&DetailBuilder](AHoudiniNode*, const EHoudiniNodeEvent Event)
			{
				if (Event != EHoudiniNodeEvent::StartCook)
					DetailBuilder.ForceRefreshDetails();
			});
	}

	// We should list nodes by the order of display strings
	DisplayNodes.Sort([](const TPair<FString, TWeakObjectPtr<AHoudiniNode>>& A, const TPair<FString, TWeakObjectPtr<AHoudiniNode>>& B)
		{ return A.Key < B.Key; });
	ConstructHoudiniNodeList(HoudiniNodeListCategoryBuilder, DisplayNodes);
	
	for (auto& FolderDisplayNodes : FolderDisplayNodesMap)
	{
		// We should list nodes by the order of display strings
		FolderDisplayNodes.Value.Sort([](const TPair<FString, TWeakObjectPtr<AHoudiniNode>>& A, const TPair<FString, TWeakObjectPtr<AHoudiniNode>>& B)
			{ return A.Key < B.Key; });
		IDetailCategoryBuilder& HoudiniNodeListFolderCategoryBuilder = DetailBuilder.EditCategory(FolderDisplayNodes.Key);
		ConstructHoudiniNodeList(HoudiniNodeListFolderCategoryBuilder, FolderDisplayNodes.Value);
	}

	OnNodeRegisteredHandle = FHoudiniEngine::Get().OnHoudiniNodeRegisteredEvent.AddLambda(
		[&DetailBuilder](AHoudiniNode*) { DetailBuilder.ForceRefreshDetails(); });

	OnNodeFolderChangedHandle = GEngine->OnLevelActorFolderChanged().AddLambda([&DetailBuilder](const AActor* Actor, FName)
		{
			if (IsValid(Actor) && !Actor->bIsEditorPreviewActor && Actor->IsA<AHoudiniNode>())
				DetailBuilder.ForceRefreshDetails();
		});

	TSet<FName> CustomFolderPaths;
	FolderDisplayNodesMap.GetKeys(CustomFolderPaths);
	OnNodeFolderRenamedHandle = FActorFolders::OnFolderMoved.AddLambda([&DetailBuilder, CustomFolderPaths](UWorld&, const FFolder& SrcFolder, const FFolder& DstFolder)
		{
			const FName SrcFolderPath = SrcFolder.GetPath();
			if (SrcFolderPath == HoudiniNodeDefaultFolderPath)
			{
				DetailBuilder.ForceRefreshDetails();
				return;
			}

			if (!CustomFolderPaths.IsEmpty())
			{
				const FString ChangedFolderPath = SrcFolderPath.ToString();
				for (const FName& CustomFolderPath : CustomFolderPaths)
				{
					if (CustomFolderPath.ToString().Contains(ChangedFolderPath))
					{
						DetailBuilder.ForceRefreshDetails();
						return;
					}
				}
			}
		});
}

TSharedRef< IDetailCustomization > FHoudiniManageToolDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniManageToolDetails);
}

void FHoudiniManageToolDetails::PendingDelete()
{
	if (HoudiniNodeEventsHandle.IsValid())
	{
		FHoudiniEngine::Get().HoudiniNodeEvents.Remove(HoudiniNodeEventsHandle);
		HoudiniNodeEventsHandle.Reset();
	}

	if (OnNodeRegisteredHandle.IsValid())
	{
		FHoudiniEngine::Get().OnHoudiniNodeRegisteredEvent.Remove(OnNodeRegisteredHandle);
		OnNodeRegisteredHandle.Reset();
	}

	if (OnNodeFolderChangedHandle.IsValid())
	{
		GEngine->OnLevelActorFolderChanged().Remove(OnNodeFolderChangedHandle);
		OnNodeFolderChangedHandle.Reset();
	}

	if (OnNodeFolderRenamedHandle.IsValid())
	{
		FActorFolders::OnFolderMoved.Remove(OnNodeFolderRenamedHandle);
		OnNodeFolderRenamedHandle.Reset();
	}
}


FText GetHoudiniInputHolderDisplayPath(const TWeakObjectPtr<UHoudiniInputHolder>& Holder)
{
	if (Holder.IsValid())
	{
		UHoudiniInput* ParentInput = Cast<UHoudiniInput>(Holder->GetOuter());
		return FText::FromString(Cast<AHoudiniNode>(ParentInput->GetOuter())->GetActorLabel() / ParentInput->GetInputName());
	}

	return FText::GetEmpty();
}

void FHoudiniMaskToolDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// get current edited objects
	TArray< TWeakObjectPtr< UObject > > ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	if (!ObjectsCustomized.IsValidIndex(0))
		return;

	TWeakObjectPtr<UHoudiniMaskTool> MaskTool = Cast<UHoudiniMaskTool>(ObjectsCustomized[0]);
	if (!MaskTool.IsValid())
		return;

	const TWeakObjectPtr<AHoudiniMaskGizmoActiveActor> MaskActor = MaskTool->MaskActor;
	TWeakObjectPtr<UHoudiniInputMask> MaskInput = MaskActor.IsValid() ? MaskTool->MaskActor->GetMaskInput() : nullptr;
	if (MaskInput.IsValid())
	{
		if (Cast<UHoudiniInput>(MaskInput->GetOuter())->IsPendingClear())  // Check if landscape has destroyed
			MaskInput.Reset();
	}

	IDetailCategoryBuilder& BrushSettingsCategoryBuilder = DetailBuilder.EditCategory(FName("Brush Settings"));

	BrushSettingsCategoryBuilder.AddCustomRow(LOCTEXT("MaskInput", "MaskInput"))
	[
		SNew(SBox)
		.Padding(0, 4, 0, 4)
		.HeightOverride(36)
		.VAlign(VAlign_Fill)
		[
			SNew(SComboButton)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4, 2, 0, 2)
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(16.0, 16.0))
						.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngineEditorMode.MaskTool"))
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(6, 2, 0, 0)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.LargeFont"))))
					.Text(GetHoudiniInputHolderDisplayPath(MaskInput))
					.ToolTipText(LOCTEXT("HoudiniMaskInputPicker", "Select A Mask Input To Brush"))
				]
			]
			.OnGetMenuContent(FOnGetContent::CreateLambda([&DetailBuilder, MaskTool]()
				{
					FMenuBuilder MenuBuilder(true, nullptr);

					bool bHasMaskInput = false;
					for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())
					{
						if (!Node.IsValid())
							continue;

						for (const UHoudiniInput* Input : Node->GetInputs())
						{
							if (Input->IsPendingClear())
								continue;

							if (Input->GetType() == EHoudiniInputType::Mask && Input->Holders.IsValidIndex(0))
							{
								const TWeakObjectPtr<UHoudiniInputMask> MaskInput = Cast<UHoudiniInputMask>(Input->Holders[0]);
								MenuBuilder.AddMenuEntry(GetHoudiniInputHolderDisplayPath(Input->Holders[0]), FText::GetEmpty(),
									FSlateIcon(),
									FUIAction(
										FExecuteAction::CreateLambda([&DetailBuilder, MaskTool, MaskInput]() -> void
											{
												if (MaskTool->MaskActor.IsValid())
													MaskTool->MaskActor->Enter(MaskInput.Get());
												else
													MaskTool->MaskActor = AHoudiniMaskGizmoActiveActor::FindOrCreate(MaskTool->GetWorld(), MaskInput.Get());
												DetailBuilder.ForceRefreshDetails();
											}),
										FCanExecuteAction()
									)
								);

								bHasMaskInput = true;
							}
						}
					}

					if (!bHasMaskInput)
						MenuBuilder.AddMenuEntry(LOCTEXT("NoMaskInput", "No Available Mask Inputs"), FText::GetEmpty(), FSlateIcon(), FUIAction());

					return MenuBuilder.MakeWidget();
				}))
		]
	];
	
	if (!MaskActor.IsValid() || !MaskInput.IsValid())
		return;

	SelectedMaskInput = MaskInput;
	OnMaskChangedHandle = SelectedMaskInput->OnChangedDelegate.AddLambda(
		[MaskActor, &DetailBuilder](const bool& bIsPendingDestroy)
		{
			if (bIsPendingDestroy && MaskActor.IsValid())  // We must notify MaskActor that MaskInput is pending destroy first, then refresh tool details
				MaskActor->OnMaskChanged(true);
			DetailBuilder.ForceRefreshDetails();
		});

	{
		FDetailWidgetRow& Row = BrushSettingsCategoryBuilder.AddCustomRow(LOCTEXT("BrushSize", "Brush Size"));
		Row.NameContent()
		//.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BrushSize", "Brush Size"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.ToolTipText(LOCTEXT("BrushSizeHelp", "Brush Radius"))
		];
		Row.ValueContent()
		[
			SNew(SNumericEntryBox< float >)
			.AllowSpin(true)
			.MinValue(0.f)
			.MinSliderValue(0.5f)
			.MaxSliderValue(100000.f)
			.TypeInterface(MakeShared<TNumericUnitTypeInterface<float>>(EUnit::Centimeters))

			.Value_Lambda([MaskActor]() { return MaskActor->GetBrushSize(); })
			.OnValueChanged_Lambda([MaskActor](float Val) { MaskActor->SetBrushSize(Val); })
			.OnValueCommitted_Lambda([MaskActor](float Val, ETextCommit::Type TextCommitType) { MaskActor->SetBrushSize(Val); })
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];
	}

	if (MaskInput->GetMaskType() == EHoudiniMaskType::Weight)
	{
		{
			FDetailWidgetRow& Row = BrushSettingsCategoryBuilder.AddCustomRow(LOCTEXT("BrushFallOff", "Brush Fall Off"));
			Row.NameContent()
			//.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BrushFallOff", "Brush Fall Off"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
			Row.ValueContent()
			[
				SNew(SNumericEntryBox< float >)
				.AllowSpin(true)
				.MinValue(0.f)
				.MinSliderValue(0.f)
				.MaxValue(1.f)
				.MaxSliderValue(1.f)

				.Value_Lambda([MaskActor]() { return MaskActor->GetBrushFallOff(); })
				.OnValueChanged_Lambda([MaskActor](float Val) { MaskActor->SetBrushFallOff(Val); })
				.OnValueCommitted_Lambda([MaskActor](float Val, ETextCommit::Type TextCommitType) { MaskActor->SetBrushFallOff(Val); })
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
		}
		
		{
			FDetailWidgetRow& Row = BrushSettingsCategoryBuilder.AddCustomRow(LOCTEXT("BrushValue", "Brush Value"));
			Row.NameContent()
			//.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BrushValue", "Brush Value"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
			Row.ValueContent()
			[
				SNew(SNumericEntryBox< float >)
				.AllowSpin(true)
				.MinValue(0.f)
				.MinSliderValue(0.f)
				.MaxValue(1.f)
				.MaxSliderValue(1.f)

				.Value_Lambda([MaskActor]() { return MaskActor->GetBrushValue(); })
				.OnValueChanged_Lambda([MaskActor](float Val) { MaskActor->SetBrushValue(Val); })
				.OnValueCommitted_Lambda([MaskActor](float Val, ETextCommit::Type TextCommitType) { MaskActor->SetBrushValue(Val); })
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			];
		}
	}


	IDetailCategoryBuilder& MaskCategoryBuilder = DetailBuilder.EditCategory(FName("Mask Settings"));

	{
		FDetailWidgetRow& Row = MaskCategoryBuilder.AddCustomRow(LOCTEXT("VisualizeMask", "Visualize Mask"));
		Row.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VisualizeMask", "Visualize Mask"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];
		Row.ValueContent()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.ToolTipText(LOCTEXT("HoudiniMaskVisibilityHelp", "Toggle Mask Hide/Show"))
			.Visibility(EVisibility::Visible)
			.HAlign(HAlign_Left)
			[
				SNew(SImage)
				.Image_Lambda([MaskActor]()
					{
						return GetEngineVisibilityIconBrush(MaskActor.IsValid() && MaskActor->ShouldVisualizeMask());
					})
			]
			.OnClicked_Lambda([MaskActor]()
				{
					if (MaskActor.IsValid())
					{
						if (MaskActor->ShouldVisualizeMask())  // Toggle(Reverse) visibility
							MaskActor->DisableMaskVisualization();
						else
							MaskActor->VisualizeMask();
					}

					return FReply::Handled();
				})
		];
	}

	if (MaskInput->GetMaskType() == EHoudiniMaskType::Byte)
	{
		for (int32 ByteLayerIdx = 0; ByteLayerIdx < MaskInput->NumByteLayers(); ++ByteLayerIdx)
		{
			FDetailWidgetRow& Row = MaskCategoryBuilder.AddCustomRow(LOCTEXT("BrushSize", "Brush Size"));
			TSharedPtr<SWidget> ColorPickerParentWidget;
			Row.NameContent()
			[
				SNew(SColorBlock)
				.AlphaBackgroundBrush(FAppStyle::Get().GetBrush("ColorPicker.RoundedAlphaBackground"))
				.Color_Lambda([MaskInput, ByteLayerIdx]() { return MaskInput->GetByteColor(ByteLayerIdx); })
				.ShowBackgroundForAlpha(true)
				.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Separate)
				.OnMouseButtonDown_Lambda([MaskInput, MaskActor, ByteLayerIdx](const FGeometry&, const FPointerEvent&)
					{
						FColorPickerArgs PickerArgs;
						{
							PickerArgs.bUseAlpha = true;
							PickerArgs.bOnlyRefreshOnMouseUp = true;
							PickerArgs.bOnlyRefreshOnOk = false;
							PickerArgs.sRGBOverride = true;
							PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
							PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([MaskActor, ByteLayerIdx](FLinearColor NewColor)
								{
									MaskActor->SetByteColor(ByteLayerIdx, NewColor.ToFColor(true));
								});
							PickerArgs.InitialColor = MaskInput->GetByteColor(ByteLayerIdx);
							PickerArgs.ParentWidget = FSlateApplication::Get().GetActiveTopLevelWindow();
							PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateLambda(
								[MaskInput](const TSharedRef<SWindow>&)
								{
									MaskInput->CommitData();
								});
						}

						OpenColorPicker(PickerArgs);

						return FReply::Handled();
					})
				.Size(FVector2D(70.0f, 20.0f))
				.CornerRadius(FVector4(4.0f,4.0f,4.0f,4.0f))
			];

			Row.ValueContent()
			.HAlign(HAlign_Fill)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonColorAndOpacity_Lambda([MaskActor, MaskInput, ByteLayerIdx]()
					{
						return (MaskActor->GetByteValue() == MaskInput->GetByteValue(ByteLayerIdx)) ?
							(FLinearColor(MaskInput->GetByteColor(ByteLayerIdx)) * 2.0f) : FLinearColor::White;  // Highlight the painting byte value
					})
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("LandscapeEditor.PaintTool"))
					]
					+ SHorizontalBox::Slot()
					.Padding(4, 0, 0, 0)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Paint", "Paint"))
					]
				]
				.OnClicked_Lambda([MaskActor, ByteLayerIdx]()
					{
						MaskActor->OnTargetByteValueChanged(ByteLayerIdx);
						return FReply::Handled();
					})
			];
		}
	}
}

TSharedRef< IDetailCustomization > FHoudiniMaskToolDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniMaskToolDetails);
}

void FHoudiniMaskToolDetails::PendingDelete()
{
	if (OnMaskChangedHandle.IsValid() && SelectedMaskInput.IsValid())
		SelectedMaskInput->OnChangedDelegate.Remove(OnMaskChangedHandle);

	OnMaskChangedHandle.Reset();
	SelectedMaskInput.Reset();
}

// Edit Tool Details
void FHoudiniEditToolDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr< UObject > > ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	if (!ObjectsCustomized.IsValidIndex(0))
		return;

	TWeakObjectPtr<UHoudiniEditTool> EditTool = Cast<UHoudiniEditTool>(ObjectsCustomized[0]);
	if (!EditTool.IsValid())
		return;

	EditTool->CleanupBrushAttributes();

	IDetailCategoryBuilder& SettingsCategoryBuilder = DetailBuilder.EditCategory(FName("Geometry Editor Settings"));

	if (UHoudiniAttributeParameterHolder::Get()->GetEditableGeometry().IsValid())
	{
	TSharedPtr<STextBlock> AttribNamesTextBlock;
	SettingsCategoryBuilder.AddCustomRow(LOCTEXT("MaskInput", "MaskInput"))
	[
		SNew(SBox)
		.Padding(0, 4, 0, 4)
		.HeightOverride(36)
		.VAlign(VAlign_Fill)
		[
			SNew(SComboButton)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4, 2, 0, 2)
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(16.0, 16.0))
						.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngineEditorMode.EditTool"))
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(6, 2, 0, 0)
				[
					SAssignNew(AttribNamesTextBlock, STextBlock)
					.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.LargeFont"))))
					.Text(EditTool->GetBrushAttributesText())
					.ToolTipText(LOCTEXT("HoudiniMaskInputPicker", "Select A Mask Input To Brush"))
				]
			]
			.OnGetMenuContent(FOnGetContent::CreateLambda([&DetailBuilder, EditTool, AttribNamesTextBlock]()
				{
					FMenuBuilder MenuBuilder(true, nullptr);
					const TWeakObjectPtr<UHoudiniEditableGeometry>& EditGeo = UHoudiniAttributeParameterHolder::Get()->GetEditableGeometry();
					if (!EditGeo.IsValid() ||
						(EditGeo->GetSelectedClass() != EHoudiniAttributeOwner::Point && EditGeo->GetSelectedClass() != EHoudiniAttributeOwner::Prim))
						return MenuBuilder.MakeWidget();

					MenuBuilder.AddMenuEntry(LOCTEXT("SelectAllAttribs", "--- Select All ---"), FText::GetEmpty(),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([EditTool, AttribNamesTextBlock]() -> void
								{
									if (EditTool.IsValid())
									{
										EditTool->AddAllAttributes();
										AttribNamesTextBlock->SetText(EditTool->GetBrushAttributesText());
									}
									else
										AttribNamesTextBlock->SetText(FText::GetEmpty());
								}),
							FCanExecuteAction()
						)
					);

					MenuBuilder.AddMenuEntry(LOCTEXT("DeselectAllAttribs", "--- Deselect All ---"), FText::GetEmpty(),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([&DetailBuilder, EditTool, AttribNamesTextBlock]() -> void
								{
									if (EditTool.IsValid() && EditTool->IsBrushing())
									{
										EditTool->ClearAttributes();
										AttribNamesTextBlock->SetText(EditTool->GetBrushAttributesText());
									}
								}),
							FCanExecuteAction()
						)
					);

					for (const TWeakObjectPtr<UHoudiniParameter> AttribParm : UHoudiniAttributeParameterHolder::Get()->GetParameters())
					{
						const EHoudiniParameterType& ParmType = AttribParm->GetType();
						if (ParmType == EHoudiniParameterType::Separator || ParmType == EHoudiniParameterType::Label ||
							ParmType == EHoudiniParameterType::Folder)
							continue;

						MenuBuilder.AddMenuEntry(FText::FromString(AttribParm->GetLabel() + TEXT("(") + AttribParm->GetParameterName() + TEXT(")")), FText::GetEmpty(),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda([&DetailBuilder, AttribParm, EditTool, AttribNamesTextBlock]() -> void
									{
										if (AttribParm.IsValid() && EditTool.IsValid() &&
											UHoudiniAttributeParameterHolder::Get()->GetParameters().Contains(AttribParm.Get()))  // Check this parameter is still an attribute
										{
											EditTool->ToggleAttribute(AttribParm);
											AttribNamesTextBlock->SetText(EditTool->GetBrushAttributesText());
										}
									}),
								FCanExecuteAction(),
								FIsActionChecked::CreateLambda([AttribParm, EditTool] { return AttribParm.IsValid() && EditTool->IsAttributeBrushing(AttribParm); })
							), NAME_None, EUserInterfaceActionType::ToggleButton
						);
					}

					return MenuBuilder.MakeWidget();
				}))
		]
	]
	.OverrideResetToDefault(FResetToDefaultOverride::Create(
		TAttribute<bool>::CreateLambda([EditTool] { return EditTool.IsValid() && EditTool->IsBrushing(); }),
		FSimpleDelegate::CreateLambda([&DetailBuilder, EditTool]
			{
				if (EditTool.IsValid() && EditTool->IsBrushing())
				{
					EditTool->ClearAttributes();
					DetailBuilder.ForceRefreshDetails();
				}
			})));

	SettingsCategoryBuilder.AddCustomRow(LOCTEXT("BrushSize", "Brush Size"))
	.NameContent()
	//.HAlign(HAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BrushSize", "Brush Size"))
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.ToolTipText(LOCTEXT("BrushSizeHelp", "Brush Radius"))
	]
	.ValueContent()
	[
		SNew(SNumericEntryBox< float >)
		.AllowSpin(true)
		.MinValue(0.0001f)
		.MaxSliderValue(100000.f)
		.TypeInterface(MakeShared<TNumericUnitTypeInterface<float>>(EUnit::Centimeters))

		.Value_Lambda([EditTool] { return EditTool.IsValid() ? EditTool->BrushSize : 0.0f; })
		.OnValueChanged_Lambda([EditTool](float Val) { if (EditTool.IsValid()) EditTool->BrushSize = Val; })
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];
	}

	SettingsCategoryBuilder.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PointSize", "Point Size"))
			.ToolTipText(LOCTEXT("EditGeoPointSizeHelp", "+/- to increase/decreate size"))
			.Font(FAppStyle::Get().GetFontStyle(1 ? "PropertyWindow.NormalFont" : "PropertyWindow.BoldFont"))
		]

		+ SHorizontalBox::Slot()
		.Padding(2.0f)
		[
			SNew(SNumericEntryBox<float>)
			.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.AllowSpin(true)
			.MinValue(3.0f)
			.MaxSliderValue(30.0f)
			.Value_Lambda([]{ return FHoudiniEditableGeometryVisualSettings::Load()->PointSize; })
			.OnValueChanged_Lambda([](float NewValue) { FHoudiniEditableGeometryVisualSettings::Load()->PointSize = NewValue; })
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EdgeThickness", "Edge Thickness"))
			//.ToolTipText(LOCTEXT("EditGeoPointSizeHelp", "+/- to increase/decreate size"))
			.Font(FAppStyle::Get().GetFontStyle(1 ? "PropertyWindow.NormalFont" : "PropertyWindow.BoldFont"))
		]

		+ SHorizontalBox::Slot()
		.Padding(2.0f)
		[
			SNew(SNumericEntryBox<float>)
			.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.AllowSpin(true)
			.MinValue(0.0f)
			.MaxSliderValue(10.0f)
			.Value_Lambda([]{ return FHoudiniEditableGeometryVisualSettings::Load()->EdgeThickness; })
			.OnValueChanged_Lambda([](float NewValue) { FHoudiniEditableGeometryVisualSettings::Load()->EdgeThickness = NewValue; })
		]
	]
	.OverrideResetToDefault(FResetToDefaultOverride::Create(
		TAttribute<bool>::CreateLambda([]()
			{
				return ((FHoudiniEditableGeometryVisualSettings::Load()->PointSize != DEFAULT_POINT_SIZE) ||
					(FHoudiniEditableGeometryVisualSettings::Load()->EdgeThickness != DEFAULT_EDGE_THICKNESS));
			}),
		FSimpleDelegate::CreateLambda([]()
			{
				FHoudiniEditableGeometryVisualSettings::Load()->PointSize = DEFAULT_POINT_SIZE;
				FHoudiniEditableGeometryVisualSettings::Load()->EdgeThickness = DEFAULT_EDGE_THICKNESS;
			})));

	SettingsCategoryBuilder.AddCustomRow(FText::GetEmpty())
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([]{ return FHoudiniEditableGeometryVisualSettings::Load()->bDistanceCulling ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					FHoudiniEditableGeometryVisualSettings::Load()->bDistanceCulling = NewState == ECheckBoxState::Checked;
				})
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CullDistance", "Cull Distance"))
			.ToolTipText(LOCTEXT("EditGeoCullDistanceHelp", "Alt + Ctrl + / to enable/disable, < and > to control distance"))
			.Font(FAppStyle::Get().GetFontStyle(1 ? "PropertyWindow.NormalFont" : "PropertyWindow.BoldFont"))
		]
	]
	.ValueContent()
	[
		SNew(SNumericEntryBox<float>)
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.AllowSpin(true)
		.TypeInterface(MakeShared<TNumericUnitTypeInterface<float>>(EUnit::Centimeters))
		.MinValue(0.0f)
		.MaxSliderValue(819200.0f)
		.Value_Lambda([] { return FHoudiniEditableGeometryVisualSettings::Load()->CullDistance; })
		.OnValueChanged_Lambda([](float NewValue) { FHoudiniEditableGeometryVisualSettings::Load()->CullDistance = NewValue; })
		.IsEnabled_Lambda([] { return FHoudiniEditableGeometryVisualSettings::Load()->bDistanceCulling; })
	]
	.OverrideResetToDefault(FResetToDefaultOverride::Create(
		TAttribute<bool>::CreateLambda([]
			{
				return (FHoudiniEditableGeometryVisualSettings::Load()->bDistanceCulling &&
					(FHoudiniEditableGeometryVisualSettings::Load()->CullDistance != DEFAULT_CULL_DISTANCE));
			}),
		FSimpleDelegate::CreateLambda([] { FHoudiniEditableGeometryVisualSettings::Load()->CullDistance = DEFAULT_CULL_DISTANCE; })));
	
	SettingsCategoryBuilder.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([]{ return FHoudiniEditableGeometryVisualSettings::Load()->bShowPointCoordinate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState) { FHoudiniEditableGeometryVisualSettings::Load()->bShowPointCoordinate = NewState == ECheckBoxState::Checked; })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShowPointCoordinate", "Show Point Coordinate"))
				.ToolTipText(LOCTEXT("EditGeoShowPointCoordinateHelp", "O to show/hide"))
				.Font(FAppStyle::Get().GetFontStyle(1 ? "PropertyWindow.NormalFont" : "PropertyWindow.BoldFont"))
			]
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([]{ return FHoudiniEditableGeometryVisualSettings::Load()->bShowEdgeLength ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([](ECheckBoxState NewState) { FHoudiniEditableGeometryVisualSettings::Load()->bShowEdgeLength = NewState == ECheckBoxState::Checked; })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShowEdgeLength", "Show Edge Length"))
				.ToolTipText(LOCTEXT("EditGeoShowEdgeLengthHelp", "L to show/hide"))
				.Font(FAppStyle::Get().GetFontStyle(1 ? "PropertyWindow.NormalFont" : "PropertyWindow.BoldFont"))
			]
		]
	];
}

TSharedRef<IDetailCustomization> FHoudiniEditToolDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniEditToolDetails);
}

#undef LOCTEXT_NAMESPACE
