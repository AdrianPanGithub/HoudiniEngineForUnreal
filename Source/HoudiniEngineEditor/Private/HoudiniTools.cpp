// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniTools.h"

#include "InteractiveToolManager.h"
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


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

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


// -------- Brush --------
void UHoudiniBrushTool::Setup()
{
	UInteractiveTool::Setup();

	AddInputBehavior(NewObject<UHoudiniBrushInputBehavior>(this));
	AddToolPropertySource(this);

	AsyncTask(ENamedThreads::GameThread, [] { FHoudiniEngineEditorUtils::DisableOverrideEngineShowFlags(); });  // As ToolManager will disable AA after tool setup, we need enable it at next tick
}

FInputCaptureRequest UHoudiniBrushInputBehavior::WantsCapture(const FInputDeviceState& InputState)
{
	if (InputState.Mouse.Left.bDown && GetTool()->CanBeginClickDragSequence())
		return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any);
	else if ((InputState.Keyboard.ActiveKey.Button == EKeys::LeftBracket) || (InputState.Keyboard.ActiveKey.Button == EKeys::RightBracket))
		return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any);
	return FInputCaptureRequest::Ignore();
}

FInputCaptureUpdate UHoudiniBrushInputBehavior::BeginCapture(const FInputDeviceState& InputState, EInputCaptureSide eSide)
{
	if (InputState.Mouse.Left.bDown)
	{
		GetTool()->OnMouseUpdate(InputState.Mouse.WorldRay, true, InputState.bShiftKeyDown);
		return FInputCaptureUpdate::Begin(this, eSide);
	}
	else if (InputState.Keyboard.ActiveKey.Button == EKeys::LeftBracket)
	{
		GetTool()->ChangeBrushSize(false);
		return FInputCaptureUpdate::Begin(this, eSide);
	}
	else if(InputState.Keyboard.ActiveKey.Button == EKeys::RightBracket)
	{
		GetTool()->ChangeBrushSize(true);
		return FInputCaptureUpdate::Begin(this, eSide);
	}
	return FInputCaptureUpdate::End();
}

FInputCaptureUpdate UHoudiniBrushInputBehavior::UpdateCapture(const FInputDeviceState& InputState, const FInputCaptureData& CaptureData)
{
	if (InputState.Mouse.Left.bDown)
	{
		GetTool()->OnMouseUpdate(InputState.Mouse.WorldRay, true, InputState.bShiftKeyDown);
		return FInputCaptureUpdate::Continue();
	}
	else if (InputState.Mouse.Left.bReleased)
	{
		GetTool()->OnClickRelease();
		return FInputCaptureUpdate::End();
	}
	return FInputCaptureUpdate::End();
}

void UHoudiniBrushInputBehavior::ForceEndCapture(const FInputCaptureData& CaptureData)
{
	GetTool()->OnClickRelease();
}

FInputCaptureRequest UHoudiniBrushInputBehavior::WantsHoverCapture(const FInputDeviceState& InputState)
{
	GetTool()->OnMouseUpdate(InputState.Mouse.WorldRay, false, InputState.bShiftKeyDown);
	return FInputCaptureRequest::Ignore();
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

void UHoudiniMaskTool::OnMouseUpdate(const FRay& MouseRay, const bool& bDragging, const bool bShiftDown)
{
	if (MaskActor.IsValid())
	{
		MaskActor->UpdateBrushPosition(MouseRay);

		if (bDragging && MaskActor->GetMaskInput().IsValid())
		{
			if (TransactionIdx < 0)
			{
				TransactionIdx = GEditor->BeginTransaction(TEXT(HOUDINI_ENGINE),
					LOCTEXT("HoudiniInputMaskChanged", "HoudiniInputMaskChanged"), MaskActor->GetMaskInput().Get());
				MaskActor->GetMaskInput()->ModifyAll();
			}
			MaskActor->OnBrush(bShiftDown);
		}
	}
}

bool UHoudiniMaskTool::CanBeginClickDragSequence() const
{
	return (FHoudiniEngine::Get().AllowEdit() && MaskActor.IsValid() && MaskActor->CanBrush());
}

void UHoudiniMaskTool::OnClickRelease()
{
	if (MaskActor.IsValid())
		MaskActor->EndBrush();

	if (TransactionIdx >= 0)
	{
		GEditor->EndTransaction();
		TransactionIdx = -1;
	}
}

void UHoudiniMaskTool::ChangeBrushSize(const bool& bIncrease)
{
	if (MaskActor.IsValid())
		MaskActor->SetBrushSize(MaskActor->GetBrushSize() + (bIncrease ? 500.0f : -500.0f));
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

void UHoudiniEditTool::Setup()
{
	Super::Setup();
	AddToolPropertySource(UHoudiniAttributeParameterHolder::Get(FHoudiniEngineEditor::Get().GetSelectedGeometry()));  // We should refresh AttribParmHolder first, then construct EdtiTool Details
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


void UHoudiniEditTool::OnMouseUpdate(const FRay& MouseRay, const bool& bDragging, const bool bShiftDown)
{
	// We should NOT change selection before cook finished, because attrib panel will refresh while cook finished, if selection changed, then brush value may also changed
	if (!FHoudiniEngine::Get().AllowEdit() || BrushAttribParms.IsEmpty())
		return;

	const TWeakObjectPtr<UHoudiniEditableGeometry>& EditGeo = UHoudiniAttributeParameterHolder::Get()->GetEditableGeometry();
	if (!EditGeo.IsValid() || EditGeo->HasChanged())
		return;

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

bool UHoudiniEditTool::CanBeginClickDragSequence() const
{
	return (FHoudiniEngine::Get().AllowEdit() && !BrushAttribParms.IsEmpty());
}

void UHoudiniEditTool::OnClickRelease()
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

void UHoudiniEditTool::ChangeBrushSize(const bool& bIncrease)
{
	BrushSize += bIncrease ? 100.0f : -100.0f;
}

#undef LOCTEXT_NAMESPACE
