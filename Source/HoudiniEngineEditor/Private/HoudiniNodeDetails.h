// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"


class IDetailCategoryBuilder;
class AHoudiniNode;
class SWindow;

class HOUDINIENGINEEDITOR_API FHoudiniNodeDetails : public IDetailCustomization
{
protected:
	TArray<TArray<TSharedPtr<FString>>> NodesAvailableOptions;

public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	static TSharedRef<IDetailCustomization> MakeInstance();

	static const FName NodeCategoryName;
	static const FName PDGCategoryName;
	static const FName ParametersCategoryName;
	static const FName InputsCategoryName;

	static void CreateNodesDetails(IDetailLayoutBuilder& DetailBuilder, const TArray<AHoudiniNode*>& Nodes,
		TArray<TArray<TSharedPtr<FString>>>& OutNodesAvailableOptions);

	static void CreateNodeSettingsWidget(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<AHoudiniNode>& Node,
		TArray<TSharedPtr<FString>>& OutAvailableOptions);

	static TSharedRef<SWindow> CreateNodeInfoWindow(const TWeakObjectPtr<AHoudiniNode>& Node);
};
