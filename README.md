# Houdini Engine For Unreal

Welcome to the [repository](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal) for the Houdini Engine For Unreal Plugin. 

This plug-in is the completely remaster of the official Houdini Engine For Unreal from zero, with similar usages but up to 2x - 15x faster data I/O performance compare to the latest official plug-in, native World-Partition support, Copernicus texture I/O support, and much more functionalities optimized for making procedural landscape and city toolset. Moreover, this plug-in provides a set of C++ API to define your own unreal classes and assets I/O translators (See [HoudiniPCGTranslator](https://github.com/AdrianPanGithub/HoudiniPCGTranslator) and [HoudiniMassTranslator](https://github.com/AdrianPanGithub/HoudiniMassTranslator))

As the usage is similar, [Official Document](https://www.sidefx.com/docs/houdini/unreal/) is also available for this plug-in. But there are still a lot of things are different. For the concrete usage of this plug-in, please see [Document (Usage Brief)](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal?tab=readme-ov-file#usage-brief) and [Document (Reference)](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal?tab=readme-ov-file#reference) below, also see [Resources/houdini/otls/examples](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/tree/HEAD/Resources/houdini/otls/examples) and [Source/HoudiniEngine/Public/HoudiniEngineCommon.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L236) (All attributes, groups and parameter-tags are listed here)

# Showreels
See what can be achieved by Only using your HDAs and this custom HoudiniEngineForUnreal plugin:
[City toolchains](https://youtu.be/5Vp5nAFq1X8?si=IGSDG4cUdsefwn5x) and [Terrain toolchains](https://youtu.be/19gIzQGnSaU?si=-t7LaDjhUEc7hjMe)

# Compatibility
- Support all builds of Houdini 20.5, Unreal Engine >= 5.4 and legacy version for 5.3.
- Support Windows, macOS-arm64(Apple Silicon M series).
- Support Editor Packaging (**Debug Info** binaries has been uploaded in [Releases](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal) separately).

- NOT compatible with official plug-in, and can NOT work together with official one.

# Installation
01. In this GitHub [repository](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal), click [Releases](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal) on the right side.
02. Download the Houdini Engine zip file that matches your platform and Unreal Engine Version.
03. Extract the **HoudiniEngine** folder to the **Plugins** of your Unreal Project Directory.

    e.g. `C:/Unreal Projects/MyGameProject/Plugins/HoudiniEngine`

This plug-in will automatically find the correct houdini version on your computer, or you could specify a custom Houdini installation in the plug-in settings(Houdini Engine menu/Settings).

N.B. But for Steam Houdini Indie, You MUST specify your steam houdini location manually, by plug-in setting: CustomHoudiniLocation, or use "HAPI_PATH" environment variable to specify your Houdini location.

# Usage Brief
All official functionalities are supported by this plug-in!

Here's a list of **Extra functionalities** than official plug-in:

N.B. This list is NOT completed, for details please see [Source/HoudiniEngine/Public/HoudiniEngineCommon.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L236) (All attributes, groups and parameter-tags are listed here) and [Resources/houdini/otls/examples](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/tree/HEAD/Resources/houdini/otls/examples)

**Common**:
- Native UE5-**World-Partition** & **Game Packaging** support, **NO** need bake! **NO** need delete houdini actor when packaging! Just using i@**unreal_split_actors** = 1, to generate standalone actors that can directly package to game. With s@**unreal_split_attr** to split actors, provides native support for world-partition actor streaming (unreal OFPA One-File-Per-Actor), See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda).
- Streamlined [Blueprint API](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal?tab=readme-ov-file#blueprint-api-reference).
- Finite state machine for achieving rich user interactions by only using HDA (See [he_example_quick_shape.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_quick_shape.hda))
- Provides a set of C++ API, allow writing custom I/O translators for your own unreal classes or assets
    See [Source/HoudiniEngine/Public/HoudiniInput.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniInput.h#L340) and [Source/HoudiniEngine/Public/HoudiniOutput.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniOutput.h#L48).
    Also see [HoudiniPCGTranslator](https://github.com/AdrianPanGithub/HoudiniPCGTranslator) and [HoudiniMassTranslator](https://github.com/AdrianPanGithub/HoudiniMassTranslator) of how to use these API.
- Light weight, compact usage and panel widgets.
- ... (And Much More)

**Parameter**:
- Much more robust nested parameter support.
- All parameters support revert, including ramps.
- All parameters support copy and paste.
- Fully Menu support, support menu script (dynamic menu), support "normal", "replace", "toggle" menu types.
- Node preset(including parameters and inputs) now save as DataTable, which can be fetched by houdini input directly.
- "unreal_ref_filter" tag for string-parameter with tag { "unreal_ref" : "1"/"2" }.
- Be careful: { "unreal_ref" : "1" } tag but on operator-path-parameter means "import as reference", which will Not actually import geometry but import references only (See [HoudiniEngineCommon.h Line 369](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L369)). So Remove "unreal_ref" or Set { "unreal_ref" : "0" } on operator-path-parameter if you want to import landscape layer data or mesh geometries.
- Import asset info(like Bounds of StaticMesh) by using parameter tag { "**unreal_ref**" : "**2**" / "**import_info**" } on string-parameter, the string value will be like: `/Script/Engine.StaticMesh'/Engine/BasicShapes/Cone.Cone';bounds:-0.5,0.5,-0.5,0.5,-0.5,0.5`, #include <[houdini_engine_utils.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/vex/include/houdini_engine_utils.h)> and use [get_asset_bounds](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/vex/include/houdini_engine_utils.h#L11) vex function to parse.
- ... (And Much More)

**Input**:
- Much more intuitive way to draw input curves (press **Enter** to end/start, Click in viewport to draw points), support fuse/split points and join/detach curves, support free-hand (**Shift** pressing) curve drawing, also could use **he_convert_curves** Sop in your HDA to resample input curves.
- Custom attributes on input curves by parameter panel (Attach parms to the folder named [input name] + "**_prim_attribs**"/"**point_attribs**" to define custom per curve/point attributes. See [he_example_spline_mesh_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_mesh_output.hda))
- Support Landscape Mask brush input with mask types of Bit, Weight, and Byte (See [he_example_byte_mask.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_byte_mask.hda)).
- Landscape layers input could be specified for individual editlayer and layer by input settings or parm tags (e.g. { "unreal_landscape_editlayer_Flat Middle" : "height Alpha Grass" }, { "unreal_landscape_editlayer_Base Landscape" : "* ^Alpha ^Mud ^Rock" }, { "**landscape_layer**" : "height" } (will import combined layer), See [HoudiniEngineCommon.h Line 384](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L384))
- Landscape visibility layer now input as "**Alpha**", should look identical in unreal and houdini.
- Landscape layers input support update while brushing.
- Unreal spline input support import **custom properties** on your Blueprint.
- **Texture input** support, See [he_example_terrain_stamp.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_terrain_stamp.hda). Allow HDA working with **Copernicus**.
- DataAsset input support.
- DynamicMeshComponent input support.
- All component types input support.
- StaticMesh and SkeletalMesh(KineFX) input using nanite fallback mesh(render data) by default, use parameter tag: { "**import_render_data**" : "**0**" } to import source model, or set on input ui panel.
- StaticMesh and SkeletalMesh(KineFX) inputs are packed before transform, #include <[houdini_engine_utils.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/vex/include/houdini_engine_utils.h)> and use [crack_packed_transform](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/vex/include/houdini_engine_utils.h#L4) vex function to retrieve p@rot and v@scale from packed meshes.
- World Input(Actor Input) support filter all actors in current level by Label, Class, FolderPath or Tags, using parameter tag { "**unreal_actor_filter_method**" : "**class**"/"**label**"/"**tag**"/"**folder**" } to specify filter method, { "**unreal_ref_class**" : "StaticMeshActor PointLight" } to specify filter classes when filter method is "Class", using { "**unreal_ref_filter**" : "Tree ^Street" } to specify filter string when using other actor filter methods. Also could set on input ui panel.
- Also support filter input components by { "**unreal_ref_class**" : "PCGComponent StaticMeshComponent" } when World Input (Actor Input).
- All settings in the houdini input ui panel could be set by parameter tags (e.g. { "check_changed", "0" }, { "unreal_ref", "1" }(Means `import-as-reference`), { "import_render_data", "0" } for individual operator path input, Rather than set all in plugin settings in official plugin. See [HoudiniEngineCommon.h Line 369](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L369)).
- All Input types support shared memory data transport, up to 15x faster than official shared memory session.

- ... (And Much More)

**Output**:
- Mesh output as StaticMesh by default, rather than preview mesh(HoudiniMesh). Use s@**unreal_output_mesh_type** = "dynamic" to output proxy mesh (GeometryScript/**DynamicMesh Output**), which is way much faster than StaticMesh output.
- Mesh output support using s@**unreal_split_attr** (like instancer output, See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda))
- All output types support partial update, including meshes, instancers, landscapes, geometry collection (chaos), curves using i@**partial_output_mode**, See [HoudiniEngineCommon.h Line 249](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L249), Must split output using s@**unreal_split_attr** before partial update.
- All output support generated to the split actors (i@**unreal_split_actors** = 1) rather than attach to parent Houdini Actors (use s@**unreal_split_attr** specified attribute to name the split actors), and could set these split actors' properties by **unreal_uproperty_*** attributes (e.g. s[]@unreal_uproperty_**DataLayerAssets**, s@unreal_uproperty_**RuntimeGrid**, s@unreal_uproperty_**HLODLayer**), which means you could begin play in editor directly without baking output in a World-Partition level.
- So Baking is deprecated, use i@**unreal_split_actors** = 1 instead (See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda)).
- **unreal_uproperty_*** support dict and array attributes, almost everything could be set.
- Support **Texture output** (volumevisualmode = "image", and as an HAPI bug, MUST add my **sharedmemory_volumeoutput** Sop to your HDA, See [he_example_texture_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_texture_output.hda)), optionally using s@**unreal_object_path** to specify output texture asset path, allow HDA working with **Copernicus**.
- "**Alpha**" heightfield layer now output as Landscape visibility layer, should look identical in houdini and unreal.
- Landscape output recommend to use shared memory output (6x faster than official plugin, add my **sharedmemory_volumeoutput** Sop to your HDA on the top of output node at last, See [he_example_terrain_stamp.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_terrain_stamp.hda). Since HAPI_GetHeightFieldData has serious bugs, especially when transport volumes with various resolutions).
- Support Delete landscape EditLayer (i@**unreal_landscape_editlayer_clear** = 2) and Delete weight layer (i@**unreal_landscape_layer_clear** = 2).
- Support instantiate USceneComponent derived Classes(e.g. SplineMeshComponent, PointLightComponent etc. See [he_example_spline_mesh_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_mesh_output.hda)).
- Geometry Collection (Chaos) output as instancers (s@**unreal_output_instance_type** = "GC"), all of the settings on UGeometryCollection could be set by **unreal_uproperty_***, rather than specific attributes in official plugin, also support split and partial output (See [he_example_chaos_geometry_collection_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_chaos_geometry_collection_output.hda)).
- Standalone MaterialInstance asset output (Still using s@**unreal_material_instance** and @**unreal_material_parameter_***, but on points, See [he_example_vdb_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_vdb_output.hda))
- Unreal spline output support specify tangents (v@**unreal_spline_point_arrive_tangent**, v@**unreal_spline_point_leave_tangent**) and spline class (s@**unreal_output_spline_class**, can be "WaterBodyLake", "/Game/Blueprints/BP_Spline", "CineSplineComponent", etc. See [he_example_spline_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_output.hda) with "Water" plugin enabled).
- All assets (including DataAsset) support create or modify by HDA output (See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda), using s@**unreal_object_path** ("having class prefix" means create, otherwise, means modify), d@**unreal_object_metadata**, @**unreal_uproperty_***).
- Allow output specific mesh and curves that could edit directly in unreal editor (See [he_example_quick_shape.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/he_example_quick_shape.hda))
- Static/AnimatedSparseVolumeTexture (VDBs) output support.

- ... (And Much More)

# Reference
Basic attributes are identical to official plugin, such as s@unreal_material, s@unreal_material_instance, s@unreal_instance, s@unreal_split_attr, @unreal_uproperty_, @unreal_material_parameter_, @unreal_data_table_, i@unreal_landscape_output_mode, s@unreal_landscape_editlayer_name, i@unreal_landscape_editlayer_subtractive, p@rot, v@scale, etc.

Beware that All attributes used for baking are **deprecated and removed**. Instead, you could use i@unreal_split_actors = 1 and @unreal_uproperty_ to set them directly without baking, to package the generated result to your game.

Beware that all attributes used for Geometry Collection output are **deprecated and removed**. Instead, all settings on Geometry Collection could be set by @unreal_uproperty_*.

Here're the lists of **Extra** attributes, groups, and parameter tags that official plugin does NOT have.

N.B. This list is NOT completed, for details please see [Source/HoudiniEngine/Public/HoudiniEngineCommon.h](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Source/HoudiniEngine/Public/HoudiniEngineCommon.h#L236) (All attributes, groups and parameter-tags are listed here)
| Attributes | Class | Type | Explanation |
| --- | --- | --- | --- |
| unreal_split_actors | vertex/point/prim/detail | int | (Output) To generate standalone split actors rather than attach to parent houdini actors, for world-partition (OFPA) and game packaging, See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda). |
| unreal_output_mesh_type | vertex/point/prim/detail | int/string | (Output) Define the output mesh to generate as StaticMesh(default)/DynamicMesh(= 1)/HoudiniMesh(= 2). |
| unreal_output_instance_type | vertex/point/prim/detail | int/string | (Output) Define the output points to generate as auto/ISMC(= 1)/HISMC(= 2)/Components/Actors(= 4)/Foliage(= 5)/Geometry Collection(Chaos, = 6), See [he_example_chaos_geometry_collection_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_chaos_geometry_collection_output.hda). |
| unreal_object_path | point/prim | string | (Input/Output) assign the path of output Texture, output Mesh, output DataTable... If on points output, means asset create/modify (e.g. = `/Script/Engine.DataLayerAsset'/Game/Maps/DL_HE_Runtime.DL_HE_Runtime'` will create a DataLayerAsset named DL_HE_Runtime, See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda)). |
| unreal_object_metadata | point | dict | (Input/Output) |
| unreal_output_spline_class | vertex/point/prim/detail | string | (Output) Define the output curve to generate as custom Actors or Components (e.g. /Game/BPs/BP_Spline, WaterLakeBody, HoudiniCurvesComponent, CineSplineComponent, See [he_example_spline_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_output.hda) with "Water" plugin enabled). |
| curve_type | vertex/point/prim/detail | int/string | (Input/Output) Define the HoudiniCurve/UnrealSpline type. |
| curve_closed | vertex/point/prim/detail | int | (Input/Output) Whether the HoudiniCurve/UnrealSpline is open(= 0)/closed(= 1). |
| unreal_spline_point_arrive_tangent | point | vector3 | (Input/Output) Define the arrive tangent on unreal SplineComponent, See [he_example_spline_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_output.hda). |
| unreal_spline_point_leave_tangent | point | vector3 | (Input/Output) Define the leave tangent on unreal SplineComponent, See [he_example_spline_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_output.hda). |
| id | point&prim | int | (Input) always >= 1, Unique id on input curves and input curve points. |
| unreal_landscape_spline_tangent_length | vertex | float | (Input) Define the length of tangents on unreal landscape splines. |
| unreal_actor_outliner_path | point | string | (Input) For World input(Actor input), difference from s@unreal_actor_path, this attribute records the path in unreal world outliner, also includes actor label |
| unreal_brush_type | point | string | (Input) For World input(BSP Actor input), == 1 means "Add", == 2 "Subtract", See EBrushType. |
| partial_output_mode | vertex/point/prim/detail | int/string | (Output) use with s@unreal_split_attr, allow incremental generated result update, = 1 means modify/create split values' output, = 2 means delete split values's output. |
| unreal_landscape_layer_clear | vertex/point/prim/detail | int/string | (Output) = 1 means clear a landscape height/weight layer, = 2 means delete weight layer |
| unreal_landscape_hole_material | vertex/point/prim/detail | string | (Input/Output) Assign landscape hole material |
| unreal_landscape_hole_material_instance | vertex/point/prim/detail | string | (Output) Create a material instance and assign this landscape hole material instance. |
| unreal_uproperty_Materials | vertex/point/prim/detail | string-array/string | (Output) Assign override materials on all PrimitiveComponents, like InstancedStaticMeshComponent, HeterogeneousVolumeComponent, See [he_example_split_actors.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_split_actors.hda) and  [he_example_vdb_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_vdb_output.hda). |
| unreal_uproperty_AggGeom | vertex/point/prim/detail | dict | (Output) Assign simple collision proxies on StaticMesh, e.g. = { "SphereElems": [{"Center": {"X": 0.1, "Y": "0.2", "Z": 0.0}, "Radius": 1.1 }]} |
| delta_info | prim | string | (Input) Allow HDA to know the changes, See [he_example_quick_shape.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/he_example_quick_shape.hda). |
| `__shared_memory_path__` | prim | string | (Output) Warning: Must be created by `sharedmemory_volumeoutput` Sop to accelerate landscape output and enable copernicus texture output, do NOT create it manually! See [he_example_terrain_stamp.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_terrain_stamp.hda) and [he_example_kinefx_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_kinefx_output.hda) |
| ... (And Much More) |

| Groups | Class | Explaination |
| --- | --- | --- |
| collision_geo | prim | (Input/Output) To separate complex collision mesh from StaticMesh output. Moreover, mesh output support split by s@unreal_split_attr just like instancer output (official feature), rather than using groups to split. |
| lod_* | prim | (Input/Output) official features. |
| changed | point&prim | (Input) on input curves and input curve points, mark current operation changed elements. |

| Parameter Name Suffix | Parm Type | Explanation |
| --- | --- | --- |
| *_point_attribs | folder | On folder parms, is [curve input name/group name] + "_point_attribs", to define custom per point attributes, See [he_example_spline_mesh_output.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_spline_mesh_output.hda). |
| *_prim_attribs | folder | On folder parms, is [curve input name/group name] + "_prim_attribs", to define custom per prim/curve attributes, See [he_example_terrain_stamp.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_terrain_stamp.hda) and [he_example_quick_shape.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/he_example_quick_shape.hda). |
| *_byte_value# | int | For Mask Input and mask type = "byte", this suffix should add to multi-parm instance, See [he_example_byte_mask.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_byte_mask.hda). |

| Parameter Tags | Parm Type | Explanation |
| --- | --- | --- |
| unreal_ref | string, string-with-menu | Display a string parameter as Unreal Asset Slot, = "2"/"import_info" will import extra info of the assets, such as bounds of the StaticMesh, etc. |
| unreal_ref_class | string, operator-path | Filter asset list by classes, e.g. = "Blueprint Texture2D MaterialInterface ^MaterialInstance". |
| unreal_ref_filter | string, operator-path | Filter asset list by string, e.g. = "tree ^street". |
| unreal_ref | operator-path | = 1 to set "import as reference", meshes will import as points, landscape will import as empty volume that matches the landscape, also set on input ui panel. |
| check_changed | operator-path | Set appressive input update import, default is true, set to "0" to defer input update import. also on input ui panel. |
| num_input_objects | operator-path | (Content Input) Default can import multiple objects to one single input, set to a number to limit the count of the input objects in one single input. |
| import_render_data | operator-path | (Mesh Input) Default will import nanite fallback mesh, set to "0" to import source model (Mesh Description). |
| lod_import_method | operator-path | (Mesh Input) Import FirstLOD(= 0)/LastLOD(= 1)/AllLODs(= 2), also on input ui panel. |
| collision_import_method | operator-path | (Mesh Input) Import NoCollsion(= 0)/ImportWithMesh(= 1)/ImportCollisionOnly(= 2). |
| import_landscape_splines | operator-path | (Landscape Input) Set to "1" to import landscape with landscape splines. |
| landscape_layer | operator-path | (Landscape Input) Combine all EditLayers to import, use this tag's value to specify the layer you want to import, e.g. = "height Alpha Grass", See [he_example_byte_mask.hda](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal/blob/HEAD/Resources/houdini/otls/examples/he_example_byte_mask.hda). |
| unreal_landscape_editlayer_* | operator-path | (Landscape Input) "unreal_landscape_editlayer_" + [EditLayer Name], like "unreal_landscape_editlayer_Base Landscape", set tag value like "landscape_layer" tag, e.g. "* ^Alpha ^Mud". |
| curve_color | operator-path | (Curve Input) Specify input curve color, e.g. "0.9, 0.4, 0.5" is pink. |
| import_rot_and_scale | operator-path | (Spline/Curve Input) import p@rot and v@scale point attributes. |
| import_collision_info | operator-path | (Curve Input) import collided actor object name and collider normal attributes. |
| unreal_actor_filter_method | operator-path | (World/Actor Input) Filter all actors in current level by Selection(default)/Class(= 1)/Label(= 2)/Tag(= 3)/Folder(= 4). |
| mask_type | operator-path | (Mask Input) Set Mask Input mask type to Bit(default)/Weight(= 1)/Byte(= 2). |

| Environment Variables | Explanation |
| --- | --- |
| HAPI_CLIENT | == "unreal". |
| CLIENT_PROJECT_DIR | Allow HDA to get the current unreal project directory. |
| CLIENT_SCENE_PATH | Allow HDA to get the current unreal level(map) path. |
| HOUDINI_ENGINE_FOLDER | Default is "/Game/HoudiniEngine/". |

# Blueprint API Reference

**HoudiniNode**::
- static AHoudiniNode* `InstantiateHoudiniAsset`(const UObject* WorldContextObject, UHoudiniAsset* HoudiniAsset)
- void `LoadParameterPreset`(const UDataTable* ParameterPreset)
- void `SetParameterValues`(const TMap<FName, FHoudiniGenericParameter>& Parameters)
- void `SetCookOnParameterChanged`(const bool bInCookOnParameterChanged)
- void `SetCookOnUpstreamChanged`(const bool bInCookOnUpstreamChanged)
- FString `GetCookFolderPath`() const
- FString `GetBakeFolderPath`() const
- void `ForceCook`()
- void `RequestRebuild`()
- void `RequestPDGCook`(const int32 TopNodeIdx)
- AActor* `Bake`()
- void `BroadcastEvent`(const EHoudiniNodeEvent NodeEvent)
- UHoudiniInput* `GetInputByName`(const FString& Name, const bool bIsOperatorPathParameter = true) const
- FHoudiniGenericParameter `GetParameterValue`(const FString& ParameterName) const
- DELEGATE_OneParam(FHoudiniNodeEvents, const EHoudiniNodeEvent, InNodeEvent) `HoudiniNodeEvents`;
- ...(And Much More)

**HoudiniInput**::
- void `Import`(UObject* Object)
- ...(And Much More)

**HoudiniMeshComponent**::
- UStaticMesh* `SaveToStaticMesh`(const FString& StaticMeshName)