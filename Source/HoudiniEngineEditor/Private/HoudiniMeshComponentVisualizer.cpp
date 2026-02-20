// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniMeshComponentVisualizer.h"

#include "DynamicMeshBuilder.h"

#include "EditorModes.h"
#include "EditorViewportClient.h"

#include "HoudiniEngine.h"
#include "HoudiniMeshComponent.h"

#include "HoudiniEditableGeometryEditorUtils.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void FHoudiniMeshComponentVisualizer::DrawVisualization(const UActorComponent* Component,
	const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UHoudiniMeshComponent* MeshComponent = Cast<UHoudiniMeshComponent>(Component);
	if (!IsValid(MeshComponent))
		return;

	const TArray<FHoudiniMeshPoly>& Polys = MeshComponent->GetPolys();
	if (Polys.IsEmpty())  // Means this HoudiniMesh does NOT editable
		return;

	const bool bShowPoint = uint32(MeshComponent->GetPointEditOptions() & EHoudiniEditOptions::Show) >= 1;
	const bool bShowPoly = uint32(MeshComponent->GetPrimEditOptions() & EHoudiniEditOptions::Show) >= 1;
	if (!bShowPoint && !bShowPoly)
		return;

	const FHoudiniEditableGeometryVisualSettings* Settings = FHoudiniEditableGeometryVisualSettings::Load();
	const float& PointSize = Settings->PointSize;
	const bool& bDistanceCulling = Settings->bDistanceCulling;
	const float& CullDistance = Settings->CullDistance;

	struct FMeshPoint
	{
		FVector Position;
		bool bCull;
	};
	TArray<FMeshPoint> MeshPoints;
	{
		const FTransform& ComponentTransform = MeshComponent->GetComponentTransform();
		const TArray<FVector3f>& MeshPositions = MeshComponent->GetPositions();
		MeshPoints.SetNumUninitialized(MeshPositions.Num());
		if (bDistanceCulling)
		{
			for (int32 PointIdx = 0; PointIdx < MeshPositions.Num(); ++PointIdx)
			{
				FMeshPoint& MeshPoint = MeshPoints[PointIdx];
				MeshPoint.Position = ComponentTransform.TransformPosition((FVector)MeshPositions[PointIdx]);
				MeshPoint.bCull = (FVector::Distance(MeshPoint.Position, View->CullingOrigin) >= CullDistance);
			}
		}
		else
		{
			for (int32 PointIdx = 0; PointIdx < MeshPositions.Num(); ++PointIdx)
			{
				FMeshPoint& MeshPoint = MeshPoints[PointIdx];
				MeshPoint.Position = ComponentTransform.TransformPosition((FVector)MeshPositions[PointIdx]);
				MeshPoint.bCull = false;
			}
		}
	}
	
	// -------- Points --------
	if (bShowPoint)
	{
		for (int32 PointIdx = 0; PointIdx < MeshPoints.Num(); ++PointIdx)
		{
			const FMeshPoint& MeshPoint = MeshPoints[PointIdx];
			if (MeshPoint.bCull)
				continue;

			PDI->SetHitProxy(new HHoudiniPointVisProxy(MeshComponent, HPP_UI, PointIdx));
			PDI->DrawPoint(MeshPoint.Position,
				MeshComponent->IsPointSelected(PointIdx) ? HOUDINI_EDIT_GEO_SELECTED_COLOR : FLinearColor::White, PointSize, SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}
	}

	// -------- Edges --------
	for (const FIntVector2& Edge : MeshComponent->GetEdges())
	{
		const FMeshPoint& EdgePt0 = MeshPoints[Edge.X];
		const FMeshPoint& EdgePt1 = MeshPoints[Edge.Y];
		if (EdgePt0.bCull && EdgePt1.bCull)
			continue;

		PDI->DrawLine(EdgePt0.Position, EdgePt1.Position, FLinearColor::Gray, SDPG_World);
	}

	// -------- Polys --------
	if (!bShowPoly)
		return;

	const float EyeAdaptation = View->GetLastEyeAdaptationExposure();
	const float Exposure = (EyeAdaptation <= 0.0f) ? 1.0f : (2.5f / EyeAdaptation);
	FDynamicColoredMaterialRenderProxy* SelectedColorInstance = new FDynamicColoredMaterialRenderProxy(
		GEngine->GeomMaterial->GetRenderProxy(), FLinearColor(0.9f * Exposure, 0.3f * Exposure, 0.0f, 0.3f));
	PDI->RegisterDynamicResource(SelectedColorInstance);
	FDynamicColoredMaterialRenderProxy* NonSelectedColorInstance = new FDynamicColoredMaterialRenderProxy(
		GEngine->GeomMaterial->GetRenderProxy(), FLinearColor(Exposure, Exposure, Exposure, MeshComponent->IsGeometrySelected() ? 0.05f : 0.02f));
	PDI->RegisterDynamicResource(NonSelectedColorInstance);

	for (int32 PolyIdx = 0; PolyIdx < Polys.Num(); ++PolyIdx)
	{
		const FHoudiniMeshPoly& Poly = Polys[PolyIdx];

		const bool bIsPolySelected = MeshComponent->IsPrimSelected(PolyIdx);
		if (bIsPolySelected)
		{
			for (int32 VtxIdx = 0; VtxIdx < Poly.PointIndices.Num(); ++VtxIdx)
				PDI->DrawLine(MeshPoints[Poly.PointIndices[VtxIdx]].Position, MeshPoints[Poly.PointIndices[(VtxIdx == 0) ? (Poly.PointIndices.Num() - 1) : VtxIdx - 1]].Position,
					FLinearColor(0.9f, 0.3f, 0.0f), SDPG_World);
		}

		if (bDistanceCulling)
		{
			bool bCull = true;
			for (const int32& PointIdx : Poly.PointIndices)
			{
				if (!MeshPoints[PointIdx].bCull)
				{
					bCull = false;
					break;
				}
			}
			if (bCull)
				continue;
		}

		PDI->SetHitProxy(new HHoudiniPrimVisProxy(Component, HPP_Foreground, PolyIdx));

		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
		for (const int32& PointIndex : Poly.PointIndices)
			MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(MeshPoints[PointIndex].Position)));

		for (const FIntVector4& Triangle : Poly.Triangles)
			MeshBuilder.AddTriangle(Triangle.X, Triangle.Y, Triangle.Z);

		MeshBuilder.Draw(PDI, FMatrix::Identity, bIsPolySelected ? SelectedColorInstance : NonSelectedColorInstance,
			SDPG_World, false, false);

		PDI->SetHitProxy(nullptr);
	}
}

bool FHoudiniMeshComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient,
	FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Key == EKeys::LeftMouseButton)
	{
		if (Event == IE_DoubleClick)
		{
			if (UHoudiniMeshComponent* MeshComponent = GetSelectedGeometry<UHoudiniMeshComponent>())
			{
				MeshComponent->ExpandSelection();
				return true;
			}
		}
		else if (Event == IE_Released)
		{
			if (bTransforming)
			{
				if (UHoudiniMeshComponent* MeshComponent = GetSelectedGeometry<UHoudiniMeshComponent>())
					MeshComponent->EndTransforming(AccumulatedTransform, Pivot.GetLocation());

				AccumulatedTransform = FTransform::Identity;
				Pivot = FTransform::Identity;
				bTransforming = false;
			}
		}
	}
	else if ((Key == EKeys::A) && (Event == IE_Pressed) && ViewportClient->IsCtrlPressed())
	{
		if (UHoudiniMeshComponent* MeshComponent = GetEditingGeometry<UHoudiniMeshComponent>())
		{
			MeshComponent->SelectAll();
			return true;
		}
	}

	return false;
}

bool FHoudiniMeshComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (!FHoudiniEngine::Get().AllowEdit())
		return true;

	UHoudiniMeshComponent* MeshComponent = GetSelectedGeometry<UHoudiniMeshComponent>();
	if (!MeshComponent)
		return false;

	const bool bNoTranslate = DeltaTranslate.IsZero();
	const bool bNoRotate = DeltaRotate.IsZero();
	const bool bNoScale = DeltaScale.IsZero();
	if (bNoTranslate && bNoRotate && bNoScale)
		return true;

	if (!bTransforming)
	{
		MeshComponent->StartTransforming(Pivot.GetLocation());

		bTransforming = true;
	}

	if (!bNoTranslate)
		MeshComponent->TranslateSelection(DeltaTranslate);
	else if (!bNoRotate)
		MeshComponent->RotateSelection(DeltaRotate, Pivot.GetLocation());
	else if (!bNoScale)
		MeshComponent->ScaleSelection(DeltaScale, Pivot.GetLocation());

	AccumulatedTransform.Accumulate(FTransform(DeltaRotate, DeltaTranslate, DeltaScale + FVector::OneVector));

	return true;
}

#undef LOCTEXT_NAMESPACE
