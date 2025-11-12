// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HoudiniAttributeParameterHolder.generated.h"


class IDetailsView;
class AHoudiniNode;
class UHoudiniParameter;
class UHoudiniEditableGeometry;
class UHoudiniCurvesComponent;

UCLASS(Transient)
class HOUDINIENGINEEDITOR_API UHoudiniAttributeParameterHolder : public UObject
{
	GENERATED_BODY()

protected:
	static UHoudiniAttributeParameterHolder* AttribParmHolderInstance;

	TWeakObjectPtr<UHoudiniEditableGeometry> EditGeo;

	float DisplayHeight = 0.0f;

	FString DisplayName;

	TArray<UHoudiniParameter*> Parms;

public:
	TWeakPtr<IDetailsView> DetailsView;

	static const FName DetailsName;  // Only for right click panel, editor mode panel has its own detail name

	static UHoudiniAttributeParameterHolder* Get(UHoudiniEditableGeometry* EditGeo = nullptr);  // Will refresh attribute parameters

	static void Reset();

	bool IsRightClickPanelOpen() const;

	bool IsEditorModePanelOpen() const;

	FORCEINLINE const float& GetDisplayHeight() const { return DisplayHeight; }

	FORCEINLINE void SetDisplayHeight(const float& InPanelHeight) { DisplayHeight = InPanelHeight; }

	FORCEINLINE const TArray<UHoudiniParameter*>& GetParameters() const { return Parms; }

	FORCEINLINE const FString& GetDisplayName() const { return DisplayName; }

	const TWeakObjectPtr<UHoudiniEditableGeometry>& GetEditableGeometry() const { return EditGeo; }

	bool RefreshParameters(const AHoudiniNode* Node);  // Check this is the attribs of this node, if node is nullptr, then will force refresh

	TWeakObjectPtr<UHoudiniCurvesComponent> GetCurvesComponent() const;
};
