// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"


class HOUDINIENGINEEDITOR_API FHoudiniEngineStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static TSharedPtr<ISlateStyle> Get();
	static FName GetStyleSetName();

private:
	static TSharedPtr<class FSlateStyleSet> StyleSet;
};
