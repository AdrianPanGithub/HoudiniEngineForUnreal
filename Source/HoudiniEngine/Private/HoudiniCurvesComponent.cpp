// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniCurvesComponent.h"

#include "EngineUtils.h"
#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniParameterAttribute.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

FVector FHoudiniCurve::Bezier(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const double& u)
{
	const float u2 = u * u;
	const float u3 = u2 * u;
	return P0 * (-u3 + 3 * u2 - 3 * u + 1) +
		P1 * (3 * u3 - 6 * u2 + 3 * u) +
		P2 * (-3 * u3 + 3 * u2) +
		P3 * u3;
}

FVector FHoudiniCurve::CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const double& u)
{
	return u * u * u * (-P0 + 3.0 * P1 - 3.0 * P2 + P3) * 0.5 +
		u * u * (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * 0.5 +
		u * (-P0 + P2) * 0.5 +
		P1;
}


const TArray<FString>& UHoudiniCurvesComponent::GetPointIntrinsicNames(const bool& bIsOutput)
{
	static const TArray<FString> IntrinsicNames{ TEXT(HAPI_ATTRIB_ROT), TEXT(HAPI_ATTRIB_SCALE) };
	static const TArray<FString> IntrinsicNames_Input{ TEXT(HAPI_ATTRIB_ROT), TEXT(HAPI_ATTRIB_SCALE), TEXT(HAPI_ATTRIB_ID) };
	return bIsOutput ? IntrinsicNames : IntrinsicNames_Input;
}

const TArray<FString>& UHoudiniCurvesComponent::GetPrimIntrinsicNames(const bool& bIsOutput)
{
	static const TArray<FString> IntrinsicNames{ TEXT(HAPI_CURVE_CLOSED), TEXT(HAPI_CURVE_TYPE) };
	static const TArray<FString> IntrinsicNames_Input{ TEXT(HAPI_CURVE_CLOSED), TEXT(HAPI_CURVE_TYPE), TEXT(HAPI_ATTRIB_ID) };
	return bIsOutput ? IntrinsicNames : IntrinsicNames_Input;
}

int32 UHoudiniCurvesComponent::NumVertices() const
{
	int32 NumVertices = 0;
	for (const FHoudiniCurve& Curve : Curves)
		NumVertices += Curve.PointIndices.Num();

	return NumVertices;
}

void UHoudiniCurvesComponent::InputUpdate(AHoudiniNode* Node, const FString& InGroupName,
	const bool& bInDefaultCurveClosed, const EHoudiniCurveType& InDefaultCurveType, const FColor& InDefaultCurveColor)
{
	UHoudiniEditableGeometry::Update(Node, InGroupName, GetPointIntrinsicNames(false), GetPrimIntrinsicNames(false));
	
	for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
		ParmAttrib->SetNum(Points.Num());

	for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
		ParmAttrib->SetNum(Curves.Num());

	bDefaultCurveClosed = bInDefaultCurveClosed;
	DefaultCurveType = InDefaultCurveType;
	DefaultCurveColor = InDefaultCurveColor;
	
	for (FHoudiniCurvePoint& Point : Points)
		Point.Color = DefaultCurveColor;
	
	for (FHoudiniCurve& Curve : Curves)
		Curve.Color = DefaultCurveColor;
}


void UHoudiniCurvesComponent::Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
	const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FRay& ClickRay, const FModifierKeysState& ModifierKeys)
{
	// DeltaInfo will record the hit position, so we should update ClickPosition
	if (SelectClass == EHoudiniAttributeOwner::Point)
		ClickPosition = GetComponentTransform().TransformPosition(Points[ElemIdx].Transform.GetLocation());
	else if (SelectClass == EHoudiniAttributeOwner::Prim && ClickRay.Direction != FVector::ZeroVector)  // Calculate the click position by ClickRay
	{
		const FHoudiniCurve& SelectedCurve = Curves[ElemIdx];
		if (SelectedCurve.NoNeedDisplay())
			return;

		if (SelectedCurve.DisplayPoints.IsEmpty())
			RefreshCurveDisplayPoints(ElemIdx);

		const TArray<FVector>& DisplayPoints = SelectedCurve.DisplayPoints;
		const FTransform& ComponentTransform = GetComponentTransform();
		const FVector TargetPos = ClickRay.PointAt(99999999.0);
		double MinDist = -1.0;
		FVector PrevDisplayPos = ComponentTransform.TransformPosition(DisplayPoints[0]);
		for (int32 DisplayIdx = 1; DisplayIdx < DisplayPoints.Num(); ++DisplayIdx)
		{
			const FVector CurrDisplayPos = ComponentTransform.TransformPosition(DisplayPoints[DisplayIdx]);
			FVector CurveMinPos, ViewMinPos;
			FMath::SegmentDistToSegment(PrevDisplayPos, CurrDisplayPos,
				ClickRay.Origin, TargetPos, CurveMinPos, ViewMinPos);
			PrevDisplayPos = CurrDisplayPos;

			const double Dist = (CurveMinPos - ViewMinPos).Length();
			if (Dist < MinDist || MinDist < 0.0)
			{
				MinDist = Dist;
				ClickPosition = CurveMinPos;
			}
		}
	}

	UHoudiniEditableGeometry::Select(SelectClass, ElemIdx, bDeselectPrevious, bDeselectIfSelected, ModifierKeys);
}

void UHoudiniCurvesComponent::ExpandSelection()
{
	if (!IsGeometrySelected())
		return;

	if (SelectIdenticals())
		return;

	// First, find point polys
	TArray<TArray<int32>> PointsPrims;
	PointsPrims.SetNum(Points.Num());
	for (int32 PrimIdx = 0; PrimIdx < Curves.Num(); ++PrimIdx)
	{
		for (const int32& PointIdx : Curves[PrimIdx].PointIndices)
			PointsPrims[PointIdx].Add(PrimIdx);
	}

	// Then find the adjacency
	const int32 NumSelected = SelectedIndices.Num();
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TSet<int32> FoundPrimIndices;
		for (int32 Iter = 0; Iter < Points.Num(); ++Iter)  // Avoid endless loop
		{
			const int32 NumFoundPolys = FoundPrimIndices.Num();
			for (const int32& SelectedPointIdx : SelectedIndices)
				FoundPrimIndices.Append(PointsPrims[SelectedPointIdx]);
			if (NumFoundPolys == FoundPrimIndices.Num())
				break;

			TSet<int32> SelectedPointIndices;
			for (const int32& FoundPrimIdx : FoundPrimIndices)
				SelectedPointIndices.Append(Curves[FoundPrimIdx].PointIndices);
			SelectedIndices = SelectedPointIndices.Array();
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> FoundPointIndices;
		for (int32 Iter = 0; Iter < Curves.Num(); ++Iter)  // Avoid endless loop
		{
			const int32 NumFoundPoints = FoundPointIndices.Num();
			for (const int32& SelectedPrimIdx : SelectedIndices)
				FoundPointIndices.Append(Curves[SelectedPrimIdx].PointIndices);
			if (NumFoundPoints == FoundPointIndices.Num())
				break;

			TSet<int32> SelectedPrimIndices;
			for (const int32& FoundPointIdx : FoundPointIndices)
				SelectedPrimIndices.Append(PointsPrims[FoundPointIdx]);
			SelectedIndices = SelectedPrimIndices.Array();
		}
	}

	if (SelectedIndices.Num() != NumSelected)
		SelectedIndices.Swap(SelectedIndices.Num() - 1, NumSelected - 1);
}

void UHoudiniCurvesComponent::FrustumSelect(const FConvexVolume& Frustum)
{
	const FTransform& ComponentTransform = GetComponentTransform();
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TArray<int32> SelectedPointIndices;
		for (int32 PointIdx = 0; PointIdx < Points.Num(); ++PointIdx)
		{
			if (Frustum.IntersectPoint(ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation())))
				SelectedPointIndices.Add(PointIdx);
		}
		SelectedIndices = SelectedPointIndices;
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TArray<int32> SelectedCurveIndices;
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
		{
			for (const FVector& DisplayPoint : Curves[CurveIdx].DisplayPoints)
			{
				if (Frustum.IntersectPoint(ComponentTransform.TransformPosition(DisplayPoint)))
				{
					SelectedCurveIndices.Add(CurveIdx);
					break;
				}
			}
		}
		SelectedIndices = SelectedCurveIndices;
	}
}

void UHoudiniCurvesComponent::SphereSelect(const FVector& Centroid, const float& Radius, const bool& bAppend)
{
	const FTransform ComponentTransform = GetComponentTransform();
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (bAppend)
		{
			const TSet<int32> SelectedIdxSet = TSet<int32>(SelectedIndices);
			for (int32 PointIdx = 0; PointIdx < Points.Num(); ++PointIdx)
			{
				if (!SelectedIdxSet.Contains(PointIdx) &&
					FVector::Distance(ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation()), Centroid) < Radius)
					SelectedIndices.Add(PointIdx);
			}
		}
		else
		{
			SelectedIndices.Empty();
			for (int32 PointIdx = 0; PointIdx < Points.Num(); ++PointIdx)
			{
				if (FVector::Distance(ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation()), Centroid) < Radius)
					SelectedIndices.Add(PointIdx);
			}
		}

		if (!SelectedIndices.IsEmpty())
			ClickPosition = Centroid;
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		if (bAppend)
		{
			TSet<int32> SelectedIdxSet = TSet<int32>(SelectedIndices);
			for (int32 PrimIdx = 0; PrimIdx < Curves.Num(); ++PrimIdx)
			{
				if (SelectedIdxSet.Contains(PrimIdx))
					continue;

				for (const int32& PointIdx : Curves[PrimIdx].PointIndices)
				{
					if (FVector::Distance(ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation()), Centroid) < Radius)
					{
						SelectedIndices.Add(PrimIdx);
						break;
					}
				}
			}
		}
		else
		{
			SelectedIndices.Empty();
			for (int32 PrimIdx = 0; PrimIdx < Curves.Num(); ++PrimIdx)
			{
				for (const int32& PointIdx : Curves[PrimIdx].PointIndices)
				{
					if (FVector::Distance(ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation()), Centroid) < Radius)
					{
						SelectedIndices.Add(PrimIdx);
						break;
					}
				}
			}
		}

		if (!SelectedIndices.IsEmpty())
			ClickPosition = Centroid;
	}
}

FTransform UHoudiniCurvesComponent::GetSelectionPivot() const
{
	const FTransform& ComponentTransform = GetComponentTransform();
	if (SelectedIndices.IsEmpty())
		return ComponentTransform;

	FVector Location = FVector::ZeroVector;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (SelectedIndices.Num() == 1)
		{
			FTransform PointTransform = Points[SelectedIndices[0]].Transform;
			PointTransform.Accumulate(ComponentTransform);
			return PointTransform;
		}

		for (const int32& SelectedPointIdx : SelectedIndices)
			Location += Points[SelectedPointIdx].Transform.GetLocation();
		Location /= SelectedIndices.Num();
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> SelectedPointIndices;
		for (const int32 SelectedCurveIdx : SelectedIndices)
			SelectedPointIndices.Append(Curves[SelectedCurveIdx].PointIndices);

		if (SelectedPointIndices.IsEmpty())  // When first AddCurve, the new curve has No vertices, so ensure this
			return ComponentTransform;

		for (const int32& SelectedPointIdx : SelectedPointIndices)
			Location += Points[SelectedPointIdx].Transform.GetLocation();
		Location /= SelectedPointIndices.Num();
	}

	return FTransform(ComponentTransform.GetRotation(),
		ComponentTransform.TransformPosition(Location), ComponentTransform.GetScale3D());
}


void UHoudiniCurvesComponent::GetPointsBounds(const TArray<int32>& PointIndices, FVector& OutMin, FVector& OutMax) const
{
	const FTransform& ComponentTransform = GetComponentTransform();

	const FVector FirstPosition = ComponentTransform.TransformPosition(Points[PointIndices[0]].Transform.GetLocation());
	OutMin = FirstPosition;
	OutMax = OutMin;
	for (const int32& PointIdx : PointIndices)
	{
		const FVector Position = ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation());
		OutMin = FVector::Min(OutMin, Position);
		OutMax = FVector::Max(OutMax, Position);
	}
}

bool UHoudiniCurvesComponent::GetSelectionBoundingBox(FBox& OutBoundingBox) const
{
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (SelectedIndices.Num() <= 1)
			return false;

		GetPointsBounds(SelectedIndices, OutBoundingBox.Min, OutBoundingBox.Max);
		return true;
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TArray<int32> SelectedPointIndices;
		for (const int32& SelectedCurveIdx : SelectedIndices)
			SelectedPointIndices.Append(Curves[SelectedCurveIdx].PointIndices);

		if (SelectedPointIndices.Num() <= 1)
			return false;

		GetPointsBounds(SelectedPointIndices, OutBoundingBox.Min, OutBoundingBox.Max);
		return true;
	}

	return false;
}

TArray<int32> UHoudiniCurvesComponent::GetSelectedCurveIndices() const
{
	if (SelectedClass == EHoudiniAttributeOwner::Prim)
		return SelectedIndices;
	else if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TArray<int32> CurveIndices;
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
		{
			for (const int32& PointIdx : Curves[CurveIdx].PointIndices)
			{
				if (SelectedIndices.Contains(PointIdx))
				{
					CurveIndices.Add(CurveIdx);
					break;
				}
			}
		}
		return CurveIndices;
	}

	return TArray<int32>{};
}

bool UHoudiniCurvesComponent::GetInsertPosition(const FConvexVolume& Frustum, const FRay& MouseRay,
	FVector& OutInsertPos, int32& OutCurveIdx, int32& OutDisplayIdx) const
{
	double MinDist = -1.0;
	const TArray<int32> SelectedCurveIndices = GetSelectedCurveIndices();

	const FVector TargetPos = MouseRay.PointAt(99999999.0);
	const FTransform& ComponentTransform = GetComponentTransform();
	for (const int32& CurveIdx : SelectedCurveIndices)
	{
		const FHoudiniCurve& Curve = Curves[CurveIdx];
		if (Curve.NoNeedDisplay())
			continue;

		if (Curve.DisplayPoints.IsEmpty())
			const_cast<UHoudiniCurvesComponent*>(this)->RefreshCurveDisplayPoints(CurveIdx);

		const TArray<FVector>& DisplayPoints = Curve.DisplayPoints;
		FVector PrevPos = ComponentTransform.TransformPosition(DisplayPoints[0]);
		for (int32 DisplayIdx = 1; DisplayIdx < DisplayPoints.Num(); ++DisplayIdx)
		{
			const FVector CurrPos = ComponentTransform.TransformPosition(DisplayPoints[DisplayIdx]);
			FVector CurveMinPos, ViewMinPos;
			FMath::SegmentDistToSegment(PrevPos, CurrPos,
				MouseRay.Origin, TargetPos, CurveMinPos, ViewMinPos);
			PrevPos = CurrPos;

			const double Dist = (CurveMinPos - ViewMinPos).Length();
			if (((MinDist > Dist) || (MinDist < 0.0)) && Frustum.IntersectPoint(CurveMinPos))
			{
				MinDist = Dist;
				OutInsertPos = CurveMinPos;
				OutCurveIdx = CurveIdx;
				OutDisplayIdx = DisplayIdx - 1;
			}
		}
	}

	return (MinDist >= 0.0);
}

void UHoudiniCurvesComponent::InsertPoint(const FConvexVolume& Frustum, const FRay& MouseRay)
{
	FVector InsertPos;
	int32 CurveIdx, DisplayIdx;
	if (GetInsertPosition(Frustum, MouseRay, InsertPos, CurveIdx, DisplayIdx))
	{
		FHoudiniCurvePoint NewPoint(HOUDINI_RANDOM_ID);
		NewPoint.Transform.SetLocation(GetComponentTransform().InverseTransformPosition(InsertPos));

		ReDeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_REMOVE;

		FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
			LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

		Modify();

		UnDeltaInfo = ReDeltaInfo;
		DeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_APPEND;
		ReDeltaInfo = DeltaInfo;

		const int32 NewPointIdx = Points.Add(NewPoint);
		
		FHoudiniCurve& Curve = Curves[CurveIdx];
		int32 InsertIdx = -1;
		if ((Curve.Type == EHoudiniCurveType::Polygon) || (Curve.DisplayPoints.Num() == 2))
			InsertIdx = DisplayIdx;
		else
		{
			InsertIdx = FHoudiniEngineUtils::BinarySearch(Curve.DisplayIndices, DisplayIdx);
			if (InsertIdx < 0)
				InsertIdx = -InsertIdx - 2;
			if (InsertIdx < 0)
				InsertIdx = Curve.PointIndices.Num() - 1;
		}
		const int32 TemplatePointIdx = Curve.PointIndices[InsertIdx];
		if (InsertIdx == (Curve.PointIndices.Num() - 1))
			Curve.PointIndices.Add(NewPointIdx);
		else
			Curve.PointIndices.Insert(NewPointIdx, InsertIdx + 1);

		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
		{
			ParmAttrib->Modify();
			ParmAttrib->DuplicateAppend(TArray<int32>{ TemplatePointIdx });
		}

		Curve.DisplayPoints.Empty();  // Will refresh later
		SelectedClass = EHoudiniAttributeOwner::Point;
		SelectedIndices = TArray<int32>{ NewPointIdx };

		TriggerParentNodeToCook();
	}
}

bool UHoudiniCurvesComponent::IsSelectionSplittable() const
{
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (const FHoudiniCurve& Curve : Curves)
		{
			const TArray<int32>& PointIndices = Curve.PointIndices;
			const int32 NumVtcs = PointIndices.Num() - (Curve.bClosed ? 0 : 1);
			for (int32 VtxIdx = 1; VtxIdx < NumVtcs; ++VtxIdx)
			{
				if (SelectedIndices.Contains(PointIndices[VtxIdx]))
					return true;
			}
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		return true;  // TODO: shall we judge this precisely?
	}
	return false;
}

bool UHoudiniCurvesComponent::IsSelectionJoinable() const
{
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		return (SelectedIndices.Num() >= 2);
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		return (SelectedIndices.Num() >= 2);
	}
	return false;
}

bool UHoudiniCurvesComponent::SplitSelection()
{
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TArray<int32> SplitCurveIndices;
		TArray<TArray<int32>> SplitCurveVtxIndices;
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
		{
			FHoudiniCurve& Curve = Curves[CurveIdx];

			TArray<int32> SplitVtxIndices;
			const bool& bCurveClosed = Curve.bClosed;
			const TArray<int32>& PointIndices = Curve.PointIndices;
			for (int32 VtxIdx = 1; VtxIdx < PointIndices.Num() - (bCurveClosed ? 0 : 1); ++VtxIdx)
			{
				if (SelectedIndices.Contains(PointIndices[VtxIdx]))
					SplitVtxIndices.Add(VtxIdx);
			}
			if (SplitVtxIndices.IsEmpty())
				continue;
			
			Curve.DisplayPoints.Empty();
			SplitCurveIndices.Add(CurveIdx);
			SplitCurveVtxIndices.Add(SplitVtxIndices);
		}

		if (SplitCurveIndices.IsEmpty())
			return false;

		// When Undo, this action will perform as join curves, so we should set UnDeltaInfo like just join curves
		SelectedClass = EHoudiniAttributeOwner::Prim;
		const TArray<int32> SelectedPointIndices = SelectedIndices;
		SelectedIndices = SplitCurveIndices;
		ReDeltaInfo = Name / DELTAINFO_CLASS_PRIM / DELTAINFO_ACTION_JOIN;

		FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
			LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

		Modify();
		for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
			ParmAttrib->Modify();

		SelectedClass = EHoudiniAttributeOwner::Point;
		SelectedIndices = SelectedPointIndices;

		UnDeltaInfo = ReDeltaInfo;
		DeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_SPLIT;
		ReDeltaInfo = DeltaInfo;

		for (int32 SplitArrayIdx = 0; SplitArrayIdx < SplitCurveIndices.Num(); ++SplitArrayIdx)
		{
			const int32& CurveIdx = SplitCurveIndices[SplitArrayIdx];
			const FHoudiniCurve& Curve = Curves[CurveIdx];
			const TArray<int32>& SplitVtxIndices = SplitCurveVtxIndices[SplitArrayIdx];

			const TArray<int32>& PointIndices = Curve.PointIndices;
			const bool& bCurveClosed = Curve.bClosed;
			for (int32 SplitIdx = 0; SplitIdx < SplitVtxIndices.Num(); ++SplitIdx)
			{
				const bool bIsLast = (SplitIdx == SplitVtxIndices.Num() - 1);
				const int32& StartIdx = SplitVtxIndices[SplitIdx];
				const int32 EndIdx = bIsLast ? (PointIndices.Num() - 1) : SplitVtxIndices[SplitIdx + 1];
				FHoudiniCurve NewCurve(HOUDINI_RANDOM_ID);
				NewCurve.bClosed = false;
				NewCurve.Type = Curve.Type;
				NewCurve.Color = Curve.Color;
				NewCurve.PointIndices.Append(PointIndices.GetData() + StartIdx, EndIdx - StartIdx + 1);
				if (bCurveClosed && bIsLast)
					NewCurve.PointIndices.Add(PointIndices[0]);
				Curves.Add(NewCurve);

				for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
					ParmAttrib->DuplicateAppend(TArray<int32>{ CurveIdx });
			}

			Curves[CurveIdx].PointIndices.SetNum(SplitVtxIndices[0] + 1);
			Curves[CurveIdx].bClosed = false;
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TMap<int32, int32> SplitPointCurveIndices;
		for (const int32& SelectedCurveIdx : SelectedIndices)
		{
			const TArray<int32>& PointIndices = Curves[SelectedCurveIdx].PointIndices;
			if (!PointIndices.IsEmpty())
			{
				if (SplitPointCurveIndices.Contains(PointIndices[0]))
					SplitPointCurveIndices.Remove(PointIndices[0]);
				else
					SplitPointCurveIndices.Add(PointIndices[0], SelectedCurveIdx);
				
				if (PointIndices[0] != PointIndices.Last())
				{
					if (SplitPointCurveIndices.Contains(PointIndices.Last()))
						SplitPointCurveIndices.Remove(PointIndices.Last());
					else
						SplitPointCurveIndices.Add(PointIndices.Last(), SelectedCurveIdx);
				}
			}
		}

		// If the split point is not connected to another curve, then we could NOT split it
		{
			TSet<int32> SplittablePoints;
			for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
			{
				const TArray<int32>& PointIndices = Curves[CurveIdx].PointIndices;
				for (const int32& PointIdx : PointIndices)
				{
					if (const int32* FoundCurveIdxPtr = SplitPointCurveIndices.Find(PointIdx))
					{
						if (*FoundCurveIdxPtr != CurveIdx)
							SplittablePoints.Add(PointIdx);
					}
				}
			}

			for (TMap<int32, int32>::TIterator SplitIter(SplitPointCurveIndices); SplitIter; ++SplitIter)
			{
				if (!SplittablePoints.Contains(SplitIter->Key))
					SplitIter.RemoveCurrent();
			}
		}

		if (SplitPointCurveIndices.IsEmpty())
			return false;

		// When Undo, this action will perform as fuse points, so we should set UnDeltaInfo like just fuse points
		SelectedClass = EHoudiniAttributeOwner::Point;
		const TArray<int32> SelectedCurveIndices = SelectedIndices;
		SplitPointCurveIndices.GetKeys(SelectedIndices);
		ReDeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_JOIN;

		FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
			LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

		Modify();
		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
			ParmAttrib->Modify();

		SelectedClass = EHoudiniAttributeOwner::Prim;
		SelectedIndices = SelectedCurveIndices;

		UnDeltaInfo = ReDeltaInfo;
		DeltaInfo = Name / DELTAINFO_CLASS_PRIM / DELTAINFO_ACTION_SPLIT;
		ReDeltaInfo = DeltaInfo;

		for (const auto& SplitPtCrvIdx : SplitPointCurveIndices)
		{
			TArray<int32>& PointIndices = Curves[SplitPtCrvIdx.Value].PointIndices;
			FHoudiniCurvePoint Point = Points[SplitPtCrvIdx.Key];
			Point.Id = HOUDINI_RANDOM_ID;
			const int32 NewPointIdx = Points.Add(Point);
			if (PointIndices[0] == SplitPtCrvIdx.Key)
				PointIndices[0] = NewPointIdx;
			if (PointIndices.Last() == SplitPtCrvIdx.Key)
				PointIndices.Last() = NewPointIdx;

			for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
				ParmAttrib->DuplicateAppend(TArray<int32>{ SplitPtCrvIdx.Key });
		}
	}

	TriggerParentNodeToCook();
	return true;
}

bool UHoudiniCurvesComponent::JoinSelection()
{
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (SelectedIndices.Num() < 2)
			return false;

		ReDeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_SPLIT;

		FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
			LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

		Modify();
		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
			ParmAttrib->Modify();

		DeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_JOIN;
		UnDeltaInfo = ReDeltaInfo;
		ReDeltaInfo = DeltaInfo;

		FVector AvgPos = FVector::ZeroVector;
		for (const int32& SelectedPointIdx : SelectedIndices)
			AvgPos += Points[SelectedPointIdx].Transform.GetLocation();
		AvgPos /= SelectedIndices.Num();

		int32 LastSelectedPointIdx = SelectedIndices.Pop();
		SelectedIndices.Sort();
		{
			const int32 FoundIdx = FHoudiniEngineUtils::BinarySearch(SelectedIndices, LastSelectedPointIdx);  // SelectedIndices should be unique
			LastSelectedPointIdx += FoundIdx + 1;
		}
		for (int32 SelectedIdx = SelectedIndices.Num() - 1; SelectedIdx >= 0; --SelectedIdx)
			Points.RemoveAt(SelectedIndices[SelectedIdx]);

		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
			ParmAttrib->RemoveIndices(SelectedIndices);

		for (FHoudiniCurve& Curve : Curves)
		{
			TArray<int32>& PointIndices = Curve.PointIndices;
			bool bPrevfused = false;
			for (int32 VtxIdx = PointIndices.Num() - 1; VtxIdx >= 0; --VtxIdx)
			{
				int32& PointIdx = PointIndices[VtxIdx];
				const int32 FoundIdx = FHoudiniEngineUtils::BinarySearch(SelectedIndices, PointIdx);
				if (FoundIdx < 0)
					PointIdx += FoundIdx + 1;
				else if (bPrevfused)  // If previous vertex is also fused, then we just remove this vertex
					PointIndices.RemoveAt(VtxIdx);
				else
					PointIdx = LastSelectedPointIdx;

				bPrevfused = (FoundIdx >= 0);
				if (bPrevfused || LastSelectedPointIdx == PointIdx)
					Curve.DisplayPoints.Empty();
			}
		}
		
		Points[LastSelectedPointIdx].Transform.SetLocation(AvgPos);
		SelectedIndices = TArray<int32>{ LastSelectedPointIdx };
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> JoinPointIndices;
		TMap<int32, TArray<int32>> MainCurveIndices;
		
		TSet<int32> JoinCurveIndices;  // Only used for avoid curve joined twice
		for (const int32& SelectedCurveIdx : SelectedIndices)
		{
			bool bJoined = false;
			JoinCurveIndices.FindOrAdd(SelectedCurveIdx, &bJoined);
			if (bJoined)
				continue;

			int32 StartPtIdx = -1;
			int32 EndPtIdx = -1;
			{
				const TArray<int32>& PointIndices = Curves[SelectedCurveIdx].PointIndices;
				if (PointIndices.IsEmpty())
					continue;

				StartPtIdx = PointIndices[0];
				EndPtIdx = PointIndices.Last();
			}
			
			for (const int32& JoinCurveIdx : SelectedIndices)
			{
				if (JoinCurveIndices.Contains(JoinCurveIdx))
					continue;

				const TArray<int32>& PointIndices = Curves[SelectedCurveIdx].PointIndices;
				if (PointIndices.IsEmpty())
					continue;

				if (StartPtIdx == PointIndices[0])
				{
					JoinPointIndices.Add(StartPtIdx);
					StartPtIdx = PointIndices.Last();
					JoinCurveIndices.Add(JoinCurveIdx);
					MainCurveIndices.FindOrAdd(SelectedCurveIdx).Add(JoinCurveIdx);
				}
				else if (EndPtIdx == PointIndices[0])
				{
					JoinPointIndices.Add(EndPtIdx);
					EndPtIdx = PointIndices.Last();
					JoinCurveIndices.Add(JoinCurveIdx);
					MainCurveIndices.FindOrAdd(SelectedCurveIdx).Add(JoinCurveIdx);
				}
				else if (StartPtIdx == PointIndices.Last())
				{
					JoinPointIndices.Add(StartPtIdx);
					StartPtIdx = PointIndices[0];
					JoinCurveIndices.Add(JoinCurveIdx);
					MainCurveIndices.FindOrAdd(SelectedCurveIdx).Add(JoinCurveIdx);
				}
				else if (EndPtIdx == PointIndices.Last())
				{
					JoinPointIndices.Add(EndPtIdx);
					EndPtIdx = PointIndices[0];
					JoinCurveIndices.Add(JoinCurveIdx);
					MainCurveIndices.FindOrAdd(SelectedCurveIdx).Add(JoinCurveIdx);
				}
			}
		}
		

		// When Undo, this action will perform as split points, so we should set UnDeltaInfo like just split points
		SelectedClass = EHoudiniAttributeOwner::Point;
		const TArray<int32> SelectedCurveIndices = SelectedIndices;
		SelectedIndices = JoinPointIndices.Array();
		ReDeltaInfo = Name / DELTAINFO_CLASS_POINT / DELTAINFO_ACTION_SPLIT;

		FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
			LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

		Modify();
		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
			ParmAttrib->Modify();

		SelectedClass = EHoudiniAttributeOwner::Prim;

		UnDeltaInfo = ReDeltaInfo;
		DeltaInfo = Name / DELTAINFO_CLASS_PRIM / DELTAINFO_ACTION_JOIN;
		ReDeltaInfo = DeltaInfo;

		for (const auto& CurveIndices : MainCurveIndices)
		{
			JoinCurveIndices.Remove(CurveIndices.Key);
			FHoudiniCurve& Curve = Curves[CurveIndices.Key];
			Curve.DisplayPoints.Empty();
			TArray<int32>& PointIndices = Curve.PointIndices;
			for (const int32& JoinCurveIdx : CurveIndices.Value)
			{
				TArray<int32>& ChildPointIndices = Curves[JoinCurveIdx].PointIndices;
				if (ChildPointIndices.Last() == PointIndices[0])
				{
					ChildPointIndices.Pop();
					ChildPointIndices.Append(PointIndices);
					PointIndices = ChildPointIndices;
				}
				else if (ChildPointIndices.Last() == PointIndices.Last())
				{
					for (int32 JoinVtxIdx = ChildPointIndices.Num() - 2; JoinVtxIdx >= 0; --JoinVtxIdx)
						PointIndices.Add(ChildPointIndices[JoinVtxIdx]);
				}
				else if (ChildPointIndices[0] == PointIndices[0])
				{
					TArray<int32> AllPointIndices = PointIndices;
					PointIndices.Empty();
					for (int32 JoinVtxIdx = ChildPointIndices.Num() - 1; JoinVtxIdx >= 1; --JoinVtxIdx)
						PointIndices.Add(ChildPointIndices[JoinVtxIdx]);
					PointIndices.Append(AllPointIndices);
				}
				else if (ChildPointIndices[0] == PointIndices.Last())
				{
					PointIndices.Pop();
					PointIndices.Append(ChildPointIndices);
				}
			}
		}

		TArray<int32> JoinedCurveIndices = JoinCurveIndices.Array();
		JoinedCurveIndices.Sort();
		for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
			ParmAttrib->RemoveIndices(JoinedCurveIndices);
		
		for (int32 JoinIdx = JoinedCurveIndices.Num() - 1; JoinIdx >= 0; --JoinIdx)
			Curves.RemoveAt(JoinedCurveIndices[JoinIdx]);

		SelectedIndices.Empty();
		for (const int32& SelectedCurveIdx : SelectedCurveIndices)
		{
			const int32 FoundJoinedIdx = FHoudiniEngineUtils::BinarySearch(JoinedCurveIndices, SelectedCurveIdx);
			if (FoundJoinedIdx < 0)
				SelectedIndices.Add(SelectedCurveIdx + FoundJoinedIdx + 1);
		}
	}

	TriggerParentNodeToCook();
	return true;
}


#if WITH_EDITOR
void UHoudiniCurvesComponent::PostEditUndo()
{
	if (DeltaInfo.EndsWith(DELTAINFO_ACTION_REMOVE))
	{
		SelectedIndices.Empty();
		if (!Curves.IsEmpty() && Curves.Last().PointIndices.IsEmpty())  // As Modify() call after AddNewCurve(), so here maybe an empty curve left, we need destroy this curve
		{
			Curves.Pop();
			const int32 NumCurves = Curves.Num();
			for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
				ParmAttrib->SetNum(NumCurves);

			if (SelectedClass == EHoudiniAttributeOwner::Prim)
				SelectedIndices.RemoveAll([NumCurves](const int32& SelectedCurveIdx) { return SelectedCurveIdx >= NumCurves; });
		}
	}

	Super::PostEditUndo();
}
#endif

void UHoudiniCurvesComponent::AddNewCurve()
{
	FHoudiniCurve& NewCurve = Curves.Add_GetRef(FHoudiniCurve(HOUDINI_RANDOM_ID));
	NewCurve.bClosed = bDefaultCurveClosed;
	NewCurve.Type = DefaultCurveType;
	for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
		ParmAttrib->SetNum(Curves.Num());
	SelectedClass = EHoudiniAttributeOwner::Prim;
	SelectedIndices = TArray<int32>{ Curves.Num() - 1 };
}

void UHoudiniCurvesComponent::BeginDrawing()
{
	const int32 CurrSelectedIdx = SelectedIndices.Last();
	if (SelectedClass == EHoudiniAttributeOwner::Prim)  // Append a new curve and dup attrib-values from selection
	{
		Curves.Add(FHoudiniCurve(HOUDINI_RANDOM_ID));
		const int32 NewCurveIdx = Curves.Num() - 1;

		Curves[NewCurveIdx].bClosed = Curves[CurrSelectedIdx].bClosed;
		Curves[NewCurveIdx].Type = Curves[CurrSelectedIdx].Type;
		for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
			ParmAttrib->DuplicateAppend(TArray<int32>{ CurrSelectedIdx });
		SelectedIndices = TArray<int32>{ NewCurveIdx };
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Point)
		SelectedIndices = TArray<int32>{ CurrSelectedIdx };
}

void UHoudiniCurvesComponent::AddPoint(const FHoudiniCurvePoint& NewPoint, bool bDoBranch)
{
	if (SelectedIndices.IsEmpty())  // if selection is empty, then we just create a nw curve
	{
		AddNewCurve();  // Will set Prim selected
	}

	if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		Points.Add(NewPoint);
		Curves[Curves.Num() - 1].PointIndices.Add(Points.Num() - 1);
		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
			ParmAttrib->SetNum(Points.Num());

		RefreshCurveDisplayPoints(Curves.Num() - 1);
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (bDoBranch)
		{
			const int32 LastSelectedPointIdx = SelectedIndices.Last();
			AddNewCurve();
			Points.Add(NewPoint);
			Curves[Curves.Num() - 1].PointIndices.Add(LastSelectedPointIdx);
			Curves[Curves.Num() - 1].PointIndices.Add(Points.Num() - 1);
			RefreshCurveDisplayPoints(Curves.Num() - 1);
		}
		else
		{
			const int32 InitInsertPointIdx = SelectedIndices[0];
			for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
			{
				FHoudiniCurve& Curve = Curves[CurveIdx];
				TArray<int32>& PointIndices = Curve.PointIndices;
				int32 InitInsertIdxPos = PointIndices.IndexOfByKey(InitInsertPointIdx);
				if (PointIndices.IsValidIndex(InitInsertIdxPos))
				{
					Points.Add(NewPoint);
					const int32 NewPointIdx = Points.Num() - 1;

					if (SelectedIndices.Num() >= 2)  // Means this is NOT the first point we insert, so we have already judged the direction, NOT to judge again
					{
						if (PointIndices[InitInsertIdxPos + 1] == SelectedIndices[1]) // Insert backward
						{
							if (SelectedIndices.Last() == PointIndices.Last())
								PointIndices.Add(NewPointIdx);
							else
								PointIndices.Insert(NewPointIdx, InitInsertIdxPos + SelectedIndices.Num());
						}
						else  // Insert forward
						{
							PointIndices.Insert(NewPointIdx, InitInsertIdxPos - SelectedIndices.Num() + 1);
						}
					}
					else  // This is the first point to insert, we need to judge whether to insert forward/backward
					{
						if (InitInsertIdxPos == 0)
							PointIndices.Insert(NewPointIdx, 0);
						else if (InitInsertIdxPos == PointIndices.Num() - 1)
							PointIndices.Add(NewPointIdx);
						else
						{
							const FVector NewPos = NewPoint.Transform.GetLocation();
							const FVector PrevPos = Points[PointIndices[InitInsertIdxPos - 1]].Transform.GetLocation();
							const FVector CurrPos = Points[InitInsertPointIdx].Transform.GetLocation();
							const FVector NextPos = Points[PointIndices[InitInsertIdxPos + 1]].Transform.GetLocation();
							FVector Dir2Prev = PrevPos - CurrPos; Dir2Prev.Normalize();
							FVector Dir2Next = NextPos - CurrPos; Dir2Next.Normalize();
							FVector Dir2New = NewPos - CurrPos; Dir2New.Normalize();
							if (Dir2New.Dot(Dir2Prev) > Dir2New.Dot(Dir2Next))  // Insert forward
								PointIndices.Insert(NewPointIdx, InitInsertIdxPos);
							else  // Insert backward
								PointIndices.Insert(NewPointIdx, InitInsertIdxPos + 1);
						}
					}
					
					SelectedIndices.Add(NewPointIdx);
					for (UHoudiniParameterAttribute* PointParmAttrib : PointParmAttribs)
						PointParmAttrib->DuplicateAppend(TArray<int32>{ SelectedIndices[0] });

					RefreshCurveDisplayPoints(CurveIdx);

					break;
				}
			}
		}
	}
}

#define RETURN_LAST_SELECT_CURVE_PROPERTY(PROPERTY) if (SelectedClass == EHoudiniAttributeOwner::Prim) \
		return Curves[SelectedIndices.Last()].PROPERTY; \
	else if (SelectedClass == EHoudiniAttributeOwner::Point) \
	{ \
		for (const FHoudiniCurve& Curve : Curves) \
		{ \
			if (Curve.PointIndices.Contains(SelectedIndices.Last())) \
				return Curve.PROPERTY; \
		} \
	}

bool UHoudiniCurvesComponent::GetSelectedCurveClosed() const
{
	RETURN_LAST_SELECT_CURVE_PROPERTY(bClosed);

	return bDefaultCurveClosed;
}

EHoudiniCurveType UHoudiniCurvesComponent::GetSelectedCurveType() const
{
	RETURN_LAST_SELECT_CURVE_PROPERTY(Type);

	return DefaultCurveType;
}

#define SET_LAST_SELECT_CURVES_PROPERTY(PROPERTY, NEW_PROPERTY_VALUE) if (SelectedClass == EHoudiniAttributeOwner::Prim)\
	{ \
		for (const int32& SelectedCurveIdx : SelectedIndices) \
		{ \
			FHoudiniCurve& Curve = Curves[SelectedCurveIdx]; \
			if (Curve.PROPERTY != NEW_PROPERTY_VALUE) \
			{ \
				Curve.PROPERTY = NEW_PROPERTY_VALUE; \
				RefreshCurveDisplayPoints(SelectedCurveIdx);\
			} \
		} \
	} \
	else if (SelectedClass == EHoudiniAttributeOwner::Point) \
	{ \
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx) \
		{ \
			FHoudiniCurve& Curve = Curves[CurveIdx]; \
			if (Curve.PROPERTY != NEW_PROPERTY_VALUE && Curve.PointIndices.Contains(SelectedIndices.Last())) \
			{ \
				Curve.PROPERTY = NEW_PROPERTY_VALUE; \
				RefreshCurveDisplayPoints(CurveIdx);\
			} \
		} \
	}

void UHoudiniCurvesComponent::SetSelectedCurvesClosed(const bool& bNewCurveClosed)
{
	TArray<int32> SelectedCurveIndices;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx) \
		{
			if (Curves[CurveIdx].PointIndices.Contains(SelectedIndices.Last()))
				SelectedCurveIndices.Add(CurveIdx);
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
		SelectedCurveIndices = SelectedIndices;

	if (SelectedCurveIndices.IsEmpty())
		return;

	SelectedCurveIndices.Sort();  // Sort it, so that houdini could get the corresponding changed values

	const FString NewValueStr = FString::FromInt(int32(bNewCurveClosed)) + TEXT("|");
	FString PrevValuesStr;
	FString CurrValuesStr;
	for (const int32& SelectedCurveIdx : SelectedCurveIndices)
	{
		PrevValuesStr += FString::FromInt(int32(Curves[SelectedCurveIdx].bClosed)) + TEXT("|");
		CurrValuesStr += NewValueStr;
	}
	PrevValuesStr.RemoveFromEnd(TEXT("|"));
	CurrValuesStr.RemoveFromEnd(TEXT("|"));
	
	const FString DeltaInfoPrefix = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
		TEXT(HAPI_CURVE_CLOSED) + TEXT("/");

	ReDeltaInfo = DeltaInfoPrefix + PrevValuesStr;

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

	Modify();

	for (const int32& SelectedCurveIdx : SelectedCurveIndices)
	{
		FHoudiniCurve& Curve = Curves[SelectedCurveIdx];
		if (Curve.bClosed != bNewCurveClosed)
		{
			Curve.bClosed = bNewCurveClosed;
			RefreshCurveDisplayPoints(SelectedCurveIdx);
		}
	}

	DeltaInfo = ReDeltaInfo;
	ReDeltaInfo = DeltaInfoPrefix + CurrValuesStr;
	UnDeltaInfo = ReDeltaInfo;

	TriggerParentNodeToCook();
}

void UHoudiniCurvesComponent::SetSelectedCurvesType(const EHoudiniCurveType& NewCurveType)
{
	TArray<int32> SelectedCurveIndices;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx) \
		{
			if (Curves[CurveIdx].PointIndices.Contains(SelectedIndices.Last()))
				SelectedCurveIndices.Add(CurveIdx);
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
		SelectedCurveIndices = SelectedIndices;

	if (SelectedCurveIndices.IsEmpty())
		return;

	SelectedCurveIndices.Sort();  // Sort it, so that houdini could get the corresponding changed values

	const FString NewValueStr = FString::FromInt(int32(NewCurveType)) + TEXT("|");
	FString PrevValuesStr;
	FString CurrValuesStr;
	for (const int32& SelectedCurveIdx : SelectedCurveIndices)
	{
		PrevValuesStr += FString::FromInt(int32(Curves[SelectedCurveIdx].Type)) + TEXT("|");
		CurrValuesStr += NewValueStr;
	}
	PrevValuesStr.RemoveFromEnd(TEXT("|"));
	CurrValuesStr.RemoveFromEnd(TEXT("|"));

	const FString DeltaInfoPrefix = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
		TEXT(HAPI_CURVE_TYPE) + TEXT("/");

	ReDeltaInfo = DeltaInfoPrefix + PrevValuesStr;

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

	Modify();

	for (const int32& SelectedCurveIdx : SelectedCurveIndices)
	{
		FHoudiniCurve& Curve = Curves[SelectedCurveIdx];
		if (Curve.Type != NewCurveType)
		{
			Curve.Type = NewCurveType;
			RefreshCurveDisplayPoints(SelectedCurveIdx);
		}
	}

	DeltaInfo = DeltaInfoPrefix + PrevValuesStr;
	ReDeltaInfo = DeltaInfoPrefix + CurrValuesStr;
	UnDeltaInfo = DeltaInfoPrefix + CurrValuesStr;

	TriggerParentNodeToCook();
}

void UHoudiniCurvesComponent::DuplicateSelection()
{
	if (!IsGeometrySelected())
		return;

#if WITH_EDITOR
	ReDeltaInfo = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
		DELTAINFO_ACTION_REMOVE;

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryElementsDuplicatedTransaction", "Houdini Editable Geometry: Elements Duplicated"), this, true);

	Modify();
#endif

	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TArray<int32> NewSelectedPointIndices;
		for (const int32& SelectedPointIdx : SelectedIndices)
		{
			FHoudiniCurvePoint NewPoint(HOUDINI_RANDOM_ID);
			NewPoint.Transform = Points[SelectedPointIdx].Transform;
			NewPoint.Color = Points[SelectedPointIdx].Color;
			Points.Add(NewPoint);
			const int32 NewPointIdx = Points.Num() - 1;
			NewSelectedPointIndices.Add(NewPointIdx);

			for (FHoudiniCurve& Curve : Curves)
			{
				const int32 VtxIdx = Curve.PointIndices.IndexOfByKey(SelectedPointIdx);
				if (Curve.PointIndices.IsValidIndex(VtxIdx))
				{
					if (VtxIdx >= Curve.PointIndices.Num() - 1)
						Curve.PointIndices.Add(NewPointIdx);
					else
						Curve.PointIndices.Insert(NewPointIdx, VtxIdx);
					break;
				}
			}
		}

		for (UHoudiniParameterAttribute* PointParmAttrib : PointParmAttribs)
		{
			PointParmAttrib->Modify();
			PointParmAttrib->DuplicateAppend(SelectedIndices);
		}

		SelectedIndices = NewSelectedPointIndices;
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		// Duplicate points
		TMap<int32, int32> DupPointIdxMap;  // Corresponding to idx after duplicated
		for (const int32& SelectedCurveIdx : SelectedIndices)
		{
			for (const int32& PointIdx : Curves[SelectedCurveIdx].PointIndices)
			{
				if (!DupPointIdxMap.Contains(PointIdx))  // We need to keep the idx for first-time.
					DupPointIdxMap.Add(PointIdx, Points.Num() + DupPointIdxMap.Num());
			}
		}

		TArray<int32> SrcPointIndicesToDup;
		for (const auto& DupPointIdx : DupPointIdxMap)
		{
			const FHoudiniCurvePoint SrcPoint = Points[DupPointIdx.Key];
			Points.Add(SrcPoint);
			SrcPointIndicesToDup.Add(DupPointIdx.Key);
		}

		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
		{
			ParmAttrib->Modify();
			ParmAttrib->DuplicateAppend(SrcPointIndicesToDup);
		}

		// Duplicate curves
		TArray<int32> NewSelectedCurveIndices;
		for (const int32& SelectedCurveIdx : SelectedIndices)
		{
			const FHoudiniCurve& SrcCurve = Curves[SelectedCurveIdx];
			FHoudiniCurve NewCurve(HOUDINI_RANDOM_ID);
			NewCurve.bClosed = SrcCurve.bClosed;
			NewCurve.Type = SrcCurve.Type;

			for (const int32& PointIdx : Curves[SelectedCurveIdx].PointIndices)
				NewCurve.PointIndices.Add(DupPointIdxMap[PointIdx]);

			NewCurve.Color = SrcCurve.Color;
			NewCurve.DisplayPoints = SrcCurve.DisplayPoints;

			Curves.Add(NewCurve);
			NewSelectedCurveIndices.Add(Curves.Num() - 1);
		}

		for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
		{
			ParmAttrib->Modify();
			ParmAttrib->DuplicateAppend(SelectedIndices);
		}

		SelectedIndices = NewSelectedCurveIndices;
	}
}

void UHoudiniCurvesComponent::RemoveSelection()
{
	if (!IsGeometrySelected())
		return;

#if WITH_EDITOR
	ReDeltaInfo = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
		DELTAINFO_ACTION_APPEND;

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryElementsRemovedTransaction", "Houdini Editable Geometry: Elements Removed"), this, true);

	Modify();
#endif

	SelectedIndices.Sort();

	FString DeltaInfoPrefix;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TArray<int32> ChangedCurveIndices;  // Will notify houdini changed curves that contains removed points
		TArray<int32> CurveIndicesToRemove;
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)  // CurveIndicesToRemove must be increased-order
		{
			TArray<int32>& CurvePointIndices = Curves[CurveIdx].PointIndices;
			const int32 NumRemovedVertices = CurvePointIndices.RemoveAll([&](int32& PointIdx)
				{
					const int32 SelectedIdxPos = FHoudiniEngineUtils::BinarySearch(SelectedIndices, PointIdx);
					if (SelectedIndices.IsValidIndex(SelectedIdxPos))
						return true;

					PointIdx += SelectedIdxPos + 1;  // the former points has been removed, so we need update the point index
					return false;
				});

			if (NumRemovedVertices >= 1)
			{
				if (CurvePointIndices.IsEmpty())
					CurveIndicesToRemove.Add(CurveIdx);
				else
				{
					ChangedCurveIndices.Add(CurveIdx - CurveIndicesToRemove.Num());  // Some curves may pending removed before, so minus the num curves to remove 
					Curves[CurveIdx].DisplayPoints.Empty();  // Do NOT RefreshDisplayPoints here, as points have NOT been removed yet
				}
			}
		}

		// Remove curves that has no vertices
		if (!CurveIndicesToRemove.IsEmpty())
		{
			for (int32 RemoveIdx = CurveIndicesToRemove.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
				Curves.RemoveAt(CurveIndicesToRemove[RemoveIdx]);

			for (UHoudiniParameterAttribute* PrimParmAttrib : PrimParmAttribs)
			{
				PrimParmAttrib->Modify();
				PrimParmAttrib->RemoveIndices(CurveIndicesToRemove);
			}
		}

		// Remove points
		for (int32 SelectedIdx = SelectedIndices.Num() - 1; SelectedIdx >= 0; --SelectedIdx)
			Points.RemoveAt(SelectedIndices[SelectedIdx]);

		for (UHoudiniParameterAttribute* PointParmAttrib : PointParmAttribs)
		{
			PointParmAttrib->Modify();
			PointParmAttrib->RemoveIndices(SelectedIndices);
		}

		if (ChangedCurveIndices.IsEmpty())
			ResetSelection();
		else  // We should mark the changed curves that contains removed points
		{
			SelectedClass = EHoudiniAttributeOwner::Prim;
			SelectedIndices = ChangedCurveIndices;
		}

		DeltaInfoPrefix += Name / DELTAINFO_CLASS_POINT + TEXT("/");
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> SelectedPointIndices;
		for (const int32& SelectedCurveIdx : SelectedIndices)
			SelectedPointIndices.Append(Curves[SelectedCurveIdx].PointIndices);

		TArray<int32> PointIndicesToRemove;
		for (const int32& SelectedPointIdx : SelectedPointIndices)
		{
			bool bUsedByNonSelectedCurve = false;
			for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
			{
				if (FHoudiniEngineUtils::BinarySearch(SelectedIndices, CurveIdx) >= 0)
					continue;

				if (Curves[CurveIdx].PointIndices.Contains(SelectedPointIdx))
				{
					bUsedByNonSelectedCurve = true;
					break;
				}
			}

			if (!bUsedByNonSelectedCurve)  // if this point is used by other non-selected curve, then we should NOT remove it.
				PointIndicesToRemove.Add(SelectedPointIdx);
		}

		// Remove curves
		PointIndicesToRemove.Sort();
		for (int32 SelectIdx = SelectedIndices.Num() - 1; SelectIdx >= 0; --SelectIdx)
			Curves.RemoveAt(SelectedIndices[SelectIdx]);

		for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
		{
			ParmAttrib->Modify();
			ParmAttrib->RemoveIndices(SelectedIndices);
		}

		// Revise curve-point-indices
		for (FHoudiniCurve& Curve : Curves)
		{
			for (int32& PointIdx : Curve.PointIndices)
				PointIdx += FHoudiniEngineUtils::BinarySearch(PointIndicesToRemove, PointIdx) + 1;
		}

		// Remove points
		for (int32 RemoveIdx = PointIndicesToRemove.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
			Points.RemoveAt(PointIndicesToRemove[RemoveIdx]);

		for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
		{
			ParmAttrib->Modify();
			ParmAttrib->RemoveIndices(PointIndicesToRemove);
		}

		ResetSelection();

		DeltaInfoPrefix += Name / DELTAINFO_CLASS_PRIM + TEXT("/");
	}

	if (!DeltaInfoPrefix.IsEmpty())
	{
		DeltaInfo = DeltaInfoPrefix + DELTAINFO_ACTION_REMOVE;
		ReDeltaInfo = DeltaInfo;
		UnDeltaInfo = DeltaInfoPrefix + DELTAINFO_ACTION_APPEND;

		TriggerParentNodeToCook();
	}
}

void UHoudiniCurvesComponent::ProjectSelection()
{
	if (!IsGeometrySelected())
		return;

	TArray<int32> SelectedPointIndices;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
		SelectedPointIndices = SelectedIndices;
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> UniquePointIndices;
		for (const int32& SelectedIdx : SelectedIndices)
		{
			UniquePointIndices.Append(Curves[SelectedIdx].PointIndices);
			Curves[SelectedIdx].DisplayPoints.Empty();  // Force to refresh display points
		}
		SelectedPointIndices = UniquePointIndices.Array();
	}
	if (SelectedPointIndices.IsEmpty())
		return;

	SelectedPointIndices.Sort();  // Sort it, so that houdini could get the corresponding changed values

	FString PrevPositionsStr;
	for (const int32& SelectedPointIdx : SelectedPointIndices)
	{
		const FVector Position = Points[SelectedPointIdx].Transform.GetLocation() * 0.01;
		PrevPositionsStr += FString::Printf(TEXT("%f,%f,%f|"), Position.X, Position.Z, Position.Y);
	}
	PrevPositionsStr.RemoveFromEnd(TEXT("|"));

	const FString DeltaInfoPrefix = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
		DELTAINFO_ACTION_TRANSFORM + TEXT("/");

	ReDeltaInfo = DeltaInfoPrefix + PrevPositionsStr;

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

	Modify();

	const UWorld* World = GetWorld();
	FCollisionQueryParams TraceParams = FCollisionQueryParams::DefaultQueryParam;
	for (TActorIterator<AActor> ActorIter(GEditor->GetEditorWorldContext().World()); ActorIter; ++ActorIter)
	{
		const AActor* Actor = *ActorIter;
		if (IsValid(Actor) && Actor->IsHiddenEd())
			TraceParams.AddIgnoredActor(Actor);
	}

	const FTransform ComponentTransform = GetComponentTransform();
	FString CurrPositionsStr;
	for (const int32& SelectedPointIdx : SelectedPointIndices)
	{
		FVector Position = ComponentTransform.TransformPosition(Points[SelectedPointIdx].Transform.GetLocation());

		FHitResult Hit(ForceInit);  // Ray to downward first
		World->LineTraceSingleByChannel(Hit, Position, FVector(Position.X, Position.Y, -102400.0), ECC_Visibility, TraceParams);

		AActor* CollideActor = Hit.GetActor();
		if (CollideActor)
		{
			Position.Z = Hit.Location.Z;
			Points[SelectedPointIdx].CollisionActorName = CollideActor->GetFName();
			Points[SelectedPointIdx].CollisionNormal = Hit.Normal;
		}
		else  // Then ray to upward
		{
			Hit = FHitResult(ForceInit);
			World->LineTraceSingleByChannel(Hit, Position, FVector(Position.X, Position.Y, 102400.0), ECC_Visibility, TraceParams);
			CollideActor = Hit.GetActor();
			if (CollideActor)
			{
				Position.Z = Hit.Location.Z;
				Points[SelectedPointIdx].CollisionActorName = CollideActor->GetFName();
				Points[SelectedPointIdx].CollisionNormal = Hit.Normal;
			}
			else
				Position.Z = 0.0;
		}

		Position = ComponentTransform.InverseTransformPosition(Position);
		Points[SelectedPointIdx].Transform.SetLocation(Position);
		Position *= 0.01;
		CurrPositionsStr += FString::Printf(TEXT("%f,%f,%f|"), Position.X, Position.Z, Position.Y);
	}
	CurrPositionsStr.RemoveFromEnd(TEXT("|"));
	
	DeltaInfo = DeltaInfoPrefix + PrevPositionsStr;
	ReDeltaInfo = DeltaInfoPrefix + CurrPositionsStr;
	UnDeltaInfo = DeltaInfoPrefix + CurrPositionsStr;

	// Finally, trigger curves to refresh display points
	for (FHoudiniCurve& Curve : Curves)
	{
		if (Curve.NoNeedDisplay() || Curve.DisplayPoints.IsEmpty())
			continue;

		for (const int32& PointIdx : Curve.PointIndices)
		{
			if (FHoudiniEngineUtils::BinarySearch(SelectedPointIndices, PointIdx) >= 0)
			{
				Curve.DisplayPoints.Empty();
				break;
			}
		}
	}

	TriggerParentNodeToCook();
}

void UHoudiniCurvesComponent::EndDrawing()
{
	if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		if (Curves.Last().PointIndices.IsEmpty())  // There are no point added, We should silently remove it.
		{
			Curves.Pop();
			const int32 NumCurves = Curves.Num();
			for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
				ParmAttrib->SetNum(NumCurves);

			if (SelectedClass == EHoudiniAttributeOwner::Prim)
				SelectedIndices.RemoveAll([NumCurves](const int32& SelectedCurveIdx) { return SelectedCurveIdx >= NumCurves; });

			return;  // No need for set DeltaInfo
		}
	}
	
	if (SelectedClass == EHoudiniAttributeOwner::Point || SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		DeltaInfo = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
			DELTAINFO_ACTION_APPEND;
		ReDeltaInfo = DeltaInfo;
		UnDeltaInfo = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) /
			DELTAINFO_ACTION_REMOVE;

		TriggerParentNodeToCook();
	}
}

#define TRANSFORM_POINTS(TRANSFORM_POINT) const FTransform& ComponentTransform = GetComponentTransform();\
	bool bMultiPointsSelected = false;\
	if (SelectedClass == EHoudiniAttributeOwner::Point)\
	{\
		bMultiPointsSelected = SelectedIndices.Num() >= 2;\
		for (const int32& SelectedPointIdx : SelectedIndices)\
		{\
			FTransform& PointTransform = Points[SelectedPointIdx].Transform;\
			TRANSFORM_POINT\
		}\
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)\
		{\
			for (const int32 PointIdx : Curves[CurveIdx].PointIndices)\
			{\
				if (SelectedIndices.Contains(PointIdx))\
				{\
					RefreshCurveDisplayPoints(CurveIdx);\
					break;\
				}\
			}\
		}\
	}\
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)\
	{\
		TSet<int32> SelectedPointIndices;\
		for (const int32 SelectedCurveIdx : SelectedIndices)\
			SelectedPointIndices.Append(Curves[SelectedCurveIdx].PointIndices);\
		bMultiPointsSelected = SelectedPointIndices.Num() >= 2;\
		for (const int32& SelectedPointIdx : SelectedPointIndices)\
		{\
			FTransform& PointTransform = Points[SelectedPointIdx].Transform;\
			TRANSFORM_POINT\
		}\
		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)\
		{\
			for (const int32 PointIdx : Curves[CurveIdx].PointIndices)\
			{\
				if (SelectedPointIndices.Contains(PointIdx))\
				{\
					RefreshCurveDisplayPoints(CurveIdx);\
					break;\
				}\
			}\
		}\
	}

void UHoudiniCurvesComponent::TranslateSelection(const FVector& DeltaTranslate)
{
	TRANSFORM_POINTS(PointTransform.SetLocation(ComponentTransform.InverseTransformPosition(
		ComponentTransform.TransformPosition(PointTransform.GetLocation()) + DeltaTranslate)););
}

void UHoudiniCurvesComponent::RotateSelection(const FRotator& DeltaRotate, const FVector& Pivot)
{
	TRANSFORM_POINTS(PointTransform.SetLocation(ComponentTransform.InverseTransformPosition(Pivot +
			DeltaRotate.RotateVector(ComponentTransform.TransformPosition(PointTransform.GetLocation()) - Pivot)));
		if (!bMultiPointsSelected)  // If a single point selected, then we should also change its rotation
		{
			FQuat OldWorldRotation = ComponentTransform.GetRotation() * PointTransform.GetRotation();
			FQuat NewWorldRotation = DeltaRotate.Quaternion() * OldWorldRotation;
			FQuat NewLocalRotation = ComponentTransform.GetRotation().Inverse() * NewWorldRotation;
			PointTransform.SetRotation(NewLocalRotation);
		});
}

void UHoudiniCurvesComponent::ScaleSelection(const FVector& DeltaScale, const FVector& Pivot)
{
	TRANSFORM_POINTS(if (bMultiPointsSelected) PointTransform.SetLocation(ComponentTransform.InverseTransformPosition(Pivot +
			(ComponentTransform.TransformPosition(PointTransform.GetLocation()) - Pivot) * (DeltaScale + FVector::OneVector)));
		else  // If a single point selected, then we should also change its scale
			PointTransform.SetScale3D(PointTransform.GetScale3D() * (FVector::OneVector + DeltaScale)););
}

int32 UHoudiniCurvesComponent::RayCast(const FRay& ClickRay, FVector& OutRayCastPos) const
{
	const FTransform& ComponentTransform = GetComponentTransform();
	const FVector TargetPos = ClickRay.PointAt(99999999.0);

	int32 ElemIdx = -1;
	double MinDist = -1.0;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (int32 PointIdx = 0; PointIdx < Points.Num(); ++PointIdx)
		{
			const FVector WorldPos = ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation());
			const double Distance = FMath::PointDistToSegment(WorldPos, ClickRay.Origin, TargetPos);
			if ((MinDist > Distance) || (ElemIdx < 0))
			{
				MinDist = Distance;
				OutRayCastPos = WorldPos;
				ElemIdx = PointIdx;
			}
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TArray<FVector> WorldPositions;
		WorldPositions.Reserve(Points.Num());
		for (const FHoudiniCurvePoint& Point : Points)
			WorldPositions.Add(ComponentTransform.TransformPosition(Point.Transform.GetLocation()));

		for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
		{
			const TArray<FVector>& DisplayPoints = Curves[CurveIdx].DisplayPoints;
			FVector PrevDisplayPos = ComponentTransform.TransformPosition(DisplayPoints[0]);
			for (int32 DisplayIdx = 1; DisplayIdx < DisplayPoints.Num(); ++DisplayIdx)
			{
				const FVector CurrDisplayPos = ComponentTransform.TransformPosition(DisplayPoints[DisplayIdx]);
				FVector CurveMinPos, ViewMinPos;
				FMath::SegmentDistToSegment(PrevDisplayPos, CurrDisplayPos,
					ClickRay.Origin, TargetPos, CurveMinPos, ViewMinPos);
				PrevDisplayPos = CurrDisplayPos;

				const double Dist = (CurveMinPos - ViewMinPos).Length();
				if (Dist < MinDist || MinDist < 0.0)
				{
					MinDist = Dist;
					OutRayCastPos = CurveMinPos;
					ElemIdx = CurveIdx;
				}
			}
		}
	}

	return ElemIdx;
}

void UHoudiniCurvesComponent::RefreshCurveDisplayPoints(const int32& CurveIdx)
{
	FHoudiniCurve& Curve = Curves[CurveIdx];
	TArray<FVector>& DisplayPoints = Curve.DisplayPoints;
	DisplayPoints.Empty();

	if (Curve.NoNeedDisplay())
	{
		Curve.DisplayIndices.Empty();
		return;
	}

	const TArray<int32>& PointIndices = Curve.PointIndices;
	if (PointIndices.Num() == 2)
	{
		Curve.DisplayIndices.Empty();

		DisplayPoints = TArray<FVector>{
			Points[PointIndices[0]].Transform.GetLocation(), Points[PointIndices[1]].Transform.GetLocation() };
		return;
	}

	switch (Curve.Type)
	{
	case EHoudiniCurveType::Polygon:
	{
		Curve.DisplayIndices.Empty();

		for (const int32& PointIdx : PointIndices)
			DisplayPoints.Add(Points[PointIdx].Transform.GetLocation());
		if (Curve.bClosed)
			DisplayPoints.Add(FVector(DisplayPoints[0]));
	}
	break;
	case EHoudiniCurveType::Subdiv:
	{
		Curve.DisplayIndices.SetNumUninitialized(PointIndices.Num());

		bool bOpen = !Curve.bClosed;
		for (const int32& PointIdx : PointIndices)
			DisplayPoints.Add(Points[PointIdx].Transform.GetLocation());

		// Calculate avg segment lengths to determinate num iters to subdivide this curve (4 - 6)
		int32 NumIters = 4;
		{
			double AvgSegLen = 0.0;
			for (int32 VtxIdx = 1; VtxIdx < DisplayPoints.Num(); ++VtxIdx)
				AvgSegLen += FVector::Distance(DisplayPoints[VtxIdx - 1], DisplayPoints[VtxIdx]);
			if (Curve.bClosed)
				AvgSegLen += FVector::Distance(DisplayPoints.Last(), DisplayPoints[0]);
			AvgSegLen /= (DisplayPoints.Num() + (Curve.bClosed ? 1 : 0));
			NumIters += FMath::Clamp(FMath::RoundToInt(FMath::Log2(AvgSegLen * POSITION_SCALE_TO_HOUDINI) / 8.0), 0, 2);  // Ensure 16m/seg
		}

		for (int32 SubdIter = 0; SubdIter < NumIters; ++SubdIter)
		{
			int32 NumPoints = DisplayPoints.Num();
			TArray<FVector> NewDisplayPoints;
			if (bOpen)
				NewDisplayPoints.Add(DisplayPoints[0]);

			for (int32 CurrIdx = bOpen; CurrIdx < NumPoints - bOpen; ++CurrIdx)
			{
				const int32 PrevIdx = (CurrIdx == 0) ? (NumPoints - 1) : (CurrIdx - 1);
				const int32 NextIdx = CurrIdx == (NumPoints - 1) ? 0 : (CurrIdx + 1);
				const FVector PrevMPoint = (DisplayPoints[PrevIdx] + DisplayPoints[CurrIdx]) * 0.5;
				const FVector NextMPoint = (DisplayPoints[CurrIdx] + DisplayPoints[NextIdx]) * 0.5;

				NewDisplayPoints.Add(PrevMPoint);
				NewDisplayPoints.Add(((NextMPoint + PrevMPoint) * 0.5 + DisplayPoints[CurrIdx]) * 0.5);
				if (bOpen && CurrIdx == NumPoints - 2)
				{
					NewDisplayPoints.Add(NextMPoint);
					NewDisplayPoints.Add(DisplayPoints[NextIdx]);
				}
			}
			DisplayPoints = NewDisplayPoints;
		}
		if (Curve.bClosed)
		{
			const FVector FirstPoint = DisplayPoints[0];
			DisplayPoints.Add(FirstPoint);
		}

		const int32 NumDivs = (1 << NumIters);
		for (int32 VtxIdx = 0; VtxIdx < Curve.DisplayIndices.Num(); ++VtxIdx)
			Curve.DisplayIndices[VtxIdx] = (VtxIdx + int32(Curve.bClosed)) * NumDivs;
	}
	break;
	case EHoudiniCurveType::Bezier:
	{
		Curve.DisplayIndices.SetNumUninitialized(PointIndices.Num());

		const int32 NumVertices = PointIndices.Num() + Curve.bClosed;
		const int32 NumSubBeziers = NumVertices / 3;
		for (int32 SubIdx = 0; SubIdx < NumSubBeziers; ++SubIdx)
		{
			Curve.DisplayIndices[SubIdx * 3] = DisplayPoints.Num();
			const FVector P0 = Points[PointIndices[SubIdx * 3]].Transform.GetLocation();
			const FVector P1 = Points[PointIndices[SubIdx * 3 + 1]].Transform.GetLocation();
			const FVector P2 = Points[PointIndices[(SubIdx * 3 + 2) % PointIndices.Num()]].Transform.GetLocation();
			const int32 LastIdx = SubIdx * 3 + 3;
			const FVector P3 = LastIdx >= PointIndices.Num() ? 
				(Curve.bClosed ? Points[PointIndices[0]].Transform.GetLocation() : P2) :
				Points[PointIndices[LastIdx]].Transform.GetLocation();

			const int32 NumDivs = FMath::Clamp(FMath::RoundToInt(
				(FVector::Distance(P0, P1) + FVector::Distance(P1, P2) + FVector::Distance(P1, P3)) * (POSITION_SCALE_TO_HOUDINI / 16.0)), 24, 96);  // Ensure 16m/seg
			const double DivU = 1.0 / NumDivs;

			DisplayPoints.Add(P0);
			for (int32 InterpIdx = 1; InterpIdx < NumDivs; ++InterpIdx)
				DisplayPoints.Add(FHoudiniCurve::Bezier(P0, P1, P2, P3, InterpIdx * DivU));
			
			const int32 NumSubVtcs = NumDivs / 3;
			if (Curve.DisplayIndices.IsValidIndex(SubIdx * 3 + 1))
				Curve.DisplayIndices[SubIdx * 3 + 1] = Curve.DisplayIndices[SubIdx * 3] + NumSubVtcs;
			if (Curve.DisplayIndices.IsValidIndex(SubIdx * 3 + 2))
				Curve.DisplayIndices[SubIdx * 3 + 2] = DisplayPoints.Num() - NumSubVtcs;
		}
		if (Curve.DisplayIndices.IsValidIndex(NumSubBeziers * 3))
			Curve.DisplayIndices[NumSubBeziers * 3] = DisplayPoints.Num();

		if ((NumVertices - NumSubBeziers * 3) == 1)
			DisplayPoints.Add(Points[Curve.bClosed ? PointIndices[0] : PointIndices.Last()].Transform.GetLocation());
		else if ((NumVertices - NumSubBeziers * 3) == 2)  // The vertices that not parsed by four point bezier
		{
			DisplayPoints.Add(Points[Curve.bClosed ? PointIndices.Last() : PointIndices.Last(1)].Transform.GetLocation());
			if (!Curve.bClosed)
				Curve.DisplayIndices.Last() = DisplayPoints.Num();
			DisplayPoints.Add(Points[Curve.bClosed ? PointIndices[0] : PointIndices.Last()].Transform.GetLocation());
		}
	}
	break;
	case EHoudiniCurveType::Interpolate:
	{
		Curve.DisplayIndices.SetNumUninitialized(PointIndices.Num());

		const auto AppendInterpolatedPoints = [&DisplayPoints](const FVector& P_1, const FVector& P0, const FVector& P1, const FVector& P2)
			{
				const int32 NumDivs = FMath::Clamp(FMath::RoundToInt(FVector::Distance(P0, P1) * (POSITION_SCALE_TO_HOUDINI / 16.0)), 16, 64);  // Ensure 16m/seg
				const double DivU = 1.0 / NumDivs;

				DisplayPoints.Add(P0);
				for (int32 InterpIdx = 1; InterpIdx < NumDivs; ++InterpIdx)
					DisplayPoints.Add(FHoudiniCurve::CatmullRom(P_1, P0, P1, P2, InterpIdx * DivU));
			};

		if (Curve.bClosed)
		{
			for (int32 VtxIdx = 0; VtxIdx < PointIndices.Num(); ++VtxIdx)
			{
				Curve.DisplayIndices[VtxIdx] = DisplayPoints.Num();
				const int32 I_1 = Curve.bClosed ? (VtxIdx == 0 ? PointIndices.Num() - 1 : VtxIdx - 1) : VtxIdx - 1;
				const int32 I1 = Curve.bClosed ? ((VtxIdx == PointIndices.Num() - 1) ? 0 : VtxIdx + 1) : VtxIdx + 1;
				const int32 I2 = Curve.bClosed ? ((VtxIdx + 2) % PointIndices.Num()) : VtxIdx + 2;
				const FVector P_1 = Points[PointIndices[I_1]].Transform.GetLocation();
				const FVector P0 = Points[PointIndices[VtxIdx]].Transform.GetLocation();
				const FVector P1 = Points[PointIndices[I1]].Transform.GetLocation();
				const FVector P2 = Points[PointIndices[I2]].Transform.GetLocation();
				AppendInterpolatedPoints(P_1, P0, P1, P2);
			}
			DisplayPoints.Add(Points[PointIndices[0]].Transform.GetLocation());
		}
		else
		{
			for (int32 VtxIdx = 0; VtxIdx < PointIndices.Num() - 1; ++VtxIdx)
			{
				Curve.DisplayIndices[VtxIdx] = DisplayPoints.Num();
				const FVector P0 = Points[PointIndices[VtxIdx]].Transform.GetLocation();
				const FVector P1 = Points[PointIndices[VtxIdx + 1]].Transform.GetLocation();
				const FVector P_1 = (VtxIdx == 0) ? Points[PointIndices[VtxIdx + 2]].Transform.GetLocation() + (P0 - P1) * 3.0 :
					Points[PointIndices[VtxIdx - 1]].Transform.GetLocation();
				const FVector P2 = (VtxIdx == (PointIndices.Num() - 2)) ? Points[PointIndices[VtxIdx - 1]].Transform.GetLocation() + (P1 - P0) * 3.0 :
					Points[PointIndices[VtxIdx + 2]].Transform.GetLocation();
				AppendInterpolatedPoints(P_1, P0, P1, P2);
			}
			DisplayPoints.Add(Points[PointIndices.Last()].Transform.GetLocation());
		}
	}
	break;
	}
}


void UHoudiniCurvesComponent::ResetCurvesData()
{
	Points.Empty();
	Curves.Empty();
}

FHoudiniCurvePoint& UHoudiniCurvesComponent::AddPoint()
{
	return Points.Add_GetRef(FHoudiniCurvePoint(Points.Num()));
}

FHoudiniCurve& UHoudiniCurvesComponent::AddCurve()
{
	return Curves.Add_GetRef(FHoudiniCurve(Curves.Num()));
}


#define COPY_CURVE_ATTRIB_VALUE(ATTRIB) for (const FHoudiniCurve& Curve : Curves) \
	{ \
		*((int32*)DataPtr) = (int32)Curve.ATTRIB; \
		++DataPtr; \
	}

bool UHoudiniCurvesComponent::HapiInputUpload(const int32& SHMInputNodeId, const bool& bImportRotAndScale, const bool& bImportCollisionInfo, size_t& InOutHandle) const
{
	const int32 NumPoints = Points.Num();
	const int32 NumPrims = Curves.Num();
	int32 NumVertices = 0;
	for (const FHoudiniCurve& Curve : Curves)
		NumVertices += Curve.PointIndices.Num();

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumPrims, NumVertices);


	// -------- Upload Infos --------
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_ID, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Int, 1, NumPoints);  // point id
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_ID, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::Int, 1, NumPrims);  // prim id
	SHMGeoInput.AppendAttribute(HAPI_CURVE_CLOSED, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::Int, 1, NumPrims);
	SHMGeoInput.AppendAttribute(HAPI_CURVE_TYPE, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::Int, 1, NumPrims);
	
	const bool bPointSelected = (SelectedClass == EHoudiniAttributeOwner::Point) && !SelectedIndices.IsEmpty();
	const bool bPrimSelected = (SelectedClass == EHoudiniAttributeOwner::Prim) && !SelectedIndices.IsEmpty();
	SHMGeoInput.AppendGroup(HAPI_GROUP_CHANGED, EHoudiniAttributeOwner::Point, bPointSelected ? NumPoints : 1, !bPointSelected);
	SHMGeoInput.AppendGroup(HAPI_GROUP_CHANGED, EHoudiniAttributeOwner::Prim, bPrimSelected ? NumPrims : 1, !bPrimSelected);
	
	if (bImportRotAndScale)
	{
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_ROT, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 4, NumPoints * 4);
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_SCALE, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 3, NumPoints * 3);
	}

	TArray<char> CollisionNamesData;
	size_t CollisionNamesDataLength = 0;
	if (bImportCollisionInfo)
	{
		for (const FHoudiniCurvePoint& Point : Points)
		{
			if (Point.CollisionActorName == NAME_None)
				CollisionNamesData.Add('\0');
			else
			{
				const std::string CString = TCHAR_TO_UTF8(*Point.CollisionActorName.ToString());
				CollisionNamesData.Append(CString.c_str(), CString.length() + 1);  // append a '\0'
			}
		}
		CollisionNamesDataLength = CollisionNamesData.Num() / 4 + 1;
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_COLLISION_NAME, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, CollisionNamesDataLength);
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_COLLISION_NORMAL, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 3, NumPoints * 3);
	}

	for (const UHoudiniParameterAttribute* PointParmAttrib : PointParmAttribs)
		PointParmAttrib->UploadInfo(SHMGeoInput);

	for (const UHoudiniParameterAttribute* PrimParmAttrib : PrimParmAttribs)
		PrimParmAttrib->UploadInfo(SHMGeoInput);

	const std::string DeltaInfoCString = TCHAR_TO_UTF8(*DeltaInfo);
	const size_t DeltaInfoDataSize32 = (DeltaInfoCString.length() + 1) / 4 + 1;
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_DELTA_INFO, EHoudiniAttributeOwner::Detail, EHoudiniInputAttributeStorage::String, 1, DeltaInfoDataSize32);


	// -------- Upload Data --------
	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), ((size_t)this)), InOutHandle);
	float* DataPtr = SHM;
	for (const FHoudiniCurvePoint& Point : Points)
	{
		FVector Position = Point.Transform.GetLocation() * POSITION_SCALE_TO_HOUDINI_F;
		*DataPtr = float(Position.X);
		*(DataPtr + 1) = float(Position.Z);
		*(DataPtr + 2) = float(Position.Y);
		DataPtr += 3;
	}

	for (const FHoudiniCurve& Curve : Curves)
	{
		FMemory::Memcpy(DataPtr, Curve.PointIndices.GetData(), Curve.PointIndices.Num() * sizeof(float));
		*((int32*)(DataPtr + Curve.PointIndices.Num())) = -2;
		DataPtr += Curve.PointIndices.Num() + 1;
	}
	
	for (const FHoudiniCurvePoint& Point : Points)
	{
		*((int32*)DataPtr) = Point.Id;
		++DataPtr;
	}

	COPY_CURVE_ATTRIB_VALUE(Id);
	COPY_CURVE_ATTRIB_VALUE(bClosed);
	COPY_CURVE_ATTRIB_VALUE(Type);

	if (bPointSelected)
	{
		for (const int32 SelectedElemIdx : SelectedIndices)
			*((int32*)(DataPtr + SelectedElemIdx)) = 1;
		DataPtr += NumPoints + 1;  // "+ 1" means prim group data, which is unique and == 0
	}
	else if (bPrimSelected)
	{
		++DataPtr;  // point group data, which is unique and == 0
		for (const int32 SelectedElemIdx : SelectedIndices)
			*((int32*)(DataPtr + SelectedElemIdx)) = 1;
		DataPtr += NumPrims;
	}
	else
		DataPtr += 2;

	if (bImportRotAndScale)
	{
		for (const FHoudiniCurvePoint& Point : Points)
		{
			const FQuat4f Rotation = FQuat4f(Point.Transform.GetRotation());
			*DataPtr = Rotation.X;
			*(DataPtr + 1) = Rotation.Z;
			*(DataPtr + 2) = Rotation.Y;
			*(DataPtr + 3) = -Rotation.W;
			DataPtr += 4;
		}

		for (const FHoudiniCurvePoint& Point : Points)
		{
			const FVector3f Scale = FVector3f(Point.Transform.GetScale3D());
			*DataPtr = Scale.X;
			*(DataPtr + 1) = Scale.Z;
			*(DataPtr + 2) = Scale.Y;
			DataPtr += 3;
		}
	}

	if (bImportCollisionInfo)
	{
		FMemory::Memcpy(DataPtr, CollisionNamesData.GetData(), CollisionNamesData.Num());
		DataPtr += CollisionNamesDataLength;
		for (const FHoudiniCurvePoint& Point : Points)
		{
			const FVector3f& CollisionNormal = FVector3f(Point.CollisionNormal);
			*DataPtr = CollisionNormal.X;
			*(DataPtr + 1) = CollisionNormal.Z;
			*(DataPtr + 2) = CollisionNormal.Y;
			DataPtr += 3;
		}
	}

	for (const UHoudiniParameterAttribute* PointParmAttrib : PointParmAttribs)
		PointParmAttrib->UploadData(DataPtr);

	for (const UHoudiniParameterAttribute* PrimParmAttrib : PrimParmAttribs)
		PrimParmAttrib->UploadData(DataPtr);

	FMemory::Memcpy(DataPtr, DeltaInfoCString.c_str(), DeltaInfoCString.length());
	DataPtr += DeltaInfoDataSize32;

	return SHMGeoInput.HapiUpload(SHMInputNodeId, SHM);
}

bool UHoudiniCurvesComponent::HapiUpload(const TArray<UHoudiniCurvesComponent*>& HCCs,
	const int32& SHMInputNodeId, size_t& InOutHandle)
{
	size_t NumPoints = 0;
	size_t NumPrims = 0;
	size_t NumVertices = 0;
	EHoudiniAttributeOwner ChangedClass = EHoudiniAttributeOwner::Invalid;
	TArray<UHoudiniParameterAttribute*> PointAttribs;
	TArray<UHoudiniParameterAttribute*> PrimAttribs;
	const FString& EditGroupName = UHoudiniEditableGeometry::ParseEditData(TConstArrayView<const UHoudiniEditableGeometry*>((const UHoudiniEditableGeometry**)HCCs.GetData(), HCCs.Num()),
		NumVertices, NumPoints, NumPrims, ChangedClass, PointAttribs, PrimAttribs);


	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumPrims, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_ROT, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 4, NumPoints * 4);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_SCALE, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 3, NumPoints * 3);
	SHMGeoInput.AppendAttribute(HAPI_CURVE_CLOSED, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::Int, 1, NumPrims);
	SHMGeoInput.AppendAttribute(HAPI_CURVE_TYPE, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::Int, 1, NumPrims);
	
	SHMGeoInput.AppendGroup(TCHAR_TO_UTF8(*EditGroupName), EHoudiniAttributeOwner::Prim, 1, true);
	const bool bPointSelected = ChangedClass == EHoudiniAttributeOwner::Point;
	const bool bPrimSelected = ChangedClass == EHoudiniAttributeOwner::Prim;
	SHMGeoInput.AppendGroup(HAPI_GROUP_CHANGED, EHoudiniAttributeOwner::Point, bPointSelected ? NumPoints : 1, !bPointSelected);
	SHMGeoInput.AppendGroup(HAPI_GROUP_CHANGED, EHoudiniAttributeOwner::Prim, bPrimSelected ? NumPrims : 1, !bPrimSelected);

	for (const UHoudiniParameterAttribute* Attrib : PointAttribs)
		Attrib->UploadInfo(SHMGeoInput);

	for (const UHoudiniParameterAttribute* Attrib : PrimAttribs)
		Attrib->UploadInfo(SHMGeoInput);


	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("curve_%08x"), SHMInputNodeId), InOutHandle);


	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	float* RotDataPtr = (float*)(VertexDataPtr + NumVertices + NumPrims);
	float* ScaleDataPtr = RotDataPtr + NumPoints * 4;
	int32* CurveClosedDataPtr = (int32*)(ScaleDataPtr + NumPoints * 3);
	int32* CurveTypeDataPtr = CurveClosedDataPtr + NumPrims;
	int32* EditGroupDataPtr = CurveTypeDataPtr + NumPrims; *EditGroupDataPtr = 1;
	int32* ChangedPointGroupDataPtr = EditGroupDataPtr + 1;
	int32* ChangedPrimGroupDataPtr = ChangedPointGroupDataPtr + (bPointSelected ? NumPoints : 1);
	
	float* AttribDataPtr = (float*)(ChangedPrimGroupDataPtr + (bPrimSelected ? NumPrims : 1));
	for (const UHoudiniParameterAttribute* Attrib : PointAttribs)
		Attrib->UploadData(AttribDataPtr);

	for (const UHoudiniParameterAttribute* Attrib : PrimAttribs)
		Attrib->UploadData(AttribDataPtr);

	NumPoints = 0;  // Here represent the previous count, and the start index
	NumPrims = 0;  // Here represent the previous count, and the start index
	for (const UHoudiniCurvesComponent* HCC : HCCs)
	{
		for (const FHoudiniCurvePoint& Point : HCC->Points)
		{
			const FVector3f Position = FVector3f(Point.Transform.GetLocation() * POSITION_SCALE_TO_HOUDINI_F);
			PositionDataPtr[0] = Position.X;
			PositionDataPtr[1] = Position.Z;
			PositionDataPtr[2] = Position.Y;
			PositionDataPtr += 3;

			const FQuat4f Rotation = FQuat4f(Point.Transform.GetRotation());
			RotDataPtr[0] = Rotation.X;
			RotDataPtr[1] = Rotation.Z;
			RotDataPtr[2] = Rotation.Y;
			RotDataPtr[3] = -Rotation.W;
			RotDataPtr += 4;

			const FVector3f Scale = FVector3f(Point.Transform.GetScale3D());
			ScaleDataPtr[0] = Scale.X;
			ScaleDataPtr[1] = Scale.Z;
			ScaleDataPtr[2] = Scale.Y;
			ScaleDataPtr += 3;
		}

		for (const FHoudiniCurve& Curve : HCC->Curves)
		{
			*CurveClosedDataPtr = int32(Curve.bClosed);
			++CurveClosedDataPtr;
			*CurveTypeDataPtr = int32(Curve.Type);
			++CurveTypeDataPtr;

			for (const int32& PointIdx : Curve.PointIndices)
			{
				*VertexDataPtr = NumPoints + PointIdx;
				++VertexDataPtr;
			}

			*VertexDataPtr = -2;
			++VertexDataPtr;
		}

		if (!HCC->DeltaInfo.IsEmpty())
		{
			if (ChangedClass == EHoudiniAttributeOwner::Point && HCC->SelectedClass == EHoudiniAttributeOwner::Point)
			{
				for (const int32& SelectedIdx : HCC->SelectedIndices)
				{
					if (HCC->Points.IsValidIndex(SelectedIdx))  // Check is valid idx, as undo maybe cause the previous added points' idx out-of-date
						ChangedPointGroupDataPtr[NumPoints + SelectedIdx] = 1;
				}
			}
			else if (ChangedClass == EHoudiniAttributeOwner::Prim && HCC->SelectedClass == EHoudiniAttributeOwner::Prim)
			{
				for (const int32& SelectedIdx : HCC->SelectedIndices)
				{
					if (HCC->Curves.IsValidIndex(SelectedIdx))  // Check is valid idx, as undo maybe cause the previous added curves' idx out-of-date
						ChangedPrimGroupDataPtr[NumPrims + SelectedIdx] = 1;
				}
			}
		}

		NumPoints += HCC->Points.Num();
		NumPrims += HCC->Curves.Num();
	}

	// We should empty delta info, to mark this EditGeo unchanged
	for (UHoudiniCurvesComponent* HCC : HCCs)
		HCC->ResetDeltaInfo();

	return SHMGeoInput.HapiUpload(SHMInputNodeId, SHM);
}

TSharedPtr<FJsonObject> UHoudiniCurvesComponent::ConvertToJson() const
{
	// Points
	TArray<TSharedPtr<FJsonValue>> JsonPs;
	TArray<TSharedPtr<FJsonValue>> JsonRots;
	TArray<TSharedPtr<FJsonValue>> JsonScales;
	for (const FHoudiniCurvePoint& Point : Points)
	{
		const FVector P = Point.Transform.GetLocation();
		JsonPs.Add(MakeShared<FJsonValueNumber>(P.X)); JsonPs.Add(MakeShared<FJsonValueNumber>(P.Y)); JsonPs.Add(MakeShared<FJsonValueNumber>(P.Z));
		const FQuat Rot = Point.Transform.GetRotation();
		JsonRots.Add(MakeShared<FJsonValueNumber>(Rot.X)); JsonRots.Add(MakeShared<FJsonValueNumber>(Rot.Y)); JsonRots.Add(MakeShared<FJsonValueNumber>(Rot.Z)); JsonRots.Add(MakeShared<FJsonValueNumber>(Rot.W));
		const FVector Scale = Point.Transform.GetScale3D();
		JsonScales.Add(MakeShared<FJsonValueNumber>(Scale.X)); JsonScales.Add(MakeShared<FJsonValueNumber>(Scale.Y)); JsonScales.Add(MakeShared<FJsonValueNumber>(Scale.Z));
	}
	TSharedPtr<FJsonObject> JsonPointData = MakeShared<FJsonObject>();
	JsonPointData->SetArrayField(TEXT(HAPI_ATTRIB_POSITION), JsonPs);
	JsonPointData->SetArrayField(TEXT(HAPI_ATTRIB_ROT), JsonRots);
	JsonPointData->SetArrayField(TEXT(HAPI_ATTRIB_SCALE), JsonScales);
	for (const UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
	{
		if (!ParmAttrib->IsParameterHolder())
			JsonPointData->SetObjectField(ParmAttrib->GetParameterName(), ParmAttrib->ConvertToJson());
	}

	// Curves
	TArray<TSharedPtr<FJsonValue>> JsonCurveCloseds;
	TArray<TSharedPtr<FJsonValue>> JsonCurveTypes;
	TArray<TSharedPtr<FJsonValue>> JsonCurvePointCounts;
	TArray<TSharedPtr<FJsonValue>> JsonCurvePointIndices;
	for (const FHoudiniCurve& Curve : Curves)
	{
		JsonCurveCloseds.Add(MakeShared<FJsonValueNumber>(int32(Curve.bClosed)));
		JsonCurveTypes.Add(MakeShared<FJsonValueNumber>(int32(Curve.Type)));
		JsonCurvePointCounts.Add(MakeShared<FJsonValueString>(FString::FromInt(Curve.PointIndices.Num())));
		for (const int32& PointIdx : Curve.PointIndices)
			JsonCurvePointIndices.Add(MakeShared<FJsonValueString>(FString::FromInt(PointIdx)));
	}

	TSharedPtr<FJsonObject> JsonCurveData = MakeShared<FJsonObject>();
	JsonCurveData->SetArrayField(TEXT(HAPI_CURVE_TYPE), JsonCurveTypes);
	JsonCurveData->SetArrayField(TEXT(HAPI_CURVE_CLOSED), JsonCurveCloseds);
	JsonCurveData->SetArrayField(HOUDINI_JSON_KEY_PRIM_VERTEX_COUNT, JsonCurvePointCounts);
	JsonCurveData->SetArrayField(HOUDINI_JSON_KEY_PRIM_VERTICES, JsonCurvePointIndices);
	for (const UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
	{
		if (!ParmAttrib->IsParameterHolder())
			JsonCurveData->SetObjectField(ParmAttrib->GetParameterName(), ParmAttrib->ConvertToJson());
	}

	TSharedPtr<FJsonObject> JsonCurves = MakeShared<FJsonObject>();
	JsonCurves->SetObjectField(HOUDINI_JSON_KEY_POINTS, JsonPointData);
	JsonCurves->SetObjectField(HOUDINI_JSON_KEY_PRIMS, JsonCurveData);

	return JsonCurves;
}

bool UHoudiniCurvesComponent::AppendFromJson(const TSharedPtr<FJsonObject>& JsonCurves)
{
	const TSharedPtr<FJsonObject>* JsonPointDataPtr = nullptr;
	if (!JsonCurves->TryGetObjectField(HOUDINI_JSON_KEY_POINTS, JsonPointDataPtr))
		return false;

	const TArray<TSharedPtr<FJsonValue>>* JsonPsPtr = nullptr;
	if (!(*JsonPointDataPtr)->TryGetArrayField(TEXT(HAPI_ATTRIB_POSITION), JsonPsPtr))  // Must have point positions
		return false;

	const TSharedPtr<FJsonObject>* JsonCurveDataPtr = nullptr;
	if (!JsonCurves->TryGetObjectField(HOUDINI_JSON_KEY_PRIMS, JsonCurveDataPtr))
		return false;

	const TArray<TSharedPtr<FJsonValue>>* JsonCurvePointCountsPtr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JsonCurvePointIndicesPtr = nullptr;
	if (!(*JsonCurveDataPtr)->TryGetArrayField(HOUDINI_JSON_KEY_PRIM_VERTEX_COUNT, JsonCurvePointCountsPtr))
		return false;

	if (!(*JsonCurveDataPtr)->TryGetArrayField(HOUDINI_JSON_KEY_PRIM_VERTICES, JsonCurvePointIndicesPtr))
		return false;

	if (JsonCurvePointCountsPtr->IsEmpty() && (JsonPsPtr->Num() / 3) <= 0)  // Empty
		return false;

	// Start append
	SelectedClass = JsonCurvePointCountsPtr->IsEmpty() ? EHoudiniAttributeOwner::Point : EHoudiniAttributeOwner::Prim;
	const FString DeltaInfoPrefix = Name / (SelectedClass == EHoudiniAttributeOwner::Point ? DELTAINFO_CLASS_POINT : DELTAINFO_CLASS_PRIM) + TEXT("/");
	DeltaInfo = DeltaInfoPrefix + DELTAINFO_ACTION_APPEND;

#if WITH_EDITOR
	ReDeltaInfo = DeltaInfoPrefix + DELTAINFO_ACTION_REMOVE;

	FScopedTransaction Transaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniEditableGeometryCurveTransaction", "Houdini Editable Geometry: Curve Changed"), this, true);

	Modify();
#endif

	// Points
	const TArray<TSharedPtr<FJsonValue>>* JsonRotsPtr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JsonScalesPtr = nullptr;
	(*JsonPointDataPtr)->TryGetArrayField(TEXT(HAPI_ATTRIB_ROT), JsonRotsPtr);
	(*JsonPointDataPtr)->TryGetArrayField(TEXT(HAPI_ATTRIB_SCALE), JsonScalesPtr);

	const int32 NumJsonPoints = JsonPsPtr->Num() / 3;
	TArray<int32> NewPointIndices;
	for (int32 JsonPointIdx = 0; JsonPointIdx < NumJsonPoints; ++JsonPointIdx)
	{
		FHoudiniCurvePoint NewPoint(HOUDINI_RANDOM_ID);
		NewPoint.Transform.SetLocation(FVector((*JsonPsPtr)[JsonPointIdx * 3]->AsNumber(), (*JsonPsPtr)[JsonPointIdx * 3 + 1]->AsNumber(), (*JsonPsPtr)[JsonPointIdx * 3 + 2]->AsNumber()));
		if (JsonRotsPtr && JsonRotsPtr->IsValidIndex(JsonPointIdx * 4 + 3))
			NewPoint.Transform.SetRotation(FQuat((*JsonRotsPtr)[JsonPointIdx * 4]->AsNumber(), (*JsonRotsPtr)[JsonPointIdx * 4 + 1]->AsNumber(), (*JsonRotsPtr)[JsonPointIdx * 4 + 2]->AsNumber(), (*JsonRotsPtr)[JsonPointIdx * 4 + 3]->AsNumber()));
		if (JsonScalesPtr && JsonRotsPtr->IsValidIndex(JsonPointIdx * 3 + 2))
			NewPoint.Transform.SetScale3D(FVector((*JsonScalesPtr)[JsonPointIdx * 3]->AsNumber(), (*JsonScalesPtr)[JsonPointIdx * 3 + 1]->AsNumber(), (*JsonScalesPtr)[JsonPointIdx * 3 + 2]->AsNumber()));
		NewPointIndices.Add(Points.Add(NewPoint));
	}

	for (UHoudiniParameterAttribute* ParmAttrib : PointParmAttribs)
	{
		if (ParmAttrib->IsParameterHolder())
			continue;

		const TSharedPtr<FJsonObject>* JsonAttribPtr = nullptr;
		if ((*JsonPointDataPtr)->TryGetObjectField(ParmAttrib->GetParameterName(), JsonAttribPtr))
			ParmAttrib->AppendFromJson(*JsonAttribPtr);
		
		ParmAttrib->SetNum(Points.Num());
	}

	// Curves
	const TArray<TSharedPtr<FJsonValue>>* JsonCurveClosedsPtr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JsonCurveTypesPtr = nullptr;
	(*JsonCurveDataPtr)->TryGetArrayField(TEXT(HAPI_CURVE_TYPE), JsonCurveTypesPtr);
	(*JsonCurveDataPtr)->TryGetArrayField(TEXT(HAPI_CURVE_CLOSED), JsonCurveClosedsPtr);

	TArray<int32> NewCurveIndices;
	const int32 NumJsonCurves = JsonCurvePointCountsPtr->Num();
	int32 AccumulatedVertexCount = 0;
	for (int32 JsonCurveIdx = 0; JsonCurveIdx < NumJsonCurves; ++JsonCurveIdx)
	{
		FHoudiniCurve Curve(HOUDINI_RANDOM_ID);

		const int32 NumCurvePoints = FCString::Atoi(*(*JsonCurvePointCountsPtr)[JsonCurveIdx]->AsString());
		for (int32 VtxIdx = AccumulatedVertexCount; VtxIdx < AccumulatedVertexCount + NumCurvePoints; ++VtxIdx)
		{
			if (JsonCurvePointIndicesPtr->IsValidIndex(VtxIdx))
			{
				const int32 JsonPointIdx = FCString::Atoi(*(*JsonCurvePointIndicesPtr)[VtxIdx]->AsString());
				if (NewPointIndices.IsValidIndex(JsonPointIdx))
					Curve.PointIndices.Add(NewPointIndices[JsonPointIdx]);
			}
		}

		Curve.bClosed = (JsonCurveClosedsPtr && JsonCurveClosedsPtr->IsValidIndex(JsonCurveIdx)) ?
			(bool)int32((*JsonCurveClosedsPtr)[JsonCurveIdx]->AsNumber()) : bDefaultCurveClosed;

		Curve.Type = (JsonCurveTypesPtr && JsonCurveTypesPtr->IsValidIndex(JsonCurveIdx)) ?
			(EHoudiniCurveType)int32((*JsonCurveTypesPtr)[JsonCurveIdx]->AsNumber()) : DefaultCurveType;

		AccumulatedVertexCount += NumCurvePoints;

		NewCurveIndices.Add(Curves.Add(Curve));
	}

	for (UHoudiniParameterAttribute* ParmAttrib : PrimParmAttribs)
	{
		if (ParmAttrib->IsParameterHolder())
			continue;

		const TSharedPtr<FJsonObject>* JsonAttribPtr = nullptr;
		if ((*JsonCurveDataPtr)->TryGetObjectField(ParmAttrib->GetParameterName(), JsonAttribPtr))
			ParmAttrib->AppendFromJson(*JsonAttribPtr);

		ParmAttrib->SetNum(Curves.Num());
	}

	UnDeltaInfo = ReDeltaInfo;
	ReDeltaInfo = DeltaInfoPrefix + DELTAINFO_ACTION_APPEND;

	SelectedIndices = SelectedClass == EHoudiniAttributeOwner::Point ? NewPointIndices : NewCurveIndices;

	TriggerParentNodeToCook();

	return true;
}

#undef LOCTEXT_NAMESPACE
