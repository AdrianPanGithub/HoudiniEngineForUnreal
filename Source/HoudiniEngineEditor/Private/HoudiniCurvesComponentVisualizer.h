// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEditableGeometryVisualizer.h"


class AHoudiniNode;
class UHoudiniCurvesComponent;


enum class EHoudiniCurveDrawingType
{
	None = 0,
	Click,
	Rectangle,
	Circle
};

class HOUDINIENGINEEDITOR_API FHoudiniCurvesComponentVisualizer : public FHoudiniEditableGeometryVisualizer
{
protected:
	EHoudiniCurveDrawingType DrawingType = EHoudiniCurveDrawingType::None;

	FORCEINLINE bool IsDrawing() const { return DrawingType != EHoudiniCurveDrawingType::None; }

	int32 DrawingTransactionIndex = -1;  // Only When Drawing that Transaction is done by FHoudiniCurvesComponentVisualizer, otherwise, done by HoudiniCurvesComponent

	TWeakPtr<class SNotificationItem> DrawingNotification;

	void BeginDrawing(const EHoudiniCurveDrawingType& TargetDrawingType);

	void Draw();

	FEditorViewportClient* CurrViewportClient = nullptr;  // Hold for FreeHandDraw()

	FTSTicker::FDelegateHandle FreeHandTickerHandle;

	float FreeHandDrawDeltaTime = 0.0f;

	bool FreeHandDraw(float DeltaTime);  // This method should only be used to add to FTSTicker.
	
	bool bDuplicated = false;  // For Alt-duplication

public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	
	virtual void EndEditing() override;
	

	void AddCurve(UHoudiniCurvesComponent* CurvesComponent,
		const EHoudiniCurveDrawingType& TargetDrawingType, const bool& bSelectThis);

	void EndDrawing();

	void TryEndDrawingForParentNode(const AHoudiniNode* Node);  // If is drawing, but user trigger cook by adjust parameters, then we should force to end drawing
};
