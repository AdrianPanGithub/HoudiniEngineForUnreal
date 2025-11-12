// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class IDetailCategoryBuilder;
class AHoudiniNode;


struct HOUDINIENGINEEDITOR_API FHoudiniPDGDetails  // Just a helper to list Top nodes
{
public:
	static void CreateWidgets(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<AHoudiniNode>& Node);
};
