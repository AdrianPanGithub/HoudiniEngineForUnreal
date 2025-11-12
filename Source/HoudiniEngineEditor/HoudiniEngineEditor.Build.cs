// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

using UnrealBuildTool;

public class HoudiniEngineEditor : ModuleRules
{
	public HoudiniEngineEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {

            }
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"HoudiniEngine/Private"
            }
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "HoudiniEngine",
                "Slate",
                "SlateCore",
                "Landscape",
                "Foliage",
                "MaterialEditor"
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "AppFramework",
                "AssetTools",
                "ContentBrowser",
                "DesktopWidgets",
                "EditorStyle",
                "EditorWidgets",
                "Engine",
                "InputCore",
                "ImageCore",
                "LevelEditor",
                "MainFrame",
                "Projects",
                "PropertyEditor",
                "RHI",
                "RenderCore",
                "TargetPlatform",
                "UnrealEd",
                "ApplicationCore",
                "CurveEditor",
                "Json",
                "SceneOutliner",
                "PropertyPath",
                "SourceControl",
                "EditorSubsystem",
                "EditorFramework",
                "InteractiveToolsFramework",
                "EditorInteractiveToolsFramework",
                "ContentBrowserData",
                "LandscapeEditor",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
			);
	}
}
