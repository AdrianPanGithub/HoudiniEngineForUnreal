// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniNodeDetails.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"

#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "SAssetDropTarget.h"
#include "ContentBrowserModule.h"
#include "Editor/ContentBrowser/Public/IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "HoudiniEngine.h"
#include "HoudiniNode.h"
#include "HoudiniAsset.h"
#include "HoudiniOutput.h"

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniParameterDetails.h"
#include "HoudiniInputDetails.h"
#include "HoudiniPDGDetails.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

const FName FHoudiniNodeDetails::NodeCategoryName("HoudiniNode");
const FName FHoudiniNodeDetails::PDGCategoryName("HoudiniPDG");
const FName FHoudiniNodeDetails::ParametersCategoryName("HoudiniParameters");
const FName FHoudiniNodeDetails::InputsCategoryName("HoudiniInputs");

void FHoudiniNodeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	TArray<AHoudiniNode*> Nodes;
	for (const TWeakObjectPtr<UObject>& Object : ObjectsCustomized)
	{
		if (Object.IsValid())
		{
			if (AHoudiniNode* Node = Cast<AHoudiniNode>(Object))
				Nodes.AddUnique(Node);
			else if (const AHoudiniNodeProxy* NodeProxy = Cast<AHoudiniNodeProxy>(Object))
			{
				Node = NodeProxy->GetParentNode();
				if (IsValid(Node))
					Nodes.AddUnique(Node);
			}
		}
	}

	CreateNodesDetails(DetailBuilder, Nodes, NodesAvailableOptions);
}

TSharedRef<IDetailCustomization> FHoudiniNodeDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniNodeDetails);
}


void FHoudiniNodeDetails::CreateNodesDetails(IDetailLayoutBuilder& DetailBuilder, const TArray<AHoudiniNode*>& Nodes,
	TArray<TArray<TSharedPtr<FString>>>& OutNodesAvailableOptions)
{
	OutNodesAvailableOptions.SetNum(Nodes.Num());
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
	{
		AHoudiniNode* Node = Nodes[NodeIdx];

		// Node Category
		IDetailCategoryBuilder& HoudiniNodeBuilder =
			DetailBuilder.EditCategory(NodeCategoryName, LOCTEXT("HoudiniNode", "Houdini Node"), ECategoryPriority::Important);
		CreateNodeSettingsWidget(HoudiniNodeBuilder, Node, OutNodesAvailableOptions[NodeIdx]);

		// PDG Category
		if (!Node->GetTopNodes().IsEmpty())
		{
			IDetailCategoryBuilder& HoudiniPDGBuilder =
				DetailBuilder.EditCategory(PDGCategoryName, LOCTEXT("HoudiniPDG", "Houdini PDG"), ECategoryPriority::Important);

			if (Nodes.Num() >= 2)  // Add a node title if multi node selected
			{
				HoudiniPDGBuilder.AddCustomRow(FText::GetEmpty())
					[
						FHoudiniEngineEditorUtils::CreateTitleSeparatorWidget(FText::FromString(Node->GetActorLabel(false)))
					];
			}

			FHoudiniPDGDetails::CreateWidgets(HoudiniPDGBuilder, Node);
		}

		// Parameter Category
		if (!Node->GetParameters().IsEmpty())
		{
			IDetailCategoryBuilder& HoudiniParameterBuilder =
				DetailBuilder.EditCategory(ParametersCategoryName, LOCTEXT("HoudiniParameters", "Houdini Parameters"), ECategoryPriority::Important);

			if (Nodes.Num() >= 2)  // Add a node title if multi node selected
			{
				HoudiniParameterBuilder.AddCustomRow(FText::GetEmpty())
					[
						FHoudiniEngineEditorUtils::CreateTitleSeparatorWidget(FText::FromString(Node->GetActorLabel(false)))
					];
			}

			FHoudiniParameterDetails::Parse(HoudiniParameterBuilder, Node->GetParameters(), false);
		}

		// Node Inputs Category
		if (FHoudiniInputDetails::HasNodeInputs(Node->GetInputs()))
		{
			IDetailCategoryBuilder& HoudiniInputBuilder =
				DetailBuilder.EditCategory(InputsCategoryName, LOCTEXT("HoudiniInputs", "Houdini Inputs"), ECategoryPriority::Important);

			if (Nodes.Num() >= 2)  // Add a node title if multi node selected
			{
				HoudiniInputBuilder.AddCustomRow(FText::GetEmpty())
					[
						FHoudiniEngineEditorUtils::CreateTitleSeparatorWidget(FText::FromString(Node->GetActorLabel(false)))
					];
			}

			FHoudiniInputDetails::CreateWidgets(HoudiniInputBuilder, Node->GetInputs());
		}
	}
}


#define SNEW_HOUDINI_NODE_ACTION_BUTTON(ICON_BRUSH, BUTTON_TEXT) SNew(SButton) \
	.IsFocusable(false) \
	.HAlign(HAlign_Center) \
	.Content() \
	[ \
		SNew(SHorizontalBox) \
		+ SHorizontalBox::Slot() \
		.Padding(2.0) \
		.AutoWidth() \
		[ \
			SNew(SImage) \
			.Image(ICON_BRUSH) \
			.DesiredSizeOverride(FVector2D(16.0, 16.0)) \
		] \
		+ SHorizontalBox::Slot() \
		.VAlign(VAlign_Center) \
		[ \
			SNew(STextBlock) \
			.Text(BUTTON_TEXT) \
		] \
	]

void FHoudiniNodeDetails::CreateNodeSettingsWidget(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<AHoudiniNode>& Node,
	TArray<TSharedPtr<FString>>& OutAvailableOptions)
{
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = CategoryBuilder.GetParentLayout().GetThumbnailPool();
	FAssetData AssetData(Node->GetAsset());
	if (!IsValid(Node->GetAsset()))
		AssetData.AssetClassPath = UHoudiniAsset::StaticClass()->GetPathName();
	TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, 64, 64, AssetThumbnailPool));

	TSharedPtr<SHorizontalBox> HoudiniAssetBox;

	CategoryBuilder.AddCustomRow(LOCTEXT("HoudiniNodeSettings", "LabelPresetHelp"))
	[
		SAssignNew(HoudiniAssetBox, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		[
			SNew(SAssetDropTarget)
			.IsEnabled_Lambda([]() { return FHoudiniEngine::Get().AllowEdit(); })
			.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData> InAssets)
				{
					return InAssets[0].AssetClassPath.GetAssetName() == UHoudiniAsset::StaticClass()->GetFName();
				})
			.OnAssetsDropped_Lambda([Node](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					if (UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(InAssets[0].GetAsset()))
						Node->Initialize(HoudiniAsset);
				})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"))
					.OnMouseDoubleClick_Lambda([Node](const FGeometry&, const FPointerEvent&)
						{
							if (IsValid(Node->GetAsset()))
								GEditor->EditObject(Node->GetAsset());

							return FReply::Handled();
						})
					[
						SNew(SBox)
						.HeightOverride(32.0f)
						.WidthOverride(32.0f)
						[
							AssetThumbnail->MakeThumbnailWidget()
						]
					]
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(0.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.IsFocusable(false)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Content()
					[
						SNew(STextBlock)
						.Text(FText::FromName(AssetData.AssetName))
						.ColorAndOpacity_Lambda([Node]() { return (Node.IsValid() && (Node->GetNodeId() >= 0)) ?
							(FHoudiniEngine::Get().IsSessionSync() ? FStyleColors::AccentGreen : FStyleColors::White) : FStyleColors::AccentGray; })
					]
					.OnClicked_Lambda([Node]()
						{
							if (IsValid(Node->GetAsset()))
								GEditor->SyncBrowserToObjects(TArray<UObject*>{ Node->GetAsset() });
							
							return FReply::Handled();
						})
				]
			]
		]
	];

	if (Node->GetAvailableOpNames().Num() >= 1)
	{
		TSharedPtr<FString> SelectedOption;
		OutAvailableOptions.Empty();
		for (const FString& AvailableOpName : Node->GetAvailableOpNames())
		{
			TSharedPtr<FString> AvailableOption = MakeShared<FString>(AvailableOpName);
			OutAvailableOptions.Add(AvailableOption);
			if (AvailableOpName == Node->GetOpName())
				SelectedOption = AvailableOption;
		}
			

		HoudiniAssetBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&OutAvailableOptions)
			.InitiallySelectedItem(SelectedOption)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) { return SNew(STextBlock).Text(FText::FromString(*InItem)); })
			.OnSelectionChanged_Lambda([Node](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
				{
					if (!NewChoice->Equals(Node->GetOpName()))
						Node->SelectOpName(*NewChoice);
				})
			.ContentPadding(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Node->GetOpName()))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	}

	HoudiniAssetBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(4.0f, 2.0f, 2.0f, 2.0f)
		[
			FHoudiniEngineEditorUtils::MakePresetWidget(Node.Get())
		];

	HoudiniAssetBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0f, 2.0f)
		[
			FHoudiniEngineEditorUtils::MakeCopyButton(Node.Get())
		];

	HoudiniAssetBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0f, 2.0f, 4.0f, 2.0f)
		[
			FHoudiniEngineEditorUtils::MakePasteButton(Node.Get())
		];

	FString CookFolderPath = Node->GetCookFolderPath();
	CookFolderPath.RemoveFromEnd(TEXT("/"));
	if (IAssetRegistry::GetChecked().PathExists(CookFolderPath))  // Check whether the cook folder exists, if not, then we need NOT to have browse button
	{
		HoudiniAssetBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(0)
			.IsFocusable(false)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.BrowseContent"))
				.DesiredSizeOverride(FVector2D(14.0, 14.0))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			.OnClicked_Lambda([CookFolderPath]()
				{
					FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					ContentBrowserModule.Get().SyncBrowserToFolders(TArray<FString>{ CookFolderPath });

					return FReply::Handled();
				})
			.ToolTipText(LOCTEXT("BrowseCookFolderInContentBrowser", "Browse Cook Folder in Content Browser"))
		];
	}

	HoudiniAssetBox->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(4.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			//.OnClicked(this, &SPropertyEditorButton::OnClick)
			.ContentPadding(0)
			.IsFocusable(false)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("MainFrame.VisitSearchForAnswersPage"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			.OnClicked_Lambda([Node]()
				{
					FSlateApplication::Get().AddWindow(FHoudiniNodeDetails::CreateNodeInfoWindow(Node));

					return FReply::Handled();
				})
		];

	CategoryBuilder.AddCustomRow(LOCTEXT("HoudiniNodeCookSettings", "CookOnParameterChangedCookOnUpstreamChanged"))
	.IsEnabled(TAttribute<bool>::CreateLambda([](){ return FHoudiniEngine::Get().AllowEdit();}))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.IsChecked(Node->CookOnParameterChanged() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Node](ECheckBoxState NewState) { Node->SetCookOnParameterChanged(NewState == ECheckBoxState::Checked); })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CookOnParameterChanged", "Cook On Parameter Changed"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			.IsChecked(Node->CookOnUpstreamChanged() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([Node](ECheckBoxState NewState) { Node->SetCookOnUpstreamChanged(NewState == ECheckBoxState::Checked); })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CookOnUpstreamChanged", "Cook On Upstream Changed"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		/*
		+ SHorizontalBox::Slot()
		[
			SNew(SCheckBox)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Verbose", "Verbose"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		]
		*/
	];

	// Node Actions
	CategoryBuilder.AddCustomRow(LOCTEXT("HoudiniNodeCommands", "CookRebuildBake"))
	.IsEnabled(TAttribute<bool>::CreateLambda([](){ return FHoudiniEngine::Get().AllowEdit();}))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNEW_HOUDINI_NODE_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Cook"), LOCTEXT("Cook", "Cook"))
			.OnClicked_Lambda([Node](){ if (Node.IsValid()) Node->ForceCook(); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot()
		[
			SNEW_HOUDINI_NODE_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"), LOCTEXT("Rebuild", "Rebuild"))
			.OnClicked_Lambda([Node](){ if (Node.IsValid()) Node->RequestRebuild(); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot()
		[
			SNEW_HOUDINI_NODE_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Bake"), LOCTEXT("Bake", "Bake"))
			.OnClicked_Lambda([Node](){ if (Node.IsValid()) Node->Bake(); UE_LOG(LogHoudiniEngine, Warning, TEXT("\"Baking\" is deprecated, please use i@unreal_split_actors = 1 Instead")); return FReply::Handled(); })
			.ToolTipText(LOCTEXT("HoudiniNodeBakeHelp", "(Deprecated) Please Use i@unreal_split_actors = 1 Instead"))
		]
	];
}

TSharedRef<SWindow> FHoudiniNodeDetails::CreateNodeInfoWindow(const TWeakObjectPtr<AHoudiniNode>& Node)
{
	FString DisplayStr = TEXT("\n\t");

	if (!Node->GetHelpText().IsEmpty())
		DisplayStr += Node->GetHelpText() + TEXT("\n\n\t");

	DisplayStr += FString::Printf(TEXT("Parameter Count: %d\n\n\t"), Node->GetParameters().Num());

	// TODO: Display outputs
	
	return SNew(SWindow)
		.Title(LOCTEXT("HoudiniNodeInfo", "Houdini Node Info"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.Content()
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayStr))
		];
}

#undef LOCTEXT_NAMESPACE
