// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEditableGeometry.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniParameterAttribute.h"
#include "HoudiniParameter.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

AHoudiniNode* UHoudiniEditableGeometry::GetParentNode() const
{
	if (AHoudiniNode* Node = Cast<AHoudiniNode>(GetOwner()))
		return Node;

	return Cast<AHoudiniNodeProxy>(GetOwner())->GetParentNode();
}

void UHoudiniEditableGeometry::TriggerParentNodeToCook() const
{
	GetParentNode()->TriggerCookByEditableGeometry(this);
}

void UHoudiniEditableGeometry::ModifyAllAttributes()
{
	for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
	{
		if (!ParmAttrib->IsParameterHolder())
			ParmAttrib->Modify();
	}

	for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
	{
		if (!ParmAttrib->IsParameterHolder())
			ParmAttrib->Modify();
	}
}

void UHoudiniEditableGeometry::Update(const AHoudiniNode* Node, const FString& InGroupName,
	const TArray<FString>& IgnorePointAttribNames, const TArray<FString>& IgnorePrimAttribNames)
{
	Name = InGroupName;

	const FHoudiniAttributeParameterSet* FoundAttribParmSetPtr = Node->GetGroupParameterSetMap().Find(InGroupName);
	if (!FoundAttribParmSetPtr)
	{
		PointParmAttribs.Empty();
		PrimParmAttribs.Empty();

		PointEditOptions = EHoudiniEditOptions::None;
		PrimEditOptions = EHoudiniEditOptions::None;

		return;
	}

	auto UpdateParmAttribsLambda = [this](const TArray<FString>& IgnoreAttribNames, const TArray<UHoudiniParameter*>& Parms,
		const TArray<UHoudiniParameterAttribute*>& Attribs, const EHoudiniAttributeOwner& AttribOwner,
		const TArray<UHoudiniParameterAttribute*>& SecondaryAttribs, TArray<UHoudiniParameterAttribute*>& OutNewAttribs) -> void
		{
			OutNewAttribs.Reserve(Parms.Num());
			for (UHoudiniParameter* Parm : Parms)
			{
				if (IgnoreAttribNames.Contains(Parm->GetParameterName()))
					continue;

				UHoudiniParameterAttribute* const* FoundParmAttribPtr = Attribs.FindByPredicate(  // Try to reuse the previous ParmAttribs
					[&](UHoudiniParameterAttribute* Attrib) { return Attrib->Correspond(Parm, AttribOwner); });
				if (!FoundParmAttribPtr)  // Maybe this attrib has moved to secondary class, so we may could find it in SecondaryAttribs
					FoundParmAttribPtr = SecondaryAttribs.FindByPredicate(
						[&](UHoudiniParameterAttribute* Attrib) { return Attrib->Correspond(Parm, AttribOwner); });  // Force set AttribOwner

				OutNewAttribs.Add(FoundParmAttribPtr ? *FoundParmAttribPtr : UHoudiniParameterAttribute::Create(this, Parm, AttribOwner));
			}
		};

	TArray<UHoudiniParameterAttribute*> NewPointParmAttribs;
	UpdateParmAttribsLambda(IgnorePointAttribNames, FoundAttribParmSetPtr->PointAttribParms, PointParmAttribs, EHoudiniAttributeOwner::Point, PrimParmAttribs, NewPointParmAttribs);
	TArray<UHoudiniParameterAttribute*> NewPrimParmAttribs;
	UpdateParmAttribsLambda(IgnorePrimAttribNames, FoundAttribParmSetPtr->PrimAttribParms, PrimParmAttribs, EHoudiniAttributeOwner::Prim, PointParmAttribs, NewPrimParmAttribs);

	PointParmAttribs = NewPointParmAttribs;
	PrimParmAttribs = NewPrimParmAttribs;

	PointEditOptions = FoundAttribParmSetPtr->PointEditOptions;
	PrimEditOptions = FoundAttribParmSetPtr->PrimEditOptions;

	UpdateCycles = FPlatformTime::Cycles64();
}

bool UHoudiniEditableGeometry::HapiUpdate(const AHoudiniNode* Node, const FString& InGroupName,
	const int32& NodeId, const int32& PartId, const TArray<int32>& PointIndices, const TArray<int32>& PrimIndices,
	const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS],
	TArray<UHoudiniParameterAttribute*>& InOutPointAttribs, TArray<UHoudiniParameterAttribute*>& InOutPrimAttribs,
	const TArray<FString>& IgnorePointAttribNames, const TArray<FString>& IgnorePrimAttribNames)
{
	// Update ParmAttribs
	Update(Node, InGroupName, IgnorePointAttribNames, IgnorePrimAttribNames);

	// Find or Retrieve attribute data
	auto HapiRetrieveAttribsLambda = [&](const TArray<UHoudiniParameterAttribute*>& LocalAttribs, const TArray<int32>& ElemIndices,
		TArray<UHoudiniParameterAttribute*>& InOutGlobalAttribs) -> bool
		{
			for (UHoudiniParameterAttribute* LocalAttrib : LocalAttribs)
			{
				if (LocalAttrib->IsParameterHolder())
					continue;

				if (UHoudiniParameterAttribute** FoundGlobalAttribPtr = InOutGlobalAttribs.FindByPredicate(
					[LocalAttrib](const UHoudiniParameterAttribute* GlobalAttrib) { return (LocalAttrib->GetParameter() == GlobalAttrib->GetParameter()); }))
					LocalAttrib->SetDataFrom(*FoundGlobalAttribPtr, ElemIndices);
				else
				{
					UHoudiniParameterAttribute* NewGlobalAttrib = nullptr;
					HOUDINI_FAIL_RETURN(LocalAttrib->HapiDuplicateRetrieve(NewGlobalAttrib, NodeId, PartId, AttribNames, AttribCounts));
					LocalAttrib->SetDataFrom(NewGlobalAttrib, ElemIndices);
					if (NewGlobalAttrib)
						InOutGlobalAttribs.Add(NewGlobalAttrib);
				}
			}
			return true;
		};

	HOUDINI_FAIL_RETURN(HapiRetrieveAttribsLambda(PointParmAttribs, PointIndices, InOutPointAttribs));
	HOUDINI_FAIL_RETURN(HapiRetrieveAttribsLambda(PrimParmAttribs, PrimIndices, InOutPrimAttribs));

	// Update selection
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		const int32 NumElems = PointIndices.Num();
		SelectedIndices.RemoveAll([NumElems](const int32& SelectedIdx) { return (SelectedIdx >= NumElems); });
	}
	if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		const int32 NumElems = PrimIndices.Num();
		SelectedIndices.RemoveAll([NumElems](const int32& SelectedIdx) { return (SelectedIdx >= NumElems); });
	}

	return true;
}

void UHoudiniEditableGeometry::ClearEditData()
{
	Name = TEXT(HAPI_GROUP_MAIN_GEO);  // All non-editable must in "main_geo" group
	PointParmAttribs.Empty();
	PrimParmAttribs.Empty();
	PointEditOptions = EHoudiniEditOptions::Hide;
	PrimEditOptions = EHoudiniEditOptions::Disabled;
	DeltaInfo.Empty();
	ReDeltaInfo.Empty();
	UnDeltaInfo.Empty();

	UpdateCycles = 0;

	ResetSelection();
}

TArray<UHoudiniParameter*> UHoudiniEditableGeometry::GetAttributeParameters(const bool& bSetParmValue) const
{
	TArray<UHoudiniParameter*> AttribParms;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
			AttribParms.Add(bSetParmValue ? ParmAttrib->GetParameter(SelectedIndices.Last()) : ParmAttrib->GetParameter());
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
			AttribParms.Add(bSetParmValue ? ParmAttrib->GetParameter(SelectedIndices.Last()) : ParmAttrib->GetParameter());
	}

	return AttribParms;
}

bool UHoudiniEditableGeometry::UpdateAttribute(const UHoudiniParameter* ChangedAttribParm, const EHoudiniAttributeOwner& ChangedClass)
{
	if ((SelectedClass != ChangedClass) || SelectedIndices.IsEmpty())
		return false;

	const TArray<UHoudiniParameterAttribute*>& ParmAttribs =
		(ChangedClass == EHoudiniAttributeOwner::Point) ? PointParmAttribs : PrimParmAttribs;
	for (UHoudiniParameterAttribute* ParmAttrib : ParmAttribs)
	{
		if (ParmAttrib->IsParameterMatched(ChangedAttribParm))
		{
			const FString DeltaInfoPrefix = Name /
					FString::Printf(TEXT("%s:%f,%f,%f"), ((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM),
					ClickPosition.X * POSITION_SCALE_TO_HOUDINI_F, ClickPosition.Z * POSITION_SCALE_TO_HOUDINI_F, ClickPosition.Y * POSITION_SCALE_TO_HOUDINI_F) /
				ParmAttrib->GetParameterName() + TEXT("/");
			
			const FString PrevValuesString = ParmAttrib->GetValuesString(SelectedIndices);
			
#if WITH_EDITOR
			ReDeltaInfo = DeltaInfoPrefix + PrevValuesString;  // Ensure we have previous values in ReDeltaInfo after undo

			FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
				LOCTEXT("HoudiniEditableGeometryAttributeTransaction", "Houdini Editable Geometry: Attribute Changed"), this, true);

			Modify();
			ParmAttrib->Modify();
#endif

			ParmAttrib->UpdateFromParameter(SelectedIndices);
			const FString CurrValuesString = ParmAttrib->GetValuesString(SelectedIndices);

			DeltaInfo = DeltaInfoPrefix + PrevValuesString;
			ReDeltaInfo = DeltaInfoPrefix + CurrValuesString;
			UnDeltaInfo = DeltaInfoPrefix + CurrValuesString;  // Will set current values to DeltaInfo after undo

			return true;
		}
	}

	return false;
}

FString UHoudiniEditableGeometry::GetPresetPathFilter() const
{
	return GetParentNode()->GetPresetPathFilter() / Name + ((SelectedClass == EHoudiniAttributeOwner::Point) ? TEXT("/Point") : TEXT("/Prim"));
}

bool UHoudiniEditableGeometry::GetGenericParameters(TMap<FName, FHoudiniGenericParameter>& OutParms) const
{
	if (!IsGeometrySelected())
		return false;

	const TArray<UHoudiniParameterAttribute*>& ParmAttribs =
		(SelectedClass == EHoudiniAttributeOwner::Point) ? PointParmAttribs : PrimParmAttribs;
	for (const UHoudiniParameterAttribute* ParmAttrib : ParmAttribs)
	{
		const UHoudiniParameter* Parm = ParmAttrib->GetParameter();
		if (!Parm->GetParameterName().Equals(Parm->GetLabel()) || !Parm->GetHelp().IsEmpty())  // If false, then means this attrib is only for record some info and not for artist to adjust
			Parm->SaveGeneric(OutParms);
	}

	return true;
}

void UHoudiniEditableGeometry::SetGenericParameters(const TSharedPtr<const TMap<FName, FHoudiniGenericParameter>>& Parms)
{
	bool bHasAttribChanged = false;

	const TArray<UHoudiniParameterAttribute*>& TargetParmAttribs =
		(SelectedClass == EHoudiniAttributeOwner::Point) ? PointParmAttribs : PrimParmAttribs;
	for (UHoudiniParameterAttribute* ParmAttrib : TargetParmAttribs)
	{
		if (ParmAttrib->GetParameter()->LoadGeneric(*Parms, false))  // Always compare with values
		{
			ParmAttrib->UpdateFromParameter(SelectedIndices);
			ParmAttrib->GetParameter()->ResetModification();
			bHasAttribChanged = true;
		}
	}

	if (bHasAttribChanged)
	{
		DeltaInfo = FString::Printf(TEXT("%s/%s:%f,%f,%f/%s"), *Name,
			((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM),
			ClickPosition.X * POSITION_SCALE_TO_HOUDINI_F, ClickPosition.Z * POSITION_SCALE_TO_HOUDINI_F, ClickPosition.Y * POSITION_SCALE_TO_HOUDINI_F,
			DELTAINFO_ACTION_SET_ALL_ATTRIBS);
		
		TriggerParentNodeToCook();
	}
}


FORCEINLINE static void AppendModifierKeysToActionString(const FModifierKeysState& ModifierKeys, FString& ActionStr)
{
	if (ModifierKeys.IsControlDown())
		ActionStr += TEXT("|ctrl");
	if (ModifierKeys.IsShiftDown())
		ActionStr += TEXT("|shift");
	if (ModifierKeys.IsAltDown())
		ActionStr += TEXT("|alt");
	if (ModifierKeys.AreCapsLocked())
		ActionStr += TEXT("|caps_lock");
	if (ModifierKeys.IsCommandDown())
		ActionStr += TEXT("|cmd");
}

void UHoudiniEditableGeometry::Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
	const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FModifierKeysState& ModifierKeys)
{
	if ((SelectedClass != SelectClass) || (bDeselectPrevious && bDeselectIfSelected))
		SelectedIndices.Empty();

	SelectedClass = SelectClass;
	if ((SelectedClass != EHoudiniAttributeOwner::Point) && (SelectedClass != EHoudiniAttributeOwner::Prim))
		return;

	const int32 CurrSelectionIdx = SelectedIndices.IndexOfByKey(ElemIdx);
	if (CurrSelectionIdx >= 0)
	{
		if (bDeselectIfSelected)
		{
			SelectedIndices.RemoveAt(CurrSelectionIdx);
			if (SelectedIndices.IsEmpty())
				SelectedClass = EHoudiniAttributeOwner::Invalid;
		}
		else if (CurrSelectionIdx != (SelectedIndices.Num() - 1))
			SelectedIndices.Swap(CurrSelectionIdx, SelectedIndices.Num() - 1);  // We should ensure current SelectedIdx at the last of SelectedIndices.
	}
	else  // Previous does not select
	{
		if (bDeselectIfSelected || !bDeselectPrevious)
			SelectedIndices.Add(ElemIdx);
		else
			SelectedIndices = TArray<int32>{ ElemIdx };
	}

	FString ActionStr = DELTAINFO_ACTION_DROP;
	AppendModifierKeysToActionString(ModifierKeys, ActionStr);

	if (((SelectClass == EHoudiniAttributeOwner::Point) && ShouldCookOnPointSelect()) ||
		((SelectClass == EHoudiniAttributeOwner::Prim) && ShouldCookOnPrimSelect()))
	{
		DeltaInfo = FString::Printf(TEXT("%s/%s:%f,%f,%f/%s"), *Name,
			((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM),
			ClickPosition.X * POSITION_SCALE_TO_HOUDINI, ClickPosition.Z * POSITION_SCALE_TO_HOUDINI, ClickPosition.Y * POSITION_SCALE_TO_HOUDINI,
			*ActionStr);

		ReDeltaInfo = DeltaInfo;

		TriggerParentNodeToCook();
	}
}

bool UHoudiniEditableGeometry::OnAssetsDropped(const TArray<UObject*>& Assets, const int32& ElemIdx, const FVector& RayCastPos, const FModifierKeysState& ModifierKeys)
{
	bool bSelectionChanged = false;
	if (!SelectedIndices.Contains(ElemIdx))
	{
		SelectedIndices = TArray<int32>{ ElemIdx };
		bSelectionChanged = true;
	}

	FString AssetsStr;
	for (UObject* Asset : Assets)
	{
		FString ObjectStr;
		UHoudiniParameterInput::GetObjectInfo(Asset, ObjectStr);
		if (ObjectStr.IsEmpty())
			ObjectStr = FHoudiniEngineUtils::GetAssetReference(Asset);
		else
			ObjectStr = FHoudiniEngineUtils::GetAssetReference(Asset) + TEXT(";") + ObjectStr;
		AssetsStr += ObjectStr + TEXT("|");
	}
	AssetsStr.RemoveFromEnd(TEXT("|"));

	FString ActionStr = DELTAINFO_ACTION_DROP;
	AppendModifierKeysToActionString(ModifierKeys, ActionStr);

	DeltaInfo = FString::Printf(TEXT("%s/%s:%f,%f,%f/%s/%s"), *Name,
		((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM),
		RayCastPos.X * POSITION_SCALE_TO_HOUDINI, RayCastPos.Z * POSITION_SCALE_TO_HOUDINI, RayCastPos.Y * POSITION_SCALE_TO_HOUDINI,
		*ActionStr, *AssetsStr);

	ReDeltaInfo.Empty();

	TriggerParentNodeToCook();
	
	return bSelectionChanged;
}

void UHoudiniEditableGeometry::SelectAll()
{
	if ((SelectedClass == EHoudiniAttributeOwner::Point) || (SelectedClass == EHoudiniAttributeOwner::Prim))
	{
		const int32 NumElems = (SelectedClass == EHoudiniAttributeOwner::Point) ? NumPoints() : NumPrims();
		SelectedIndices.SetNumUninitialized(NumElems);
		for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
			SelectedIndices[ElemIdx] = ElemIdx;
	}
}

bool UHoudiniEditableGeometry::SelectIdenticals()
{
	if (!IsGeometrySelected())
		return true;

	const bool bIsPointSelected = SelectedClass == EHoudiniAttributeOwner::Point;
	if (const FHoudiniAttributeParameterSet* AttribParmSet = GetParentNode()->GetGroupParameterSetMap().Find(Name))
	{
		const FString& IdentifierName = bIsPointSelected ? AttribParmSet->PointIdentifierName : AttribParmSet->PrimIdentifierName;
		if (IdentifierName.IsEmpty())
			return false;

		const TArray<UHoudiniParameterAttribute*>& ParmAttribs = bIsPointSelected ? PointParmAttribs : PrimParmAttribs;
		for (const UHoudiniParameterAttribute* ParmAttrib : ParmAttribs)
		{
			if (!ParmAttrib->IsParameterHolder() && (ParmAttrib->GetParameterName() == IdentifierName))
			{
				ParmAttrib->SelectIdenticals(SelectedIndices);
				return true;
			}
		}
	}

	return false;
}

void UHoudiniEditableGeometry::StartTransforming(const FVector& Pivot)
{
#if WITH_EDITOR
	ReDeltaInfo = Name / ((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
		DELTAINFO_ACTION_TRANSFORM / FHoudiniEngineUtils::ConvertXformToString(FTransform::Identity, Pivot);

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryTransformTransaction", "Houdini Editable Geometry: Transformed"), this, true);

	Modify();
#endif
}

void UHoudiniEditableGeometry::EndTransforming(const FTransform& Transform, const FVector& Pivot)
{
	if ((SelectedClass == EHoudiniAttributeOwner::Point) || (SelectedClass == EHoudiniAttributeOwner::Prim))
	{
		DeltaInfo = Name / ((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
			DELTAINFO_ACTION_TRANSFORM / FHoudiniEngineUtils::ConvertXformToString(Transform, Pivot);
		ReDeltaInfo = Name / ((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
			DELTAINFO_ACTION_TRANSFORM / FHoudiniEngineUtils::ConvertXformToString(FTransform::Identity, Pivot);
		UnDeltaInfo = Name / ((SelectedClass == EHoudiniAttributeOwner::Point) ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
			DELTAINFO_ACTION_TRANSFORM / FHoudiniEngineUtils::ConvertXformToString(Transform.Inverse(), Pivot);

		TriggerParentNodeToCook();
	}
}

const FString& UHoudiniEditableGeometry::ParseEditData(TConstArrayView<const UHoudiniEditableGeometry*> EditGeos,
	size_t& OutNumVertices, size_t& OutNumPoints, size_t& OutNumPrims,
	EHoudiniAttributeOwner& OutChangedClass,
	TArray<UHoudiniParameterAttribute*>& OutPointAttribs,
	TArray<UHoudiniParameterAttribute*>& OutPrimAttribs)
{
	// Find the NewestEditGeo as standard
	const UHoudiniEditableGeometry* NewestEditGeo = EditGeos[0];
	
	if (EditGeos.Num() == 1)
	{
		OutNumVertices = NewestEditGeo->NumVertices();
		OutNumPoints = NewestEditGeo->NumPoints();
		OutNumPrims = NewestEditGeo->NumPrims();
		OutChangedClass = NewestEditGeo->IsGeometrySelected() ? NewestEditGeo->GetSelectedClass() : EHoudiniAttributeOwner::Invalid;
		OutPointAttribs = NewestEditGeo->PointParmAttribs;
		OutPrimAttribs = NewestEditGeo->PrimParmAttribs;

		return NewestEditGeo->GetGroupName();
	}
	
	uint64 NewestUpdateTime = NewestEditGeo->GetUpdateCycles();
	for (const UHoudiniEditableGeometry* EditGeo : EditGeos)
	{
		if (EditGeo->GetUpdateCycles() > NewestUpdateTime)
		{
			NewestUpdateTime = EditGeo->GetUpdateCycles();
			NewestEditGeo = EditGeo;
		}
	}

	// Use ParmAttribs from NewestEditGeo as the standard ParmAttribs, since some EditGeo maybe partial output thus out-of-date
	for (const UHoudiniParameterAttribute* ParmAttrib : NewestEditGeo->PointParmAttribs)
	{
		if (UHoudiniParameterAttribute* Attrib = ParmAttrib->DuplicateNew())
			OutPointAttribs.Add(Attrib);
	}

	for (const UHoudiniParameterAttribute* ParmAttrib : NewestEditGeo->PrimParmAttribs)
	{
		if (UHoudiniParameterAttribute* Attrib = ParmAttrib->DuplicateNew())
			OutPrimAttribs.Add(Attrib);
	}

	// Upload all EditDatas
	OutNumVertices = 0;
	OutNumPoints = 0;
	OutNumPrims = 0;
	OutChangedClass = EHoudiniAttributeOwner::Invalid;
	for (const UHoudiniEditableGeometry* EditGeo : EditGeos)
	{
		OutNumVertices += EditGeo->NumVertices();
		OutNumPoints += EditGeo->NumPoints();
		OutNumPrims += EditGeo->NumPrims();


		if (!EditGeo->DeltaInfo.IsEmpty())  // Means this EditGeo has changed, then we could set global edit data to this
		{
			// TODO: append delta info value part, if multi geo select is allow
			if (!EditGeo->SelectedIndices.IsEmpty())  // Maybe selection removed
				OutChangedClass = EditGeo->SelectedClass;
		}

		auto AppendParmAttribValuesLambda = [](const TArray<UHoudiniParameterAttribute*>& GlobalAttribs,
			const TArray<UHoudiniParameterAttribute*>& LocalAttribs, const int32& NumElems) -> void
			{
				for (UHoudiniParameterAttribute* GlobalAttrib : GlobalAttribs)
				{
					if (!LocalAttribs.ContainsByPredicate([GlobalAttrib](const UHoudiniParameterAttribute* LocalAttrib)
						{
							if (LocalAttrib->GetParameterName().Equals(GlobalAttrib->GetParameterName(), ESearchCase::CaseSensitive))
							{
								if (GlobalAttrib->Join(LocalAttrib))  // If not successfully joined, then we need SetNum() to match size
									return true;
							}

							return false;
						}))
						GlobalAttrib->SetNum(NumElems);  // If not successfully joined, then we need SetNum() to match size
				}
			};

		AppendParmAttribValuesLambda(OutPointAttribs, EditGeo->PointParmAttribs, OutNumPoints);
		AppendParmAttribValuesLambda(OutPrimAttribs, EditGeo->PrimParmAttribs, OutNumPrims);
	}

	return NewestEditGeo->GetGroupName();
}

#undef LOCTEXT_NAMESPACE
