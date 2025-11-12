// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineModeCommands.h"


#define LOCTEXT_NAMESPACE "HoudiniEngineModeCommands"

FHoudiniEngineModeCommands::FHoudiniEngineModeCommands()
	: TCommands<FHoudiniEngineModeCommands>("HoudiniEngineEditorMode",
		NSLOCTEXT("HoudiniEngineEditorMode", "HoudiniEngineModeCommands", "Houdini Engine Editor Mode"),
		NAME_None,
		FName(TEXT("HoudiniEngineStyle")))
{
}

void FHoudiniEngineModeCommands::RegisterCommands()
{
	TArray<TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);  // NAME_Default is the palette name

	UI_COMMAND(ManageTool, "Manage", "Manage Houdini Nodes", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(ManageTool);

	UI_COMMAND(MaskTool, "Mask", "Provide Brush Tools For Houdini Landscape Mask Inputs", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(MaskTool);

	UI_COMMAND(EditTool, "Edit", "Edit Attributes and Elements of Houdini Editable Geometries", EUserInterfaceActionType::ToggleButton, FInputChord());
	ToolCommands.Add(EditTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FHoudiniEngineModeCommands::GetCommands()
{
	return FHoudiniEngineModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
