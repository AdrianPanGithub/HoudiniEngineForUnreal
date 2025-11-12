// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniCurvesComponentVisualizer.h"

#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "EngineUtils.h"
#include "Engine/Font.h"
#include "CanvasTypes.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "EditorViewportClient.h"

#include "HoudiniEngine.h"
#include "HoudiniCurvesComponent.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEngineStyle.h"
#include "HoudiniEditableGeometryEditorUtils.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void FHoudiniCurvesComponentVisualizer::DrawVisualization(const UActorComponent* Component,
	const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UHoudiniCurvesComponent* CurvesComponent = Cast<UHoudiniCurvesComponent>(Component);
	if (!IsValid(CurvesComponent) || !CurvesComponent->GetVisibleFlag())
		return;

	const bool bShowPoint = uint32(CurvesComponent->GetPointEditOptions() & EHoudiniEditOptions::Hide) == 0;
	const bool bShowCurve = uint32(CurvesComponent->GetPrimEditOptions() & EHoudiniEditOptions::Hide) == 0;
	if (!bShowPoint && !bShowCurve)
		return;

	const FTransform& ComponentTransform = CurvesComponent->GetComponentTransform();
	
	const float DPIScale = FHoudiniEngineEditorUtils::FindEditorViewportClient(Component->GetWorld())->GetDPIScale();

	const FHoudiniEditableGeometryVisualSettings* Settings = FHoudiniEditableGeometryVisualSettings::Load();
	const float PointSize = Settings->PointSize * DPIScale;
	const float EdgeThickness = Settings->EdgeThickness * DPIScale;
	const bool& bDistanceCulling = Settings->bDistanceCulling;
	const float& CullDistance = Settings->CullDistance;

	// -------- Points --------
	const TArray<FHoudiniCurvePoint>& Points = CurvesComponent->GetPoints();
	TArray<bool> bPointsShouldDisplay;  // Cache results for curve edge shown
	if (bShowPoint)
	{
		if (bDistanceCulling)
			bPointsShouldDisplay.SetNumZeroed(Points.Num());
		for (int32 PointIdx = 0; PointIdx < Points.Num(); ++PointIdx)
		{
			const FHoudiniCurvePoint& Point = Points[PointIdx];
			if (Point.Color.A < 0.5f)
				continue;

			const FVector Position = ComponentTransform.TransformPosition(Point.Transform.GetLocation());
			if (bDistanceCulling)
			{
				if (FVector::Distance(Position, View->CullingOrigin) > CullDistance)
					continue;

				bPointsShouldDisplay[PointIdx] = true;
			}

			PDI->SetHitProxy(new HHoudiniPointVisProxy(CurvesComponent, HPP_UI, PointIdx));
			PDI->DrawPoint(Position,
				CurvesComponent->IsPointSelected(PointIdx) ? FLinearColor(Point.Color) * FLinearColor(1.0f, 0.2f, 0.1f) : Point.Color,
				PointSize, SDPG_Foreground);
			PDI->SetHitProxy(nullptr);
		}
	}

	// -------- Curves --------
	if (!bShowCurve)
		return;

	const TArray<FHoudiniCurve>& Curves = CurvesComponent->GetCurves();
	for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
	{
		const FHoudiniCurve& Curve = Curves[CurveIdx];
		if (Curve.NoNeedDisplay())
			continue;
		
		if (bDistanceCulling)
		{
			bool bCull = true;
			for (const int32& PointIdx : Curve.PointIndices)
			{
				if (bPointsShouldDisplay.IsValidIndex(PointIdx))
				{
					if (bPointsShouldDisplay[PointIdx])
					{
						bCull = false;
						break;
					}
				}
				else
				{
					const FVector Position = ComponentTransform.TransformPosition(Points[PointIdx].Transform.GetLocation());
					if (FVector::Distance(Position, View->CullingOrigin) < CullDistance)
					{
						bCull = false;
						break;
					}
				}
			}
			if (bCull)
				continue;
		}

		if (Curve.DisplayPoints.IsEmpty())  // We should generate display points
			const_cast<UHoudiniCurvesComponent*>(CurvesComponent)->RefreshCurveDisplayPoints(CurveIdx);

		const bool bCurveSelected = CurvesComponent->IsPrimSelected(CurveIdx);
		const TArray<FVector>& DisplayPoints = Curve.DisplayPoints;
		const FLinearColor CurveColor = bCurveSelected ? FLinearColor(Curve.Color) * FLinearColor(1.0f, 0.2f, 0.1f) : Curve.Color;

		PDI->SetHitProxy(new HHoudiniPrimVisProxy(CurvesComponent, HPP_UI, CurveIdx));

		FVector PrevPos = ComponentTransform.TransformPosition(DisplayPoints[0]);
		for (int32 DisplayIdx = 1; DisplayIdx < DisplayPoints.Num(); ++DisplayIdx)
		{
			const FVector CurrPos = ComponentTransform.TransformPosition(DisplayPoints[DisplayIdx]);
			PDI->DrawLine(PrevPos, CurrPos, CurveColor,
				SDPG_Foreground, EdgeThickness, 0.0f, true);
			PrevPos = CurrPos;
		}
		
		// Draw curve handles, only for subdiv and bezier
		const TArray<int32>& PointIndices = Curve.PointIndices;
		if (PointIndices.Num() >= 3)
		{
			if (Curve.Type == EHoudiniCurveType::Subdiv)
			{
				PrevPos = ComponentTransform.TransformPosition(
					Points[Curve.bClosed ? PointIndices.Last() : PointIndices[0]].Transform.GetLocation());
				for (int32 VtxIdx = !Curve.bClosed; VtxIdx < PointIndices.Num(); ++VtxIdx)
				{
					const FVector CurrPos = ComponentTransform.TransformPosition(Points[PointIndices[VtxIdx]].Transform.GetLocation());
					PDI->DrawLine(PrevPos, CurrPos, CurveColor,
						SDPG_Foreground);
					PrevPos = CurrPos;
				}
			}
			else if (Curve.Type == EHoudiniCurveType::Bezier)
			{
				const int32 NumVertices = PointIndices.Num() + Curve.bClosed;
				const int32 NumSubBeziers = NumVertices / 3;
				PrevPos = ComponentTransform.TransformPosition(Points[PointIndices[0]].Transform.GetLocation());
				for (int32 SubIdx = 0; SubIdx < NumSubBeziers; ++SubIdx)
				{
					PDI->DrawLine(PrevPos, ComponentTransform.TransformPosition(Points[PointIndices[SubIdx * 3 + 1]].Transform.GetLocation()),
						CurveColor, SDPG_Foreground);
					
					const int32 CurrIdx = SubIdx * 3 + 3;
					if (CurrIdx <= PointIndices.Num() + Curve.bClosed - 1)
					{
						const FVector CurrPos = ComponentTransform.TransformPosition(
							Points[PointIndices[CurrIdx % PointIndices.Num()]].Transform.GetLocation());
						PDI->DrawLine(ComponentTransform.TransformPosition(Points[PointIndices[(SubIdx * 3 + 2) % PointIndices.Num()]].Transform.GetLocation()), CurrPos,
							CurveColor, SDPG_Foreground);
						PrevPos = CurrPos;
					}
				}
			}
		}

		PDI->SetHitProxy(nullptr);
	}
}

static FRay GetMouseRayFromView(const FViewport* Viewport, const FSceneView* View)
{
	// See FViewportCursorLocation::FViewportCursorLocation
	const FVector4 ScreenPos = View->CursorToScreen(Viewport->GetMouseX(), Viewport->GetMouseY(), 0);
	const FMatrix InvViewMatrix = View->ViewMatrices.GetInvViewMatrix();
	const FMatrix InvProjMatrix = View->ViewMatrices.GetInvProjectionMatrix();

	const double ScreenX = ScreenPos.X;
	const double ScreenY = ScreenPos.Y;

	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	if (View->IsPerspectiveProjection())
	{
		Origin = View->ViewMatrices.GetViewOrigin();
		Direction = InvViewMatrix.TransformVector(FVector(InvProjMatrix.TransformFVector4(FVector4(ScreenX * GNearClippingPlane, ScreenY * GNearClippingPlane, 0.0f, GNearClippingPlane)))).GetSafeNormal();
	}
	else
	{
		Origin = InvViewMatrix.TransformFVector4(InvProjMatrix.TransformFVector4(FVector4(ScreenX, ScreenY, 0.5f, 1.0f)));
		Direction = InvViewMatrix.TransformVector(FVector(0, 0, 1)).GetSafeNormal();
	}

	return FRay(Origin, Direction);
}

void FHoudiniCurvesComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component,
	const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	const UHoudiniCurvesComponent* CurvesComponent = Cast<UHoudiniCurvesComponent>(Component);
	if (!IsValid(CurvesComponent) || !CurvesComponent->IsGeometrySelected())
		return;

	const FHoudiniEditableGeometryVisualSettings* Settings = FHoudiniEditableGeometryVisualSettings::Load();
	const float DPIScale = 1.0f / Canvas->GetDPIScale();
	if (IsDrawing())
		Canvas->DrawNGon(FVector2D(Viewport->GetMouseX(), Viewport->GetMouseY()) * DPIScale,
			CurvesComponent->GetDefaultCurveColor(), 4, Settings->PointSize * Canvas->GetDPIScale() * 0.25f);
	else if (FSlateApplication::Get().GetModifierKeys().IsShiftDown() &&
		(ComponentPropertyPath.GetComponent() == CurvesComponent))
	{
		FVector InsertPos;
		int32 CurveIdx, DisplayIdx;
		if (CurvesComponent->GetInsertPosition(View->ViewFrustum, GetMouseRayFromView(Viewport, View), InsertPos, CurveIdx, DisplayIdx))
		{
			FVector2D PixelPos;
			View->WorldToPixel(InsertPos, PixelPos);
			Canvas->DrawNGon(PixelPos * DPIScale,
				CurvesComponent->GetDefaultCurveColor(), 4, Settings->PointSize * Canvas->GetDPIScale() * 0.25f);
		}
	}
	const bool& bShowPointCoordinate = Settings->bShowPointCoordinate;
	const bool& bShowEdgeLength = Settings->bShowEdgeLength;
	if (!bShowPointCoordinate && !bShowEdgeLength)
		return;

	const FTransform& ComponentTransform = CurvesComponent->GetComponentTransform();

	const bool& bDistanceCulling = Settings->bDistanceCulling;
	const float& CullDistance = Settings->CullDistance;

	static const UFont* MetricDisplayFont = nullptr;
	if (!MetricDisplayFont)
		MetricDisplayFont = FindFirstObject<UFont>(TEXT("Font'/Engine/EngineFonts/Roboto.Roboto'"), EFindFirstObjectOptions::NativeFirst);

	const int32 NumPoints = CurvesComponent->GetPoints().Num();
	TArray<bool> bPointsShouldDisplay;  // Cache results for curve edge shown
	if (bDistanceCulling)
		bPointsShouldDisplay.SetNumZeroed(NumPoints);
	TArray<FVector> Positions;
	Positions.SetNumUninitialized(NumPoints);  // We will cache the positions whenever bShowPointCoordinate or not, for curve length shown
	const TArray<FHoudiniCurvePoint>& Points = CurvesComponent->GetPoints();
	for (int32 PointIdx = 0; PointIdx < NumPoints; ++PointIdx)
	{
		const FHoudiniCurvePoint& Point = Points[PointIdx];
		FVector Position = ComponentTransform.TransformPosition(Point.Transform.GetLocation());
		Positions[PointIdx] = Position;

		if (bDistanceCulling)
		{
			if (FVector::Distance(Position, View->CullingOrigin) > CullDistance)
				continue;
		}

		if (View->ViewFrustum.IntersectPoint(Position))
		{
			if (bDistanceCulling)
				bPointsShouldDisplay[PointIdx] = true;

			if (bShowPointCoordinate)
			{
				FVector2D PixelPos;
				View->WorldToPixel(Position, PixelPos);
				Position *= POSITION_SCALE_TO_HOUDINI;
				PixelPos *= DPIScale;
				Canvas->DrawShadowedString(PixelPos.X, PixelPos.Y,
					*FString::Printf(TEXT("  X:%.02f, Y:%.02f, Z:%.02f"), Position.X, Position.Y, Position.Z),
					MetricDisplayFont,
					(CurvesComponent->IsPointSelected(PointIdx) ? FLinearColor(Point.Color) * HOUDINI_EDIT_GEO_SELECTED_COLOR : Point.Color) * 0.8f);
			}
		}
	}

	if (!bShowEdgeLength)
		return;

	const TArray<FHoudiniCurve>& Curves = CurvesComponent->GetCurves();
	for (int32 CurveIdx = 0; CurveIdx < Curves.Num(); ++CurveIdx)
	{
		const FHoudiniCurve& Curve = Curves[CurveIdx];
		if (Curve.NoNeedDisplay())
			continue;

		const FLinearColor CurveColor = CurvesComponent->IsPrimSelected(CurveIdx) ?
			FLinearColor(Curve.Color) * HOUDINI_EDIT_GEO_SELECTED_COLOR : Curve.Color;

		const TArray<int32>& PointIndices = Curve.PointIndices;
		const int32 NumVtcs = Curve.PointIndices.Num();
		for (int32 VtxIdx = 0; VtxIdx < NumVtcs + Curve.bClosed - 1; ++VtxIdx)
		{
			const int32& CurrPointIdx = PointIndices[VtxIdx];
			const int32& NextPointIdx = PointIndices[(VtxIdx >= NumVtcs - 1) ? 0 : (VtxIdx + 1)];
			if (bDistanceCulling)
			{
				if (!bPointsShouldDisplay[CurrPointIdx] && !bPointsShouldDisplay[NextPointIdx])
					continue;
			}
				
			const FVector& CurrPos = Positions[CurrPointIdx];
			const FVector& NextPos = Positions[NextPointIdx];
			FVector MidPos = (CurrPos + NextPos) * 0.5;
			if (View->ViewFrustum.IntersectPoint(MidPos)) {
				FVector2D PixelPos;
				View->WorldToPixel(MidPos, PixelPos);

				PixelPos /= Canvas->GetDPIScale();
				Canvas->DrawShadowedString(PixelPos.X, PixelPos.Y,
					*(FString::Printf(TEXT("%.02fm"), FVector::Dist(CurrPos, NextPos) * POSITION_SCALE_TO_HOUDINI)), MetricDisplayFont, CurveColor);
			}
		}
	}
}

bool FHoudiniCurvesComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient,
	FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Key == EKeys::LeftMouseButton)
	{
		if (Event == IE_DoubleClick)
		{
			if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
			{
				CurvesComponent->ExpandSelection();
				return true;
			}
		}
		else if (Event == IE_Pressed)
		{
			if (IsDrawing())
			{
				CurrViewportClient = ViewportClient;
				if (ViewportClient->IsShiftPressed())
				{
					FreeHandDrawDeltaTime = 0.0f;
					FreeHandTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
						FTickerDelegate::CreateRaw(this, &FHoudiniCurvesComponentVisualizer::FreeHandDraw));
				}
				else
					Draw();

				return true;
			}
			else if (ViewportClient->IsAltPressed() && !ViewportClient->IsCtrlPressed())  // If both alt and ctrl pressed, means we are frustum selecting
			{
				if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
				{
					CurvesComponent->DuplicateSelection();
					bTransforming = true;
					bDuplicated = true;
				}
			}
		}
		else if (Event == IE_Released)
		{
			if (FreeHandTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(FreeHandTickerHandle);
				FreeHandTickerHandle.Reset();
			}

			if (bTransforming)
			{
				if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
				{
					if (bDuplicated)
						CurvesComponent->EndDrawing();
					else
						CurvesComponent->EndTransforming(AccumulatedTransform, Pivot.GetLocation());
				}

				AccumulatedTransform = FTransform::Identity;
				Pivot = FTransform::Identity;
				bTransforming = false;
				bDuplicated = false;
			}
			else if (!IsDrawing() && ViewportClient->IsShiftPressed())  // Insert point on curves
			{
				if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
				{
					// FEditorViewportClient::GetCursorWorldLocationFromMousePos()
					FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
						Viewport,
						ViewportClient->GetScene(),
						ViewportClient->EngineShowFlags)
						.SetRealtimeUpdate(ViewportClient->IsRealtime()));  // Create the scene view context
					FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);  // Calculate the scene view
					CurvesComponent->InsertPoint(View->ViewFrustum, GetMouseRayFromView(Viewport, View));
				}
				return true;
			}
		}
	}
	if (Key == EKeys::End && Event == IE_Released)
	{
		if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
		{
			CurvesComponent->ProjectSelection();
			return true;
		}
	}
	else if (Key == EKeys::Enter)
	{
		if (Event == IE_Pressed)
		{
			if (IsDrawing())
			{
				EndDrawing();
				return true;
			}
			else
			{
				if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
				{
					DrawingTransactionIndex = GEditor->BeginTransaction(TEXT(HOUDINI_ENGINE),
						LOCTEXT("HoudiniCurvesDrawing", "Houdini Curves: Drawing"), CurvesComponent);

					CurvesComponent->BeginDrawing();
					BeginDrawing(EHoudiniCurveDrawingType::Click);

					return true;
				}
			}
		}
	}
	else if ((Key == EKeys::A) && (Event == IE_Pressed) && ViewportClient->IsCtrlPressed())
	{
		if (!IsDrawing())
		{
			if (UHoudiniCurvesComponent* CurvesComponent = GetEditingGeometry<UHoudiniCurvesComponent>())
			{
				CurvesComponent->SelectAll();
				return true;
			}
		}
	}

	
	if (IsDrawing())  // Mark all input key as handled, so only thing we need to handle is EKeys::LeftMouseButton when drawing
	{
		return !(Key == EKeys::RightMouseButton || Key == EKeys::MiddleMouseButton ||
			Key == EKeys::O || Key == EKeys::L || Key == EKeys::Period || Key == EKeys::Comma || Key == EKeys::Slash);  // Some editgeo visual shortcut
	}
	
	
	else if (Key == EKeys::Delete && Event == IE_Pressed)
	{
		if (UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>())
		{
			CurvesComponent->RemoveSelection();
			GEditor->RedrawLevelEditingViewports(true);
			RefreshEditorModeAttributePanel(CurvesComponent);
		}
		return true;
	}

	return false;
}

bool FHoudiniCurvesComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (!FHoudiniEngine::Get().AllowEdit())
		return true;

	UHoudiniCurvesComponent* CurvesComponent = GetSelectedGeometry<UHoudiniCurvesComponent>();
	if (!CurvesComponent)
		return false;

	if (!bTransforming)
	{
		if (!bDuplicated)
			CurvesComponent->StartTransforming(Pivot.GetLocation());

		bTransforming = true;
	}

	if (!DeltaTranslate.IsZero())
		CurvesComponent->TranslateSelection(DeltaTranslate);
	else if (!DeltaRotate.IsZero())
		CurvesComponent->RotateSelection(DeltaRotate, Pivot.GetLocation());
	else if (!DeltaScale.IsZero())
		CurvesComponent->ScaleSelection(DeltaScale, Pivot.GetLocation());

	AccumulatedTransform.Accumulate(FTransform(DeltaRotate, DeltaTranslate, DeltaScale + FVector::OneVector));

	return true;
}

void FHoudiniCurvesComponentVisualizer::EndEditing()
{
	EndDrawing();
	
	FHoudiniEditableGeometryVisualizer::EndEditing();
}


void FHoudiniCurvesComponentVisualizer::AddCurve(UHoudiniCurvesComponent* CurvesComponent,
	const EHoudiniCurveDrawingType& TargetDrawingType, const bool& bSelectThis)
{
	if (IsDrawing())  // If drawing, then we just end the last draw and return
	{
		EndDrawing();
		return;
	}

	// We must reselect the actors, so that the comp-vis could display the component
	if (bSelectThis)
	{
		GEditor->SelectNone(false, false, false);
		GEditor->SelectActor(CurvesComponent->GetOwner(), true, true, true, true);
	}
	else
		FHoudiniEngineEditorUtils::ReselectSelectedActors();

	// Find active LevelViewport
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();
	if (!ActiveLevelViewport.IsValid())
		return;

	// Force disable GameView
	if (ActiveLevelViewport->IsInGameView())
		ActiveLevelViewport->ToggleGameView();

	// Activate CurvesComponentVisualizer
	TSharedPtr<FComponentVisualizer> Visualizer = FHoudiniEngineEditor::Get().GetCurvesComponentVisualizer();
	GUnrealEd->ComponentVisManager.SetActiveComponentVis(
		&ActiveLevelViewport->GetAssetViewportClient(), Visualizer);

	ComponentPropertyPath = FComponentPropertyPath(CurvesComponent);
	
	DrawingTransactionIndex = GEditor->BeginTransaction(TEXT(HOUDINI_ENGINE),
		LOCTEXT("HoudiniCurvesDrawing", "Houdini Curves: Drawing"), CurvesComponent);
	
	CurvesComponent->AddNewCurve();

	// Finally we could start drawing
	BeginDrawing(TargetDrawingType);
}

void FHoudiniCurvesComponentVisualizer::BeginDrawing(const EHoudiniCurveDrawingType& TargetDrawingType)
{
	DrawingType = TargetDrawingType;

	if (!DrawingNotification.IsValid())
	{
		FNotificationInfo Info(TargetDrawingType == EHoudiniCurveDrawingType::Click ?
			LOCTEXT("CurveDrawing", "Drawing... Click In Viewport\nPress Enter To End/Start") : (TargetDrawingType == EHoudiniCurveDrawingType::Circle ?
			LOCTEXT("CircleDrawing", "Drawing... Triple Click In Viewport\nTo Define a Circle") :
			LOCTEXT("RectDrawing", "Drawing... Triple Click In Viewport\nTo Define a Rectangle")));
		Info.bFireAndForget = false;
		Info.FadeInDuration = 0.1f;
		Info.ExpireDuration = 0.0f;
		Info.FadeOutDuration = 0.1f;
		Info.Image = FHoudiniEngineStyle::Get()->GetBrush("HoudiniEngine.CurveDraw");
		DrawingNotification = FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void FHoudiniCurvesComponentVisualizer::Draw()
{
	UHoudiniCurvesComponent* CurvesComponent = Cast<UHoudiniCurvesComponent>(ComponentPropertyPath.GetComponent());
	if (!IsValid(CurvesComponent))
		return;

	if (DrawingTransactionIndex >= 0)
	{
		CurvesComponent->Modify();
		CurvesComponent->ModifyAllAttributes();
	}

	const FViewportCursorLocation CursorLocation = CurrViewportClient->GetCursorWorldLocationFromMousePos();
	const FVector TraceStart = CursorLocation.GetOrigin();
	const FVector Direction = CursorLocation.GetDirection();
	const FVector TraceEnd = TraceStart + (Direction * 10000000.0);

	FCollisionQueryParams TraceParams = FCollisionQueryParams::DefaultQueryParam;
	for (TActorIterator<AActor> ActorIter(GEditor->GetEditorWorldContext().World()); ActorIter; ++ActorIter)
	{
		const AActor* Actor = *ActorIter;
		if (IsValid(Actor) && Actor->IsHiddenEd())
			TraceParams.AddIgnoredActor(Actor);	
	}
	
	const FTransform& ComponentTransform = CurvesComponent->GetComponentTransform();
	const bool bCtrlPressed = CurrViewportClient->IsCtrlPressed();

	FHitResult HitResult(ForceInit);
	if (CurrViewportClient->GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
	{
		FHoudiniCurvePoint NewPoint(HOUDINI_RANDOM_ID);
		NewPoint.Transform.SetLocation(ComponentTransform.InverseTransformPosition(HitResult.Location));
		NewPoint.CollisionActorName = HitResult.GetActor()->GetFName();
		NewPoint.CollisionNormal = HitResult.Normal;
		NewPoint.Color = CurvesComponent->GetDefaultCurveColor();
		CurvesComponent->AddPoint(NewPoint, bCtrlPressed);
	}
	else  // Fallback to draw on XY plane
	{
		double TraceLength = -TraceStart.Z / Direction.Z;
		if (TraceLength >= 0.0)
		{
			FHoudiniCurvePoint NewPoint(HOUDINI_RANDOM_ID);
			NewPoint.Transform.SetLocation(ComponentTransform.InverseTransformPosition(
				FVector(TraceStart.X + TraceLength * Direction.X, TraceStart.Y + TraceLength * Direction.Y, 0.0)));
			NewPoint.Color = CurvesComponent->GetDefaultCurveColor();
			CurvesComponent->AddPoint(NewPoint, bCtrlPressed);
		}
	}

	const TArray<int32>& TempPointIndices = CurvesComponent->GetCurves().Last().PointIndices;
	if ((DrawingType == EHoudiniCurveDrawingType::Rectangle || DrawingType == EHoudiniCurveDrawingType::Circle) &&
		TempPointIndices.Num() >= 3)
	{
		const TArray<FHoudiniCurvePoint>& Points = CurvesComponent->GetPoints();
		FVector TempPoints[3] = {
			ComponentTransform.TransformPosition(Points[TempPointIndices[0]].Transform.GetLocation()),
			ComponentTransform.TransformPosition(Points[TempPointIndices[1]].Transform.GetLocation()),
			ComponentTransform.TransformPosition(Points[TempPointIndices[2]].Transform.GetLocation()) };

		EHoudiniCurveType& CurveType = *(const_cast<EHoudiniCurveType*>(&(CurvesComponent->GetCurves().Last().Type)));
		if (DrawingType == EHoudiniCurveDrawingType::Rectangle)
		{
			const FVector ShapeNormal = FVector::CrossProduct(
				(TempPoints[0] - TempPoints[1]).GetUnsafeNormal(), (TempPoints[1] - TempPoints[2]).GetUnsafeNormal()).GetUnsafeNormal();
			const FVector SideDir = FVector::CrossProduct((TempPoints[0] - TempPoints[1]).GetUnsafeNormal(), ShapeNormal).GetUnsafeNormal();
			const_cast<FTransform*>(&Points[TempPointIndices[2]].Transform)->SetLocation(
				ComponentTransform.InverseTransformPosition(TempPoints[1] + SideDir * FVector::Distance(TempPoints[0], TempPoints[1])));
			FHoudiniCurvePoint LastPoint = Points[TempPointIndices[2]];
			LastPoint.Transform.SetLocation(
				ComponentTransform.InverseTransformPosition(TempPoints[0] + SideDir * FVector::Distance(TempPoints[0], TempPoints[1])));
			CurvesComponent->AddPoint(LastPoint, false);
			CurveType = EHoudiniCurveType::Polygon;
		}
		else  // Draw Circle
		{
			const FVector& Center = TempPoints[0];
			const FVector FirstDir = (TempPoints[1] - Center).GetUnsafeNormal();
			const FVector ShapeNormal = FVector::CrossProduct(
				FirstDir, (TempPoints[2] - TempPoints[1]).GetUnsafeNormal()).GetUnsafeNormal();
			const double Radius = FVector::Distance(TempPoints[1], Center);

			if (CurveType == EHoudiniCurveType::Bezier)  // Use 4 bezier arcs to construct a circle
			{
				static const double BezierArcBias = 0.5517;
				const FVector CircleDirs[4] = {
					FQuat(ShapeNormal, 0).RotateVector(FirstDir), FQuat(ShapeNormal, PI * 0.5f).RotateVector(FirstDir),
					FQuat(ShapeNormal, PI).RotateVector(FirstDir), FQuat(ShapeNormal, PI * 1.5f).RotateVector(FirstDir), };
				for (int32 SubBezierIdx = 0; SubBezierIdx < 4; ++SubBezierIdx)
				{
					const FVector& Dir0 = CircleDirs[SubBezierIdx];
					const FVector& Dir1 = CircleDirs[(SubBezierIdx + 1) % 4];
					const FVector Pos0 = Center + Dir0 * Radius;
					const FVector Pos1 = Pos0 + Dir1 * Radius * BezierArcBias;
					const FVector Pos2 = Center + Dir0 * Radius * BezierArcBias + Dir1 * Radius;
					if (SubBezierIdx == 0)
					{
						const_cast<FTransform*>(&(Points[TempPointIndices[0]].Transform))->SetLocation(Pos0);
						const_cast<FTransform*>(&(Points[TempPointIndices[1]].Transform))->SetLocation(Pos1);
						const_cast<FTransform*>(&(Points[TempPointIndices[2]].Transform))->SetLocation(Pos2);
					}
					else
					{
						FHoudiniCurvePoint Point = Points[TempPointIndices[0]];
						Point.Transform.SetLocation(Pos0);
						CurvesComponent->AddPoint(Point, false);
						Point.Transform.SetLocation(Pos1);
						CurvesComponent->AddPoint(Point, false);
						Point.Transform.SetLocation(Pos2);
						CurvesComponent->AddPoint(Point, false);
					}
				}
			}
			else
			{
				static const int32 NumCircleDivs = 12;
				static const double DivRad = 2.0 * PI / NumCircleDivs;

				for (int32 CirclePointIdx = -1; CirclePointIdx < NumCircleDivs - 1; ++CirclePointIdx)
				{
					const FVector Position = ComponentTransform.InverseTransformPosition(Center + FQuat(ShapeNormal, DivRad * CirclePointIdx).RotateVector(FirstDir) * Radius);
					if (-1 <= CirclePointIdx && CirclePointIdx <= 1)
						const_cast<FTransform*>(&(Points[TempPointIndices[CirclePointIdx + 1]].Transform))->SetLocation(Position);
					else
					{
						FHoudiniCurvePoint LastPoint = Points[TempPointIndices[0]];
						LastPoint.Transform.SetLocation(ComponentTransform.InverseTransformPosition(Position));
						CurvesComponent->AddPoint(LastPoint, false);
					}
				}

				if (CurveType == EHoudiniCurveType::Points)
					CurveType = EHoudiniCurveType::Interpolate;
			}
		}

		*(const_cast<bool*>(&(CurvesComponent->GetCurves().Last().bClosed))) = true;
		CurvesComponent->RefreshCurveDisplayPoints(CurvesComponent->GetCurves().Num() - 1);

		EndDrawing();
	}
}

bool FHoudiniCurvesComponentVisualizer::FreeHandDraw(float DeltaTime)
{
	FreeHandDrawDeltaTime += DeltaTime;
	if (FreeHandDrawDeltaTime > 0.05f)
	{
		Draw();
		FreeHandDrawDeltaTime = 0.0f;
	}

	return true;
}

void FHoudiniCurvesComponentVisualizer::EndDrawing()
{
	if (IsDrawing())
	{
		if (DrawingTransactionIndex >= 0)
		{
			GEditor->EndTransaction();
			DrawingTransactionIndex = -1;
		}

		if (UHoudiniCurvesComponent* CurvesComponent = GetEditingGeometry<UHoudiniCurvesComponent>())
		{
			CurvesComponent->EndDrawing();
			GEditor->RedrawLevelEditingViewports(true);
			RefreshEditorModeAttributePanel(CurvesComponent);
		}

		DrawingType = EHoudiniCurveDrawingType::None;
	}
	
	if (DrawingNotification.IsValid())
	{
		DrawingNotification.Pin()->ExpireAndFadeout();
		DrawingNotification.Reset();
	}
}

void FHoudiniCurvesComponentVisualizer::TryEndDrawingForParentNode(const AHoudiniNode* Node)
{
	if (!IsDrawing())
		return;

	UHoudiniCurvesComponent* CurvesComponent = GetEditingGeometry<UHoudiniCurvesComponent>();
	if (CurvesComponent && (CurvesComponent->GetParentNode() == Node))
		EndDrawing();
}

#undef LOCTEXT_NAMESPACE
