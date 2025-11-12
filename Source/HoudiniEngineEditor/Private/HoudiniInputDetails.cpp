// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputDetails.h"

#include "Widgets/Input/SComboBox.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PropertyCustomizationHelpers.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "Editor/UnrealEd/Public/Selection.h"
#include "SAssetDropTarget.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineCommon.h"
#include "HoudiniNode.h"
#include "HoudiniInputs.h"
#include "HoudiniCurvesComponent.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniCurvesComponentVisualizer.h"
#include "HoudiniAttributeParameterHolder.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniTools.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

bool FHoudiniInputDetails::HasNodeInputs(const TArray<UHoudiniInput*>& Inputs)
{
	for (UHoudiniInput* Input : Inputs)
	{
		if (!Input->IsParameter())
			return true;
	}

	return false;
}

void FHoudiniInputDetails::CreateWidgets(IDetailCategoryBuilder& CategoryBuilder, const TArray<UHoudiniInput*>& Inputs)
{
	for (UHoudiniInput* Input : Inputs)
	{
		if (Input->IsParameter())
			continue;

		const FText Label = FText::FromString(Input->GetInputName());
		FDetailWidgetRow& Row = CategoryBuilder.AddCustomRow(Label);
		Row.NameContent()
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
		];

		CreateWidget(CategoryBuilder, Row.ValueContent(), Input);
	}
}

static const FSlateBrush* GetHoudiniInputTypeIcon(const EHoudiniInputType& InputType)
{
	switch (InputType)
	{		
	case EHoudiniInputType::Content: return FAppStyle::Get().GetBrush("ClassIcon.Object");
	case EHoudiniInputType::Curves: return FHoudiniEngineStyle::Get()->GetBrush("ClassIcon.HoudiniCurvesComponent");
	case EHoudiniInputType::World: return FAppStyle::Get().GetBrush("ClassThumbnail.World");
	case EHoudiniInputType::Node: return FHoudiniEngineStyle::Get()->GetBrush("ClassIcon.HoudiniNode");
	case EHoudiniInputType::Mask: return FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngineEditorMode.MaskTool");
	}

	return FAppStyle::Get().GetBrush("ClassIcon.Object");
}

#define HOUDINI_INPUT_TYPE_WIDGET(INPUT_TYPE, INPUT_TYPE_TEXT) SNew(SHorizontalBox) \
	+ SHorizontalBox::Slot() \
	.Padding(0.0f, 2.0f, 4.0f, 2.0f) \
	.AutoWidth() \
	[ \
		SNew(SImage) \
		.Image(GetHoudiniInputTypeIcon(INPUT_TYPE)) \
		.DesiredSizeOverride(FVector2D(16.0, 16.0)) \
	] \
	+ SHorizontalBox::Slot() \
	.VAlign(VAlign_Center) \
	[ \
		SNew(STextBlock) \
		.Text(INPUT_TYPE_TEXT) \
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont"))) \
	]


void FHoudiniInputDetails::CreateWidget(IDetailCategoryBuilder& CategoryBuilder, FDetailWidgetDecl& ValueContent, const TWeakObjectPtr<UHoudiniInput>& Input)
{
	static const TArray<TSharedPtr<FString>> InputTypeOptions = TArray<TSharedPtr<FString>>{
		MakeShared<FString>(TEXT("Content Input")), MakeShared<FString>(TEXT("Curves Input")), MakeShared<FString>(TEXT("World Input")),
		MakeShared<FString>(TEXT("Node Input")), MakeShared<FString>(TEXT("Mask Input")) };

	TSharedPtr<SVerticalBox> VerticalBox;

	ValueContent
	.MinDesiredWidth(225.0f)
	[
		SAssignNew(VerticalBox, SVerticalBox)
	];

	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&InputTypeOptions)
		.InitiallySelectedItem(InputTypeOptions[(int32)Input->GetType()])
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
			{
				return HOUDINI_INPUT_TYPE_WIDGET((EHoudiniInputType)InputTypeOptions.IndexOfByKey(InItem), FText::FromString(*InItem));
			})
		.OnSelectionChanged_Lambda([Input, &CategoryBuilder](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
			{
				if (!Input.IsValid())
					return;

				const EHoudiniInputType NewInputType = (EHoudiniInputType)InputTypeOptions.IndexOfByKey(NewChoice);
				if (NewInputType != Input->GetType())
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniInputChange", "Houdini Input: Changing Input Type"),
						Input->GetOuter());

					Input->Modify();
					Input->SetType(NewInputType);

					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
				}
			})
		.ContentPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
		[
			HOUDINI_INPUT_TYPE_WIDGET(Input->GetType(), FText::FromString(*InputTypeOptions[(int32)Input->GetType()]))
		]
	];

	switch (Input->GetType())
	{
	case EHoudiniInputType::Content:
		CreateWidgetContent(CategoryBuilder, VerticalBox, Input);
		break;
	case EHoudiniInputType::Curves:
		CreateWidgetCurves(CategoryBuilder, VerticalBox, Input);
		break;
	case EHoudiniInputType::World:
		CreateWidgetWorld(CategoryBuilder, VerticalBox, Input);
		break;
	case EHoudiniInputType::Node:
		CreateWidgetNode(CategoryBuilder, VerticalBox, Input);
		break;
	case EHoudiniInputType::Mask:
		CreateWidgetMask(CategoryBuilder, VerticalBox, Input);
		break;
	}
}

#define HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(SETTING_NAME, SETTING_TEXT, SETTING_TOOLTIP) SNew(SCheckBox)\
	.Padding(2.0f)\
	.Content()\
	[\
		SNew(STextBlock)\
		.Text(SETTING_TEXT)\
		.ToolTipText(SETTING_TOOLTIP)\
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))\
	]\
	.IsChecked(Input->GetSettings().b##SETTING_NAME ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)\
	.OnCheckStateChanged_Lambda([Input](ECheckBoxState NewState) { Input->Set##SETTING_NAME(NewState == ECheckBoxState::Checked); })

template<typename TEnum>
TArray<FName>* GetEnumOptionSource()
{
	static TArray<FName> Options;
	if (Options.IsEmpty())
	{
		for (int32 EnumIdx = 0; EnumIdx < StaticEnum<TEnum>()->NumEnums() - 1; ++EnumIdx)
			Options.Add(StaticEnum<TEnum>()->GetNameByIndex(EnumIdx));
	}

	return &Options;
}

#define HOUDINI_INPUT_OPTION_SETTING_WIDGET(SETTING_NAME, SETTING_ENUM) SNew(SComboBox<FName>)\
	.InitiallySelectedItem(StaticEnum<SETTING_ENUM>()->GetNameByValue(int64(Input->GetSettings().SETTING_NAME)))\
	.OptionsSource(GetEnumOptionSource<SETTING_ENUM>())\
	.OnGenerateWidget_Lambda([](FName InItem)\
	{\
		return SNew(STextBlock).Text(StaticEnum<SETTING_ENUM>()->GetDisplayNameTextByIndex(\
			StaticEnum<SETTING_ENUM>()->GetIndexByName(InItem)));\
	})\
	.OnSelectionChanged_Lambda([Input](FName NewChoice, ESelectInfo::Type SelectType)\
		{\
			Input->Set##SETTING_NAME((SETTING_ENUM)StaticEnum<SETTING_ENUM>()->GetValueByName(NewChoice));\
		})\
	[\
		SNew(STextBlock)\
		.Text_Lambda([Input]()\
			{\
				return StaticEnum<SETTING_ENUM>()->GetDisplayNameTextByValue(\
					int64(Input->GetSettings().SETTING_NAME));\
			})\
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))\
	]

void FHoudiniInputDetails::CreateWidgetContent(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input)
{
	VerticalBox->AddSlot()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("ContentInputSettings", "Content Input Settings"))
		.InitiallyCollapsed(!Input->bExpandSettings)
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(CheckChanged,
					LOCTEXT("CheckChanged", "Check Changed"),
					LOCTEXT("ContentInputCheckChangedHelp", "Only Check Asset Modified By Houdini Output And Auto Reimport When Cook"))
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportAsReference,
					LOCTEXT("ImportAsReference", "Import As Reference"),
					LOCTEXT("ImportAsReferenceHelp", "Import As Points With Asset Infos"))
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportRenderData,
					LOCTEXT("ImportRenderData", "Import Render Data (Nanite Fallback)"),
					LOCTEXT("ImportRenderDataHelp", "Import Static Mesh Render Data"))
				.IsEnabled_Lambda([Input]() { return !Input->GetSettings().bImportAsReference; })
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.IsEnabled_Lambda([Input]() { return !Input->GetSettings().bImportAsReference; })
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					HOUDINI_INPUT_OPTION_SETTING_WIDGET(LODImportMethod, EHoudiniStaticMeshLODImportMethod)
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					HOUDINI_INPUT_OPTION_SETTING_WIDGET(CollisionImportMethod, EHoudiniMeshCollisionImportMethod)
				]
			]
		]
		.OnAreaExpansionChanged_Lambda([Input](const bool& bIsExpanded)
			{
				if (Input.IsValid())
					Input->bExpandSettings = bIsExpanded;
			})
	];

	// -------- Asset slot class filters --------
	TArray<const UClass*> ContentInputAllowClasses;
	for (const TSharedPtr<IHoudiniContentInputBuilder>& InputBuilder : FHoudiniEngine::Get().GetContentInputBuilders())
		InputBuilder->AppendAllowClasses(ContentInputAllowClasses);

	TArray<const UClass*> AllowClasses, DisallowClasses;
	Input->GetSettings().GetFilterClasses(AllowClasses, DisallowClasses);
	AllowClasses.RemoveAll([&](const UClass*& Class)
		{
			for (const UClass* ContentInputClass : ContentInputAllowClasses)
			{
				if (Class == ContentInputClass || Class->IsChildOf(ContentInputClass))
					return false;
				else if (ContentInputClass->IsChildOf(Class))
				{
					Class = ContentInputClass;
					return false;
				}
			}
			return true;
		});
	if (AllowClasses.IsEmpty())
		AllowClasses = ContentInputAllowClasses;

	const bool bMutableHolderCount = Input->GetSettings().NumHolders <= 0;

	// -------- Dynamic slot control widges --------
	if (bMutableHolderCount)
	{
		TSharedPtr<SHorizontalBox> SlotActionBox;

		VerticalBox->AddSlot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SAssetDropTarget)  // Drop multiple assets into this inputs
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([AllowClasses, DisallowClasses](TArrayView<FAssetData> InAssets)
				{
					for (const FAssetData& AssetData : InAssets)
					{
						if (FHoudiniEngineUtils::FilterClass(AllowClasses, DisallowClasses, AssetData.GetClass()))
							return true;
					}
					return false;
				})
			.OnAssetsDropped_Lambda([Input, &CategoryBuilder](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					if (!Input.IsValid())
						return;

					TSet<TSoftObjectPtr<UObject>> ImportedAssets;  // We should NOT allow asset to import twice within one single input
					for (const UHoudiniInputHolder* Holder : Input->Holders)
					{
						if (IsValid(Holder))
							ImportedAssets.Add(Holder->GetObject());
					}

					int32 HolderIdx = Input->Holders.Num() - 1;
					for (; HolderIdx >= 0; --HolderIdx)
					{
						if (IsValid(Input->Holders[HolderIdx]))
							break;
					}
					++HolderIdx;

					bool bHasNewAsset = false;
					const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& InputBuilders = FHoudiniEngine::Get().GetContentInputBuilders();
					for (const FAssetData& AssetData : InAssets)
					{
						UObject* Asset = AssetData.GetAsset();
						if (!IsValid(Asset))
							continue;

						if (ImportedAssets.Contains(Asset))  // If asset has been imported, then we just reimport it
						{
							for (UHoudiniInputHolder* Holder : Input->Holders)
							{
								if (IsValid(Holder) && Holder->GetObject() == Asset)
									Holder->RequestReimport();
							}
							continue;
						}

						for (int32 BuilderIdx = InputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
						{
							if (UHoudiniInputHolder* NewHolder = InputBuilders[BuilderIdx]->CreateOrUpdate(Input.Get(), Asset, nullptr))
							{
								if (Input->Holders.IsValidIndex(HolderIdx))
									Input->Holders[HolderIdx] = NewHolder;
								else
									Input->Holders.Add(NewHolder);
								++HolderIdx;
								bHasNewAsset = true;
							}
						}
					}

					if (bHasNewAsset)
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();
				})
			[
				SAssignNew(SlotActionBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([Input]() { return FText::Format(LOCTEXT("AssetCount", "Num Of Assets: {0}"), Input->Holders.Num()); })
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				[
					PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([Input, &CategoryBuilder]()
						{
							Input->Holders.Add(nullptr);
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						}))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([Input, &CategoryBuilder]()
						{
							Input->RequestClear();
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						}))
				]
			]
		];

		if (Input->Holders.Num() >= 2)
		{
			SlotActionBox->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(0.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.IsFocusable(false)
				.Content()
				[
					SNew(SImage)
					.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"))
					.DesiredSizeOverride(FVector2D(16.0, 16.0))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				.OnClicked_Lambda([Input]()
					{
						if (Input.IsValid())
						{
							for (UHoudiniInputHolder* Holder : Input->Holders)
							{
								if (IsValid(Holder))
									Holder->RequestReimport();
							}
						}
						return FReply::Handled();
					})
				.ToolTipText(LOCTEXT("ReimportAll", "Reimport All"))
			];
		}
	}

	// -------- Asset slot widges --------
	for (int32 HolderIdx = 0; HolderIdx < Input->Holders.Num(); ++HolderIdx)
	{
		const TWeakObjectPtr<UHoudiniInputHolder> CurrHolder = Input->Holders[HolderIdx];  // CurrHolder may be nullptr

		TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = CategoryBuilder.GetParentLayout().GetThumbnailPool();
		FAssetData AssetData = CurrHolder.IsValid() ? IAssetRegistry::GetChecked().GetAssetByObjectPath(CurrHolder->GetObject().ToSoftObjectPath()) : FAssetData(nullptr);
		if (!AssetData.IsValid() && AllowClasses.Num() == 1)
			AssetData.AssetClassPath = AllowClasses[0]->GetPathName();  // Set Default class thumbnail
		TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, 64, 64, AssetThumbnailPool));
	
		TSharedPtr<STextBlock> AssetNameBlock;
		TSharedRef<SComboButton> AssetComboButton = SNew(SComboButton)
			.IsFocusable(false)  // Avoid attrib-panel close automatically
			.ContentPadding(1.0f)
			.ButtonContent()
			[
				SAssignNew(AssetNameBlock, STextBlock)
				.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.Text(FText::FromName(AssetData.AssetName))
			];


		auto OnAssetPickedLambda = [Input, HolderIdx, ContentInputAllowClasses, AssetThumbnailPool, AssetThumbnail, AssetNameBlock, AssetComboButton]
			(const FAssetData& AssetData)
			{
				if (AssetComboButton->IsOpen())
					AssetComboButton->SetIsOpen(false);

				if (!AssetData.IsValid())
				{
					if (!Input->Holders.IsValidIndex(HolderIdx) || !IsValid(Input->Holders[HolderIdx]))
						return;

					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniInputChange", "Houdini Input: Changing a value"),
						Input->GetOuter());

					Input->Modify();
					HOUDINI_FAIL_INVALIDATE(Input->HapiRemoveHolder(HolderIdx, false));

					return;
				}

				FSoftObjectPath NewAssetPath = AssetData.ToSoftObjectPath();  // NewAsset must be valid
				UHoudiniInputHolder* CurrHolder = Input->Holders[HolderIdx];  // CurrHolder may be nullptr
				const FSoftObjectPath OldAssetPath = IsValid(CurrHolder) ? CurrHolder->GetObject().ToSoftObjectPath() : nullptr;
				if (OldAssetPath != NewAssetPath)
				{
					AssetThumbnail->SetAsset(AssetData);
					AssetThumbnailPool->RefreshThumbnail(AssetThumbnail);
					AssetNameBlock->SetText(FText::FromName(AssetData.AssetName));

					const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& InputBuilders = FHoudiniEngine::Get().GetContentInputBuilders();
					for (int32 BuilderIdx = InputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
					{
						if (UHoudiniInputHolder* NewHolder = InputBuilders[BuilderIdx]->CreateOrUpdate(Input.Get(), AssetData.GetAsset(), CurrHolder))
						{
							if (NewHolder != CurrHolder && IsValid(CurrHolder))
								HOUDINI_FAIL_INVALIDATE(CurrHolder->HapiDestroy());
							Input->Holders[HolderIdx] = NewHolder;
						}
					}
				}
			};

		TSharedPtr<SHorizontalBox> AssetToolBarBox;

		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SAssetDropTarget)
			.OnAreAssetsAcceptableForDrop_Lambda([AllowClasses, DisallowClasses](TArrayView<FAssetData> InAssets)
				{
					return FHoudiniEngineUtils::FilterClass(AllowClasses, DisallowClasses, InAssets[0].GetClass());
				})
			.OnAssetsDropped_Lambda([OnAssetPickedLambda](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					OnAssetPickedLambda(InAssets[0]);
				})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"))
					.OnMouseDoubleClick_Lambda([CurrHolder](const FGeometry&, const FPointerEvent&)
						{
							if (!CurrHolder.IsValid())
								return FReply::Handled();

							UObject* Asset = CurrHolder->GetObject().LoadSynchronous();
							if (IsValid(Asset))
								GEditor->EditObject(Asset);

							return FReply::Handled();
						})
					[
						SNew(SBox)
						.HeightOverride(48.0f)
						.WidthOverride(48.0f)
						[
							AssetThumbnail->MakeThumbnailWidget()
						]
					]
				]

				+ SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.AutoHeight()
					.Padding(2.0f)
					[
						AssetComboButton
					]

					+ SVerticalBox::Slot()
					.VAlign(VAlign_Bottom)
					.AutoHeight()
					[
						SAssignNew(AssetToolBarBox, SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.Padding(2.0f)
						.AutoWidth()
						[
							PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateLambda([OnAssetPickedLambda]()
									{
										TArray<FAssetData> Selections;
										GEditor->GetContentBrowserSelections(Selections);

										if (Selections.IsValidIndex(0))
											OnAssetPickedLambda(Selections[0]);
									}),
								LOCTEXT("PickAssetFromContentBrowser", "Pick Asset From Content Browser"))
						]

						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.Padding(2.0f)
						.AutoWidth()
						[
							PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateLambda([CurrHolder]()
									{
										if (!CurrHolder.IsValid())
											return;

										FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(CurrHolder->GetObject().ToSoftObjectPath());
										if (AssetData.IsValid())
											GEditor->SyncBrowserToObjects(TArray<FAssetData>{ AssetData });
									}),
								LOCTEXT("BrowseAssetInContentBrowser", "Browse Asset in Content Browser"),
								AssetData.IsUAsset())
						]

						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.IsFocusable(false)
							.Content()
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("GenericCommands.Copy"))
								.DesiredSizeOverride(FVector2D(12.0, 12.0))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
							.OnClicked_Lambda([CurrHolder]()
								{
									if (CurrHolder.IsValid())
									{
										const FString AssetRef = FHoudiniEngineUtils::GetAssetReference(CurrHolder->GetObject().ToSoftObjectPath());
										if (!AssetRef.IsEmpty())
											FPlatformApplicationMisc::ClipboardCopy(*AssetRef);
									}

									return FReply::Handled();
								})
							.IsEnabled(CurrHolder.IsValid())
						]

						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.IsFocusable(false)
							.Content()
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("GenericCommands.Paste"))
								.DesiredSizeOverride(FVector2D(12.0, 12.0))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
							.OnClicked_Lambda([OnAssetPickedLambda]()
								{
									FString ValueStr;
									FPlatformApplicationMisc::ClipboardPaste(ValueStr);
									if (!ValueStr.IsEmpty())
									{
										FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(ValueStr);
										if (AssetData.IsValid())
											OnAssetPickedLambda(AssetData);
									}
									
									return FReply::Handled();
								})
						]
						
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.IsFocusable(false)
							.Content()
							[
								SNew(SImage)
								.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"))
								.DesiredSizeOverride(FVector2D(16.0, 16.0))
								.ColorAndOpacity_Lambda([CurrHolder]()
									{
										if (!CurrHolder.IsValid())
											return FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);
										
										return CurrHolder->HasChanged() ? FLinearColor::White : FLinearColor(0.9f, 1.0f, 0.8f, 0.8f);
									})
							]
							.OnClicked_Lambda([CurrHolder]()
								{
									if (CurrHolder.IsValid())
										CurrHolder->RequestReimport();
									return FReply::Handled();
								})
							.IsEnabled(CurrHolder.IsValid())
						]
					]
				]
			]
		];
		
		if (bMutableHolderCount)  // We need NOT have dynamic slot widgets is slot count is const
		{
			AssetToolBarBox->AddSlot()
			.HAlign(HAlign_Left)
			.Padding(2.0f)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
					FExecuteAction::CreateLambda([Input, HolderIdx, &CategoryBuilder]()
						{
							Input->Holders.Insert(nullptr, HolderIdx);
							CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						}),
					FExecuteAction::CreateLambda([Input, HolderIdx, &CategoryBuilder]()
						{
							if (!Input.IsValid() || !Input->Holders.IsValidIndex(HolderIdx))
								return;

							FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
								LOCTEXT("HoudiniInputChange", "Houdini Input: Changing a value"),
								Input->GetOuter());

							Input->Modify();
							HOUDINI_FAIL_INVALIDATE(Input->HapiRemoveHolder(HolderIdx));

							CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						}),
					FExecuteAction())
			];
		}

		AssetComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([OnAssetPickedLambda, AssetData, AllowClasses, DisallowClasses, Input]()
			{
				return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(AssetData, true, false,
					AllowClasses, DisallowClasses, TArray<UFactory*>{},
					Input->GetSettings().HasAssetFilters() ? FOnShouldFilterAsset::CreateLambda([Input](const FAssetData& AssetData)
						{
							return !Input->GetSettings().FilterString(AssetData.GetFullName());
						}) : FOnShouldFilterAsset(),
					FOnAssetSelected::CreateLambda(OnAssetPickedLambda),
					FSimpleDelegate::CreateLambda([]() {}));
			}));
	}
}

static TSharedRef<SButton> CreateDrawButton(const TWeakObjectPtr<UHoudiniInput>& Input,
	const FSlateBrush* Icon, const FText& Text, const EHoudiniCurveDrawingType& DrawingType)
{
	return SNew(SButton)
		.IsFocusable(false)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(-4.0, 0.0))
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(Icon)
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
				.ColorAndOpacity(Input->GetSettings().DefaultCurveColor)
			]
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.ColorAndOpacity(Input->GetSettings().DefaultCurveColor)
			]
		]
		.OnClicked_Lambda([Input, DrawingType]()
			{
				TArray<TObjectPtr<UHoudiniInputHolder>>& Holders = Input->Holders;
				if (!Holders.IsValidIndex(0))
					Holders.Add(UHoudiniInputCurves::Create(Input.Get()));

				FHoudiniEngineEditor::Get().GetCurvesComponentVisualizer()->AddCurve(
					Cast<UHoudiniInputCurves>(Holders[0])->GetCurvesComponent(), DrawingType, true);

				return FReply::Handled();
			});
}

void FHoudiniInputDetails::CreateWidgetCurves(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input)
{
	VerticalBox->AddSlot()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("CurveInputSettings", "Curve Input Settings"))
		.InitiallyCollapsed(!Input->bExpandSettings)
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(CheckChanged,
					LOCTEXT("CheckChanged", "Check Changed"),
					LOCTEXT("CurveInputCheckChangedHelp", "Check curves modification and trigger cook"))
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportRotAndScale,
					LOCTEXT("ImportRotAndScale", "Import rot and scale"),
					LOCTEXT("ImportRotAndScaleHelp", "Import rot and scale attributes"))
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportCollisionInfo,
					LOCTEXT("ImportPointCollidedPathAndNormal", "Import point collided name and normal"),
					LOCTEXT("ImportPointCollidedPathAndNormalHelp", "Import point collided name and normal attributes"))
			]
		]
		.OnAreaExpansionChanged_Lambda([Input](const bool& bIsExpanded)
			{
				if (Input.IsValid())
					Input->bExpandSettings = bIsExpanded;
			})
	];

	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(2.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([Input]()
				{
					if (Input->GetType() != EHoudiniInputType::Curves)
						return LOCTEXT("NoCurve", "Num Of Curves: 0");

					TArray<TObjectPtr<UHoudiniInputHolder>>& Holders = Input->Holders;
					if (!Holders.IsValidIndex(0))
						return LOCTEXT("NoCurve", "Num Of Curves: 0");

					const int32 NumCurves = Cast<UHoudiniInputCurves>(Holders[0])->GetCurvesComponent()->NumPrims();
					return NumCurves ? FText::Format(LOCTEXT("CurveCount", "Num Of Curves: {0}"), NumCurves) :
						LOCTEXT("NoCurve", "Num Of Curves: 0");
				})
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
		]
		
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsFocusable(false)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("GenericCommands.Copy"))
				.DesiredSizeOverride(FVector2D(12.0, 12.0))
			]
			.OnClicked_Lambda([Input]()
				{
					if (Input->Holders.IsValidIndex(0))
					{
						FJsonDataBag DataBag;
						DataBag.JsonObject = Cast<UHoudiniInputCurves>(Input->Holders[0])->GetCurvesComponent()->ConvertToJson();
						if (DataBag.JsonObject)
							FPlatformApplicationMisc::ClipboardCopy(*DataBag.ToJson());
					}

					return FReply::Handled();
				})
			.ToolTipText(LOCTEXT("CopyCurves", "Copy Curves To Clipboard"))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsFocusable(false)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("GenericCommands.Paste"))
				.DesiredSizeOverride(FVector2D(12.0, 12.0))
			]
			.OnClicked_Lambda([Input]()
				{
					FString JsonStr;
					FPlatformApplicationMisc::ClipboardPaste(JsonStr);
					
					TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonStr);
					TSharedPtr<FJsonObject> JsonCurves;
					if (FJsonSerializer::Deserialize(JsonReader, JsonCurves))
					{
						TArray<TObjectPtr<UHoudiniInputHolder>>& Holders = Input->Holders;
						if (!Holders.IsValidIndex(0))
						{
							Holders.Add(UHoudiniInputCurves::Create(Input.Get()));
							FHoudiniEngineEditorUtils::ReselectSelectedActors();
						}
						
						if (Cast<UHoudiniInputCurves>(Holders[0])->GetCurvesComponent()->AppendFromJson(JsonCurves))
							return FReply::Handled();
					}

					return FReply::Unhandled();
				})
			.ToolTipText(LOCTEXT("PastCurves", "Append Curves From Clipboard"))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsFocusable(false)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
				.ColorAndOpacity(FStyleColors::AccentRed)
			]
			.OnClicked_Lambda([Input]()
				{
					UHoudiniCurvesComponent* EditGeo = nullptr;
					if (Input->Holders.IsValidIndex(0))  // Check if there has a CurvesComponent
					{
						if (UHoudiniInputCurves* CurveInput = Cast<UHoudiniInputCurves>(Input->Holders[0]))
							EditGeo = CurveInput->GetCurvesComponent();
					}

					Input->RequestClear();
					
					if (EditGeo)
					{
						if (UHoudiniAttributeParameterHolder* AttribParmHolder = UHoudiniAttributeParameterHolder::Get())
						{
							// Maybe editor mode attribute panel is open and displaying this CurvesComponent, then we should refresh it
							if (AttribParmHolder->GetEditableGeometry() == EditGeo && AttribParmHolder->IsEditorModePanelOpen())
								AttribParmHolder->DetailsView.Pin()->ForceRefresh();
						}
					}

					return FReply::Handled();
				})
		]
	];

	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(2.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			CreateDrawButton(Input, FAppStyle::Get().GetBrush("Icons.Plus"), LOCTEXT("Draw", "Draw"), EHoudiniCurveDrawingType::Click)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			CreateDrawButton(Input, FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.PlusCircle"), LOCTEXT("Circle", "Circle"), EHoudiniCurveDrawingType::Circle)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			CreateDrawButton(Input, FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.PlusRect"), LOCTEXT("Rect", "Rect"), EHoudiniCurveDrawingType::Rectangle)
		]
	];
}


FORCEINLINE static void RequestLandscapeReimport(const TWeakObjectPtr<UHoudiniInput>& Input)
{
	for (UHoudiniInputHolder* Holder : Input->Holders)
	{
		UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder);
		if (IsValid(LandscapeInput))
			LandscapeInput->RequestReimport();
	}
}

static void CreateLandscapeLayerFilterWidgets(const TWeakObjectPtr<UHoudiniInput> Input, const TSharedPtr<SVerticalBox> LandscapeLayersBox)
{
	LandscapeLayersBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsFocusable(false)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
				.ColorAndOpacity(FStyleColors::AccentGreen)
			]
			.OnClicked_Lambda([Input, LandscapeLayersBox]()
				{
					FHoudiniInputSettings* Settings = const_cast<FHoudiniInputSettings*>(&Input->GetSettings());
					if (Settings->LandscapeLayerFilterMap.Contains(FName("")))
						return FReply::Unhandled();

					Settings->LandscapeLayerFilterMap.Add(FName(""));

					LandscapeLayersBox->ClearChildren();
					CreateLandscapeLayerFilterWidgets(Input, LandscapeLayersBox);

					return FReply::Handled();
				})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(LOCTEXT("HoudiniInputLandscapeLayerFilters", "Landscape Layer Filters"))
		]
	];

	for (const auto& EditLayerFilter : Input->GetSettings().LandscapeLayerFilterMap)
	{
		const FName EditLayerName = EditLayerFilter.Key;
		const FString Filter = EditLayerFilter.Value;

		TSharedPtr<SEditableTextBox> EditLayerNameTextBox;
		TSharedPtr<SEditableTextBox> LayerFilterTextBox;
		TSharedPtr<SHorizontalBox> EditLayerFilterRow;

		LandscapeLayersBox->AddSlot()
		.AutoHeight()
		[
			SAssignNew(EditLayerFilterRow, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			[
				SAssignNew(EditLayerNameTextBox, SEditableTextBox)
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(FText::FromName(EditLayerName))
				.OnTextCommitted_Lambda([Input, LandscapeLayersBox, EditLayerName](const FText& NewText, ETextCommit::Type TextCommitType)
					{
						const FName NewEditLayerName = *NewText.ToString();
						if (EditLayerName == NewEditLayerName)
							return;

						FHoudiniInputSettings* Settings = const_cast<FHoudiniInputSettings*>(&Input->GetSettings());
						if (!Settings->LandscapeLayerFilterMap.Contains(NewEditLayerName))
						{
							FString* FilterPtr = Settings->LandscapeLayerFilterMap.Find(EditLayerName);
							Settings->LandscapeLayerFilterMap.Add(NewEditLayerName, (FilterPtr ? *FilterPtr : FString()));
							if (FilterPtr)
								Settings->LandscapeLayerFilterMap.Remove(EditLayerName);

							if (Input->GetType() == EHoudiniInputType::World && Settings->bCheckChanged)
								RequestLandscapeReimport(Input);
						}

						LandscapeLayersBox->ClearChildren();
						CreateLandscapeLayerFilterWidgets(Input, LandscapeLayersBox);
					})
				.ToolTipText(LOCTEXT("HoudiniLandscapeEditLayerFilterHelp", "EditLayer Name"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.6f)
			[
				SAssignNew(LayerFilterTextBox, SEditableTextBox)
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(FText::FromString(EditLayerFilter.Value))
				.OnTextCommitted_Lambda([Input, EditLayerName](const FText& NewText, ETextCommit::Type TextCommitType)
					{
						const FString& NewFilter = NewText.ToString();

						FHoudiniInputSettings* Settings = const_cast<FHoudiniInputSettings*>(&Input->GetSettings());
						if (FString* FilterPtr = Settings->LandscapeLayerFilterMap.Find(EditLayerName))
						{
							if (!FilterPtr->Equals(NewFilter))
							{
								*FilterPtr = NewFilter;
								if (Input->GetType() == EHoudiniInputType::World && Settings->bCheckChanged)
									RequestLandscapeReimport(Input);
							}
						}
					})
				.ToolTipText(LOCTEXT("HoudiniLandscapeEditLayerFilterHelp", "Weight Layer Names, or \"height\" and \"Alpha\""))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.IsFocusable(false)
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
					.DesiredSizeOverride(FVector2D(16.0, 16.0))
					.ColorAndOpacity(FStyleColors::AccentRed)
				]
				.OnClicked_Lambda([Input, LandscapeLayersBox, EditLayerName]()
					{
						FHoudiniInputSettings* Settings = const_cast<FHoudiniInputSettings*>(&Input->GetSettings());
						Settings->LandscapeLayerFilterMap.Remove(EditLayerName);
						
						LandscapeLayersBox->ClearChildren();
						CreateLandscapeLayerFilterWidgets(Input, LandscapeLayersBox);

						if (Input->GetType() == EHoudiniInputType::World && Settings->bCheckChanged)
							RequestLandscapeReimport(Input);

						return FReply::Handled();
					})
			]
		];
	}
}


static FText ConvertInputFiltersToText(const FHoudiniInputSettings& Settings)
{
	const bool bUseClassFilters = (Settings.ActorFilterMethod == EHoudiniActorFilterMethod::Class);
	const TArray<FString>& SettingFilters = bUseClassFilters ? Settings.AllowClassNames : Settings.Filters;
	const TArray<FString>& SettingInvertFilders = bUseClassFilters ? Settings.DisallowClassNames : Settings.InvertedFilters;

	FString FilterStr;
	for (const FString& Filter : SettingFilters)
	{
		if (!FilterStr.IsEmpty())
			FilterStr += TEXT(" ");
		FilterStr += Filter;
	}

	for (const FString& InvertedFilter : SettingInvertFilders)
	{
		FilterStr += FilterStr.IsEmpty() ? TEXT("* ^") : TEXT(" ^");
		FilterStr += InvertedFilter;
	}

	return FText::FromString(FilterStr);
}

static void CreateActorFilterWidget(const TWeakObjectPtr<UHoudiniInput> Input, const TSharedPtr<SVerticalBox>& SettingsBox)
{
	if (Input->GetSettings().ActorFilterMethod != EHoudiniActorFilterMethod::Selection)  // Filter actors by filters
	{
		TSharedPtr<SEditableTextBox> ActorFilterTextBox;

		SettingsBox->AddSlot()
		.Padding(2.0f)
		.AutoHeight()
		[
			SAssignNew(ActorFilterTextBox, SEditableTextBox)
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.Text(ConvertInputFiltersToText(Input->GetSettings()))
			.ToolTipText(LOCTEXT("ActorFilterHelp", "Actor Filter String\ne.g.\nLight StaticMeshActor ^PointLight\nMesh ^Cube ^Sphere"))
		];

		class SSetOnTextCommited : public SEditableTextBox
		{
		public:
			void Set(const FOnTextCommitted& TextCommitted) { OnTextCommitted = TextCommitted; }
		};

		StaticCastSharedPtr<SSetOnTextCommited>(ActorFilterTextBox)->Set(FOnTextCommitted::CreateLambda(
			[Input, ActorFilterTextBox](const FText& NewText, ETextCommit::Type TextCommitType)
			{
				if (Input.IsValid())
				{
					Input->SetFilterString(NewText.ToString());
					if (ActorFilterTextBox.IsValid())
						ActorFilterTextBox->SetText(ConvertInputFiltersToText(Input->GetSettings()));
				}
			}));
	}

	TSharedPtr<SScrollBox> ImportedActorListBox;
	SettingsBox->AddSlot()
	.Padding(2.0f)
	.AutoHeight()
	[
		SNew(SBox)
		.HeightOverride(FMath::Clamp(Input->Holders.Num() * 20.0f, 50.0f, 100.0f))
		[
			SNew(SBorder)
			.BorderImage(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CheckBox").BackgroundImage)
			[
				SAssignNew(ImportedActorListBox, SScrollBox)
			]
		]
	];
	
	// List all actors on details panel
	for (int32 HolderIdx = 0; HolderIdx < Input->Holders.Num(); ++HolderIdx)
	{
		const TWeakObjectPtr<UHoudiniInputHolder> Holder = Input->Holders[HolderIdx];
		if (!Holder.IsValid())
			continue;

		FName ActorName;
		AActor* Actor = nullptr;
		if (UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(Holder))
		{
			Actor = ActorInput->GetActor();
			ActorName = ActorInput->GetActorName();
		}
		else if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
		{
			Actor = (AActor*)LandscapeInput->GetLandscape();
			ActorName = LandscapeInput->GetLandscapeName();
		}
		else
			continue;

		TSharedRef<SHorizontalBox> ImportedActorBox = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4.0, 0.0, 0.0, 0.0)
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(Actor ? FText::FromString(Actor->GetActorLabel()) : FText::FromName(ActorName))
				.IsEnabled(bool(Actor))
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.IsFocusable(false)
				.Content()
				[
					SNew(SImage)
					.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"))
					.DesiredSizeOverride(FVector2D(14.0, 14.0))
				]
				.OnClicked_Lambda([Holder]()
					{
						if (Holder.IsValid())
							Holder->RequestReimport();
						return FReply::Handled();
					})
			];

		if (Input->GetSettings().ActorFilterMethod == EHoudiniActorFilterMethod::Selection)
		{
			ImportedActorBox->AddSlot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.IsFocusable(false)
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Delete"))
					.ColorAndOpacity(FStyleColors::AccentRed)
					.DesiredSizeOverride(FVector2D(14.0, 14.0))
				]
				.OnClicked_Lambda([Input, HolderIdx]()
					{
						if (Input.IsValid() && Input->Holders.IsValidIndex(HolderIdx))
						{
							FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
								LOCTEXT("HoudiniInputChange", "Houdini Input: Changing a value"),
								Input->GetOuter());

							Input->Modify();
							HOUDINI_FAIL_INVALIDATE(Input->HapiRemoveHolder(HolderIdx));
						}
						return FReply::Handled();
					})
			];
		}

		ImportedActorListBox->AddSlot() [ ImportedActorBox ];
	}
}


void FHoudiniInputDetails::CreateWidgetWorld(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input)
{
	TSharedPtr<SVerticalBox> LandscapeLayersBox;
	TSharedPtr<SVerticalBox> SettingsBox;
	VerticalBox->AddSlot()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("WorldInputSettings", "World Input Settings"))
		.InitiallyCollapsed(!Input->bExpandSettings)
		.BodyContent()
		[
			SAssignNew(SettingsBox, SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(CheckChanged,
					LOCTEXT("CheckChanged", "Check Changed"),
					LOCTEXT("WorldInputCheckChangedHelp", "Check whether landscape layer brushed or actor changed"))
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportAsReference,
					LOCTEXT("ImportAsReference", "Import As Reference"),
					LOCTEXT("ImportAsReferenceHelp", "Import As Points With Asset Infos"))
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportRenderData,
					LOCTEXT("ImportRenderData", "Import Render Data (Nanite Fallback)"),
					LOCTEXT("ImportRenderDataHelp", "Import Static Mesh Render Data"))
				.IsEnabled_Lambda([Input]() { return !Input->GetSettings().bImportAsReference; })
			]
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.IsEnabled_Lambda([Input]() { return !Input->GetSettings().bImportAsReference; })
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					HOUDINI_INPUT_OPTION_SETTING_WIDGET(LODImportMethod, EHoudiniStaticMeshLODImportMethod)
				]
				
				+ SHorizontalBox::Slot()
				[
					HOUDINI_INPUT_OPTION_SETTING_WIDGET(CollisionImportMethod, EHoudiniMeshCollisionImportMethod)
				]
			]
			
			// Landscape settings
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			.AutoHeight()
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(ImportLandscapeSplines,
					LOCTEXT("ImportLandscapeSplines", "Import Landscape Splines"),
					FText::GetEmpty())
			]
			
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			.AutoHeight()
			[
				SAssignNew(LandscapeLayersBox, SVerticalBox)
				.IsEnabled_Lambda([Input]() { return !Input->GetSettings().bImportAsReference; })
			]

			// Actor import method
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ActorFilterMethod", "Actor Filter Method"))
					.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SComboBox<FName>)
					.InitiallySelectedItem(StaticEnum<EHoudiniActorFilterMethod>()->GetNameByValue(int64(Input->GetSettings().ActorFilterMethod)))
					.OptionsSource(GetEnumOptionSource<EHoudiniActorFilterMethod>())
					.OnGenerateWidget_Lambda([](FName InItem)
						{
							return SNew(STextBlock).Text(StaticEnum<EHoudiniActorFilterMethod>()->GetDisplayNameTextByIndex(
								StaticEnum<EHoudiniActorFilterMethod>()->GetIndexByName(InItem)));
						})
					.OnSelectionChanged_Lambda([&CategoryBuilder, Input](FName NewChoice, ESelectInfo::Type SelectType)
						{
							if (Input.IsValid())
							{
								Input->SetActorFilterMethod((EHoudiniActorFilterMethod)StaticEnum<EHoudiniActorFilterMethod>()->GetValueByName(NewChoice));
								CategoryBuilder.GetParentLayout().ForceRefreshDetails();
							}
						})
					[
						SNew(STextBlock)
						.Text_Lambda([Input]()
						{
							return StaticEnum<EHoudiniActorFilterMethod>()->GetDisplayNameTextByValue(
								int64(Input->GetSettings().ActorFilterMethod));
						})
						.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
			]
		]
		.OnAreaExpansionChanged_Lambda([Input](const bool& bIsExpanded)
			{
				if (Input.IsValid())
					Input->bExpandSettings = bIsExpanded;
			})
	];

	// Landscape layer filter settings, could specify editlayers
	CreateLandscapeLayerFilterWidgets(Input, LandscapeLayersBox);

	CreateActorFilterWidget(Input, SettingsBox);

	if (Input->GetSettings().ActorFilterMethod == EHoudiniActorFilterMethod::Selection)
	{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
		TSharedPtr<IDetailsView> DetailsView = CategoryBuilder.GetParentLayout().GetDetailsViewSharedPtr();
#else
		IDetailsView* DetailsView = CategoryBuilder.GetParentLayout().GetDetailsView();
#endif
		TSharedRef<SImage> SelectButtonIcon = SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("FoliageEditMode.SetSelect"))
			.ColorAndOpacity(FStyleColors::White)
			.DesiredSizeOverride(FVector2D(16.0, 16.0));
		TSharedRef<STextBlock> SelectButtonTextBlock = SNew(STextBlock)
			.Text(DetailsView->IsLocked() ?
				LOCTEXT("ImportSelection", "Import Selection") : LOCTEXT("StartSelect", "Start Select"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	
		class FHoudiniDetailsViewLocker : public SDetailsViewBase
		{
		public:
			FORCEINLINE void Lock() { bIsLocked = true; }
			FORCEINLINE void Unlock() { bIsLocked = false; }
		};

		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(2.0)
					.AutoWidth()
					[
						SelectButtonIcon
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SelectButtonTextBlock
					]
				]
				.OnClicked_Lambda([Input, SelectButtonIcon, SelectButtonTextBlock, DetailsView]()
					{
						if (DetailsView->IsLocked())
						{
							TArray<AActor*> Actors;
							USelection* SelectedActors = GEditor->GetSelectedActors();
							for (FSelectionIterator SelectionIter(*SelectedActors); SelectionIter; ++SelectionIter)
							{
								if (AActor* Actor = Cast<AActor>(*SelectionIter))
									Actors.Add(Actor);
							}

							HOUDINI_FAIL_INVALIDATE(Input->HapiImportActors(Actors));
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
							StaticCastSharedPtr<FHoudiniDetailsViewLocker>(DetailsView)->Unlock();
#else
							((FHoudiniDetailsViewLocker*)DetailsView)->Unlock();
#endif
							SelectButtonTextBlock->SetText(LOCTEXT("StartSelect", "Start Select"));
							SelectButtonIcon->SetImage(FAppStyle::Get().GetBrush("FoliageEditMode.SetSelect"));
							SelectButtonIcon->SetColorAndOpacity(FStyleColors::White);
						}
						else
						{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
							StaticCastSharedPtr<FHoudiniDetailsViewLocker>(DetailsView)->Lock();
#else
							((FHoudiniDetailsViewLocker*)DetailsView)->Lock();
#endif
							SelectButtonTextBlock->SetText(LOCTEXT("ImportSelection", "Import Selection"));
							SelectButtonIcon->SetImage(FAppStyle::Get().GetBrush("Icons.Plus"));
							SelectButtonIcon->SetColorAndOpacity(FStyleColors::AccentGreen);
						}

						return FReply::Handled();
					})
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.Content()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(2.0)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Delete"))
						.ColorAndOpacity(FStyleColors::AccentRed)
						.DesiredSizeOverride(FVector2D(16.0, 16.0))
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Clear", "Clear"))
						.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
					]
				]
				.OnClicked_Lambda([Input]
					{
						Input->RequestClear();
						return FReply::Handled();
					})
			]
		];
	}

	VerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SButton)
		.IsFocusable(false)
		.HAlign(HAlign_Center)
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Reimport", "Reimport"))
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			]
		]
		.OnClicked_Lambda([Input]()
			{
				if (Input->GetSettings().ActorFilterMethod != EHoudiniActorFilterMethod::Selection)
					HOUDINI_FAIL_INVALIDATE(Input->HapiReimportActorsByFilter())

				// Force to trigger every holder to re-import
				for (UHoudiniInputHolder* Holder : Input->Holders)
				{
					if (IsValid(Holder))
						Holder->RequestReimport();
				}

				return FReply::Handled();
			})
	];

	// TODO: other actor filter type
}

void FHoudiniInputDetails::CreateWidgetNode(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input)
{
	AHoudiniNode* SelectedNode = nullptr;
	if (Input->Holders.Num() == 1)
		SelectedNode = Cast<UHoudiniInputNode>(Input->Holders[0])->GetNode();

	TSharedPtr<STextBlock> AssetNameBlock;
	TSharedRef<SComboButton> AssetComboButton = SNew(SComboButton)
		.IsFocusable(false)  // Avoid attrib-panel close automatically
		.ContentPadding(1.0f)
		.ButtonContent()
		[
			SAssignNew(AssetNameBlock, STextBlock)
			.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
			.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
			.Text(SelectedNode ? FText::FromString(SelectedNode->GetActorLabel(false)) : FText::GetEmpty())
		];

	AssetComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([Input, SelectedNode, AssetNameBlock, AssetComboButton]()
		{
			return PropertyCustomizationHelpers::MakeActorPickerWithMenu(SelectedNode, true,
				FOnShouldFilterActor::CreateLambda([Input](const AActor* Actor)
					{
						const AHoudiniNode* Node = Cast<const AHoudiniNode>(Actor);
						if (!IsValid(Node))
							return false;
						
						if (!Input->CanNodeInput(Node))
							return false;
							
						return true;
					}),
				FOnActorSelected::CreateLambda([Input, AssetNameBlock, AssetComboButton](AActor* Actor)
					{
						if (AssetComboButton->IsOpen())
							AssetComboButton->SetIsOpen(false);

						AHoudiniNode* Node = Cast<AHoudiniNode>(Actor);
						if (IsValid(Node))
						{
							if (Input->Holders.Num() == 1)
								Cast<UHoudiniInputNode>(Input->Holders[0])->SetNode(Node);
							else
								Input->Holders = TArray<UHoudiniInputHolder*>{ UHoudiniInputNode::Create(Input.Get(), Node) };
							AssetNameBlock->SetText(FText::FromString(Actor->GetActorLabel(false)));
						}
						else
							Input->RequestClear();
					}),
				FSimpleDelegate::CreateLambda([]() {}), FSimpleDelegate::CreateLambda([]() {}));
		}));

	VerticalBox->AddSlot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			AssetComboButton
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(CheckChanged,
				LOCTEXT("Notify", "Notify"),
				LOCTEXT("NodeInputCheckChangedTooltip", "If upstream node updated, then will trigger this node to cook"))
		]
	];
}

void FHoudiniInputDetails::CreateWidgetMask(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input)
{
	VerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("MaskInputSettings", "Mask Input Settings"))
		.InitiallyCollapsed(!Input->bExpandSettings)
		.BodyContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f)
			[
				HOUDINI_INPUT_TOGGLE_SETTRING_WIDGET(CheckChanged,
					LOCTEXT("CheckChanged", "Check Changed"),
					LOCTEXT("MaskInputCheckChangedHelp", "Check whether mask brushed"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(LOCTEXT("MaskType", "Mask Type"))
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					HOUDINI_INPUT_OPTION_SETTING_WIDGET(MaskType, EHoudiniMaskType)
				]
			]
		]
		.OnAreaExpansionChanged_Lambda([Input](const bool& bIsExpanded)
			{
				if (Input.IsValid())
					Input->bExpandSettings = bIsExpanded;
			})
	];


	AActor* Landscape = nullptr;
	if (Input->Holders.Num() == 1)
		Landscape = (AActor*)Cast<UHoudiniInputMask>(Input->Holders[0])->GetLandscape();

	TSharedPtr<STextBlock> AssetNameBlock;
	TSharedPtr<SComboButton> AssetComboButton;
	VerticalBox->AddSlot()
	[
		SAssignNew(AssetComboButton, SComboButton)
		.IsFocusable(false)
		.ContentPadding(1.0f)
		.ButtonContent()
		[
			SAssignNew(AssetNameBlock, STextBlock)
			.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
			.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
			.Text(Landscape ? FText::FromString(Landscape->GetActorLabel(false)) : FText::GetEmpty())
		]
	];

	AssetComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([Input, Landscape, AssetNameBlock, AssetComboButton]()
		{
			return PropertyCustomizationHelpers::MakeActorPickerWithMenu(Landscape, true,
				FOnShouldFilterActor::CreateLambda([Input](const AActor* Actor) { return UHoudiniInputLandscape::IsLandscape(Actor); }),
				FOnActorSelected::CreateLambda([Input, AssetNameBlock, AssetComboButton](AActor* Actor)
					{
						if (AssetComboButton->IsOpen())
							AssetComboButton->SetIsOpen(false);

						ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor);
						if (!Landscape)
							return;

						if (Input->Holders.Num() == 1)
						{
							UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Input->Holders[0]);
							if (MaskInput->GetLandscapeName() != Actor->GetFName())
								MaskInput->SetNewLandscape(Landscape);
							return;
						}
						
						Input->Holders = TArray<UHoudiniInputHolder*>{ UHoudiniInputMask::Create(Input.Get(), Landscape) };
						AssetNameBlock->SetText(FText::FromString(Actor->GetActorLabel(false)));
					}),
				FSimpleDelegate::CreateLambda([]() {}), FSimpleDelegate::CreateLambda([]() {}));
		}));


	VerticalBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("LandscapeEditor.PaintTool"))
					.DesiredSizeOverride(FVector2D(16.0, 16.0))
				]
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Paint", "Paint"))
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				]
			]
			.OnClicked_Lambda([Input]()
				{
					if (Input->Holders.IsValidIndex(0))
						UHoudiniMaskTool::ForceActivate(Cast<UHoudiniInputMask>(Input->Holders[0]));
					
					return FReply::Handled();
				})
			.IsEnabled_Lambda([Input]() { return Input.IsValid() && Input->GetType() == EHoudiniInputType::Mask && Input->Holders.IsValidIndex(0); })
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"))
					.DesiredSizeOverride(FVector2D(16.0, 16.0))
					.ColorAndOpacity_Lambda([Input]()
						{
							if (Input.IsValid() && (Input->GetType() == EHoudiniInputType::Mask) && Input->Holders.IsValidIndex(0))
							{
								if (UHoudiniInputMask* InputMask = Cast<UHoudiniInputMask>(Input->Holders[0]))
								{
									if (InputMask->HasDataChanged())
										return FLinearColor::White;
								}
							}
							return FLinearColor(0.9f, 1.0f, 0.8f, 0.8f);
						})
				]
				+ SHorizontalBox::Slot()
				.Padding(2.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Reimport", "Reimport"))
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				]
			]
			.OnClicked_Lambda([Input]()
				{
					for (UHoudiniInputHolder* Holder : Input->Holders)
					{
						if (IsValid(Holder))
							Holder->RequestReimport();
					}

					return FReply::Handled();
				})
			.IsEnabled_Lambda([Input]() { return Input.IsValid() && (Input->GetType() == EHoudiniInputType::Mask) && Input->Holders.IsValidIndex(0); })
		]
	];
}

#undef LOCTEXT_NAMESPACE
