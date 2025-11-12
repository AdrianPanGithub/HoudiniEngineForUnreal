// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class IDetailCategoryBuilder;
class FDetailWidgetDecl;
class SVerticalBox;
class UHoudiniInput;

class HOUDINIENGINEEDITOR_API FHoudiniInputDetails
{
protected:
	static void CreateWidgetContent(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input);

	static void CreateWidgetCurves(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input);

	static void CreateWidgetWorld(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input);

	static void CreateWidgetNode(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input);

	static void CreateWidgetMask(IDetailCategoryBuilder& CategoryBuilder, const TSharedPtr<SVerticalBox>& VerticalBox, const TWeakObjectPtr<UHoudiniInput>& Input);

public:
	static bool HasNodeInputs(const TArray<UHoudiniInput*>& Inputs);

	static void CreateWidgets(IDetailCategoryBuilder& CategoryBuilder, const TArray<UHoudiniInput*>& Inputs);

	static void CreateWidget(IDetailCategoryBuilder& CategoryBuilder, FDetailWidgetDecl& ValueContent, const TWeakObjectPtr<UHoudiniInput>& Input);
};
