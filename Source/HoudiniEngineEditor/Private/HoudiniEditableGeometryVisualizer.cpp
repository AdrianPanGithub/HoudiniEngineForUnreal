// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEditableGeometryVisualizer.h"

#include "IDetailsView.h"
#include "EditorViewportClient.h"

#include "HoudiniEngine.h"
#include "HoudiniEditableGeometry.h"

#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEditableGeometryEditorUtils.h"
#include "HoudiniAttributeParameterHolder.h"


IMPLEMENT_HIT_PROXY(HHoudiniPointVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HHoudiniPrimVisProxy, HComponentVisProxy);


void FHoudiniEditableGeometryVisualizer::RefreshEditorModeAttributePanel(UHoudiniEditableGeometry* EditGeo)
{
	if (UHoudiniAttributeParameterHolder::Get()->IsEditorModePanelOpen())
		UHoudiniAttributeParameterHolder::Get(EditGeo)->DetailsView.Pin()->ForceRefresh();
}

bool FHoudiniEditableGeometryVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	UHoudiniEditableGeometry* EditGeo = Cast<UHoudiniEditableGeometry>(ComponentPropertyPath.GetComponent());
	if (IsValid(EditGeo) && EditGeo->HasChanged())  // As selection will construct "changed" group, so we should NOT change selection Until EditGeo has been uploaded
		return true;

	if (VisProxy->Component.IsValid() && VisProxy->Component != EditGeo)
	{
		if (IsValid(EditGeo))
			EditGeo->ResetSelection();

		EditGeo = const_cast<UHoudiniEditableGeometry*>(Cast<const UHoudiniEditableGeometry>(VisProxy->Component));
		ComponentPropertyPath = FComponentPropertyPath(EditGeo);
	}

	if (!IsValid(EditGeo))
		return false;

	const bool bCtrlPressed = InViewportClient->IsCtrlPressed();
	const bool bRightClick = Click.GetKey() == EKeys::RightMouseButton;
	const bool bPointSelected = VisProxy->IsA(HHoudiniPointVisProxy::StaticGetType());

	EditGeo->Select(bPointSelected ? EHoudiniAttributeOwner::Point : EHoudiniAttributeOwner::Prim,
		bPointSelected ? ((HHoudiniPointVisProxy*)VisProxy)->PointIdx : ((HHoudiniPrimVisProxy*)VisProxy)->PrimIdx,
		!bCtrlPressed, !bRightClick, FRay(Click.GetOrigin(), Click.GetDirection()),
		FSlateApplication::Get().GetModifierKeys());

	if (UHoudiniAttributeParameterHolder::Get()->IsEditorModePanelOpen())
		UHoudiniAttributeParameterHolder::Get(EditGeo)->DetailsView.Pin()->ForceRefresh();

	if (EditGeo && EditGeo->CanDropAssets())
		FHoudiniEditableGeometryAssetDrop::Register();
	else
		FHoudiniEditableGeometryAssetDrop::Unregister();

	return true;
}

void FHoudiniEditableGeometryVisualizer::EndEditing()
{
	if (!FHoudiniEditableGeometryEditingScope::AllowResetSelection())
		return;

	UHoudiniEditableGeometry* EditGeo = GetEditingGeometry<UHoudiniEditableGeometry>();
	if (EditGeo && !EditGeo->HasChanged())
	{
		EditGeo->ResetSelection();  // We must reset selection, as selection is used for construct the "changed" group
		
		// And then, we should also let parms on this EditGeo to disappear
		UHoudiniAttributeParameterHolder* AttribParmHolder = UHoudiniAttributeParameterHolder::Get();
		if (AttribParmHolder->IsEditorModePanelOpen() && (AttribParmHolder->GetEditableGeometry() == EditGeo))
		{
			AttribParmHolder->RefreshParameters(nullptr);
			AttribParmHolder->DetailsView.Pin()->ForceRefresh();
		}
	}

	FHoudiniEditableGeometryAssetDrop::Unregister();

	ComponentPropertyPath.Reset();
}

TSharedPtr<SWidget> FHoudiniEditableGeometryVisualizer::GenerateContextMenu() const
{
	if (UHoudiniAttributeParameterHolder::Get()->IsEditorModePanelOpen())  // Attributes has been displayed on editor mode panel, so we need NOT construct a right-click panel
		return nullptr;

	if (UHoudiniEditableGeometry* EditGeo = GetSelectedGeometry<UHoudiniEditableGeometry>())
		return FHoudiniEngineEditorUtils::GetAttributePanel(EditGeo);

	return nullptr;
}

bool FHoudiniEditableGeometryVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (GetSelectedGeometry<UHoudiniEditableGeometry>())
	{
		OutLocation = Pivot.GetLocation();
		return true;
	}
	return false;
}

bool FHoudiniEditableGeometryVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (const UHoudiniEditableGeometry* EditGeo = GetSelectedGeometry<UHoudiniEditableGeometry>())
	{
		Pivot = EditGeo->GetSelectionPivot();
		if (Pivot.GetRotation() != FQuat::Identity)
		{
			OutMatrix = FMatrix::Identity;
			OutMatrix *= FRotationMatrix(Pivot.Rotator());
			return true;
		}
	}

	return false;
}

bool FHoudiniEditableGeometryVisualizer::HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	if (UHoudiniEditableGeometry* EditGeo = GetSelectedGeometry<UHoudiniEditableGeometry>())
	{
		if (EditGeo->HasChanged())  // As selection will construct "changed" group, so we should NOT change selection Until EditGeo has been uploaded
			return true;

		EditGeo->FrustumSelect(InFrustum);

		if (UHoudiniAttributeParameterHolder::Get()->IsEditorModePanelOpen())
			UHoudiniAttributeParameterHolder::Get(EditGeo)->DetailsView.Pin()->ForceRefresh();

		return true;
	}

	return false;
}

bool FHoudiniEditableGeometryVisualizer::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox)
{
	UHoudiniEditableGeometry* EditGeo = Cast<UHoudiniEditableGeometry>((ComponentPropertyPath.GetComponent()));
	return IsValid(EditGeo) ? EditGeo->GetSelectionBoundingBox(OutBoundingBox) : false;
}


UWorld* FHoudiniEditableGeometryVisualizer::GetEditingWorld() const
{
	UActorComponent* Component = ComponentPropertyPath.IsValid() ? ComponentPropertyPath.GetComponent() : nullptr;
	return Component ? Component->GetWorld() : nullptr;
}
