// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"

#include "HoudiniTools.generated.h"


// -------- Manage --------
UCLASS()
class UHoudiniManageToolBuilder : public UInteractiveToolBuilder
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


UCLASS(Abstract)
class HOUDINIENGINEEDITOR_API UHoudiniBrushTool : public UInteractiveTool
{
	GENERATED_BODY()

protected:
	UWorld* TargetWorld = nullptr;  // Target World we will raycast into

public:
	virtual void Setup() override;

	virtual void OnMouseUpdate(const FRay& MouseRay, const bool& bDragging, const bool bShiftDown) {}
	virtual bool CanBeginClickDragSequence() const { return false; }
	virtual void OnClickRelease() {}
	virtual void ChangeBrushSize(const bool& bIncrease) {}
};

UCLASS()
class UHoudiniBrushInputBehavior : public UInputBehavior
{
	GENERATED_BODY()

public:
	FORCEINLINE UHoudiniBrushTool* GetTool() const { return static_cast<UHoudiniBrushTool*>(GetOuter()); }

	EInputDevices virtual GetSupportedDevices() override { return EInputDevices::Mouse | EInputDevices::Keyboard; }
	virtual FInputCaptureRequest WantsCapture(const FInputDeviceState& InputState) override;
	virtual FInputCaptureUpdate BeginCapture(const FInputDeviceState& InputState, EInputCaptureSide eSide) override;
	virtual FInputCaptureUpdate UpdateCapture(const FInputDeviceState& InputState, const FInputCaptureData& CaptureData) override;
	virtual void ForceEndCapture(const FInputCaptureData& CaptureData) override;

	virtual bool WantsHoverEvents() override { return true; }
	virtual FInputCaptureRequest WantsHoverCapture(const FInputDeviceState& InputState) override;
};


// -------- Mask --------
class UHoudiniInputMask;
class AHoudiniMaskGizmoActiveActor;

UCLASS()
class UHoudiniMaskToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniMaskTool : public UHoudiniBrushTool
{
	GENERATED_BODY()

protected:
	static UHoudiniInputMask* PendingTarget;  // If is valid, then means we need to activate

	int32 TransactionIdx = -1;

public:
	TWeakObjectPtr<AHoudiniMaskGizmoActiveActor> MaskActor;

	static void ForceActivate(UHoudiniInputMask* MaskInput);
	static bool IsPendingActivate();
	void SetWorld(UWorld* World);
	
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnMouseUpdate(const FRay& MouseRay, const bool& bDragging, const bool bShiftDown) override;
	virtual bool CanBeginClickDragSequence() const override;
	virtual void OnClickRelease() override;
	virtual void ChangeBrushSize(const bool& bIncrease) override;
};


// -------- Edit --------
class UHoudiniParameter;

UCLASS()
class UHoudiniEditToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditTool : public UHoudiniBrushTool
{
	GENERATED_BODY()

protected:
	static bool bForceActivated;

	FVector HitPosition;  // Cached in OnMouseUpdate(), used in Render()

	TArray<TWeakObjectPtr<UHoudiniParameter>> BrushAttribParms;

public:
	float BrushSize = 5000.0f;

	static void ForceActivate();
	FORCEINLINE static bool IsPendingActivate() { return bForceActivated; };
	FORCEINLINE void SetWorld(UWorld* World) { TargetWorld = World; }

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

	virtual void OnMouseUpdate(const FRay& MouseRay, const bool& bDragging, const bool bShiftDown) override;
	virtual bool CanBeginClickDragSequence() const override;
	virtual void OnClickRelease() override;
	virtual void ChangeBrushSize(const bool& bIncrease) override;
};
