// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAttributeParameterDetails.h"

#include "Editor.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#include "HoudiniEngine.h"
#include "HoudiniCurvesComponent.h"

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniAttributeParameterHolder.h"
#include "HoudiniParameterDetails.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniTools.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

template<class FOnClickLambda>
FORCEINLINE static TSharedRef<SButton> CreateHoudiniCurvesEditButton(const FSlateBrush* Brush, FOnClickLambda OnClick, const FText& ToolTip, const bool bEnable)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsFocusable(false)
		.ToolTipText(ToolTip)
		.Content()
		[
			SNew(SImage)
			.Image(Brush)
			.DesiredSizeOverride(FVector2D(16.0, 16.0))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		.OnClicked_Lambda(OnClick)
		.IsEnabled_Lambda([bEnable] { return bEnable && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniAttributeParameterDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	UHoudiniAttributeParameterHolder* AttribParmHolder = Cast<UHoudiniAttributeParameterHolder>(ObjectsCustomized[0]);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
	AttribParmHolder->DetailsView = DetailBuilder.GetDetailsViewSharedPtr();
#else
	AttribParmHolder->DetailsView = StaticCastWeakPtr<IDetailsView>(DetailBuilder.GetDetailsView()->AsWeak());
#endif
	const TWeakObjectPtr<UHoudiniEditableGeometry>& EditGeo = AttribParmHolder->GetEditableGeometry();
	if (!EditGeo.IsValid() || !EditGeo->IsGeometrySelected())
		return;
	
	IDetailCategoryBuilder& HoudiniParameterBuilder = DetailBuilder.EditCategory(
		UHoudiniAttributeParameterHolder::DetailsName,
		FText::FromString(AttribParmHolder->GetDisplayName()), ECategoryPriority::Important);

	// Build AttribParm widgets
	AttribParmHolder->SetDisplayHeight(
		FHoudiniParameterDetails::Parse(HoudiniParameterBuilder, AttribParmHolder->GetParameters(), true) +
		HOUDINI_STANDARD_PARM_ROW_HEIGHT * 2);  // Category and preset row
	
	// Build preset widgets
	FDetailWidgetRow& Row = HoudiniParameterBuilder.AddCustomRow(FText::GetEmpty());
	TSharedPtr<SHorizontalBox> AttribShortcutBox;
	Row.NameContent()
	[
		SAssignNew(AttribShortcutBox, SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(2.0f)
		[
			FHoudiniEngineEditorUtils::MakePresetWidget(EditGeo.Get())
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(0.0f, 2.0f)
		[
			FHoudiniEngineEditorUtils::MakeCopyButton(EditGeo.Get())
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(0.0f, 2.0f)
		[
			FHoudiniEngineEditorUtils::MakePasteButton(EditGeo.Get())
		]
	];
	
	// If this is right click panel, then we should add a shortcut that could show attributes on editor mode panel
	if (AttribParmHolder->DetailsView.Pin()->GetIdentifier() == UHoudiniAttributeParameterHolder::DetailsName)
	{
		AttribShortcutBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.Content()
			[
				SNew(SImage)
				.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.AttribEdit"))
				.DesiredSizeOverride(FVector2D(16.0, 16.0))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			.OnClicked_Lambda([]()
				{
					FSlateApplication::Get().DismissMenuByWidget(UHoudiniAttributeParameterHolder::Get()->DetailsView.Pin().ToSharedRef());
					UHoudiniEditTool::ForceActivate();
					return FReply::Handled();
				})
			.ToolTipText(LOCTEXT("HoudiniAttribEdit", "Edit Attributes On Editor Mode"))
		];
	}

	// Build curves intrinsic attribs
	const TWeakObjectPtr<UHoudiniCurvesComponent> CurvesComponent = AttribParmHolder->GetCurvesComponent();
	if (!CurvesComponent.IsValid())
		return;

	static const TArray<TSharedPtr<FString>> CurveTypeOptions = TArray<TSharedPtr<FString>>{
		MakeShared<FString>(TEXT("Points")), MakeShared<FString>(TEXT("Polygon")), MakeShared<FString>(TEXT("Subdiv")),
		MakeShared<FString>(TEXT("Bezier")), MakeShared<FString>(TEXT("Interpolate")) };

	TSharedPtr<SHorizontalBox> CurveSettingsBox;
	Row.ValueContent()
	.MinDesiredWidth(220.0f)
	[
		SAssignNew(CurveSettingsBox, SHorizontalBox)
	];

	if (CurvesComponent->IsGeometrySelected() && ((CurvesComponent->GetSelectedClass() != EHoudiniAttributeOwner::Point) ||
		(CurvesComponent->GetSelectedCurveType() == EHoudiniCurveType::Points)))  // Curve settings won't display on curve points, unless point cloud
	{
		CurveSettingsBox->AddSlot()
		.HAlign(HAlign_Left)
		.AutoWidth()
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([CurvesComponent]
				{
					return (!CurvesComponent.IsValid() || !CurvesComponent->IsGeometrySelected()) ? ECheckBoxState::Undetermined :
						(CurvesComponent->GetSelectedCurveClosed() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
				})
			.OnCheckStateChanged_Lambda([CurvesComponent](ECheckBoxState NewState)
				{
					const bool bNewCurveClosed = NewState == ECheckBoxState::Checked;
					if (bNewCurveClosed != CurvesComponent->GetSelectedCurveClosed())
					{
						CurvesComponent->SetSelectedCurvesClosed(bNewCurveClosed);  // Transcation has been made in this method

						GEditor->RedrawLevelEditingViewports(true);  // Let viewport refresh to display the change
					}
				})
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Closed", "Closed"))
				.ToolTipText(LOCTEXT("CurveClosedHelp", "Toggle to close/open curves"))
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
			]
		];

		CurveSettingsBox->AddSlot()
		.AutoWidth()
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&CurveTypeOptions)
			.InitiallySelectedItem(CurveTypeOptions[int32(CurvesComponent->GetSelectedCurveType()) + 1])
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
				{
					return SNew(STextBlock).Text(FText::FromString(*InItem))
						.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"));
				})
			.OnSelectionChanged_Lambda([CurvesComponent](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
				{
					const EHoudiniCurveType NewCurveType = (EHoudiniCurveType)(CurveTypeOptions.IndexOfByKey(NewChoice) - 1);
					if (NewCurveType != CurvesComponent->GetSelectedCurveType())
					{
						CurvesComponent->SetSelectedCurvesType(NewCurveType);  // Transcation has been made in this method

						GEditor->RedrawLevelEditingViewports(true);  // Let viewport refresh to display the change
					}
				})
			[
				SNew(STextBlock)
				.Text_Lambda([CurvesComponent]()
					{
						return (CurvesComponent.IsValid() && CurvesComponent->IsGeometrySelected()) ?
							FText::FromString(*CurveTypeOptions[int32(CurvesComponent->GetSelectedCurveType()) + 1]) : FText::GetEmpty();
					})
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	}

	const auto SplitSeletionLamda = [CurvesComponent]
		{
			if (CurvesComponent.IsValid() && CurvesComponent->IsGeometrySelected())
			{
				if (CurvesComponent->SplitSelection())
					GEditor->RedrawLevelEditingViewports(true);  // Let viewport refresh to display the change
			}
			
			return FReply::Handled();
		};

	const auto JoinSeletionLamda = [CurvesComponent]
		{
			if (CurvesComponent.IsValid() && CurvesComponent->IsGeometrySelected())
			{
				if (CurvesComponent->JoinSelection())
					GEditor->RedrawLevelEditingViewports(true);  // Let viewport refresh to display the change
			}
			
			return FReply::Handled();
		};

	if (CurvesComponent->GetSelectedClass() == EHoudiniAttributeOwner::Point)
	{
		CurveSettingsBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			CreateHoudiniCurvesEditButton(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.SplitPoints"), SplitSeletionLamda,
				LOCTEXT("SplitHoudiniCurvePoints", "Split Curves"), CurvesComponent->IsSelectionSplittable())
		];

		CurveSettingsBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			CreateHoudiniCurvesEditButton(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.FusePoints"), JoinSeletionLamda,
				LOCTEXT("FuseHoudiniCurvePoints", "Fuse Points"), CurvesComponent->IsSelectionJoinable())
		];
	}
	else if (CurvesComponent->GetSelectedClass() == EHoudiniAttributeOwner::Prim)
	{
		CurveSettingsBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			CreateHoudiniCurvesEditButton(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.DetachPrims"), SplitSeletionLamda,
				LOCTEXT("DetachHoudiniCurvePrims", "Detach Primitives"), CurvesComponent->IsSelectionSplittable())
		];

		CurveSettingsBox->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			CreateHoudiniCurvesEditButton(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.JoinPrims"), JoinSeletionLamda,
				LOCTEXT("JoinHoudiniCurvePrims", "Join Primitives"), CurvesComponent->IsSelectionJoinable())
		];
	}
}

TSharedRef<IDetailCustomization> FHoudiniAttributeParameterDetails::MakeInstance()
{
	return MakeShareable(new FHoudiniAttributeParameterDetails);
}

#undef LOCTEXT_NAMESPACE
