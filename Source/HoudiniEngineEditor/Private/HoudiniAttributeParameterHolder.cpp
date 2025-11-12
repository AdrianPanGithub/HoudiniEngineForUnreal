// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAttributeParameterHolder.h"

#include "IDetailsView.h"

#include "HoudiniParameter.h"
#include "HoudiniCurvesComponent.h"


const FName UHoudiniAttributeParameterHolder::DetailsName("HoudiniAttrbuteParameterDetails");

UHoudiniAttributeParameterHolder* UHoudiniAttributeParameterHolder::AttribParmHolderInstance = nullptr;

UHoudiniAttributeParameterHolder* UHoudiniAttributeParameterHolder::Get(UHoudiniEditableGeometry* EditGeo)
{
	if (!AttribParmHolderInstance)
		AttribParmHolderInstance = GetMutableDefault<UHoudiniAttributeParameterHolder>();

	if (IsValid(EditGeo))
	{
		AttribParmHolderInstance->EditGeo = EditGeo;
		AttribParmHolderInstance->RefreshParameters(nullptr);
	}

	return AttribParmHolderInstance;
}

void UHoudiniAttributeParameterHolder::Reset()
{
	if (AttribParmHolderInstance)
		AttribParmHolderInstance = nullptr;
}

bool UHoudiniAttributeParameterHolder::RefreshParameters(const AHoudiniNode* Node)
{
	if (EditGeo.IsValid())
	{
		if (Node && (EditGeo->GetParentNode() != Node))  // If Node is nullptr, then means we need NOT to check is node corresponding
			return false;

		Parms.Empty();

		if (EditGeo->IsGeometrySelected() || UHoudiniParameter::GDisableAttributeActions)  // If we are brushing attribs, then we should force refresh parameter
			Parms = EditGeo->GetAttributeParameters(!UHoudiniParameter::GDisableAttributeActions);

		DisplayName = FName::NameToDisplayString(EditGeo->GetGroupName(), false) +
			(EditGeo->GetSelectedClass() == EHoudiniAttributeOwner::Point ? TEXT(" Point") : TEXT(" Prim"));

		return true;
	}

	return false;
}

TWeakObjectPtr<UHoudiniCurvesComponent> UHoudiniAttributeParameterHolder::GetCurvesComponent() const
{
	return Cast<UHoudiniCurvesComponent>(EditGeo);
}

bool UHoudiniAttributeParameterHolder::IsRightClickPanelOpen() const
{
	return DetailsView.IsValid() && DetailsView.Pin()->GetIdentifier() == DetailsName;
}

bool UHoudiniAttributeParameterHolder::IsEditorModePanelOpen() const
{
	return DetailsView.IsValid() && DetailsView.Pin()->GetIdentifier() != DetailsName;
}
