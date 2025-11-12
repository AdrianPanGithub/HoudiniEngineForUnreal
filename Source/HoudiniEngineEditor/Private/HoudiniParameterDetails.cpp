// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniParameterDetails.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Colors/SColorPicker.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Editor.h"
#include "AssetThumbnail.h"
#include "ScopedTransaction.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PropertyCustomizationHelpers.h"
#include "SAssetDropTarget.h"
#include "EditorDirectories.h"
#include "SCurveEditor.h"
#include "CurveEditorSettings.h"

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniParameters.h"
#include "HoudiniEngineCommon.h"

#include "HoudiniInputDetails.h"
#include "HoudiniEngineStyle.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

float FHoudiniParameterDetails::Parse(IDetailCategoryBuilder& CategoryBuilder, const TArray<UHoudiniParameter*>& Parms, const bool& bIsAttribPanel)
{
	// We should Reset Spin infos first
	SpinningParm.Reset();
	SpinningIdx = -1;

	FHoudiniParameterDetails ParmDetails;
	ParmDetails.bIsAttribPanel = bIsAttribPanel;

	if (Parms.IsValidIndex(0))  // Ensure parms that has no parent to display, especially when parsing attrib-parms, all attrib-parms has parent
		ParmDetails.VisibleParentIds.Add(Parms[0]->GetParentId());

	for (UHoudiniParameter* Parm : Parms)
		ParmDetails.Parse(CategoryBuilder, Parm);

	ParmDetails.EndAllFolders(CategoryBuilder);

	return ParmDetails.ParmPanelHeight;
}

#define INDENTATION_HORIZONTAL_GAP 20.0f
#define INDENTATION_HORIZONTAL_GAP_MULTIPARM 44.0f
#define INDENTATION_VERTICAL_GAP 2.0f

#define PARM_TOOLTIP_TEXT(Parm) FText::FromString(!Parm->GetHelp().IsEmpty() ? Parm->GetHelp() : Parm->GetParameterName())

#define FOLDER_LINE_SETTINGS ESlateDrawEffect::None, FLinearColor(0.25f, 0.25f, 0.25f)

class SHoudiniDrawerBox : public SHorizontalBox
{
protected:
	enum class EHoudiniLineDrawerType
	{
		NameWidget = 0,
		Separator,
		Tabs,
		FolderEnd
	};

	EHoudiniLineDrawerType DrawerType = EHoudiniLineDrawerType::NameWidget;

	float Indentation = 0.0f;

	int32 EndIndentationIdx = 0;

	TArray<float> IndentationPositions;

public:
	void AsNameWidget(const TArray<float>& InIndentationPositions)
	{ 
		DrawerType = EHoudiniLineDrawerType::NameWidget;
		IndentationPositions = InIndentationPositions;
	}

	void AsSeparator(const TArray<float>& InIndentationPositions, const float& InIndentation)
	{
		DrawerType = EHoudiniLineDrawerType::Separator;
		IndentationPositions = InIndentationPositions;
		Indentation = InIndentation;
	}

	void AsFolderEnd(const TArray<float>& InIndentationPositions, const int32& StartIndentationIdx)
	{
		DrawerType = EHoudiniLineDrawerType::FolderEnd;
		IndentationPositions = InIndentationPositions;
		EndIndentationIdx = IndentationPositions.Num() - StartIndentationIdx;
	}

	void AsTabs(const TArray<float>& InIndentationPositions)
	{ 
		DrawerType = EHoudiniLineDrawerType::Tabs;
		IndentationPositions = InIndentationPositions;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		SHorizontalBox::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

		if (DrawerType == EHoudiniLineDrawerType::FolderEnd)
		{
			for (int32 LineIdx = 0; LineIdx < IndentationPositions.Num(); ++LineIdx)
			{
				const float& LinePos = IndentationPositions[LineIdx];
				if (LineIdx < EndIndentationIdx)
				{
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
						TArray<FVector2f>{
							FVector2f(LinePos, -200.0f),
							FVector2f(LinePos, 400.0f) },
						FOLDER_LINE_SETTINGS);
				}
				else
				{
					const float PosX = (IndentationPositions.Num() - LineIdx) * 2.0f - 1.0f;
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
						TArray<FVector2f>{
							FVector2f(LinePos, -200.0f),
							FVector2f(LinePos, PosX),
							FVector2f(AllottedGeometry.Size.X, PosX) },
						FOLDER_LINE_SETTINGS);
				}
			}
		}
		else
		{
			// Draw folder line
			for (int32 LineIdx = 0; LineIdx < IndentationPositions.Num() - (DrawerType == EHoudiniLineDrawerType::Tabs); ++LineIdx)
			{
				const float& LinePos = IndentationPositions[LineIdx];
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
					TArray<FVector2f>{
						FVector2f(LinePos, -200.0f),
						FVector2f(LinePos, 400.0f) },
					FOLDER_LINE_SETTINGS);
			}

			if (DrawerType == EHoudiniLineDrawerType::Separator)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
					TArray<FVector2f> {
						FVector2f(Indentation, AllottedGeometry.Size.Y * 0.5f),
						FVector2f(AllottedGeometry.Size.X, AllottedGeometry.Size.Y * 0.5f) });
			}
			else if (DrawerType == EHoudiniLineDrawerType::Tabs)
			{
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
					TArray<FVector2f>{
						FVector2f(IndentationPositions.Last() - 1.0f, AllottedGeometry.Size.Y),
						FVector2f(AllottedGeometry.Size.X, AllottedGeometry.Size.Y) },
					FOLDER_LINE_SETTINGS);
			}
		}

		return LayerId;
	}
};

template<typename NumericType>
struct THoudiniParameterUnitTypeInterface : TDefaultNumericTypeInterface<NumericType>  // If cannot recognize unit then just display it directly
{
	FString UnitStr;

	THoudiniParameterUnitTypeInterface(const FString& InUniStr) : UnitStr(InUniStr)
	{
		UnitStr.ReplaceInline(TEXT("-2"), TEXT("\u00B2"));
		UnitStr.ReplaceInline(TEXT("2"), TEXT("\u00B2"));

		UnitStr.ReplaceInline(TEXT("-3"), TEXT("\u00B3"));
		UnitStr.ReplaceInline(TEXT("3"), TEXT("\u00B3"));
	}

	virtual FString ToString(const NumericType& Value) const override
	{
		return TDefaultNumericTypeInterface<NumericType>::ToString(Value) + TEXT(" ") + UnitStr;
	}

	virtual TOptional<NumericType> FromString(const FString& ValueString, const NumericType& InExistingValue) override
	{
		FString ValueStr = ValueString;
		ValueStr.RemoveFromEnd(TEXT(" ") + UnitStr);
		return TDefaultNumericTypeInterface<NumericType>::FromString(ValueStr, InExistingValue);
	}

	virtual bool IsCharacterValid(TCHAR InChar) const override
	{
		return InChar != TEXT('\t');  // TDefaultNumericTypeInterface<NumericType>::IsCharacterValid(InChar) || UnitStr.GetCharArray().Contains(InChar);
	}
};

template<typename NumericType>
TSharedPtr<INumericTypeInterface<NumericType>> ConvertHoudiniUnitToTypeInterface(const FString& Unit)
{
	if (Unit.IsEmpty())
		return nullptr;

	if (FUnitConversion::Settings().ShouldDisplayUnits())
	{
		auto ParmUnit = FUnitConversion::UnitFromString(*Unit);
		if (ParmUnit.IsSet())
			return MakeShared<TNumericUnitTypeInterface<NumericType>>(ParmUnit.GetValue());
		else  // If cannot recognize unit then just display it directly
			return MakeShared<THoudiniParameterUnitTypeInterface<NumericType>>(Unit);
	}

	return MakeShared<TNumericUnitTypeInterface<NumericType>>(EUnit::Unspecified);
}

// Spinning
TWeakObjectPtr<const UHoudiniParameter> FHoudiniParameterDetails::SpinningParm;
int32 FHoudiniParameterDetails::SpinningIdx = -1;
int32 FHoudiniParameterDetails::SpinningIntValue = 0;
float FHoudiniParameterDetails::SpinningFloatValue = 0.0f;


// -------- Common ---------
void FHoudiniParameterDetails::Parse(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm)
{
	if (!PreParse(Parm))
		return;

	ParseIndentations(CategoryBuilder, Parm->GetParentId());

	switch (Parm->GetType())
	{
	case EHoudiniParameterType::Separator: { ParseSeparator(CategoryBuilder); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::Folder: ParseFolder(CategoryBuilder); break;
	case EHoudiniParameterType::Button: { ParseButton(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;

		// Int types
	case EHoudiniParameterType::Int: { ParseInt(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::IntChoice: { ParseIntChoice(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::ButtonStrip: { ParseButtonStrip(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::Toggle: { ParseToggle(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::FolderList: { ParseFolderList(Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;

		// Float types
	case EHoudiniParameterType::Float: { ParseFloat(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::Color: { ParseColor(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;

		// String types
	case EHoudiniParameterType::String: { ParseString(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::StringChoice: { ParseStringChoice(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::Asset: { ParseAsset(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_ASSETREF_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::AssetChoice: { ParseAssetChoice(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_ASSETREF_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::Input:
		FHoudiniInputDetails::CreateWidget(CategoryBuilder,
			CreateNameWidget(CategoryBuilder, Parm).ValueContent(), Cast<UHoudiniParameterInput>(Parm)->GetInput());
		break;

	case EHoudiniParameterType::Label: ParseLabel(CategoryBuilder, Parm); break;

		// Extras
	case EHoudiniParameterType::MultiParm:
	{
		ParseMultiParm(CategoryBuilder, Parm);
		ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT;
		if (bIsAttribPanel)
		{
			PrevParm = Parm;  // Update PrevParm here
			for (UHoudiniParameter* ChildAttribParm : Cast<UHoudiniMultiParameter>(Parm)->ChildAttribParmInsts)
				Parse(CategoryBuilder, ChildAttribParm);
			return;  // We have updated PrevParm, so just quit
		}
	}
	break;
	case EHoudiniParameterType::FloatRamp: { ParseFloatRamp(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_RAMP_PARM_ROW_HEIGHT; } break;
	case EHoudiniParameterType::ColorRamp: { ParseColorRamp(CategoryBuilder, Parm); ParmPanelHeight += HOUDINI_RAMP_PARM_ROW_HEIGHT; } break;
	}

	PrevParm = Parm;
}

bool FHoudiniParameterDetails::PreParse(const TWeakObjectPtr<const UHoudiniParameter>& CurrParm)
{
	if (CurrParm->IsParent())
	{
		IdParmParentMap.FindOrAdd(CurrParm->GetId()) = CurrParm;

		if (CurrParm->GetType() == EHoudiniParameterType::Folder)
		{
			if (CurrFolderList.IsValid())  // Means the parent folder-list is visible
			{
				CurrFolders.Add(Cast<const UHoudiniParameterFolder>(CurrParm));
				if (CurrFolderList->GetSize() == CurrFolders.Num())  // This is the last folder
				{
					for (const TWeakObjectPtr<const UHoudiniParameterFolder>& Folder : CurrFolders)
					{
						// If there's at least one forder is visibile, we should parse it
						if (Folder->IsVisible())
							return true;
					}
				}
			}

			return false;
		}
	}

	if (!CurrParm->IsVisible())
		return false;

	if (CurrParm->GetParentId() >= 0 && !VisibleParentIds.Contains(CurrParm->GetParentId()))
		return false;

	return true;
}

void FHoudiniParameterDetails::ParseIndentations(IDetailCategoryBuilder& CategoryBuilder, int32 CurrParentId)
{
	if (!PrevParm.IsValid())  // Means this is the first parm to parse
		return;

	if (PrevParm->GetParentId() == CurrParentId)  // Means the hierarchy is identical as prev-pared-parm, so we just reuse the previous one
		return;

	CurrIndentation = 0.0f;
	TArray<int32> CurrParentIdStack;
	CurrIndentationPositions.Empty();

	if ((PrevParm->GetParentId() < 0) && (CurrParentId < 0))
		return;

	// -------- Recursive to the root-parm-id(-1), get all parent ids, and indentations --------
	TArray<float> ReversedIndentationPositions;
	for (int32 Iter = IdParmParentMap.Num() * 2 + 1; Iter >= 0; --Iter)
	{
		if (TWeakObjectPtr<const UHoudiniParameter>* ParentParmPtr = IdParmParentMap.Find(CurrParentId))
		{
			TWeakObjectPtr<const UHoudiniParameter> ParentParm = *ParentParmPtr;
			if (ParentParm->GetType() != EHoudiniParameterType::Folder)  // A folder and a folder-list represent the same group 
			{
				// Only tab-likes group need vertical-indentation-line
				if (const UHoudiniParameterFolderList* ParentFolderList = Cast<const UHoudiniParameterFolderList>(ParentParm))
				{
					if (ParentFolderList->IsTab())
						ReversedIndentationPositions.Add(CurrIndentation);
				}

				CurrIndentation += ParentParm->GetType() != EHoudiniParameterType::MultiParm ?
					INDENTATION_HORIZONTAL_GAP : INDENTATION_HORIZONTAL_GAP_MULTIPARM;
				CurrParentIdStack.Add(CurrParentId);
			}
			CurrParentId = ParentParm->GetParentId();
		}
		else
			break;
	}
	
	for (int32 Idx = ReversedIndentationPositions.Num() - 1; Idx >= 0; --Idx)
		CurrIndentationPositions.Add(CurrIndentation - INDENTATION_HORIZONTAL_GAP - ReversedIndentationPositions[Idx]);


	// -------- If need to end folder(s) --------
	for (int32 Idx = PrevParentIdStack.Num() - 1; Idx >= 0; --Idx)
	{
		if (!CurrParentIdStack.Contains(PrevParentIdStack[Idx]))  // Means some parm-parent(s) need to have an end.
		{
			int32 NumTabsToEnd = 0;
			for (; Idx >= 0; --Idx)  // Find all tabs that need to end
			{
				TWeakObjectPtr<const UHoudiniParameter> ParmParent = IdParmParentMap[PrevParentIdStack[Idx]];
				if (ParmParent->GetType() == EHoudiniParameterType::FolderList && Cast<const UHoudiniParameterFolderList>(ParmParent)->IsTab())
					++NumTabsToEnd;
			}

			if (NumTabsToEnd)
			{
				TSharedPtr<SHoudiniDrawerBox> HoudiniBox = SNew(SHoudiniDrawerBox);
				CategoryBuilder.AddCustomRow(FText::GetEmpty()).WholeRowWidget.Widget = HoudiniBox.ToSharedRef();
				HoudiniBox->AsFolderEnd(PrevIndentationPositions, NumTabsToEnd);
				ParmPanelHeight += HOUDINI_STANDARD_PARM_ROW_HEIGHT;
			}

			break;
		}
	}

	PrevParentIdStack = CurrParentIdStack;
	PrevIndentationPositions = CurrIndentationPositions;
}

void FHoudiniParameterDetails::CreateMultiParmInstanceButtons(IDetailCategoryBuilder& CategoryBuilder, TSharedPtr<SHorizontalBox> HorizontalBox,
	const TWeakObjectPtr<UHoudiniMultiParameter>& CurrMultiParm, const int32& MultiParmInstIdx) const
{
	const bool bIsAttrib_ = bIsAttribPanel;
	
	HorizontalBox->AddSlot()
	.Padding(CurrIndentation - (INDENTATION_HORIZONTAL_GAP_MULTIPARM + 2.0f), 0.0f, 0.0f, 0.0f)
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsFocusable(false)
		.ContentPadding(-1.0f)
		.Content()
		[
			SNew(SImage)
			.DesiredSizeOverride(FVector2D(18.0, 18.0))
			.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.InsertBefore"))
		]
		.OnClicked_Lambda([&CategoryBuilder, CurrMultiParm, MultiParmInstIdx, bIsAttrib_]
			{
				CurrMultiParm->InsertInstance(MultiParmInstIdx);
				if (!CurrMultiParm->GetChildAttributeParameters().IsEmpty())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
				
				return FReply::Handled();
			})
		.ToolTipText(LOCTEXT("HoudiniMultiParmInsertBefore", "Insert A Multi Parm Instance Before This"))
	];

	HorizontalBox->AddSlot()
	.Padding(0.0, 0.0f, 4.0f, 0.0f)
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.IsFocusable(false)
		.ContentPadding(-1.0f)
		.Content()
		[
			SNew(SImage)
			.DesiredSizeOverride(FVector2D(18.0, 18.0))
			.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Remove"))
		]
		.OnClicked_Lambda([&CategoryBuilder, CurrMultiParm, MultiParmInstIdx, bIsAttrib_]
			{
				CurrMultiParm->RemoveInstance(MultiParmInstIdx);
				if (!CurrMultiParm->GetChildAttributeParameters().IsEmpty())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();

				return FReply::Handled();
			})
		.ToolTipText(LOCTEXT("HoudiniMultiParmRemove", "Remove This Multi Parm Instance"))
	];
}

FDetailWidgetRow& FHoudiniParameterDetails::CreateNameWidget(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& CurrParm) const
{
	FText Label = FText::FromString(CurrParm->GetLabel());
	FDetailWidgetRow& Row = CategoryBuilder.AddCustomRow(Label);

	TSharedPtr<SHorizontalBox> HorizontalBox;
	if (CurrIndentationPositions.IsEmpty())
		HorizontalBox = SNew(SHorizontalBox);
	else
	{
		TSharedRef<SHoudiniDrawerBox> HoudiniBox = SNew(SHoudiniDrawerBox);
		HoudiniBox->AsNameWidget(CurrIndentationPositions);
		HorizontalBox = HoudiniBox;
	}
	
	float Indentation = CurrIndentation;
	if (UHoudiniMultiParameter* CurrMultiParm = NeedMultiParmButton(CurrParm))  // Add multi-parm insert/remove buttons
	{
		CreateMultiParmInstanceButtons(CategoryBuilder, HorizontalBox, CurrMultiParm, CurrParm->GetMultiParmInstanceIndex());
		Indentation = 0.0f;  // CreateMultiParmInstanceButtons already have Indentation
	}

	// Add parm-label
	HorizontalBox->AddSlot()
	.Padding(Indentation, 0.0f, 0.0f, 0.0f)
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Left)
	.AutoWidth()
	[
		SNew(STextBlock)
		.Text(Label)
		.ToolTipText(PARM_TOOLTIP_TEXT(CurrParm))
		.Font(FAppStyle::Get().GetFontStyle(CurrParm->IsInDefault() ? "PropertyWindow.NormalFont" : "PropertyWindow.BoldFont"))
	];
	Row.NameWidget.Widget = HorizontalBox.ToSharedRef();

	Row.IsEnabled(TAttribute<bool>::CreateLambda([]() { return FHoudiniEngine::Get().AllowEdit(); }));
	Row.CustomResetToDefault = FResetToDefaultOverride::Create(
		TAttribute<bool>::CreateLambda([CurrParm]() { return !CurrParm->IsInDefault(); }),
		FSimpleDelegate::CreateLambda([CurrParm]() { if (CurrParm->IsEnabled()) CurrParm->RevertToDefault(); }));  // Check whether this parm is disabled, if not, then revert

	return Row;
}

void FHoudiniParameterDetails::EndAllFolders(IDetailCategoryBuilder& CategoryBuilder)
{
	int32 NumTabsToEnd = 0;
	for (int32 Idx = PrevParentIdStack.Num() - 1; Idx >= 0; --Idx)
	{
		TWeakObjectPtr<const UHoudiniParameter> ParmParent = IdParmParentMap[PrevParentIdStack[Idx]];
		if (ParmParent->GetType() == EHoudiniParameterType::FolderList && Cast<const UHoudiniParameterFolderList>(ParmParent)->IsTab())
			++NumTabsToEnd;
	}

	if (NumTabsToEnd)
	{
		TSharedPtr<SHoudiniDrawerBox> HoudiniBox = SNew(SHoudiniDrawerBox);
		CategoryBuilder.AddCustomRow(FText::GetEmpty()).WholeRowWidget.Widget = HoudiniBox.ToSharedRef();
		HoudiniBox->AsFolderEnd(PrevIndentationPositions, NumTabsToEnd);
	}
}

UHoudiniMultiParameter* FHoudiniParameterDetails::NeedMultiParmButton(const TWeakObjectPtr<const UHoudiniParameter> CurrParm) const
{
	if (CurrParm->GetMultiParmInstanceIndex() < 0)
		return nullptr;

	const TWeakObjectPtr<const UHoudiniParameter>* ParmParentPtr = IdParmParentMap.Find(CurrParm->GetParentId());
	if (ParmParentPtr && (*ParmParentPtr)->GetType() == EHoudiniParameterType::MultiParm)  // is direct child of multi parm
	{
		const TPair<int32, int32> ParentIdPair((*ParmParentPtr)->GetId(), CurrParm->GetMultiParmInstanceIndex());
		if (!MultiParmIdIndexSet.Contains(ParentIdPair))
		{
			MultiParmIdIndexSet.Add(ParentIdPair);
			return (UHoudiniMultiParameter*)(ParmParentPtr->Get());
		}
	}

	return nullptr;
}

void FHoudiniParameterDetails::ParseSeparator(IDetailCategoryBuilder& CategoryBuilder) const
{
	TSharedRef<SHoudiniDrawerBox> HoudiniBox = SNew(SHoudiniDrawerBox);
	CategoryBuilder.AddCustomRow(FText::GetEmpty()).WholeRowWidget.Widget = HoudiniBox;
	HoudiniBox->AsSeparator(CurrIndentationPositions, CurrIndentation);
}

void FHoudiniParameterDetails::ParseFolder(IDetailCategoryBuilder& CategoryBuilder)
{
	class SHoudiniTabButton : public SButton
	{
	protected:
		bool bSelected = false;

		bool bIsRadio = false;

	public:
		void SetAppearance(const bool& bInSelected, const bool& bInIsRadio)
		{
			bSelected = bInSelected;
			bIsRadio = bInIsRadio;

			if (bIsRadio)
				SetContentPadding(FMargin(8.0f, 2.0f, 0.0f, 2.0f));
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			SButton::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

			if (bIsRadio)
			{
				static const int32 NumCircleDivision = 12;
				static const float CircleSize = 5.0f;
				static const float CircleDivision = PI * 2.0f / NumCircleDivision;

				TArray<FVector2f> CirclePoints;
				CirclePoints.SetNumUninitialized(NumCircleDivision + 1);

				for (int PtIdx = 0; PtIdx < NumCircleDivision; ++PtIdx)
					CirclePoints[PtIdx] = FVector2f(
						FMath::Sin(CircleDivision * PtIdx) * CircleSize + 10.0,
						FMath::Cos(CircleDivision * PtIdx) * CircleSize + AllottedGeometry.Size.Y * 0.5f);
				CirclePoints[NumCircleDivision] = CirclePoints[0];

				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CirclePoints);

				if (bSelected)
				{
					static const float SmallCircleSize = 1.0f;

					for (int PtIdx = 0; PtIdx < NumCircleDivision; ++PtIdx)
						CirclePoints[PtIdx] = FVector2f(
							FMath::Sin(CircleDivision * PtIdx) * SmallCircleSize + 10.0,
							FMath::Cos(CircleDivision * PtIdx) * SmallCircleSize + AllottedGeometry.Size.Y * 0.5f);
					CirclePoints[NumCircleDivision] = CirclePoints[0];

					FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CirclePoints,
						ESlateDrawEffect::None, FLinearColor::White, true, 4.0f);
				}
			}

			return LayerId;
		}
	};

	// Check is the last folder, and check should display,
	// has already been done in FHoudiniParameterDetails::PreParse, so we need not to check them again

	FString SearchStr;
	for (int32 FolderIdx = 0; FolderIdx < CurrFolders.Num(); ++FolderIdx)
	{
		const TWeakObjectPtr<const UHoudiniParameterFolder> Folder = CurrFolders[FolderIdx];
		if (Folder->IsVisible())
		{
			if (CurrFolderList->IsFolderExpanded(FolderIdx))
				VisibleParentIds.FindOrAdd(Folder->GetId());
			else if (bIsAttribPanel)
				VisibleParentIds.Remove(Folder->GetId());  // For folders under attrib multiparm
		}
		SearchStr += Folder->GetLabel() + '|';
	}

	TSharedPtr<SHorizontalBox> HorizontalBox;
	if (CurrIndentationPositions.IsEmpty())
		HorizontalBox = SNew(SHorizontalBox);
	else
	{
		TSharedPtr<SHoudiniDrawerBox> HoudiniBox = SNew(SHoudiniDrawerBox);
		if (CurrFolderList->IsTab())
			HoudiniBox->AsTabs(CurrIndentationPositions);
		else
			HoudiniBox->AsNameWidget(CurrIndentationPositions);
		HorizontalBox = HoudiniBox;
	}
	CategoryBuilder.AddCustomRow(FText::FromString(SearchStr)).WholeRowWidget.Widget = HorizontalBox.ToSharedRef();

	float Indentation = CurrIndentation;
	if (UHoudiniMultiParameter* CurrMultiParm = NeedMultiParmButton(CurrFolderList))  // Add multi-parm insert/remove buttons
	{
		CurrIndentation -= (INDENTATION_HORIZONTAL_GAP + 1.0f);  // Folder exclusively
		CreateMultiParmInstanceButtons(CategoryBuilder, HorizontalBox, CurrMultiParm, CurrFolderList->GetMultiParmInstanceIndex());
		CurrIndentation = Indentation;
		Indentation = (INDENTATION_HORIZONTAL_GAP + 3.0f);  // CreateMultiParmInstanceButtons already have Indentation
	}

	// Construct Folder Widgets
	const TWeakObjectPtr<UHoudiniParameterFolderList> CurrFolderList_ = CurrFolderList;
	if (CurrFolderList->IsTab())
	{
		for (int32 FolderIdx = 0; FolderIdx < CurrFolders.Num(); ++FolderIdx)
		{
			if (!CurrFolders[FolderIdx]->IsVisible())
				continue;

			TSharedPtr<SHoudiniTabButton> HoudiniTabButton;

			HorizontalBox->AddSlot()
			.Padding(FolderIdx == 0 ? (Indentation - INDENTATION_HORIZONTAL_GAP - 1.0f) : 0.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Bottom)
			[
				SAssignNew(HoudiniTabButton, SHoudiniTabButton)
				.IsFocusable(false)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Dark")
				//.ContentPadding(FMargin(8.0f, 2.0f, 0.0f, 2.0f))
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(CurrFolders[FolderIdx]->GetLabel()))
					.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				]
				.OnClicked_Lambda([&CategoryBuilder, CurrFolderList_, FolderIdx]()
					{
						if (CurrFolderList_->IsFolderExpanded(FolderIdx))
							return FReply::Handled();

						CurrFolderList_->SetFolderStateValue(FolderIdx);
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();
						return FReply::Handled();
					})
				.ButtonColorAndOpacity(CurrFolderList->IsFolderExpanded(FolderIdx) ? FLinearColor::White : FLinearColor(0.33f, 0.33f, 0.33f))
			];

			HoudiniTabButton->SetAppearance(CurrFolderList->IsFolderExpanded(FolderIdx), CurrFolderList->GetFolderType() == EHoudiniFolderType::Radio);
		}
	}
	else  // Simple or Collapsible
	{
		const bool bHasValue = CurrFolderList_->HasValue();

		HorizontalBox->AddSlot()
		.Padding(Indentation - INDENTATION_HORIZONTAL_GAP - 5.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.IsFocusable(false)
			.ButtonStyle(FAppStyle::Get(), bHasValue ? "Button" : "NoBorder")
			.ContentPadding(bHasValue ? FMargin(-10.0f, 0.0f) : FMargin(2.0f))
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(CurrFolderList->IsFolderExpanded(0) ? "TreeArrow_Expanded" : "TreeArrow_Collapsed"))
			]
			.OnClicked_Lambda([&CategoryBuilder, CurrFolderList_]()
				{
					CurrFolderList_->SetFolderStateValue(CurrFolderList_->IsFolderExpanded(0) ? 0 : 1);
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
					return FReply::Handled();
				})
			.IsEnabled(CurrFolderList->GetFolderType() != EHoudiniFolderType::Simple)
		];

		HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(4.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(CurrFolders[0]->GetLabel()))
			.Font(FAppStyle::Get().GetFontStyle((bHasValue && CurrFolderList->IsFolderExpanded(0)) ?
				"PropertyWindow.BoldFont" : "PropertyWindow.NormalFont"))
		];

		if (CurrFolderList_->IsFolderExpanded(0))
		{
			HorizontalBox->AddSlot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SSeparator)
				.SeparatorImage(FCoreStyle::Get().GetBrush("Header.Post"))
				.Orientation(Orient_Horizontal)
				.ColorAndOpacity(bHasValue ? FStyleColors::White : FLinearColor(0.75f, 0.75f, 0.75f))
			];
		}
	}

	if (CurrFolderList->HasValue())
		HorizontalBox->SetEnabled(TAttribute<bool>::CreateLambda([CurrFolderList_]() { return FHoudiniEngine::Get().AllowEdit() && CurrFolderList_->IsEnabled(); }));
}

void FHoudiniParameterDetails::ParseButton(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	TWeakObjectPtr<UHoudiniParameterButton> CurrParm = Cast<UHoudiniParameterButton>(Parm);

	CreateNameWidget(CategoryBuilder, Parm).ValueContent()
	.MinDesiredWidth(125.0f)
	[
		SNew(SButton)
		.IsFocusable(false)
		.HAlign(HAlign_Center)
		.Text(FText::FromString(CurrParm->GetLabel()))
		.OnClicked_Lambda([CurrParm]() { CurrParm->Press(); return FReply::Handled(); })
		.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
	];
}

// Int types
void FHoudiniParameterDetails::ParseInt(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterInt> CurrParm = Cast<UHoudiniParameterInt>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;
	const TSharedPtr<INumericTypeInterface<int32>> TypeInterface = ConvertHoudiniUnitToTypeInterface<int32>(CurrParm->GetUnit());

	TSharedPtr<SHorizontalBox> HorizontalBox;
	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(125.0f * CurrParm->GetSize())
	//.MaxDesiredWidth(125.0f * CurrParm->GetSize())
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
		.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
	];

	auto OnValueCommittedLambda = [CurrParm, bIsAttrib_](const int32& Idx, const int32& NewValue, const ETextCommit::Type& TextCommitType)
		{
			if (SpinningIdx >= 0)
				SpinningIdx = -1;

			if (TextCommitType == ETextCommit::Default)
				return;

			if (NewValue != CurrParm->GetValue(Idx))
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(Idx, NewValue);
			}
		};

	for (int32 Idx = 0; Idx < CurrParm->GetSize(); ++Idx)
	{
		HorizontalBox->AddSlot()
		[
			SNew(SNumericEntryBox<int32>)
			.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.AllowSpin(true)
			.TypeInterface(TypeInterface)
			.MinValue(CurrParm->GetMin())
			.MaxValue(CurrParm->GetMax())
			.MinSliderValue(CurrParm->GetUIMin())
			.MaxSliderValue(CurrParm->GetUIMax())

			.Value_Lambda([CurrParm, Idx]()
				{
					return (SpinningParm == CurrParm && SpinningIdx == Idx) ?
						SpinningIntValue : CurrParm->GetValue(Idx);
				})
			.OnBeginSliderMovement_Lambda([CurrParm](){ SpinningParm = CurrParm; })
			.OnValueChanged_Lambda([Idx](int32 NewValue)  // On spinning
				{
					SpinningIdx = Idx;
					SpinningIntValue = NewValue;
				})
			.OnValueCommitted_Lambda([Idx, OnValueCommittedLambda](int32 NewValue, ETextCommit::Type TextCommitType) { OnValueCommittedLambda(Idx, NewValue, TextCommitType); })
		];
	}


	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);

			if (ValueStr.IsEmpty())
				return;

			TArray<FString> ValueStrs;
			ValueStr.ParseIntoArray(ValueStrs, TEXT(","));
			TArray<int32> NewValues;
			NewValues.SetNumUninitialized(FMath::Min(ValueStrs.Num(), CurrParm->GetSize()));
			bool bValueChanged = false;
			for (int32 TupleIdx = 0; TupleIdx < NewValues.Num(); ++TupleIdx)
			{
				int32& NewValue = NewValues[TupleIdx];
				if (ValueStrs[TupleIdx].IsNumeric())
				{
					NewValue = FCString::Atoi(*ValueStrs[TupleIdx]);
					if (NewValue != CurrParm->GetValue(TupleIdx))
						bValueChanged = true;
				}
				else
					NewValue = CurrParm->GetValue(TupleIdx);
			}

			if (bValueChanged)
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterIntChange", "Houdini Parameter Int: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				for (int32 TupleIdx = 0; TupleIdx < NewValues.Num(); ++TupleIdx)
					CurrParm->SetValue(TupleIdx, NewValues[TupleIdx]);
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseIntChoice(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterIntChoice> CurrParm = Cast<UHoudiniParameterIntChoice>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(125.0f)
	[
		SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(CurrParm->GetOptionsSource())
		.InitiallySelectedItem(CurrParm->GetChosen())
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) { return SNew(STextBlock).Text(FText::FromString(*InItem)); })
		.OnSelectionChanged_Lambda([CurrParm, bIsAttrib_](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
			{
				if (NewChoice != CurrParm->GetChosen())
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniParameterIntChoiceChange", "Houdini Parameter Int Choice: Changing a value"),
						CurrParm->GetOuter(), !bIsAttrib_);

					CurrParm->Modify();
					CurrParm->SetChosen(NewChoice);
				}
			})
		[
			SNew(STextBlock)
			.Text_Lambda([CurrParm]()
				{
					TSharedPtr<FString> Option = CurrParm->GetChosen();
					return Option ? FText::FromString(*Option) : FText::GetEmpty();
				})
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
		.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
	];

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);

			if (!ValueStr.IsNumeric())
				return;

			const int32 NewValue = FCString::Atoi(*ValueStr);
			if (NewValue != CurrParm->GetValue())
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterIntChoiceChange", "Houdini Parameter Int Choice: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewValue);
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseButtonStrip(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterButtonStrip> CurrParm = Cast<UHoudiniParameterButtonStrip>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	FDetailWidgetDecl& Row = CreateNameWidget(CategoryBuilder, Parm).ValueContent()
		.MinDesiredWidth(200.0f);

	if (CurrParm->NumButtons() <= 0)
		return;

	auto CreateButtonLambda = [&](const TSharedPtr<SHorizontalBox>& HorizontalBox, const int32 ButtonIdx)
	{
		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 1.0f, 0.0f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ButtonColorAndOpacity_Lambda([CurrParm, ButtonIdx]() { return CurrParm->IsPressed(ButtonIdx) ? FLinearColor(1.5f, 2.5f, 3.75f) : FStyleColors::AccentGray; })
			.OnClicked_Lambda([CurrParm, bIsAttrib_, ButtonIdx]()
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniParameterButtonStripChange", "Houdini Parameter Button Strip: Changing a value"),
						CurrParm->GetOuter(), !bIsAttrib_);
					
					CurrParm->Modify();
					CurrParm->Press(ButtonIdx);
					return FReply::Handled();
				})
			.ContentPadding(FMargin(-4.0, 2.0))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CurrParm->GetButtonLabel(ButtonIdx)))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	};

	int32 NumRowSpaces = 2 + CurrParm->GetButtonLabel(0).Len();
	TArray<int32> RowIndices = { 0 };
	for (int32 ButtonIdx = 1; ButtonIdx < CurrParm->NumButtons(); ++ButtonIdx)
	{
		NumRowSpaces += 4 + CurrParm->GetButtonLabel(ButtonIdx).Len();
		if (NumRowSpaces > 60)
		{
			RowIndices.Add(ButtonIdx);
			NumRowSpaces = CurrParm->GetButtonLabel(ButtonIdx).Len();
		}
		else
			++RowIndices.Last();
	}

	if (RowIndices.Num() <= 1)
	{
		TSharedPtr<SHorizontalBox> HorizontalBox;
		Row
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
				.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
		];
		for (int32 ButtonIdx = 0; ButtonIdx < CurrParm->NumButtons(); ++ButtonIdx)
			CreateButtonLambda(HorizontalBox, ButtonIdx);
	}
	else
	{
		TSharedPtr<SVerticalBox> VerticalBox;
		Row
		[
			SAssignNew(VerticalBox, SVerticalBox)
				.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
		];
		for (int32 RowIdx = 0; RowIdx < RowIndices.Num(); ++RowIdx)
		{
			TSharedPtr<SHorizontalBox> HorizontalBox;
			VerticalBox->AddSlot()
				.Padding(0.0f, 1.0f)
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
				];
			for (int32 ButtonIdx = (RowIdx == 0) ? 0 : (RowIndices[RowIdx - 1] + 1); ButtonIdx <= RowIndices[RowIdx]; ++ButtonIdx)
				CreateButtonLambda(HorizontalBox, ButtonIdx);
		}
	}
}

void FHoudiniParameterDetails::ParseToggle(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterToggle> CurrParm = Cast<UHoudiniParameterToggle>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(50.0f)
	[
		SNew(SCheckBox)
		.IsChecked(CurrParm->GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged_Lambda([CurrParm, bIsAttrib_](ECheckBoxState NewState)
			{
				const bool bNewValue = NewState == ECheckBoxState::Checked;
				if (bNewValue != CurrParm->GetValue())
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniParameterToggleChange", "Houdini Parameter Toggle: Changing a value"),
						CurrParm->GetOuter(), !bIsAttrib_);

					CurrParm->Modify();
					CurrParm->SetValue(bNewValue);
				}
			})
		.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
	];

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);

			if (!ValueStr.IsNumeric())
				return;

			const bool NewValue = bool(FCString::Atoi(*ValueStr));
			if (NewValue != CurrParm->GetValue())
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterToggleChange", "Houdini Parameter Toggle: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewValue);
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseFolderList(const TWeakObjectPtr<UHoudiniParameter>& Parm)
{
	CurrFolderList = Cast<UHoudiniParameterFolderList>(Parm);
	CurrFolders.Empty();
	VisibleParentIds.FindOrAdd(Parm->GetId());
}

// Float types
void FHoudiniParameterDetails::ParseFloat(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterFloat> CurrParm = Cast<UHoudiniParameterFloat>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;
	const TSharedPtr<INumericTypeInterface<float>> TypeInterface = ConvertHoudiniUnitToTypeInterface<float>(CurrParm->GetUnit());

	TSharedPtr<SHorizontalBox> HorizontalBox;
	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(125.0f * CurrParm->GetSize())
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
		.IsEnabled(CurrParm->IsEnabled())
	];

	auto OnValueCommittedLambda = [CurrParm, bIsAttrib_](const int32& Idx, const float& NewValue, const ETextCommit::Type& TextCommitType)
		{
			if (SpinningIdx >= 0)
				SpinningIdx = -1;

			if (TextCommitType == ETextCommit::Default)
				return;

			if (NewValue != CurrParm->GetValue(Idx))
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(Idx, NewValue);
			}
		};

	if (CurrParm->GetSize() == 3)
	{
		HorizontalBox->AddSlot()
		[
			SNew(SVectorInputBox)
			.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
			.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.AllowSpin(true)
			.TypeInterface(TypeInterface)
			.bColorAxisLabels(true)
			.MinVector(FVector3f(CurrParm->GetMin()))
			.MaxVector(FVector3f(CurrParm->GetMax()))
			.MinSliderVector(FVector3f(CurrParm->GetUIMin()))
			.MaxSliderVector(FVector3f(CurrParm->GetUIMax()))
			.SpinDelta(0.01f)

			.X_Lambda([CurrParm]() { return (SpinningParm == CurrParm && SpinningIdx == 0) ? SpinningFloatValue : CurrParm->GetValue(0); })
			.Y_Lambda([CurrParm]() { return (SpinningParm == CurrParm && SpinningIdx == 2) ? SpinningFloatValue : CurrParm->GetValue(2); })
			.Z_Lambda([CurrParm]() { return (SpinningParm == CurrParm && SpinningIdx == 1) ? SpinningFloatValue : CurrParm->GetValue(1); })

			.OnBeginSliderMovement_Lambda([CurrParm]() { SpinningParm = CurrParm; })
			.OnXChanged_Lambda([](float NewValue) { SpinningIdx = 0; SpinningFloatValue = NewValue; })
			.OnYChanged_Lambda([](float NewValue) { SpinningIdx = 2; SpinningFloatValue = NewValue; })
			.OnZChanged_Lambda([](float NewValue) { SpinningIdx = 1; SpinningFloatValue = NewValue; })

			.OnXCommitted_Lambda([OnValueCommittedLambda](float NewValue, ETextCommit::Type TextCommitType) { OnValueCommittedLambda(0, NewValue, TextCommitType); })
			.OnYCommitted_Lambda([OnValueCommittedLambda](float NewValue, ETextCommit::Type TextCommitType) { OnValueCommittedLambda(2, NewValue, TextCommitType); })
			.OnZCommitted_Lambda([OnValueCommittedLambda](float NewValue, ETextCommit::Type TextCommitType) { OnValueCommittedLambda(1, NewValue, TextCommitType); })
		];
	}
	else 
	{
		for (int32 Idx = 0; Idx < CurrParm->GetSize(); ++Idx)
		{
			HorizontalBox->AddSlot()
			[
				SNew(SNumericEntryBox<float>)
				.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.AllowSpin(true)
				.TypeInterface(TypeInterface)
				.MinValue(CurrParm->GetMin())
				.MaxValue(CurrParm->GetMax())
				.MinSliderValue(CurrParm->GetUIMin())
				.MaxSliderValue(CurrParm->GetUIMax())

				.Value_Lambda([CurrParm, Idx]()
					{
						return ((SpinningParm == CurrParm) && (SpinningIdx == Idx)) ?
							SpinningFloatValue : CurrParm->GetValue(Idx);
					})
				.OnBeginSliderMovement_Lambda([CurrParm](){ SpinningParm = CurrParm; })
				.OnValueChanged_Lambda([Idx](float NewValue)  // On spinning
					{
						SpinningIdx = Idx;
						SpinningFloatValue = NewValue;
					})
				.OnValueCommitted_Lambda([Idx, OnValueCommittedLambda](float NewValue, ETextCommit::Type TextCommitType) { OnValueCommittedLambda(Idx, NewValue, TextCommitType); })
			];
		}
	}

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);

			if (ValueStr.IsEmpty())
				return;

			if ((CurrParm->GetSize() == 3) && ValueStr.StartsWith(TEXT("(X=")))
			{
				FVector3f NewValue(CurrParm->GetValue(0), CurrParm->GetValue(2), CurrParm->GetValue(1));
				const FVector3f OldValue = NewValue;
				NewValue.InitFromString(ValueStr);
				if (NewValue != OldValue)
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
						CurrParm->GetOuter(), !bIsAttrib_);

					CurrParm->Modify();
					CurrParm->SetValue(0, NewValue.X);
					CurrParm->SetValue(1, NewValue.Z);
					CurrParm->SetValue(2, NewValue.Y);
				}
			}
			else
			{
			TArray<FString> ValueStrs;
			ValueStr.ParseIntoArray(ValueStrs, TEXT(","));
			TArray<float> NewValues;
			NewValues.SetNumUninitialized(FMath::Min(ValueStrs.Num(), CurrParm->GetSize()));
			bool bValueChanged = false;
			for (int32 TupleIdx = 0; TupleIdx < NewValues.Num(); ++TupleIdx)
			{
				float& NewValue = NewValues[TupleIdx];
				if (ValueStrs[TupleIdx].IsNumeric())
				{
					NewValue = FCString::Atof(*ValueStrs[TupleIdx]);
					if (NewValue != CurrParm->GetValue(TupleIdx))
						bValueChanged = true;
				}
				else
					NewValue = CurrParm->GetValue(TupleIdx);
			}

			if (bValueChanged)
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterFloatChange", "Houdini Parameter Float: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				for (int32 TupleIdx = 0; TupleIdx < NewValues.Num(); ++TupleIdx)
					CurrParm->SetValue(TupleIdx, NewValues[TupleIdx]);
			}
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseColor(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	class SHoudiniScreenColorPicker : public SButton
	{
	protected:
		TWeakObjectPtr<UHoudiniParameterColor> Parm;
		bool bIsAttrib = false;

		bool bCapturing = false;

		FReply OnClicked()
		{
			bCapturing = true;
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (bCapturing)
			{
				if (Parm.IsValid())
				{
					const FLinearColor Color = FPlatformApplicationMisc::GetScreenPixelColor(MouseEvent.GetScreenSpacePosition());
					if (Color != Parm->GetValue())
					{
						FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
							LOCTEXT("HoudiniParameterColorChange", "Houdini Parameter Color: Changing a value"),
							Parm->GetOuter(), !bIsAttrib);

						Parm->Modify();
						Parm->SetValue(Color);
					}
				}

				bCapturing = false;
			}

			return SButton::OnMouseButtonUp(MyGeometry, MouseEvent);
		}

	public:
		void Construct(const FArguments& InArgs)
		{
			SButton::Construct(
				SButton::FArguments()
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &SHoudiniScreenColorPicker::OnClicked)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.EyeDropper"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			);
		}

		void SetParm(const TWeakObjectPtr<UHoudiniParameterColor>& InParm, const bool bInIsAttrib)
		{
			Parm = InParm;
			bIsAttrib = bInIsAttrib;
		}
	};


	const TWeakObjectPtr<UHoudiniParameterColor> CurrParm = Cast<UHoudiniParameterColor>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;
	TSharedPtr<SHoudiniScreenColorPicker> ScreenSampler;
	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(150.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f)
		[
			SNew(SColorBlock)
			.AlphaBackgroundBrush(FAppStyle::Get().GetBrush("ColorPicker.RoundedAlphaBackground"))
			.Color_Lambda([CurrParm]() { return CurrParm->GetValue(); })
			.ShowBackgroundForAlpha(true)
			.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Separate)
			.OnMouseButtonDown_Lambda([CurrParm, bIsAttrib_](const FGeometry&, const FPointerEvent&)
				{
					FColorPickerArgs PickerArgs;
					{
						PickerArgs.bUseAlpha = (CurrParm->GetSize() == 4);
						PickerArgs.bOnlyRefreshOnMouseUp = true;
						PickerArgs.bOnlyRefreshOnOk = false;
						PickerArgs.sRGBOverride = true;
						PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
						PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([CurrParm, bIsAttrib_](const FLinearColor& NewValue)
							{
								if (NewValue != CurrParm->GetValue())
								{
									FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
										LOCTEXT("HoudiniParameterColorChange", "Houdini Parameter Color: Changing a value"),
										CurrParm->GetOuter(), !bIsAttrib_);

									CurrParm->Modify();
									CurrParm->SetValue(NewValue);
								}
							});
						PickerArgs.InitialColor = CurrParm->GetValue();
						if (bIsAttrib_)
							PickerArgs.bIsModal = true;
						else
							PickerArgs.ParentWidget = FSlateApplication::Get().GetActiveTopLevelWindow();
						PickerArgs.bOnlyRefreshOnOk = true;
					}

					OpenColorPicker(PickerArgs);

					return FReply::Handled();
				})
			.Size(FVector2D(70.0f, 20.0f))
			.CornerRadius(FVector4(4.0f, 4.0f, 4.0f, 4.0f))
			.IsEnabled(CurrParm->IsEnabled())
		]
		
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 2.0f)
		.AutoWidth()
		[
			SAssignNew(ScreenSampler, SHoudiniScreenColorPicker)
		]
	];

	ScreenSampler->SetParm(CurrParm, bIsAttribPanel);

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);

			if (ValueStr.IsEmpty())
				return;

			TArray<FString> ValueStrs;
			ValueStr.ParseIntoArray(ValueStrs, TEXT(","));
			FLinearColor NewValue = CurrParm->GetValue();
			if (ValueStrs.IsValidIndex(0) && ValueStrs[0].IsNumeric()) NewValue.R = FCString::Atof(*ValueStrs[0]);
			if (ValueStrs.IsValidIndex(1) && ValueStrs[1].IsNumeric()) NewValue.G = FCString::Atof(*ValueStrs[1]);
			if (ValueStrs.IsValidIndex(2) && ValueStrs[2].IsNumeric()) NewValue.B = FCString::Atof(*ValueStrs[2]);
			if (ValueStrs.IsValidIndex(3) && ValueStrs[3].IsNumeric() && CurrParm->GetSize() >= 4) NewValue.A = FCString::Atof(*ValueStrs[3]);
			
			if (NewValue != CurrParm->GetValue())
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterColorChange", "Houdini Parameter Color: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewValue);
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

// String types
void FHoudiniParameterDetails::ParseString(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterString> CurrParm = Cast<UHoudiniParameterString>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	FDetailWidgetDecl& ValueContent = Row.ValueContent();

	auto OnValueCommittedLambda = [CurrParm, bIsAttrib_](const FString& NewValue)
		{
			if (!NewValue.Equals(CurrParm->GetValue(), ESearchCase::CaseSensitive))
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterStringChange", "Houdini Parameter String: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewValue);
			}
		};

	if (CurrParm->GetPathType() == EHoudiniPathParameterType::Invalid)  // Is a normal string parameter
	{
		ValueContent
		.MinDesiredWidth(125.0f)
		[
			SNew(SAssetDropTarget)
			.OnAssetsDropped_Lambda([OnValueCommittedLambda](const FDragDropEvent&, TArrayView<FAssetData> InAssets)
				{
					OnValueCommittedLambda(InAssets[0].GetObjectPathString());
				})
			[
				SNew(SEditableTextBox)
				.ClearKeyboardFocusOnCommit(false)
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([CurrParm](){ return FText::FromString(CurrParm->GetValue());})
				.OnTextCommitted_Lambda([OnValueCommittedLambda](const FText& NewText, ETextCommit::Type TextCommitType)
					{
						if (TextCommitType != ETextCommit::Default)
							OnValueCommittedLambda(NewText.ToString());
					})
			]
			.IsEnabled(CurrParm->IsEnabled())
		];
	}
	else if (CurrParm->GetPathType() == EHoudiniPathParameterType::Directory)  // Is a directory picker
	{
		ValueContent
		.MinDesiredWidth(324.0f)
		[
			SNew(SDirectoryPicker)
			.Directory(CurrParm->GetValue())
			.OnDirectoryChanged_Lambda(OnValueCommittedLambda)
			.ToolTipText(LOCTEXT("DirectoryToolTipText", "Choose a Directory from this computer"))
			.IsEnabled(CurrParm->IsEnabled())
		];
	}
	else  // Is a file picker
	{
		FString FileTypeFilter;
		if (CurrParm->GetPathType() == EHoudiniPathParameterType::Geo)
			FileTypeFilter = TEXT("Houdini Geometry (*.geo;*.bgeo;*.bgeo.sc)|*.geo;*.bgeo;*.bgeo.sc");
		else if (CurrParm->GetPathType() == EHoudiniPathParameterType::Image)
			FileTypeFilter = TEXT("Image|*.png;*.jpg;*.jpeg;*.bmp;*.tiff;*.tif");

		ValueContent
		.MinDesiredWidth(324.0f)
		[
			SNew(SFilePathPicker)
			.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a file from this computer"))
			.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
			.BrowseTitle(LOCTEXT("PropertyEditorTitle", "File picker..."))
			.FilePath_Lambda([CurrParm]{ return CurrParm.IsValid() ? CurrParm->GetValue() : FString(); })  // Use lambda to ensure path won't be committed twice
			.FileTypeFilter(FileTypeFilter)
			.OnPathPicked_Lambda(OnValueCommittedLambda)
			.IsEnabled(CurrParm->IsEnabled())
		];
	}

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([OnValueCommittedLambda]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);
			OnValueCommittedLambda(ValueStr);
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseStringChoice(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterStringChoice> CurrParm = Cast<UHoudiniParameterStringChoice>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	FDetailWidgetDecl& ValueContent = Row.ValueContent();

	auto OnValueCommittedLambda = [CurrParm, bIsAttrib_](const FString& NewValue)
		{
			if (!NewValue.Equals(CurrParm->GetValue(), ESearchCase::CaseSensitive))
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterStringChange", "Houdini Parameter String: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewValue);
			}
		};

	auto OnValueSelectedLambda = [CurrParm, bIsAttrib_](const TSharedPtr<FString>& NewChoice)
		{
			if (NewChoice != CurrParm->GetChosen())
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterStringChange", "Houdini Parameter String: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetChosen(NewChoice);
			}
		};


	switch (CurrParm->GetChoiceListType())
	{
	case EHoudiniChoiceListType::Normal:
	{
		ValueContent
		.MinDesiredWidth(125.0f)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(CurrParm->GetOptionsSource())
			.InitiallySelectedItem(CurrParm->GetChosen())
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) { return SNew(STextBlock).Text(FText::FromString(*InItem)); })
			.OnSelectionChanged_Lambda(
				[OnValueSelectedLambda](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
				{
					OnValueSelectedLambda(NewChoice);
				})
			.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
			[
				SNew(STextBlock)
				.Text_Lambda([CurrParm]()
					{
						TSharedPtr<FString> Option = CurrParm->GetChosen();
						return Option ? FText::FromString(*Option) : FText::GetEmpty();
					})
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	}
	break;
	case EHoudiniChoiceListType::Replace:
	case EHoudiniChoiceListType::Toggle:
	{
		ValueContent
		.MinDesiredWidth((CurrParm->GetChoiceListType() == EHoudiniChoiceListType::Replace) ? 160.0f : 195.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()  // A EditableTextBox that can input string directly
			[
				SNew(SEditableTextBox)
				.ClearKeyboardFocusOnCommit(false)
				.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([CurrParm](){ return FText::FromString(CurrParm->GetValue());})
				.OnTextCommitted_Lambda([OnValueCommittedLambda](const FText& NewText, ETextCommit::Type TextCommitType)
					{
						if (TextCommitType != ETextCommit::Default)
							OnValueCommittedLambda(NewText.ToString());
					})
			]

			+ SHorizontalBox::Slot()  // A Menu that can pick string as input
			.AutoWidth()
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(CurrParm->GetOptionsSource())
				.InitiallySelectedItem(CurrParm->GetChosen())
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) { return SNew(STextBlock).Text(FText::FromString(*InItem)); })
				.OnSelectionChanged_Lambda(
					[OnValueSelectedLambda](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
					{
						OnValueSelectedLambda(NewChoice);
					})
				.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
				[
					SNew(SOverlay)
				]
			]
		];
	}
	break;
	}

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([OnValueCommittedLambda]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);
			OnValueCommittedLambda(ValueStr);
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseAsset(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterAsset> CurrParm = Cast<UHoudiniParameterAsset>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	TArray<const UClass*> AllowClasses, DisallowClasses;
	for (const FString& ClassName : CurrParm->GetAllowClassNames())
	{
		const UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
		if (IsValid(FoundClass))
			AllowClasses.AddUnique(FoundClass);
	}
	for (const FString& ClassName : CurrParm->GetDisallowClassNames())
	{
		const UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
		if (IsValid(FoundClass))
			DisallowClasses.AddUnique(FoundClass);
	}
	
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = CategoryBuilder.GetParentLayout().GetThumbnailPool();
	FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(CurrParm->GetAssetPath());
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
			.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
			.Text(FText::FromName(AssetData.AssetName))
		];


	auto OnAssetPickedLambda = [CurrParm, bIsAttrib_, AssetThumbnailPool, AssetThumbnail, AssetNameBlock, AssetComboButton]
		(const FAssetData& AssetData)
		{
			const FSoftObjectPath NewAssetPath = AssetData.GetSoftObjectPath();
			if (NewAssetPath != CurrParm->GetAssetPath())
			{
				if (AssetComboButton->IsOpen())
					AssetComboButton->SetIsOpen(false);

				AssetThumbnail->SetAsset(AssetData);
				if (AssetData.IsValid())
					AssetThumbnailPool->RefreshThumbnail(AssetThumbnail);
				AssetNameBlock->SetText(FText::FromName(AssetData.AssetName));

				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterAssetChange", "Houdini Parameter Asset: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetAssetPath(NewAssetPath);
			}
		};


	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(225.0f)
	[
		SNew(SAssetDropTarget)
		.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
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
				.OnMouseDoubleClick_Lambda([CurrParm](const FGeometry&, const FPointerEvent&)
					{
						UObject* Asset = CurrParm->GetAssetPath().TryLoad();
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
					SNew(SHorizontalBox)
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
						PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateLambda([CurrParm]()
								{
									FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(CurrParm->GetAssetPath());
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
							.Image(FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.Rebuild"))
							.DesiredSizeOverride(FVector2D(16.0, 16.0))
							.ColorAndOpacity(FLinearColor(0.9f, 1.0f, 0.8f, 0.8f))
						]
						.OnClicked_Lambda([CurrParm]() { CurrParm->Reimport(); return FReply::Handled(); })
						.ToolTipText(CurrParm->ShouldImportInfo() ?
							LOCTEXT("ParmAssetReimportToolTip", "Refresh Asset Path And Import Info") :
							LOCTEXT("ParmAssetReimportToolTip", "Refresh Asset Path"))
						.IsEnabled(AssetData.IsUAsset())
					]
				]
			]
		]
	];

	AssetComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([OnAssetPickedLambda, AssetData, AllowClasses, DisallowClasses, CurrParm]()
		{
			return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(AssetData, true, false,
				AllowClasses, DisallowClasses, TArray<UFactory*>{},
				CurrParm->HasAssetFilters() ? FOnShouldFilterAsset::CreateLambda([CurrParm](const FAssetData& AssetData)
					{
						const FString& FullName = AssetData.GetFullName();
						for (const FString& InvertedFilter : CurrParm->GetAssetInvertedFilters())
						{
							if (FullName.Contains(InvertedFilter, ESearchCase::IgnoreCase))
								return true;
						}
						for (const FString& Filter : CurrParm->GetAssetFilters())
						{
							if (FullName.Contains(Filter, ESearchCase::IgnoreCase))
								return false;
						}
						return true;
					}) : FOnShouldFilterAsset(),
				FOnAssetSelected::CreateLambda(OnAssetPickedLambda),
				FSimpleDelegate::CreateLambda([]() {}));
		}));

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([OnAssetPickedLambda]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);
			if (ValueStr.IsEmpty())
				return;

			int32 SplitIdx;
			if (ValueStr.FindChar(TCHAR(';'), SplitIdx))
				ValueStr.LeftInline(SplitIdx);

			FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(ValueStr);
			OnAssetPickedLambda(AssetData);
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseAssetChoice(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<UHoudiniParameterAssetChoice> CurrParm = Cast<UHoudiniParameterAssetChoice>(Parm);

	const bool bIsAttrib_ = bIsAttribPanel;

	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = CategoryBuilder.GetParentLayout().GetThumbnailPool();
	FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(CurrParm->GetAssetPath());
	TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, 64, 64, AssetThumbnailPool));

	TSharedPtr<STextBlock> AssetNameBlock;
	TSharedRef<SComboButton> AssetComboButton = SNew(SComboButton)
		.IsFocusable(false)  // Avoid attrib-panel close automatically
		.ContentPadding(1.0f)
		.ButtonContent()
		[
			SAssignNew(AssetNameBlock, STextBlock)
				.TextStyle(FAppStyle::Get(), "PropertyEditor.AssetClass")
				.Font(FAppStyle::Get().GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
				.Text(FText::FromName(AssetData.AssetName))
		];

	auto OnAssetPickedLambda = [CurrParm, bIsAttrib_, AssetThumbnailPool, AssetThumbnail, AssetNameBlock, AssetComboButton]
		(const FAssetData& AssetData)
		{
			const FSoftObjectPath NewAssetPath = AssetData.GetSoftObjectPath();
			if (NewAssetPath != CurrParm->GetAssetPath())
			{
				if (AssetComboButton->IsOpen())
					AssetComboButton->SetIsOpen(false);

				AssetThumbnail->SetAsset(AssetData);
				AssetThumbnailPool->RefreshThumbnail(AssetThumbnail);
				AssetNameBlock->SetText(FText::FromName(AssetData.AssetName));

				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterAssetChange", "Houdini Parameter Asset: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetAssetPath(NewAssetPath);
			}
		};

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(225.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"))
			.OnMouseDoubleClick_Lambda([CurrParm](const FGeometry&, const FPointerEvent&)
				{
					UObject* CurrAsset = CurrParm->GetAssetPath().TryLoad();
					if (IsValid(CurrAsset))
						GEditor->EditObject(CurrAsset);

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
		.VAlign(VAlign_Center)
		[
			AssetComboButton
		]
	];

	AssetComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda([OnAssetPickedLambda, AssetData, CurrParm]()
		{
			TArray<TPair<FName, FName>> ChoiceAssetFilters;
			for (FString AssetPath : CurrParm->GetChoiceAssetPaths())
			{
				int32 SplitIdx;
				if (AssetPath.FindChar(TCHAR(';'), SplitIdx))
					AssetPath.LeftInline(SplitIdx);

				AssetPath = FPackageName::ExportTextPathToObjectPath(AssetPath);
				FString PackagePath;
				FString AssetName;
				if (!AssetPath.Split(TEXT("."), &PackagePath, &AssetName))
				{
					PackagePath = AssetPath;
					AssetName = FPaths::GetBaseFilename(AssetPath);
				}
				ChoiceAssetFilters.Add(TPair<FName, FName>(*PackagePath, *AssetName));
			}

			return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(AssetData, true, false,
				TArray<const UClass*>{}, TArray<const UClass*>{}, TArray<UFactory*>{},
				FOnShouldFilterAsset::CreateLambda([ChoiceAssetFilters](const FAssetData& AssetData)
					{
						return !ChoiceAssetFilters.Contains(TPair<FName, FName>(AssetData.PackageName, AssetData.AssetName));
					}),
				FOnAssetSelected::CreateLambda(OnAssetPickedLambda),
				FSimpleDelegate::CreateLambda([]() {}));
		}));

	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([OnAssetPickedLambda]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);
			if (ValueStr.IsEmpty())
				return;

			int32 SplitIdx;
			if (ValueStr.FindChar(TCHAR(';'), SplitIdx))
				ValueStr.LeftInline(SplitIdx);

			FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(ValueStr);
			OnAssetPickedLambda(AssetData);
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseLabel(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	const TWeakObjectPtr<const UHoudiniParameterLabel> CurrParm = Cast<UHoudiniParameterLabel>(Parm);

	CreateNameWidget(CategoryBuilder, Parm).ValueContent()
	.MinDesiredWidth(125.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(CurrParm->GetValue()))
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	];
}

// Extras
void FHoudiniParameterDetails::ParseMultiParm(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm)
{
	const TWeakObjectPtr<UHoudiniMultiParameter> CurrParm = Cast<UHoudiniMultiParameter>(Parm);
	VisibleParentIds.FindOrAdd(Parm->GetId());

	TSharedPtr<SHorizontalBox> HorizontalBox;
	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	.MinDesiredWidth(192.0f)
	//.MaxDesiredWidth(125.0f * CurrParm->GetSize())
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
		.IsEnabled_Lambda([CurrParm]() { return FHoudiniEngine::Get().AllowEdit() && CurrParm->IsEnabled(); })
	];
	
	const bool bIsAttrib_ = bIsAttribPanel;
	HorizontalBox->AddSlot()
	[
		SNew(SNumericEntryBox<int32>)
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		//.AllowSpin(false)
		.Value_Lambda([CurrParm]() { return CurrParm.IsValid() ? CurrParm->GetInstanceCount() : 0; })
		.OnBeginSliderMovement_Lambda([CurrParm]() { SpinningParm = CurrParm; })
		.OnValueCommitted_Lambda([&CategoryBuilder, CurrParm, bIsAttrib_](int32 NewValue, ETextCommit::Type TextCommitType)
			{
				if (TextCommitType == ETextCommit::Default)
					return;

				if (NewValue != CurrParm->GetInstanceCount())
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniMultiParameterChange", "Houdini Multi Parameter: Changing a value"),
						CurrParm->GetOuter(), !bIsAttrib_);

					CurrParm->Modify();
					CurrParm->SetInstanceCount(NewValue);

					if (!CurrParm->GetChildAttributeParameters().IsEmpty())
						CategoryBuilder.GetParentLayout().ForceRefreshDetails();
				}
			})
	];

	HorizontalBox->AddSlot()
	.AutoWidth()
	[
		PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateLambda([&CategoryBuilder, CurrParm, bIsAttrib_]()
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniMultiParameterChange", "Houdini Multi Parameter: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetInstanceCount(CurrParm->GetInstanceCount() + 1);
				
				if (!CurrParm->GetChildAttributeParameters().IsEmpty())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
			}))
	];

	HorizontalBox->AddSlot()
	.AutoWidth()
	[
		PropertyCustomizationHelpers::MakeRemoveButton(FSimpleDelegate::CreateLambda([&CategoryBuilder, CurrParm, bIsAttrib_]()
			{
				if (CurrParm->GetInstanceCount() <= 0)
					return;

				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniMultiParameterChange", "Houdini Multi Parameter: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetInstanceCount(CurrParm->GetInstanceCount() - 1);
				
				if (!CurrParm->GetChildAttributeParameters().IsEmpty())
					CategoryBuilder.GetParentLayout().ForceRefreshDetails();
			}))
	];

	HorizontalBox->AddSlot()
	.AutoWidth()
	[
		PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateLambda([CurrParm, bIsAttrib_]()
			{
				if (CurrParm->GetInstanceCount() <= 0)
					return;

				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniMultiParameterChange", "Houdini Multi Parameter: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetInstanceCount(0);
			}))
	];

	if (bIsAttribPanel)  // Will copy and paste both count and parm insts
	{
		Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]()
			{
				TMap<FName, FHoudiniGenericParameter> Parms;
				CurrParm->SaveGeneric(Parms);

				FJsonDataBag DataBag;
				DataBag.JsonObject = IHoudiniPresetHandler::ConvertParametersToJson(Parms);
				FPlatformApplicationMisc::ClipboardCopy(*DataBag.ToJson());
			});
		Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm]()
			{
				class FHoudiniModifyParameter : public UHoudiniParameter
				{
				public:
					FORCEINLINE void OnValueSet() { SetModification(EHoudiniParameterModification::SetValue); }
				};

				FString JsonStr;
				FPlatformApplicationMisc::ClipboardPaste(JsonStr);

				TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonStr);
				TSharedPtr<FJsonObject> JsonParms;
				if (FJsonSerializer::Deserialize(JsonReader, JsonParms))
				{
					if (CurrParm->LoadGeneric(*IHoudiniPresetHandler::ConvertJsonToParameters(JsonParms), false))
						((FHoudiniModifyParameter*)CurrParm.Get())->OnValueSet();
				}
			});
		Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
	}
}

void FHoudiniParameterDetails::ParseFloatRamp(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	class FHoudiniCurveEditorHideTangent : public SCurveEditor
	{
	public:
		FORCEINLINE void Set() { GetSettings()->SetTangentVisibility(ECurveEditorTangentVisibility::NoTangents); }
	};

	TWeakObjectPtr<UHoudiniParameterFloatRamp> CurrParm = Cast<UHoudiniParameterFloatRamp>(Parm);

	TSharedPtr<SCurveEditor> CurveEditor;

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	[
		SAssignNew(CurveEditor, SCurveEditor)
		.ViewMinInput(0.0f)
		.ViewMaxInput(1.0f)
		.ViewMinOutput(0.0f)
		.ViewMaxOutput(1.0f)
		.TimelineLength(1.0f)
		.AllowZoomOutput(false)
		.ShowZoomButtons(false)
		.ZoomToFitHorizontal(false)
		.ZoomToFitVertical(false)
		.XAxisName(FString("X"))
		.YAxisName(FString("Y"))
		.ShowCurveSelector(false)
		.HideUI(true)
		.DesiredSize(FVector2D(300.0, 90.0))
		.IsEnabled(CurrParm->IsEnabled())
	];
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(CurveEditor.Get());  // To avoid crash, when StaticMeshCompiler running after level switched

	CurveEditor->SetCurveOwner(CurrParm.Get());
	StaticCastSharedPtr<FHoudiniCurveEditorHideTangent>(CurveEditor)->Set();

	// ------- Copy and Paste --------
	const bool bIsAttrib_ = bIsAttribPanel;
	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);
			if (!ValueStr.Contains(TEXT("{")))
				return;
			
			FRichCurve NewFloatCurve;
			UHoudiniParameterFloatRamp::ParseString(NewFloatCurve, ValueStr);
			
			if (CurrParm->IsDifferent(NewFloatCurve))
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterRampChange", "Houdini Parameter Ramp: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewFloatCurve);
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

void FHoudiniParameterDetails::ParseColorRamp(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const
{
	class SHoudiniScreenColorPicker : public SButton
	{
	protected:
		TWeakObjectPtr<UHoudiniParameterColorRamp> Parm;
		bool bIsAttrib = false;

		bool bCapturing = false;
		bool bIsSampling = false;

		TArray<FLinearColor> Colors;

		FReply OnClicked()
		{
			bCapturing = true;
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (bCapturing)
				bIsSampling = true;

			return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
		}

		virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (bIsSampling && Parm.IsValid())
				Colors.Add(FPlatformApplicationMisc::GetScreenPixelColor(MouseEvent.GetScreenSpacePosition()));

			return SButton::OnMouseMove(MyGeometry, MouseEvent);
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (bCapturing)
			{
				class FHoudiniColorRampModifier : public UHoudiniParameterColorRamp
				{
				public:
					void SetColors(const TArray<FLinearColor>& NewColors)
					{
						const int32 NumKeys = NewColors.Num();
						for (int32 ChannelIdx = 0; ChannelIdx < 3; ++ChannelIdx)
						{
							Value[ChannelIdx].Keys.SetNum(NumKeys);
							for (int32 KeyIdx = 0; KeyIdx < NumKeys; ++KeyIdx)
							{
								FRichCurveKey& Key = Value[ChannelIdx].Keys[KeyIdx];
								Key.Time = KeyIdx / (NumKeys - 1.0f);
								Key.Value = *((float*)&NewColors[KeyIdx] + ChannelIdx);
							}
						}
						SetModification(EHoudiniParameterModification::SetValue);
					}
				};

				if (Parm.IsValid())
				{
					FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
						LOCTEXT("HoudiniParameterRampChange", "Houdini Parameter Ramp: Changing a value"),
						Parm->GetOuter(), !bIsAttrib);

					Parm->Modify();
					((FHoudiniColorRampModifier*)(Parm.Get()))->SetColors(Colors);
				}

				bCapturing = false;
				bIsSampling = false;
				Colors.Empty();
			}

			return SButton::OnMouseButtonUp(MyGeometry, MouseEvent);
		}

	public:
		void Construct(const FArguments& InArgs)
		{
			SButton::Construct(
				SButton::FArguments()
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.OnClicked(this, &SHoudiniScreenColorPicker::OnClicked)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.EyeDropper"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			);
		}

		void SetParm(const TWeakObjectPtr<UHoudiniParameterColorRamp>& InParm, const bool bInIsAttrib)
		{
			Parm = InParm;
			bIsAttrib = bInIsAttrib;
		}
	};

	class SHoudiniColorRampEditor : public SColorGradientEditor
	{
	protected:
		TWeakObjectPtr<UHoudiniParameterColorRamp> Parm;

	public:
		void SetParm(const TWeakObjectPtr<UHoudiniParameterColorRamp>& CurrParm)
		{
			SetCurveOwner(CurrParm.Get());
			Parm = CurrParm;
		}

		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			FReply Reply = SColorGradientEditor::OnMouseButtonUp(MyGeometry, MouseEvent);
			Parm->TriggerCookIfChanged();
			return Reply;
		}

		virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
		{
			FReply Reply = SColorGradientEditor::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
			if (Reply.IsEventHandled())  // Means color picker opened
			{
				const TWeakObjectPtr<UHoudiniParameterColorRamp> CurrParm = Parm;
				SColorPicker::OnColorPickerDestroyOverride.BindLambda([CurrParm]()
					{
						if (CurrParm.IsValid())
							CurrParm->TriggerCookIfChanged();
						SColorPicker::OnColorPickerDestroyOverride.Unbind();
					});
			}
			return Reply;
		}
	};

	TWeakObjectPtr<UHoudiniParameterColorRamp> CurrParm = Cast<UHoudiniParameterColorRamp>(Parm);

	TSharedPtr<SHoudiniColorRampEditor> ColorRampEditor;
	TSharedPtr<SHoudiniScreenColorPicker> ScreenSampler;

	FDetailWidgetRow& Row = CreateNameWidget(CategoryBuilder, Parm);
	Row.ValueContent()
	[
		SNew(SHorizontalBox)
		.IsEnabled(CurrParm->IsEnabled())
		+ SHorizontalBox::Slot()
		[
			SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(75.0f)
			[
				SAssignNew(ColorRampEditor, SHoudiniColorRampEditor)
				.ViewMinInput(0.0f)
				.ViewMaxInput(1.0f)
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 30.0f)
		.AutoWidth()
		[
			SAssignNew(ScreenSampler, SHoudiniScreenColorPicker)
		]
	];

	ColorRampEditor->SetParm(CurrParm);
	ScreenSampler->SetParm(CurrParm, bIsAttribPanel);

	// ------- Copy and Paste --------
	const bool bIsAttrib_ = bIsAttribPanel;
	Row.CopyMenuAction.ExecuteAction.BindLambda([CurrParm]() { FPlatformApplicationMisc::ClipboardCopy(*CurrParm->GetValueString()); });
	Row.PasteMenuAction.ExecuteAction.BindLambda([CurrParm, bIsAttrib_]()
		{
			FString ValueStr;
			FPlatformApplicationMisc::ClipboardPaste(ValueStr);
			if (!ValueStr.Contains(TEXT("{")))
				return;
			
			FRichCurve NewFloatCurves[4];
			UHoudiniParameterColorRamp::ParseString(NewFloatCurves, ValueStr);

			if (CurrParm->IsDifferent(NewFloatCurves))
			{
				FScopedTransaction Transaction(TEXT(HOUDINI_LOCTEXT_NAMESPACE),
					LOCTEXT("HoudiniParameterRampChange", "Houdini Parameter Ramp: Changing a value"),
					CurrParm->GetOuter(), !bIsAttrib_);

				CurrParm->Modify();
				CurrParm->SetValue(NewFloatCurves);
			}
		});
	Row.PasteMenuAction.CanExecuteAction.BindLambda([CurrParm]() { return CurrParm->IsEnabled() && FHoudiniEngine::Get().AllowEdit(); });
}

#undef LOCTEXT_NAMESPACE
