// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniMeshComponent.h"
#include "HoudiniMeshSceneProxy.h"

#include "MeshUtilitiesCommon.h"
#include "UObject/ObjectSaveContext.h"

#include "HoudiniParameterAttribute.h"
#include "HoudiniOperatorUtils.h"

//#include "Runtime/GeometryFramework/Public/Components/DynamicMeshComponent.h"
//#include "Runtime/GeometryFramework/Private/Components/DynamicMeshSceneProxy.h"


FPrimitiveSceneProxy* UHoudiniMeshComponent::CreateSceneProxy()
{
	return Triangles.IsEmpty() ? nullptr : new FHoudiniMeshSceneProxy(this);
}

int32 UHoudiniMeshComponent::NumVertices() const
{
	int32 NumVertices = 0;
	for (const FHoudiniMeshPoly& Poly : Polys)
		NumVertices += Poly.PointIndices.Num();

	return NumVertices;
}

void UHoudiniMeshComponent::AddTriangle(const FIntVector3& Triangle, const int32& SectionIdx)
{
	Sections[SectionIdx].TriangleIndices.Add(Triangles.Add(Triangle));
}

int32 UHoudiniMeshComponent::AddSection(UMaterialInterface* Material)
{
	const int32 MaterialIdx = Sections.Num();

	FHoudiniMeshSection Section;
	Section.MaterialSlotName = FName(TEXT("Slot_") + FString::FromInt(MaterialIdx));
	Section.Material = Material;
	return Sections.Add(Section);
}


#define COPY_MESH_ATTRIB_DATA(DATA_TYPE, TUPLE_SIZE, ATTRIB_DATA, COPY_FUNC) switch (Owner) \
	{ \
	case EHoudiniAttributeOwner::Vertex: \
	{ \
		ATTRIB_DATA.SetNumUninitialized(TriangleIndices.Num() * 3); \
		for (int32 LocalTriIdx = 0; LocalTriIdx < TriangleIndices.Num(); ++LocalTriIdx) \
		{ \
			const int32 StartGlobalVertexIdx = TriangleIndices[LocalTriIdx] * 3; \
			for (int32 TriVtxIdx = 2; TriVtxIdx >= 0; --TriVtxIdx) \
			{ \
				const int32& DataIdx = (StartGlobalVertexIdx + TriVtxIdx) * TUPLE_SIZE; \
				DATA_TYPE& AttribValue = ATTRIB_DATA[LocalTriIdx * 3 + TriVtxIdx]; \
				COPY_FUNC \
			} \
		} \
	} \
	break; \
	case EHoudiniAttributeOwner::Point: \
	{ \
		ATTRIB_DATA.SetNumUninitialized(PointIndices.Num()); \
		for (int32 LocalPointIdx = 0; LocalPointIdx < PointIndices.Num(); ++LocalPointIdx) \
		{ \
			const int32& DataIdx = PointIndices[LocalPointIdx] * TUPLE_SIZE; \
			DATA_TYPE& AttribValue = ATTRIB_DATA[LocalPointIdx]; \
			COPY_FUNC \
		} \
	} \
	break; \
	case EHoudiniAttributeOwner::Prim: \
	{ \
		ATTRIB_DATA.SetNumUninitialized(TriangleIndices.Num()); \
		for (int32 LocalTriIdx = 0; LocalTriIdx < TriangleIndices.Num(); ++LocalTriIdx) \
		{ \
			const int32& DataIdx = TriangleIndices[LocalTriIdx] * TUPLE_SIZE; \
			DATA_TYPE& AttribValue = ATTRIB_DATA[LocalTriIdx]; \
			COPY_FUNC \
		} \
	} \
	break; \
	case EHoudiniAttributeOwner::Detail: \
	{ \
		ATTRIB_DATA.SetNumUninitialized(1); \
		DATA_TYPE& AttribValue = ATTRIB_DATA[0]; \
		const int32 DataIdx = 0; \
		COPY_FUNC \
	} \
	break; \
	}

void UHoudiniMeshComponent::SetNormalData(const EHoudiniAttributeOwner& Owner, const TArray<float>& Data,
	const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices)
{
	NormalOwner = Owner;
	COPY_MESH_ATTRIB_DATA(FVector3f, 3, NormalData,
		AttribValue.X = Data[DataIdx];
		AttribValue.Y = Data[DataIdx + 2];
		AttribValue.Z = Data[DataIdx + 1];);
}

void UHoudiniMeshComponent::SetTangentUData(const EHoudiniAttributeOwner& Owner, const TArray<float>& Data,
	const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices)
{
	TangentUOwner = Owner;
	COPY_MESH_ATTRIB_DATA(FVector3f, 3, TangentUData,
		AttribValue.X = Data[DataIdx];
		AttribValue.Y = Data[DataIdx + 2];
		AttribValue.Z = Data[DataIdx + 1];);
}

void UHoudiniMeshComponent::SetTangentVData(const EHoudiniAttributeOwner& Owner, const TArray<float>& Data,
	const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices)
{
	TangentVOwner = Owner;
	COPY_MESH_ATTRIB_DATA(FVector3f, 3, TangentVData,
		AttribValue.X = Data[DataIdx];
		AttribValue.Y = Data[DataIdx + 2];
		AttribValue.Z = Data[DataIdx + 1];);
}


void UHoudiniMeshComponent::SetColorData(const EHoudiniAttributeOwner& CdOwner, const TArray<float>& CdData,
	const EHoudiniAttributeOwner& AlphaOwner, const TArray<float>& AlphaData,  // DataLength is global
	const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices)  // these length is local, but indices is global
{
	if (CdOwner == AlphaOwner)
	{
		const EHoudiniAttributeOwner& Owner = CdOwner;
		ColorOwner = Owner;
		COPY_MESH_ATTRIB_DATA(FColor, 1, ColorData,
			AttribValue.R = FMath::RoundToInt(CdData[DataIdx * 3] * 255.0f);
			AttribValue.G = FMath::RoundToInt(CdData[DataIdx * 3 + 1] * 255.0f);
			AttribValue.B = FMath::RoundToInt(CdData[DataIdx * 3 + 2] * 255.0f);
			AttribValue.A = FMath::RoundToInt(AlphaData[DataIdx] * 255.0f););
	}
	else if (CdOwner != EHoudiniAttributeOwner::Invalid && AlphaOwner == EHoudiniAttributeOwner::Invalid)
	{
		const EHoudiniAttributeOwner& Owner = CdOwner;
		ColorOwner = Owner;
		COPY_MESH_ATTRIB_DATA(FColor, 3, ColorData,
			AttribValue.R = FMath::RoundToInt(CdData[DataIdx] * 255.0f);
			AttribValue.G = FMath::RoundToInt(CdData[DataIdx + 1] * 255.0f);
			AttribValue.B = FMath::RoundToInt(CdData[DataIdx + 2] * 255.0f);
			AttribValue.A = 255;);
	}
	else if (CdOwner == EHoudiniAttributeOwner::Invalid && AlphaOwner != EHoudiniAttributeOwner::Invalid)
	{
		const EHoudiniAttributeOwner& Owner = AlphaOwner;
		ColorOwner = Owner;
		COPY_MESH_ATTRIB_DATA(FColor, 1, ColorData,
			AttribValue = FColor::White;
			AttribValue.A = FMath::RoundToInt(AlphaData[DataIdx] * 255.0f););
	}
	else if (CdOwner != EHoudiniAttributeOwner::Invalid && AlphaOwner != EHoudiniAttributeOwner::Invalid)
	{
		// CdOwner != AlphaOwner, so the ColorOwner != EHoudiniAttributeOwner::Detail
		ColorOwner = EHoudiniAttributeOwner::Vertex;
		if (CdOwner == EHoudiniAttributeOwner::Detail)
			ColorOwner = AlphaOwner;
		else if (AlphaOwner == EHoudiniAttributeOwner::Detail)
			ColorOwner = CdOwner;
		
		int32 NumElems = 1;
		switch (ColorOwner)
		{
		case EHoudiniAttributeOwner::Vertex: NumElems = TriangleIndices.Num() * 3; break;
		case EHoudiniAttributeOwner::Point: NumElems = Positions.Num(); break;
		case EHoudiniAttributeOwner::Prim: NumElems = Triangles.Num(); break;
		}
		ColorData.SetNumUninitialized(NumElems);

		auto ConvertElemIdxToDataIdxLambda = [&](const int32& ElemIdx, const EHoudiniAttributeOwner& ElemOwner, const EHoudiniAttributeOwner& DataOwner) -> int32
			{
				switch (ElemOwner)
				{
				case EHoudiniAttributeOwner::Vertex:
				{
					switch (DataOwner)
					{
					case EHoudiniAttributeOwner::Vertex: return TriangleIndices[ElemIdx / 3] + (2 - (ElemIdx % 3));
					case EHoudiniAttributeOwner::Point: return PointIndices[Triangles[ElemIdx / 3][2 - (ElemIdx % 3)]];
					case EHoudiniAttributeOwner::Prim: return TriangleIndices[ElemIdx / 3];
					}
				}
				break;
				case EHoudiniAttributeOwner::Point:
					if (DataOwner == EHoudiniAttributeOwner::Point)
						return PointIndices[ElemIdx];
					break;
				case EHoudiniAttributeOwner::Prim:
					if (DataOwner == EHoudiniAttributeOwner::Prim)
						return TriangleIndices[ElemIdx];
					break;
				}

				return 0;
			};

		for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
		{
			const int32 CdDataIdx = ConvertElemIdxToDataIdxLambda(ElemIdx, ColorOwner, CdOwner) * 3;
			const int32 AlphaDataIdx = ConvertElemIdxToDataIdxLambda(ElemIdx, ColorOwner, AlphaOwner);

			FColor& Color = ColorData[ElemIdx];
			Color.R = FMath::RoundToInt(CdData[CdDataIdx] * 255.0f);
			Color.G = FMath::RoundToInt(CdData[CdDataIdx + 1] * 255.0f);
			Color.B = FMath::RoundToInt(CdData[CdDataIdx + 2] * 255.0f);
			Color.A = FMath::RoundToInt(AlphaData[AlphaDataIdx] * 255.0f);;
		}
	}
}

void UHoudiniMeshComponent::SetUVData(const TArray<EHoudiniAttributeOwner>& Owners, const TArray<TArray<float>>& Datas,
	const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices)
{
	for (int32 UVIdx = 0; UVIdx < Owners.Num(); ++UVIdx)
	{
		const EHoudiniAttributeOwner& Owner = Owners[UVIdx];
		const TArray<float>& Data = Datas[UVIdx];

		UVs.Add(FHoudiniMeshUV());
		FHoudiniMeshUV& UV = UVs.Last();
		UV.Owner = Owner;
		COPY_MESH_ATTRIB_DATA(FVector2f, 2, UV.Data,
			AttribValue.X = Data[DataIdx];
			AttribValue.Y = 1.0f - Data[DataIdx + 1];);
	}
}

void UHoudiniMeshComponent::ComputeNormal()
{
	NormalOwner = EHoudiniAttributeOwner::Point;

	NormalData.SetNumZeroed(Positions.Num());
	TArray<float> Weights;
	Weights.SetNumZeroed(Positions.Num());

	for (int32 TriIdx = 0; TriIdx < Triangles.Num(); ++TriIdx)
	{
		const FIntVector& Triangle = Triangles[TriIdx];
		const FVector3f& V0 = Positions[Triangle.X];
		const FVector3f& V1 = Positions[Triangle.Y];
		const FVector3f& V2 = Positions[Triangle.Z];

		const FVector3f Normal = FVector3f::CrossProduct((V2 - V0) * POSITION_SCALE_TO_HOUDINI_F, (V1 - V0) * POSITION_SCALE_TO_HOUDINI_F);
		const float Area = Normal.Size();

		{
			const float Weight = Area * TriangleUtilities::ComputeTriangleCornerAngle(V0, V1, V2);
			NormalData[Triangle.X] += Normal * Weight;
			Weights[Triangle.X] += Weight;
		}
		{
			const float Weight = Area * TriangleUtilities::ComputeTriangleCornerAngle(V1, V2, V0);
			NormalData[Triangle.Y] += Normal * Weight;
			Weights[Triangle.Y] += Weight;
		}
		{
			const float Weight = Area * TriangleUtilities::ComputeTriangleCornerAngle(V2, V0, V1);
			NormalData[Triangle.Z] += Normal * Weight;
			Weights[Triangle.Z] += Weight;
		}
	}

	for (int32 PointIdx = 0; PointIdx < NormalData.Num(); ++PointIdx)
	{
		FVector3f& Normal = NormalData[PointIdx];
		Normal /= Weights[PointIdx];
		Normal.Normalize();
	}
}


void UHoudiniMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	Sections[ElementIndex].Material = Material;
	Modify();
}

void UHoudiniMeshComponent::SetMaterialByName(FName MaterialSlotName, class UMaterialInterface* Material)
{
	const int32 ElementIndex = Sections.IndexOfByPredicate(
		[&MaterialSlotName](const FHoudiniMeshSection& StaticMaterial) { return MaterialSlotName == StaticMaterial.MaterialSlotName; });
	if (Sections.IsValidIndex(ElementIndex))
	{
		Sections[ElementIndex].Material = Material;
		Modify();
	}
}

void UHoudiniMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	for (int32 ElementIndex = 0; ElementIndex < GetNumMaterials(); ElementIndex++)
	{
		if (UMaterialInterface* MaterialInterface = GetMaterial(ElementIndex))
		{
			OutMaterials.Add(MaterialInterface);
		}
	}
}

void UHoudiniMeshComponent::Build(const TArray<int32>& PrimIds,  // Num() == HAPI_PartInfo::faceCount, are the attribValue of __primitive_id
	const TArray<int32>& TriangleIndices,  // Num() == UHoudiniMeshComponent::Triangles.Num(), are the Global indices of the triangles
	TArray<int32>& OutPrimIndices)  // Num() == UHoudiniMeshComponent::Polys.Num(), are the Global indices of the first triangle
{
	TMap<int32, TArray<int32>> IdTrisMap;  // <PrimId, LocalTriangleIndices>
	for (int32 TriIdx = 0; TriIdx < Triangles.Num(); ++TriIdx)
		IdTrisMap.FindOrAdd(PrimIds[TriangleIndices[TriIdx]]).Add(TriIdx);

	TSet<FIntVector2> UniqueEdges;
	for (const auto& IdTris : IdTrisMap)
	{
		FHoudiniMeshPoly Poly;
		if (IdTris.Value.Num() == 1)
		{
			const FIntVector3& Triangle = Triangles[IdTris.Value[0]];
			Poly.PointIndices = TArray<int32>{ Triangle.X, Triangle.Y, Triangle.Z };
			Poly.Triangles.Add(FIntVector4(0, 1, 2, IdTris.Value[0]));
		}
		else
		{
			TArray<FIntVector2> PolyEdges;
			for (const int32& TriIdx : IdTris.Value)
			{
				const FIntVector3& Triangle = Triangles[TriIdx];
				TArray<FIntVector2> CurrEdges;
				for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
				{
					const FIntVector2 CurrEdge(Triangle[TriVtxIdx], Triangle[(TriVtxIdx == 2) ? 0 : (TriVtxIdx + 1)]);
					const int32 FoundEdgeIdx = PolyEdges.IndexOfByPredicate([CurrEdge](const FIntVector2& ExistEdge)
						{
							return (CurrEdge.X == ExistEdge.Y && CurrEdge.Y == ExistEdge.X) || (CurrEdge == ExistEdge);
						});

					if (PolyEdges.IsValidIndex(FoundEdgeIdx))  // If this found in prev edges, means this edge is a internal edge of this poly
						PolyEdges.RemoveAt(FoundEdgeIdx);
					else
						CurrEdges.Add(CurrEdge);
				}
				PolyEdges.Append(CurrEdges);
			}
			
			const int32 StartPointIdx = PolyEdges[0].X;
			int32 CurrPointIdx = StartPointIdx;
			for (int32 Iter = PolyEdges.Num(); Iter >= 0; --Iter)  // To avoid endless loop
			{
				Poly.PointIndices.Add(CurrPointIdx);
				FIntVector2* FoundNextEdgePtr = PolyEdges.FindByPredicate([CurrPointIdx](const FIntVector2& Edge) { return Edge.X == CurrPointIdx; });
				if (FoundNextEdgePtr->Y == StartPointIdx)
					break;

				CurrPointIdx = FoundNextEdgePtr->Y;
			}

			for (const int32& TriIdx : IdTris.Value)
			{
				const FIntVector3& Triangle = Triangles[TriIdx];
				Poly.Triangles.Add(FIntVector4(
					Poly.PointIndices.IndexOfByKey(Triangle.X),
					Poly.PointIndices.IndexOfByKey(Triangle.Y),
					Poly.PointIndices.IndexOfByKey(Triangle.Z), TriIdx
				));
			}
		}

		Polys.Add(Poly);
		OutPrimIndices.Add(TriangleIndices[IdTris.Value[0]]);

		for (int32 VtxIdx = 0; VtxIdx < Poly.PointIndices.Num(); ++VtxIdx)
		{
			const int32& PtIdx0 = Poly.PointIndices[VtxIdx];
			const int32& PtIdx1 = Poly.PointIndices[(VtxIdx == Poly.PointIndices.Num() - 1) ? 0 : (VtxIdx + 1)];
			UniqueEdges.FindOrAdd(PtIdx0 < PtIdx1 ? FIntVector2(PtIdx0, PtIdx1) : FIntVector2(PtIdx1, PtIdx0));
		}
	}
	Edges = UniqueEdges.Array();
}

void UHoudiniMeshComponent::ClearEditData()
{
	Polys.Empty();
	Edges.Empty();

	UHoudiniEditableGeometry::ClearEditData();
}

void UHoudiniMeshComponent::Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
	const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FRay& ClickRay, const FModifierKeysState& ModifierKeys)
{
	// DeltaInfo will record the hit position, so we should update ClickPosition
	if (SelectClass == EHoudiniAttributeOwner::Point)
		ClickPosition = GetComponentTransform().TransformPosition(FVector(Positions[ElemIdx]));
	else if (SelectClass == EHoudiniAttributeOwner::Prim && ClickRay.Direction != FVector::ZeroVector)  // Calculate the click position by ClickRay
	{
		const FHoudiniMeshPoly& SelectedPoly = Polys[ElemIdx];

		const FTransform& ComponentTransform = GetComponentTransform();
		const FVector TargetPos = ClickRay.PointAt(99999999.0);

		float MinDistance = -1.0f;
		for (const FIntVector4& Triangle : SelectedPoly.Triangles)
		{
			FVector IntersectPos;
			FVector TriangleNormal;
			if (FMath::SegmentTriangleIntersection(ClickRay.Origin, TargetPos,
				ComponentTransform.TransformPosition((FVector)Positions[SelectedPoly.PointIndices[Triangle.X]]),
				ComponentTransform.TransformPosition((FVector)Positions[SelectedPoly.PointIndices[Triangle.Y]]),
				ComponentTransform.TransformPosition((FVector)Positions[SelectedPoly.PointIndices[Triangle.Z]]),
				IntersectPos, TriangleNormal))
			{
				const float Distance = FVector::Distance(IntersectPos, ClickRay.Origin);
				if ((MinDistance > Distance) || (MinDistance < 0.0f))
				{
					MinDistance = Distance;
					ClickPosition = IntersectPos;
				}
			}
		}
	}

	UHoudiniEditableGeometry::Select(SelectClass, ElemIdx, bDeselectPrevious, bDeselectIfSelected, ModifierKeys);
}

void UHoudiniMeshComponent::ExpandSelection()
{
	if (!IsGeometrySelected())
		return;

	if (SelectIdenticals())
		return;

	// First, find point polys
	TArray<TArray<int32>> PointsPrims;
	PointsPrims.SetNum(Positions.Num());
	for (int32 PrimIdx = 0; PrimIdx < Polys.Num(); ++PrimIdx)
	{
		for (const int32& PointIdx : Polys[PrimIdx].PointIndices)
			PointsPrims[PointIdx].Add(PrimIdx);
	}

	// Then find the adjacencies
	const int32 NumSelected = SelectedIndices.Num();
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TSet<int32> FoundPrimIndices;
		for (int32 Iter = 0; Iter < Positions.Num(); ++Iter)  // Avoid endless loop
		{
			const int32 NumFoundPolys = FoundPrimIndices.Num();
			for (const int32& SelectedPointIdx : SelectedIndices)
				FoundPrimIndices.Append(PointsPrims[SelectedPointIdx]);
			if (NumFoundPolys == FoundPrimIndices.Num())
				break;

			TSet<int32> SelectedPointIndices;
			for (const int32& FoundPrimIdx : FoundPrimIndices)
				SelectedPointIndices.Append(Polys[FoundPrimIdx].PointIndices);
			SelectedIndices = SelectedPointIndices.Array();
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> FoundPointIndices;
		for (int32 Iter = 0; Iter < Polys.Num(); ++Iter)  // Avoid endless loop
		{
			const int32 NumFoundPoints = FoundPointIndices.Num();
			for (const int32& SelectedPrimIdx : SelectedIndices)
				FoundPointIndices.Append(Polys[SelectedPrimIdx].PointIndices);
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

FTransform UHoudiniMeshComponent::GetSelectionPivot() const
{
	FTransform ComponentTransform = GetComponentTransform();

	FVector Location = FVector::ZeroVector;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (const int32& SelectedPointIdx : SelectedIndices)
			Location += (FVector)Positions[SelectedPointIdx];
		Location /= SelectedIndices.Num();

		if ((SelectedIndices.Num() == 1) &&
			(TangentUOwner == EHoudiniAttributeOwner::Point) && (TangentVOwner == EHoudiniAttributeOwner::Point))
		{
			FVector3f TangentU = TangentUData[SelectedIndices[0]];
			FVector3f TangentV = TangentVData[SelectedIndices[0]];
			if (TangentU.Normalize() && TangentV.Normalize())
			{
				Location = ComponentTransform.TransformPosition(Location);
				ComponentTransform.SetRotation(ComponentTransform.TransformRotation((FQuat)FRotationMatrix44f::MakeFromXY(TangentV, TangentU).Rotator().Quaternion()));
				ComponentTransform.SetLocation(Location);
				return ComponentTransform;
			}
		}
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TSet<int32> SelectedPointIndices;
		for (const int32 SelectedPolyIdx : SelectedIndices)
			SelectedPointIndices.Append(Polys[SelectedPolyIdx].PointIndices);
		for (const int32& SelectedPointIdx : SelectedPointIndices)
			Location += (FVector)Positions[SelectedPointIdx];
		Location /= SelectedPointIndices.Num();

		if ((SelectedIndices.Num() == 1) &&
			(TangentUOwner == EHoudiniAttributeOwner::Prim) && (TangentVOwner == EHoudiniAttributeOwner::Prim))
		{
			FVector3f TangentU = FVector3f::ZeroVector;
			FVector3f TangentV = FVector3f::ZeroVector;
			for (const FIntVector4& Triangle : Polys[SelectedIndices[0]].Triangles)
			{
				TangentU += TangentUData[Triangle.W];
				TangentV += TangentVData[Triangle.W];
			}
			if (TangentU.Normalize() && TangentV.Normalize())
			{
				Location = ComponentTransform.TransformPosition(Location);
				ComponentTransform.SetRotation(ComponentTransform.TransformRotation((FQuat)FRotationMatrix44f::MakeFromXY(TangentV, TangentU).Rotator().Quaternion()));
				ComponentTransform.SetLocation(Location);
				return ComponentTransform;
			}
		}
	}

	ComponentTransform.SetLocation(ComponentTransform.TransformPosition(Location));
	return ComponentTransform;
}

void UHoudiniMeshComponent::FrustumSelect(const FConvexVolume& Frustum)
{
	const FTransform& ComponentTransform = GetComponentTransform();
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		TArray<int32> SelectedPointIndices;
		for (int32 PointIdx = 0; PointIdx < Positions.Num(); ++PointIdx)
		{
			if (Frustum.IntersectPoint(ComponentTransform.TransformPosition(FVector(Positions[PointIdx]))))
				SelectedPointIndices.Add(PointIdx);
		}
		SelectedIndices = SelectedPointIndices;
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TArray<int32> SelectedCurveIndices;
		for (int32 PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx)
		{
			for (const int32& PointIdx : Polys[PolyIdx].PointIndices)
			{
				if (Frustum.IntersectPoint(ComponentTransform.TransformPosition(FVector(Positions[PointIdx]))))
				{
					SelectedCurveIndices.Add(PolyIdx);
					break;
				}
			}
		}
		SelectedIndices = SelectedCurveIndices;
	}
}

void UHoudiniMeshComponent::SphereSelect(const FVector& Centroid, const float& Radius, const bool& bAppend)
{
	const FTransform ComponentTransform = GetComponentTransform();
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (bAppend)
		{
			TSet<int32> SelectedIdxSet = TSet<int32>(SelectedIndices);
			for (int32 PointIdx = 0; PointIdx < Positions.Num(); ++PointIdx)
			{
				if (!SelectedIdxSet.Contains(PointIdx) &&
					FVector::Distance(ComponentTransform.TransformPosition(FVector(Positions[PointIdx])), Centroid) < Radius)
					SelectedIndices.Add(PointIdx);
			}
		}
		else
		{
			SelectedIndices.Empty();
			for (int32 PointIdx = 0; PointIdx < Positions.Num(); ++PointIdx)
			{
				if (FVector::Distance(ComponentTransform.TransformPosition(FVector(Positions[PointIdx])), Centroid) < Radius)
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
			for (int32 PrimIdx = 0; PrimIdx < Polys.Num(); ++PrimIdx)
			{
				if (SelectedIdxSet.Contains(PrimIdx))
					continue;

				for (const int32& PointIdx : Polys[PrimIdx].PointIndices)
				{
					if (FVector::Distance(ComponentTransform.TransformPosition(FVector(Positions[PointIdx])), Centroid) < Radius)
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
			for (int32 PrimIdx = 0; PrimIdx < Polys.Num(); ++PrimIdx)
			{
				for (const int32& PointIdx : Polys[PrimIdx].PointIndices)
				{
					if (FVector::Distance(ComponentTransform.TransformPosition(FVector(Positions[PointIdx])), Centroid) < Radius)
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


void UHoudiniMeshComponent::GetPointsBounds(const TArray<int32>& PointIndices,
	FVector& OutMin, FVector& OutMax) const
{
	const FTransform& ComponentTransform = GetComponentTransform();

	const FVector FirstPosition = ComponentTransform.TransformPosition(FVector(Positions[PointIndices[0]]));
	OutMin = FirstPosition;
	OutMax = OutMin;
	for (const int32& PointIdx : PointIndices)
	{
		const FVector Position = ComponentTransform.TransformPosition(FVector(Positions[PointIdx]));
		OutMin = FVector::Min(OutMin, Position);
		OutMax = FVector::Max(OutMax, Position);
	}
}

bool UHoudiniMeshComponent::GetSelectionBoundingBox(FBox& OutBoundingBox) const
{
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		if (SelectedIndices.Num() <= 1)
			return false;

		GetPointsBounds(SelectedIndices, OutBoundingBox.Min,OutBoundingBox.Max);
		return true;
	}
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)
	{
		TArray<int32> SelectedPointIndices;
		for (const int32& SelectedPolyIdx : SelectedIndices)
			SelectedPointIndices.Append(Polys[SelectedPolyIdx].PointIndices);

		if (SelectedPointIndices.Num() <= 1)
			return false;

		GetPointsBounds(SelectedPointIndices, OutBoundingBox.Min, OutBoundingBox.Max);
		return true;
	}

	return false;
}

int32 UHoudiniMeshComponent::RayCast(const FRay& ClickRay, FVector& OutRayCastPos) const
{
	const FTransform& ComponentTransform = GetComponentTransform();
	const FVector TargetPos = ClickRay.PointAt(99999999.0);

	int32 ElemIdx = -1;
	double MinDist = -1.0;
	if (SelectedClass == EHoudiniAttributeOwner::Point)
	{
		for (int32 PointIdx = 0; PointIdx < Positions.Num(); ++PointIdx)
		{
			const FVector WorldPos = ComponentTransform.TransformPosition((FVector)Positions[PointIdx]);
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
		WorldPositions.Reserve(Positions.Num());
		for (const FVector3f& LocalPos : Positions)
			WorldPositions.Add(ComponentTransform.TransformPosition((FVector)LocalPos));

		for (int32 PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx)
		{
			const TArray<int32>& PointIndices = Polys[PolyIdx].PointIndices;
			for (const FIntVector4& Triangle : Polys[PolyIdx].Triangles)
			{
				FVector IntersectPos;
				FVector TriangleNormal;
				if (FMath::SegmentTriangleIntersection(ClickRay.Origin, TargetPos,
					WorldPositions[PointIndices[Triangle.X]], WorldPositions[PointIndices[Triangle.Y]], WorldPositions[PointIndices[Triangle.Z]],
					IntersectPos, TriangleNormal))
				{
					const double Distance = FVector::Distance(IntersectPos, ClickRay.Origin);
					if ((MinDist > Distance) || (ElemIdx < 0))
					{
						MinDist = Distance;
						OutRayCastPos = IntersectPos;
						ElemIdx = PolyIdx;
					}
				}
			}
		}
	}

	return ElemIdx;
}

FBoxSphereBounds UHoudiniMeshComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	if (Positions.IsEmpty())
		return FBoxSphereBounds();

	FVector BBoxMin = InLocalToWorld.TransformPosition(FVector(Positions[0]));
	FVector BBoxMax = BBoxMin;
	for (int32 PointIdx = Positions.Num() - 1; PointIdx >= 1; --PointIdx)
	{
		const FVector Position = InLocalToWorld.TransformPosition(FVector(Positions[PointIdx]));
		BBoxMin = FVector::Min(BBoxMin, Position);
		BBoxMax = FVector::Max(BBoxMax, Position);
	}

	return FBox(BBoxMin, BBoxMax);
}

void UHoudiniMeshComponent::ResetMeshData()
{
	Positions.Empty();
	Triangles.Empty();

	Sections.Empty();

	NormalOwner = EHoudiniAttributeOwner::Invalid;
	NormalData.Empty();

	TangentUOwner = EHoudiniAttributeOwner::Invalid;
	TangentUData.Empty();

	TangentVOwner = EHoudiniAttributeOwner::Invalid;
	TangentVData.Empty();

	ColorOwner = EHoudiniAttributeOwner::Invalid;
	ColorData.Empty();

	UVs.Empty();

	Polys.Empty();
	Edges.Empty();
}

#define TRANSFORM_POSITIONS(TRANSFORM_POSITION) const FTransform& ComponentTransform = GetComponentTransform();\
	if (SelectedClass == EHoudiniAttributeOwner::Point)\
	{\
		for (const int32& SelectedPointIdx : SelectedIndices)\
		{\
			FVector3f& Position = Positions[SelectedPointIdx];\
			TRANSFORM_POSITION\
		}\
	}\
	else if (SelectedClass == EHoudiniAttributeOwner::Prim)\
	{\
		TSet<int32> SelectedPointIndices;\
		for (const int32 SelectedPolyIdx : SelectedIndices)\
			SelectedPointIndices.Append(Polys[SelectedPolyIdx].PointIndices);\
		for (const int32& SelectedPointIdx : SelectedPointIndices)\
		{\
			FVector3f& Position = Positions[SelectedPointIdx];\
			TRANSFORM_POSITION\
		}\
	}

void UHoudiniMeshComponent::TranslateSelection(const FVector& DeltaTranslate)
{
	TRANSFORM_POSITIONS(Position = (FVector3f)ComponentTransform.InverseTransformPosition(
		ComponentTransform.TransformPosition((FVector)Position) + DeltaTranslate););
}

void UHoudiniMeshComponent::RotateSelection(const FRotator& DeltaRotate, const FVector& Pivot)
{
	TRANSFORM_POSITIONS(Position = FVector3f(Pivot + DeltaRotate.RotateVector((FVector)Position - Pivot)););
}

void UHoudiniMeshComponent::ScaleSelection(const FVector& DeltaScale, const FVector& Pivot)
{
	TRANSFORM_POSITIONS(Position = FVector3f(Pivot + (DeltaScale + FVector::OneVector) * ((FVector)Position - Pivot)););
}

bool UHoudiniMeshComponent::HapiUpload(const TArray<UHoudiniMeshComponent*>& HMCs,
	const int32& SHMInputNodeId, size_t& InOutHandle)
{
	size_t NumPoints = 0;
	size_t NumPrims = 0;
	size_t NumVertices = 0;
	EHoudiniAttributeOwner ChangedClass = EHoudiniAttributeOwner::Invalid;
	TArray<UHoudiniParameterAttribute*> PointAttribs;
	TArray<UHoudiniParameterAttribute*> PrimAttribs;
	const FString& EditGroupName = UHoudiniEditableGeometry::ParseEditData(TConstArrayView<const UHoudiniEditableGeometry*>((const UHoudiniEditableGeometry**)HMCs.GetData(), HMCs.Num()),
		NumVertices, NumPoints, NumPrims, ChangedClass, PointAttribs, PrimAttribs);


	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumPrims, NumVertices);
	SHMGeoInput.AppendGroup(TCHAR_TO_UTF8(*EditGroupName), EHoudiniAttributeOwner::Prim, 1, true);
	const bool bPointSelected = ChangedClass == EHoudiniAttributeOwner::Point;
	const bool bPrimSelected = ChangedClass == EHoudiniAttributeOwner::Prim;
	SHMGeoInput.AppendGroup(HAPI_GROUP_CHANGED, EHoudiniAttributeOwner::Point, bPointSelected ? NumPoints : 1, !bPointSelected);
	SHMGeoInput.AppendGroup(HAPI_GROUP_CHANGED, EHoudiniAttributeOwner::Prim, bPrimSelected ? NumPrims : 1, !bPrimSelected);

	for (const UHoudiniParameterAttribute* Attrib : PointAttribs)
		Attrib->UploadInfo(SHMGeoInput);

	for (const UHoudiniParameterAttribute* Attrib : PrimAttribs)
		Attrib->UploadInfo(SHMGeoInput);


	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("mesh_%08x"), SHMInputNodeId), InOutHandle);


	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	int32* EditGroupDataPtr = VertexDataPtr + NumVertices + NumPrims; *EditGroupDataPtr = 1;
	int32* ChangedPointGroupDataPtr = EditGroupDataPtr + 1;
	int32* ChangedPrimGroupDataPtr = ChangedPointGroupDataPtr + (bPointSelected ? NumPoints : 1);

	float* AttribDataPtr = (float*)(ChangedPrimGroupDataPtr + (bPrimSelected ? NumPrims : 1));
	for (const UHoudiniParameterAttribute* Attrib : PointAttribs)
		Attrib->UploadData(AttribDataPtr);

	for (const UHoudiniParameterAttribute* Attrib : PrimAttribs)
		Attrib->UploadData(AttribDataPtr);


	NumPoints = 0;  // Here represent the previous count, and the start index
	NumPrims = 0;  // Here represent the previous count, and the start index
	for (const UHoudiniMeshComponent* HMC : HMCs)
	{
		for (FVector3f Position : HMC->Positions)
		{
			Position *= POSITION_SCALE_TO_HOUDINI_F;
			PositionDataPtr[0] = Position.X;
			PositionDataPtr[1] = Position.Z;
			PositionDataPtr[2] = Position.Y;
			PositionDataPtr += 3;
		}

		for (const FHoudiniMeshPoly& Poly : HMC->Polys)
		{
			for (int32 LocalVtxIdx = Poly.PointIndices.Num() - 1; LocalVtxIdx >= 0; --LocalVtxIdx)
			{
				*VertexDataPtr = NumPoints + Poly.PointIndices[LocalVtxIdx];
				++VertexDataPtr;
			}

			*VertexDataPtr = -1;
			++VertexDataPtr;
		}

		if (!HMC->DeltaInfo.IsEmpty())
		{
			if (ChangedClass == EHoudiniAttributeOwner::Point && HMC->SelectedClass == EHoudiniAttributeOwner::Point)
			{
				for (const int32& SelectedIdx : HMC->SelectedIndices)
					ChangedPointGroupDataPtr[NumPoints + SelectedIdx] = 1;
			}
			else if (ChangedClass == EHoudiniAttributeOwner::Prim && HMC->SelectedClass == EHoudiniAttributeOwner::Prim)
			{
				for (const int32& SelectedIdx : HMC->SelectedIndices)
					ChangedPrimGroupDataPtr[NumPrims + SelectedIdx] = 1;
			}
		}

		NumPoints += HMC->Positions.Num();
		NumPrims += HMC->Polys.Num();
	}

	// We should empty delta info, to mark this EditGeo unchanged
	for (UHoudiniMeshComponent* HMC : HMCs)
		HMC->ResetDeltaInfo();

	return SHMGeoInput.HapiUpload(SHMInputNodeId, SHM);
}
