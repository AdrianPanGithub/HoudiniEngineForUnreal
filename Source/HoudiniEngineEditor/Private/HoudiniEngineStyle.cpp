// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineStyle.h"

#include "Styling/SlateStyleRegistry.h"

#include "HoudiniEngine.h"



#define ICON_SIZE_128 FVector2D(128.0, 128.0)
#define ICON_SIZE_40 FVector2D(40.0, 40.0)
#define ICON_SIZE_16 FVector2D(16.0, 16.0)

#define CLASS_ICON_SIZE ICON_SIZE_16
#define CLASS_THUMBNAIL_SIZE FVector2D(64.0f, 64.0f)
#define COMMAND_ICON_SIZE ICON_SIZE_16
#define TOOL_ICON_SIZE FVector2D(20.0, 20.0)

TSharedPtr<FSlateStyleSet> FHoudiniEngineStyle::StyleSet = nullptr;

void FHoudiniEngineStyle::Initialize()
{
	if (StyleSet.IsValid())  // Only register the StyleSet once
		return;

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	const FString IconsDir = FHoudiniEngine::GetPluginDir() / TEXT("Resources/Icons/");
	StyleSet->Set("ClassThumbnail.HoudiniAsset",
		new FSlateImageBrush(IconsDir + TEXT("houdini_asset_128x.png"), CLASS_THUMBNAIL_SIZE));

	StyleSet->Set("ClassIcon.HoudiniNode",
		new FSlateImageBrush(IconsDir + TEXT("houdini_node_128x.png"), CLASS_ICON_SIZE));

	StyleSet->Set("ClassIcon.HoudiniCurvesComponent",
		new FSlateImageBrush(IconsDir + TEXT("curve_128x.png"), CLASS_ICON_SIZE));

	StyleSet->Set("ClassIcon.HoudiniMeshComponent",
		new FSlateImageBrush(IconsDir + TEXT("mesh_128x.png"), CLASS_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.HoudiniEngine",
		new FSlateImageBrush(IconsDir + TEXT("../Icon128.png"), ICON_SIZE_128));

	StyleSet->Set("HoudiniEngine.Unreal",
		new FSlateImageBrush(IconsDir + TEXT("houdini_engine_for_unreal_128x.png"), ICON_SIZE_128));

	StyleSet->Set("HoudiniEngine.Rebuild",
		new FSlateImageBrush(IconsDir + TEXT("rebuild_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.Cook",
		new FSlateImageBrush(IconsDir + TEXT("cook_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.Bake",
		new FSlateImageBrush(IconsDir + TEXT("bake_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.ImageChooser",
		new FSlateImageBrush(IconsDir + TEXT("image_chooser_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.CurveDraw",
		new FSlateImageBrush(IconsDir + TEXT("curve_draw_128x.png"), ICON_SIZE_128));

	StyleSet->Set("HoudiniEngine.RestartSession",
		new FSlateImageBrush(IconsDir + TEXT("session_restart_128x.png"), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.SaveHoudiniScene",
		new FSlateImageBrush(IconsDir + TEXT("save_to_hip_128x.png"), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.OpenSessionSync",
		new FSlateImageBrush(IconsDir + TEXT("session_sync_start_128x.png"), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.StopSession",
		new FSlateImageBrush(IconsDir + TEXT("session_stop_128x.png"), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.OpenSceneInHoudini",
		new FSlateImageBrush(IconsDir + TEXT("open_in_houdini_128x.png"), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.CleanUpCookFolder",
		new FSlateImageBrush(IconsDir + TEXT("clean_up_128x.png"), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.Settings",
		new FSlateImageBrush(FAppStyle::Get().GetBrush("Launcher.EditSettings")->GetResourceName(), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.About",
		new FSlateVectorImageBrush(FAppStyle::Get().GetBrush("MainFrame.VisitSearchForAnswersPage")->GetResourceName().ToString(), COMMAND_ICON_SIZE));

	StyleSet->Set("HoudiniEngine.InsertBefore",
		new FSlateImageBrush(IconsDir + TEXT("multi_insertbefore_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.Remove",
		new FSlateImageBrush(IconsDir + TEXT("multi_remove_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.PlusCircle",
		new FSlateImageBrush(IconsDir + TEXT("plus_circle_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.PlusRect",
		new FSlateImageBrush(IconsDir + TEXT("plus_rect_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.AttribEdit",
		new FSlateImageBrush(IconsDir + TEXT("attrib_edit_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.SplitPoints",
		new FSlateImageBrush(IconsDir + TEXT("split_points_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.FusePoints",
		new FSlateImageBrush(IconsDir + TEXT("fuse_points_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.DetachPrims",
		new FSlateImageBrush(IconsDir + TEXT("detach_prims_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.JoinPrims",
		new FSlateImageBrush(IconsDir + TEXT("join_prims_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniEngine.Output",
		new FSlateImageBrush(IconsDir + TEXT("output_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniPDG.Link",
		new FSlateImageBrush(IconsDir + TEXT("pdg_link_40x.png"), ICON_SIZE_40));

	StyleSet->Set("HoudiniPDG.DirtyNode",
		new FSlateImageBrush(IconsDir + TEXT("pdg_dirty_node_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Reset",
		new FSlateImageBrush(IconsDir + TEXT("pdg_reset_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Cooking",
		new FSlateImageBrush(IconsDir + TEXT("pdg_cooking_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Pause",
		new FSlateImageBrush(IconsDir + TEXT("pdg_pause_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Cancel",
		new FSlateImageBrush(IconsDir + TEXT("pdg_cancel_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Done",
		new FSlateImageBrush(IconsDir + TEXT("pdg_done_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Error",
		new FSlateImageBrush(IconsDir + TEXT("pdg_error_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniPDG.Waiting",
		new FSlateImageBrush(IconsDir + TEXT("pdg_waiting_40x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniEngineEditorMode.ManageTool",
		new FSlateImageBrush(IconsDir + TEXT("houdini_node_128x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniEngineEditorMode.MaskTool",
		new FSlateImageBrush(IconsDir + TEXT("mask_paint_128x.png"), TOOL_ICON_SIZE));

	StyleSet->Set("HoudiniEngineEditorMode.EditTool",
		new FSlateImageBrush(IconsDir + TEXT("attrib_paint_128x.png"), TOOL_ICON_SIZE));

	// Register Slate style.
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void
FHoudiniEngineStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

TSharedPtr<ISlateStyle> FHoudiniEngineStyle::Get()
{
	return StyleSet;
}

static const FName HoudiniEngineStyleSetName("HoudiniEngineStyle");

FName FHoudiniEngineStyle::GetStyleSetName()
{
	return HoudiniEngineStyleSetName;
}
