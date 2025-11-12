// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniPDGDetails.h"

#include "Widgets/Input/SButton.h"

#include "DetailWidgetRow.h"
#include "Styling/StyleColors.h"

#include "HoudiniEngine.h"
#include "HoudiniNode.h"

#include "HoudiniEngineStyle.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define SNEW_HOUDINI_PDG_ACTION_BUTTON(ICON_BRUSH, BUTTON_TEXT) SNew(SButton) \
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
			.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))\
		] \
	]

#define HOUDINI_PDG_STATE_WIDGETS(ICON_NAME, STATE, STATE_COLOR) + SHorizontalBox::Slot()\
	.Padding(4.0f, 0.0f, 0.0f, 0.0f)\
	.AutoWidth()\
	[\
		SNew(SImage)\
		.Image(FHoudiniEngineStyle::Get()->GetBrush(ICON_NAME))\
		.DesiredSizeOverride(FVector2D(20.0, 20.0))\
	]\
	+ SHorizontalBox::Slot()\
	.Padding(4.0f, 2.0f)\
	.AutoWidth()\
	.VAlign(VAlign_Center)\
	[\
		SNew(STextBlock)\
		.Text_Lambda([Node, TopNodeIdx]\
			{\
				if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))\
				{\
					const FHoudiniTopNode& TopNode = Node->GetTopNodes()[TopNodeIdx];\
					if (TopNode.NodeId >= 0)\
						return FText::Format(LOCTEXT("Num" #STATE "WorkItems", "{0}"), TopNode.Num##STATE##WorkItems);\
				}\
				return LOCTEXT("NoHoudiniPDGWorkItems", "0");\
			})\
		.ToolTipText(LOCTEXT("HoudiniPDP" #STATE, "Num " #STATE " Work Items"))\
		.ColorAndOpacity_Lambda([Node, TopNodeIdx]\
				{\
					if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))\
					{\
						const FHoudiniTopNode& TopNode = Node->GetTopNodes()[TopNodeIdx];\
						if (TopNode.NodeId >= 0 && TopNode.Num##STATE##WorkItems >= 1)\
							return STATE_COLOR;\
					}\
					return FStyleColors::AccentGray;\
				})\
	]
	

void FHoudiniPDGDetails::CreateWidgets(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<AHoudiniNode>& Node)
{
	for (int32 TopNodeIdx = 0; TopNodeIdx < Node->GetTopNodes().Num(); ++TopNodeIdx)
	{
		const FHoudiniTopNode& TopNode = Node->GetTopNodes()[TopNodeIdx];
		
		const bool bIsOutputTopNode = TopNode.IsOutput();

		const FText TopNodePathText = FText::FromString(TopNode.Path);
		FDetailWidgetRow& Row = CategoryBuilder.AddCustomRow(TopNodePathText);
		Row.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FHoudiniEngineStyle::Get()->GetBrush(bIsOutputTopNode ? "HoudiniEngine.Output" : "HoudiniPDG.Link"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(TopNodePathText)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			]
		];

		TSharedPtr<SHorizontalBox> TopNodeActionsBox;

		Row.ValueContent()
		.MinDesiredWidth(500.0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TopNodeActionsBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNEW_HOUDINI_PDG_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Cook"), LOCTEXT("Cook", "Cook"))
					.OnClicked_Lambda([Node, TopNodeIdx]()
						{
							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
							{
								Node->GetTopNodes()[TopNodeIdx].Task = FHoudiniTopNode::EPDGTaskType::PendingCook;
								Node->ForceCook();
							}
							
							return FReply::Handled();
						})
					.IsEnabled_Lambda([] { return FHoudiniEngine::Get().AllowEdit(); })
				]

				+ SHorizontalBox::Slot()
				[
					SNEW_HOUDINI_PDG_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniPDG.Pause"), LOCTEXT("Pause", "Pause"))
					.OnClicked_Lambda([Node, TopNodeIdx]()
						{
							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
								Node->GetTopNodes()[TopNodeIdx].Task = FHoudiniTopNode::EPDGTaskType::Pause;

							return FReply::Handled();
						})
					.IsEnabled_Lambda([Node, TopNodeIdx]
						{
							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
							{
								const FHoudiniTopNode& TopNode = Node->GetTopNodes()[TopNodeIdx];
								if (TopNode.NodeId >= 0 && TopNode.Task == FHoudiniTopNode::EPDGTaskType::Cooking)
									return true;
							}

							return false;
						})
				]
					
				+ SHorizontalBox::Slot()
				[
					SNEW_HOUDINI_PDG_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniPDG.Cancel"), LOCTEXT("Cancel", "Cancel"))
					.OnClicked_Lambda([Node, TopNodeIdx]()
						{
							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
								Node->GetTopNodes()[TopNodeIdx].Task = FHoudiniTopNode::EPDGTaskType::Cancel;

							return FReply::Handled();
						})
					.IsEnabled_Lambda([Node, TopNodeIdx]
						{
							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
							{
								const FHoudiniTopNode& TopNode = Node->GetTopNodes()[TopNodeIdx];
								if (TopNode.NodeId >= 0 && TopNode.Task == FHoudiniTopNode::EPDGTaskType::Cooking)
									return true;
							}

							return false;
						})
				]

				+ SHorizontalBox::Slot()
				[
					SNEW_HOUDINI_PDG_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniPDG.DirtyNode"), LOCTEXT("Dirty", "Dirty"))
					.OnClicked_Lambda([Node, TopNodeIdx]()
						{
							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
								HOUDINI_FAIL_INVALIDATE(Node->GetTopNodes()[TopNodeIdx].HapiDirty());

							return FReply::Handled();
						})
					.IsEnabled_Lambda([Node, TopNodeIdx]
						{
							if (!FHoudiniEngine::Get().AllowEdit())
								return false;

							if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx) && Node->GetTopNodes()[TopNodeIdx].NodeId >= 0)
								return true;

							return false;
						})
				]
			]
			
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(4.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				HOUDINI_PDG_STATE_WIDGETS("HoudiniPDG.Error", Error, FStyleColors::Error)
				HOUDINI_PDG_STATE_WIDGETS("HoudiniPDG.Done", Completed, FStyleColors::Success)
				HOUDINI_PDG_STATE_WIDGETS("HoudiniPDG.Cooking", Running, FStyleColors::AccentGreen)
				HOUDINI_PDG_STATE_WIDGETS("HoudiniPDG.Waiting", Waiting, FStyleColors::AccentGray)
			]
		];

		if (bIsOutputTopNode)
		{
			TopNodeActionsBox->AddSlot()
			[
				SNEW_HOUDINI_PDG_ACTION_BUTTON(FHoudiniEngineStyle::Get()->GetBrush("HoudiniPDG.Reset"), LOCTEXT("Clear", "Clear"))
				.OnClicked_Lambda([Node, TopNodeIdx]()
					{
						if (!Node.IsValid())
							return FReply::Handled();

						if (!Node->GetTopNodes().IsValidIndex(TopNodeIdx))
							return FReply::Handled();

						HOUDINI_FAIL_INVALIDATE(Node->GetTopNodes()[TopNodeIdx].HapiDirtyAll());
						return FReply::Handled();
					})
				.IsEnabled_Lambda([Node, TopNodeIdx]
					{
						if (!FHoudiniEngine::Get().AllowEdit())
							return false;

						if (Node.IsValid() && Node->GetTopNodes().IsValidIndex(TopNodeIdx))
							return true;

						return false;
					})
			];
		}
	}
}

#undef LOCTEXT_NAMESPACE
