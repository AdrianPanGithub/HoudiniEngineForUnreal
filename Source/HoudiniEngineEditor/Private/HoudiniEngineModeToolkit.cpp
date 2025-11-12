// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineModeToolkit.h"


#define LOCTEXT_NAMESPACE "HoudiniEngineModeToolkit"

void FHoudiniEngineModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}

FName FHoudiniEngineModeToolkit::GetToolkitFName() const
{
	return FName("HoudiniEngineEditorMode");
}

FText FHoudiniEngineModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "HoudiniEngineEditorMode Toolkit");
}

#undef LOCTEXT_NAMESPACE
