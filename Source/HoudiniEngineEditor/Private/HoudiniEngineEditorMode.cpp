// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineEditorMode.h"

#include "InteractiveToolManager.h"

#include "HoudiniEngineModeToolkit.h"
#include "HoudiniEngineModeCommands.h"
#include "HoudiniTools.h"


#define LOCTEXT_NAMESPACE "HoudiniEngineEditorMode"

const FEditorModeID UHoudiniEngineEditorMode::EM_HoudiniEngineEditorModeId("EM_HoudiniEngine");

FString UHoudiniEngineEditorMode::ManageToolName = TEXT("HoudiniEngine_ManageTool");
FString UHoudiniEngineEditorMode::MaskToolName = TEXT("HoudiniEngine_MaskTool");
FString UHoudiniEngineEditorMode::EditToolName = TEXT("HoudiniEngine_EditTool");


UHoudiniEngineEditorMode::UHoudiniEngineEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
	Info = FEditorModeInfo(UHoudiniEngineEditorMode::EM_HoudiniEngineEditorModeId,
		LOCTEXT("ModeName", "Houdini Engine"),
		FSlateIcon("HoudiniEngineStyle", "HoudiniEngine.HoudiniEngine"),
		true);
}

bool UHoudiniEngineEditorMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	return true;
}

void UHoudiniEngineEditorMode::Enter()
{
	UEdMode::Enter();

	const FHoudiniEngineModeCommands& ToolCommands = FHoudiniEngineModeCommands::Get();

	// Register the ToolBuilders for Tools here.
	RegisterTool(ToolCommands.ManageTool, ManageToolName, NewObject<UHoudiniManageToolBuilder>(this));
	RegisterTool(ToolCommands.MaskTool, MaskToolName, NewObject<UHoudiniMaskToolBuilder>(this));
	RegisterTool(ToolCommands.EditTool, EditToolName, NewObject<UHoudiniEditToolBuilder>(this));

	// active tool type is not relevant here, we just set to default
	GetToolManager()->SelectActiveToolType(EToolSide::Left, UHoudiniMaskTool::IsPendingActivate() ? MaskToolName :
		(UHoudiniEditTool::IsPendingActivate() ? EditToolName : ManageToolName));
	GetToolManager()->ActivateTool(EToolSide::Left);
}

void UHoudiniEngineEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FHoudiniEngineModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UHoudiniEngineEditorMode::GetModeCommands() const
{
	return FHoudiniEngineModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
