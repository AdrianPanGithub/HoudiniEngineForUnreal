// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniTools.h"

#include "InteractiveToolManager.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "CollisionQueryParams.h"
#include "ToolDataVisualizer.h"

#include "EditorModeManager.h"

#include "HoudiniEngine.h"
#include "HoudiniInputs.h"
#include "HoudiniParameter.h"
#include "HoudiniEditableGeometry.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniAttributeParameterHolder.h"
#include "HoudiniMaskGizmoActiveActor.h"


#define LOCTEXT_NAMESPACE "UHoudiniMaskTool"

// -------- Manage --------
UInteractiveTool* UHoudiniManageToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UHoudiniManageTool* NewTool = NewObject<UHoudiniManageTool>(SceneState.ToolManager);
	return NewTool;
}

void UHoudiniManageTool::Setup()
{
	UInteractiveTool::Setup();

	AddToolPropertySource(this);

	AsyncTask(ENamedThreads::GameThread, [] { FHoudiniEngineEditorUtils::DisableOverrideEngineShowFlags(); });  // As ToolManager will disable AA after tool setup, we need enable it at next tick
}

// -------- Mask --------
UInteractiveTool* UHoudiniMaskToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UHoudiniMaskTool* NewTool = NewObject<UHoudiniMaskTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}

UHoudiniInputMask* UHoudiniMaskTool::PendingTarget = nullptr;

void UHoudiniMaskTool::ForceActivate(UHoudiniInputMask* MaskInput)
{
	PendingTarget = MaskInput;
	
	if (UEdMode* HoudiniEd = GLevelEditorModeTools().GetActiveScriptableMode("EM_HoudiniEngine"))
	{
		HoudiniEd->GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("HoudiniEngine_MaskTool"));
		HoudiniEd->GetToolManager()->ActivateTool(EToolSide::Left);
	}
	else
		GLevelEditorModeTools().ActivateMode("EM_HoudiniEngine");
	
	PendingTarget = nullptr;
}

bool UHoudiniMaskTool::IsPendingActivate()
{
	return IsValid(PendingTarget);
}



void UHoudiniMaskTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
	MaskActor = AHoudiniMaskGizmoActiveActor::FindOrCreate(World, PendingTarget);
	PendingTarget = nullptr;
}

void UHoudiniMaskTool::Setup()
{
	UInteractiveTool::Setup();

	// Add mouse input behavior
	UClickDragInputBehavior* DragBehavior = NewObject<UClickDragInputBehavior>(); 
	DragBehavior->Modifiers.RegisterModifier(InverseBrushModifierID, FInputDeviceState::IsShiftKeyDown);  // This call tells the Behavior to call our OnUpdateModifierState() function on mouse-down and mouse-move
	DragBehavior->Initialize(this);
	AddInputBehavior(DragBehavior);

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);
	
	AddToolPropertySource(this);

	AsyncTask(ENamedThreads::GameThread, [] { FHoudiniEngineEditorUtils::DisableOverrideEngineShowFlags(); });  // As ToolManager will disable AA after tool setup, we need enable it at next tick
}

void UHoudiniMaskTool::OnUpdateModifierState(int ModifierID, bool bIsOn)
{
	// keep track of the "invert brush" modifier (shift key for mouse input)
	if (ModifierID == InverseBrushModifierID)
	{
		bBrushInversed = bIsOn;
	}
}


void UHoudiniMaskTool::OnMouseUpdate(const FInputDeviceRay& DevicePos, const bool& bDragging)
{
	if (MaskActor.IsValid())
	{
		MaskActor->UpdateBrushPosition(DevicePos.WorldRay);

		if (bDragging)
			MaskActor->OnBrush(bBrushInversed);
	}
}

bool UHoudiniMaskTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	OnMouseUpdate(DevicePos, false);
	return true;
}

FInputRayHit UHoudiniMaskTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	return (FHoudiniEngine::Get().AllowEdit() && MaskActor.IsValid() && MaskActor->CanBrush()) ? FInputRayHit(true) : FInputRayHit();
}

void UHoudiniMaskTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	if (MaskActor.IsValid() && MaskActor->GetMaskInput().IsValid())
	{
		TransactionIdx = GEditor->BeginTransaction(TEXT(HOUDINI_ENGINE),
			LOCTEXT("HoudiniInputMaskChanged", "HoudiniInputMaskChanged"), MaskActor->GetMaskInput().Get());
		MaskActor->GetMaskInput()->ModifyAll();

		OnMouseUpdate(PressPos, true);
	}
}

void UHoudiniMaskTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	OnMouseUpdate(DragPos, true);
}

void UHoudiniMaskTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (MaskActor.IsValid())
		MaskActor->EndBrush();

	if (TransactionIdx >= 0)
	{
		GEditor->EndTransaction();
		TransactionIdx = -1;
	}
}

void UHoudiniMaskTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (MaskActor.IsValid())
		MaskActor->Exit();

	if (TransactionIdx >= 0)
	{
		GEditor->EndTransaction();
		TransactionIdx = -1;
	}
}


// -------- Edit --------
UInteractiveTool* UHoudiniEditToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UHoudiniEditTool* NewTool = NewObject<UHoudiniEditTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}


bool UHoudiniEditTool::bForceActivated = false;

void UHoudiniEditTool::ForceActivate()
{
	bForceActivated = true;

	if (UEdMode* HoudiniEd = GLevelEditorModeTools().GetActiveScriptableMode("EM_HoudiniEngine"))
	{
		HoudiniEd->GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("HoudiniEngine_EditTool"));
		HoudiniEd->GetToolManager()->ActivateTool(EToolSide::Left);
	}
	else
		GLevelEditorModeTools().ActivateMode("EM_HoudiniEngine");

	bForceActivated = false;
}

#define CHECK_ATTRIB_BRUSH_TURN_OFF(ATTRIB_BRUSH_TOGGLE) const bool bWasBurshing = UHoudiniParameter::GDisableAttributeActions;\
	ATTRIB_BRUSH_TOGGLE\
	UHoudiniParameter::GDisableAttributeActions = !BrushAttribParms.IsEmpty();\
	if (bWasBurshing && !UHoudiniParameter::GDisableAttributeActions)\
		UHoudiniAttributeParameterHolder::Get()->RefreshParameters(nullptr);  // Parm values will NOT update while brushing, so we should force refresh value here

void UHoudiniEditTool::ToggleAttribute(const TWeakObjectPtr<UHoudiniParameter>& AttribParm)
{
	CHECK_ATTRIB_BRUSH_TURN_OFF(
	if (BrushAttribParms.Remove(AttribParm) == 0)  // Previous does NOT have this parm
		BrushAttribParms.Add(AttribParm);
	)
}

void UHoudiniEditTool::ClearAttributes()
{
	CHECK_ATTRIB_BRUSH_TURN_OFF(
	BrushAttribParms.Empty();
	)
}

void UHoudiniEditTool::AddAllAttributes()
{
	CHECK_ATTRIB_BRUSH_TURN_OFF(
	BrushAttribParms.Empty();
	for (UHoudiniParameter* AttribParm : UHoudiniAttributeParameterHolder::Get()->GetParameters())
		BrushAttribParms.Add(AttribParm);
	)
}

void UHoudiniEditTool::CleanupBrushAttributes()
{
	const TWeakObjectPtr<UHoudiniEditableGeometry>& EditGeo = UHoudiniAttributeParameterHolder::Get()->GetEditableGeometry();
	if (!EditGeo.IsValid())
	{
		ClearAttributes();
		return;
	}

	CHECK_ATTRIB_BRUSH_TURN_OFF(
	BrushAttribParms.RemoveAll([](const TWeakObjectPtr<UHoudiniParameter>& AttribParm)
		{
			return !AttribParm.IsValid() || !UHoudiniAttributeParameterHolder::Get()->GetParameters().Contains(AttribParm);
		});
	)
}

FText UHoudiniEditTool::GetBrushAttributesText() const
{
	FString AttribsStr;
	for (const TWeakObjectPtr<UHoudiniParameter>& AttribParm : BrushAttribParms)
	{
		if (AttribParm.IsValid())
		{
			if (!AttribsStr.IsEmpty())
				AttribsStr += TEXT(" | ");
			AttribsStr += AttribParm->GetLabel().IsEmpty() ? AttribParm->GetParameterName() : AttribParm->GetLabel();
		}
	}

	return FText::FromString(AttribsStr);
}

void UHoudiniEditTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UHoudiniEditTool::Setup()
{
	UInteractiveTool::Setup();

	// Add mouse input behaviors
	UClickDragInputBehavior* DragBehavior = NewObject<UClickDragInputBehavior>();
	DragBehavior->Initialize(this);
	AddInputBehavior(DragBehavior);

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	AddToolPropertySource(this);
	AddToolPropertySource(UHoudiniAttributeParameterHolder::Get(FHoudiniEngineEditor::Get().GetSelectedGeometry()));  // We should refresh AttribParmHolder first, then construct EdtiTool Details

	AsyncTask(ENamedThreads::GameThread, [] { FHoudiniEngineEditorUtils::DisableOverrideEngineShowFlags(); });  // As ToolManager will disable AA after tool setup, we need enable it at next tick
}

void UHoudiniEditTool::Shutdown(EToolShutdownType ShutdownType)
{
	UHoudiniAttributeParameterHolder::Get()->DetailsView.Reset();  // Notify attribute panel is closed
	UHoudiniParameter::GDisableAttributeActions = false;
}

void UHoudiniEditTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (BrushAttribParms.IsEmpty())
		return;

	// Draw a sphere represent brush region
	FToolDataVisualizer Draw;
	Draw.BeginFrame(RenderAPI);

	Draw.DrawCircle(HitPosition, FVector(1.0, 0.0, 0.0), BrushSize, 24, FLinearColor::White, 0.0, false);
	Draw.DrawCircle(HitPosition, FVector(0.0, 1.0, 0.0), BrushSize, 24, FLinearColor::White, 0.0, false);
	Draw.DrawCircle(HitPosition, FVector(0.0, 0.0, 1.0), BrushSize, 24, FLinearColor::White, 0.0, false);

	Draw.EndFrame();
}


void UHoudiniEditTool::OnMouseUpdate(const FInputDeviceRay& DevicePos, const bool& bDragging)
{
	// We should NOT change selection before cook finished, because attrib panel will refresh while cook finished, if selection changed, then brush value may also changed
	if (!FHoudiniEngine::Get().AllowEdit() || BrushAttribParms.IsEmpty())
		return;

	const TWeakObjectPtr<UHoudiniEditableGeometry>& EditGeo = UHoudiniAttributeParameterHolder::Get()->GetEditableGeometry();
	if (!EditGeo.IsValid() || EditGeo->HasChanged())
		return;

	const FRay& MouseRay = DevicePos.WorldRay;
	const FVector& TraceStart = MouseRay.Origin;
	const FVector& Direction = MouseRay.Direction;

	FHitResult Hit(ForceInit);
	if (GetWorld()->LineTraceSingleByChannel(Hit, MouseRay.Origin,
		MouseRay.Origin + MouseRay.Direction * 999999.0, ECC_Visibility, FCollisionQueryParams::DefaultQueryParam))
		HitPosition = Hit.Location;
	else
	{
		double TraceLength = -TraceStart.Z / Direction.Z;
		if (TraceLength >= 0.0)
			HitPosition = FVector(TraceStart.X + TraceLength * Direction.X, TraceStart.Y + TraceLength * Direction.Y, 0.0);
	}

	UHoudiniAttributeParameterHolder::Get()->GetEditableGeometry()->SphereSelect(HitPosition, BrushSize, bDragging);
}

bool UHoudiniEditTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	OnMouseUpdate(DevicePos, false);
	return true;
}

FInputRayHit UHoudiniEditTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	return (FHoudiniEngine::Get().AllowEdit() && !BrushAttribParms.IsEmpty()) ? FInputRayHit(true) : FInputRayHit();
}

void UHoudiniEditTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	OnMouseUpdate(DragPos, true);
}

void UHoudiniEditTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	CleanupBrushAttributes();

	if (!BrushAttribParms.IsEmpty())
	{
		UHoudiniParameter::GDisableAttributeActions = false;

		class FHoudiniParmChangedNotifier : UHoudiniParameter
		{
		public:
			FORCEINLINE void Trigger() { SetModification(EHoudiniParameterModification::SetValue); }
		};

		for (const TWeakObjectPtr<UHoudiniParameter>& AttribParm : BrushAttribParms)
			((FHoudiniParmChangedNotifier*)AttribParm.Get())->Trigger();

		UHoudiniParameter::GDisableAttributeActions = true;
	}
}

#undef LOCTEXT_NAMESPACE
