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

	TArray<FVector> Positions;
	{
		const FTransform& ComponentTransform = MeshComponent->GetComponentTransform();
		const TArray<FVector3f>& MeshPositions = MeshComponent->GetPositions();
		Positions.SetNumUninitialized(MeshPositions.Num());
		for (int32 PointIdx = 0; PointIdx < MeshPositions.Num(); ++PointIdx)
			Positions[PointIdx] = ComponentTransform.TransformPosition((FVector)MeshPositions[PointIdx]);
	}
	
	// -------- Points --------
	if (bShowPoint)
	{
		for (int32 PointIdx = 0; PointIdx < Positions.Num(); ++PointIdx)
		{
			const FVector& Position = Positions[PointIdx];
			if (bDistanceCulling)
			{
				if (FVector::Distance(Position, View->CullingOrigin) > CullDistance)
					continue;
			}

			PDI->SetHitProxy(new HHoudiniPointVisProxy(MeshComponent, HPP_UI, PointIdx));
			PDI->DrawPoint(Position,
				MeshComponent->IsPointSelected(PointIdx) ? HOUDINI_EDIT_GEO_SELECTED_COLOR : FLinearColor::White, PointSize, SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}
	}

	// -------- Edges --------
	for (const FIntVector2& Edge : MeshComponent->GetEdges())
		PDI->DrawLine(Positions[Edge.X], Positions[Edge.Y], FLinearColor::Gray, SDPG_World);

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
				PDI->DrawLine(Positions[Poly.PointIndices[VtxIdx]], Positions[Poly.PointIndices[(VtxIdx == 0) ? (Poly.PointIndices.Num() - 1) : VtxIdx - 1]],
					FLinearColor(0.9f, 0.3f, 0.0f), SDPG_World);
		}

		if (bDistanceCulling)
		{
			bool bCull = true;
			for (const int32& PointIdx : Poly.PointIndices)
			{
				if (FVector::Distance(Positions[PointIdx], View->CullingOrigin) < CullDistance)
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
			MeshBuilder.AddVertex(FDynamicMeshVertex(FVector3f(Positions[PointIndex])));

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

	if (!bTransforming)
	{
		MeshComponent->StartTransforming(Pivot.GetLocation());

		bTransforming = true;
	}

	if (!DeltaTranslate.IsZero())
		MeshComponent->TranslateSelection(DeltaTranslate);
	else if (!DeltaRotate.IsZero())
		MeshComponent->RotateSelection(DeltaRotate, Pivot.GetLocation());
	else if (!DeltaScale.IsZero())
		MeshComponent->ScaleSelection(DeltaScale, Pivot.GetLocation());

	AccumulatedTransform.Accumulate(FTransform(DeltaRotate, DeltaTranslate, DeltaScale + FVector::OneVector));

	return true;
}

#undef LOCTEXT_NAMESPACE
