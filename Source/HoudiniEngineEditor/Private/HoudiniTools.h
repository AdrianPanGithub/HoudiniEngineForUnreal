// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"

#include "HoudiniTools.generated.h"


// -------- Manage --------
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniManageToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniManageTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
};


// -------- Mask --------
class UHoudiniInputMask;
class AHoudiniMaskGizmoActiveActor;
class UModeManagerInteractiveToolsContext;

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniMaskToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniMaskTool : public UInteractiveTool, public IHoverBehaviorTarget, public IClickDragBehaviorTarget
{
	GENERATED_BODY()

protected:
	static UHoudiniInputMask* PendingTarget;  // If is valid, then means we need to activate

	UWorld* TargetWorld = nullptr;		// Target World we will raycast into

	int32 TransactionIdx = -1;

	static const int InverseBrushModifierID = 2;

	bool bBrushInversed = false;
	
	virtual void OnMouseUpdate(const FInputDeviceRay& DevicePos, const bool& bDragging);

public:
	TWeakObjectPtr<AHoudiniMaskGizmoActiveActor> MaskActor;

	FORCEINLINE UWorld* GetWorld() const { return TargetWorld; }

	static void ForceActivate(UHoudiniInputMask* MaskInput);

	static bool IsPendingActivate();
	
	virtual void SetWorld(UWorld* World);

	// -------- UInteractiveTool --------
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	
	// -------- IHoverBehaviorTarget --------
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override { return FInputRayHit(true); }
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override {}

	// -------- IClickDragBehaviorTarget --------
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override {}

	// -------- IModifierToggleBehaviorTarget implementation (inherited via IClickDragBehaviorTarget) --------
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;
};


// -------- Edit --------
class UHoudiniParameter;

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditTool : public UInteractiveTool, public IHoverBehaviorTarget, public IClickDragBehaviorTarget
{
	GENERATED_BODY()

protected:
	static bool bForceActivated;

	UWorld* TargetWorld = nullptr;		// Target World we will raycast into

	FVector HitPosition;  // Cached in OnMouseUpdate(), used in Render()

	TArray<TWeakObjectPtr<UHoudiniParameter>> BrushAttribParms;

	virtual void OnMouseUpdate(const FInputDeviceRay& DevicePos, const bool& bDragging);

public:
	float BrushSize = 8192.0f;

	
	static void ForceActivate();

	FORCEINLINE static bool IsPendingActivate() { return bForceActivated; };

	virtual void SetWorld(UWorld* World);

	// -------- BrushAttribParms --------
	void ToggleAttribute(const TWeakObjectPtr<UHoudiniParameter>& AttribParm);

	void ClearAttributes();
	
	void AddAllAttributes();

	FORCEINLINE bool IsAttributeBrushing(const TWeakObjectPtr<UHoudiniParameter>& AttribParm) const { return BrushAttribParms.Contains(AttribParm); }

	FORCEINLINE bool IsBrushing() const { return !BrushAttribParms.IsEmpty(); }

	void CleanupBrushAttributes();  // If no editing geo, then will empty BrushAttribParms

	FText GetBrushAttributesText() const;

	// -------- UInteractiveTool --------
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	// -------- IHoverBehaviorTarget --------
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override { return FInputRayHit(true); }
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override {}

	// -------- IClickDragBehaviorTarget --------
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override {}
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override {}
};
