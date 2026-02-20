// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HitProxies.h"
#include "ComponentVisualizer.h"


class UHoudiniEditableGeometry;


struct HHoudiniPointVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniPointVisProxy(const UActorComponent* InComponent, const EHitProxyPriority& InPriority, const int32& InPointIdx)
		: HComponentVisProxy(InComponent, InPriority)
		, PointIdx(InPointIdx)
	{}

	int32 PointIdx;
	virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::CardinalCross; }
};

struct HHoudiniPrimVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	HHoudiniPrimVisProxy(const UActorComponent* InComponent, const EHitProxyPriority& InPriority, const int32& InPolyIdx)
		: HComponentVisProxy(InComponent, InPriority)
		, PrimIdx(InPolyIdx)
	{}

	int32 PrimIdx;

	virtual bool AlwaysAllowsTranslucentPrimitives() const override { return true; }
	virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::Default; }
};

class HOUDINIENGINEEDITOR_API FHoudiniEditableGeometryVisualizer : public FComponentVisualizer
{
protected:
	FComponentPropertyPath ComponentPropertyPath;

	// --------- Transform --------
	FTransform AccumulatedTransform = FTransform::Identity;

	bool bTransforming = false;

	mutable FTransform Pivot = FTransform::Identity;

	static void RefreshEditorModeAttributePanel(UHoudiniEditableGeometry* EditGeo);

public:
	//virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	
	//virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	
	//virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	
	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;

	//virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;

	virtual void EndEditing() override;
	
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;
	
	virtual bool HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) override;

	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override;


	UWorld* GetEditingWorld() const;

	template<typename TEditableGeometryClass>
	FORCEINLINE TEditableGeometryClass* GetSelectedGeometry() const  // Will check whether the editing geometry is actually being selected
	{
		TEditableGeometryClass* EditGeo = Cast<TEditableGeometryClass>((ComponentPropertyPath.GetComponent()));
		return (IsValid(EditGeo) && EditGeo->IsGeometrySelected()) ? EditGeo : nullptr;  // After remove curve or point, the selected indices will be empty
	}

	template<typename TEditableGeometryClass>
	FORCEINLINE TEditableGeometryClass* GetEditingGeometry() const
	{
		TEditableGeometryClass* EditGeo = Cast<TEditableGeometryClass>((ComponentPropertyPath.GetComponent()));
		return IsValid(EditGeo) ? EditGeo : nullptr;
	}
};
