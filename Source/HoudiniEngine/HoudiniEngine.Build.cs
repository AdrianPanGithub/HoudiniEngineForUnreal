// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

using UnrealBuildTool;

public class HoudiniEngine : ModuleRules
{
	public HoudiniEngine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Projects",
				"Slate",
				"SlateCore",
                "Foliage",
                "Landscape",
                "RHI",
                "RenderCore",
                "StaticMeshDescription",
                "SkeletalMeshDescription",
                "Json",
                "DeveloperSettings",
                "MeshDescription",
                "GeometryFramework",
                "GeometryCore",
                "GeometryCollectionEngine",
                "JsonUtilities"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
			);

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "AppFramework",
                    "AssetTools",
                    "EditorWidgets",
                    "Kismet",
                    "LevelEditor",
                    "UnrealEd",
                    "FoliageEdit",
                    "ApplicationCore",
                    "SubobjectEditor",
                    "BlueprintGraph"
                }
            );
        }
    }
}
