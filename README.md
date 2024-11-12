# Houdini Engine For Unreal

Welcome to the [repository](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal) for the Houdini Engine For Unreal Plugin. 

This plug-in is the completely remaster of the official Houdini Engine For Unreal from zero, with similar usages but up to 2x - 40x faster data I/O performance compare to the lastest official plug-in, native World-Partition support and much more functionalities optimized for making procedural landscape and city toolset. Moreover, this plug-in provides a set of C++ API to define your own unreal classes and assets I/O translators

As the usage is similar, [Official Documentation](https://www.sidefx.com/docs/houdini/unreal/) is also available for this plug-in . But there are still a lot of things are different. For the concrete usage of this plug-in, please see **Usage Brief** below, `Resources/houdini/otls/examples` and `Source/HoudiniEngine/Public/HoudiniEngineCommon.h`

# Compatibility

Support all builds of Houdini 20.5, and Unreal Engine >= 5.3.
NOT compatible with official plug-in, and can NOT work together with official one.

# Installation
01. In this GitHub [repository](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal), click **Releases** on the right side. 
02. Download the Houdini Engine zip file that matches your Unreal Engine Version.  
03. Extract the **HoudiniEngine** folder to the **Plugins** of your Unreal Project Directory.

    e.g. `C:/Unreal Projects/MyGameProject/Plugins/HoudiniEngine`

This Plugin will automatically find the correct houdini version on your computer, or you can specify a custom Houdini installation in the plug-in settings.

# Usage Brief

Here's a list of extra functionalities than official plug-in:

N.B. This list is NOT completed, for details please see `Source/HoudiniEngine/Public/HoudiniEngineCommon.h` and `Resources/houdini/otls/examples`

**Common**:
01. Provides a set of C++ API, allow writing custom I/O translator for your own unreal classes or assets

    See `Source/HoudiniEngine/Public/HoudiniInput.h` and `Source/HoudiniEngine/Public/HoudiniOutput.h`
    Also see [HoudiniMassTranslator](https://github.com/AdrianPanGithub/HoudiniMassTranslator) of how to use these API.
02. Finite state machine for achieving rich user interactions by only using HDA (See `Resources/houdini/otls/examples/he_example_quick_shape.hda`)
03. Streamlined Blueprint API.
04. Light weight, compact usage and panel widgets.

05. ... (And Much More)

**Paramerter**:
01. Much more robust nested parameter support.
02. All parameters support revert, including ramps.
03. All parameters support copy and paste.
04. Fully Menu support, support menu script (dynamic menu), support "normal", "replace", "toggle" menu types.
05. Node preset(including paramerters and inputs) now save as DataTable, which could be fetched by houdini input directly.

06. ... (And Much More)

**Input**:
01. Much more intutive way to draw input curves (press **Enter** to end/start, click in viewport to draw points), support fuse/split points and join/detach curves, support free-hand (**Shift** pressing) curve drawing.
02. Custom attributes on input curves by parameter panel (See `Resources/houdini/otls/examples/he_example_spline_mesh_output.hda`)
03. Support Landscape Mask brush input with mask types of Bit, Weight, and Byte (See `Resources/houdini/otls/examples/he_example_byte_mask.hda`).
04. Landscape layers input could be specified for individual editlayer and layer by input settings or parm tags (e.g. { "unreal_landscape_editlayer_Flat Middle" : "height Alpha Grass" }, { "unreal_landscape_editlayer_Base Landscape" : "* ^Alpha ^Mud ^Rock" })
05. Landscape visibility layer now input as "Alpha", should look identical in unreal and houdini.
06. Landscape layers input support update while brushing.
07. Unreal spline input support import custom properties on your Blueprint.
08. Texture input support.
09. DynamicMeshComponent input support.
10. All component type input support.
11. All settings in the operator-path input panel could be set by parameter tags (e.g. { "check_changed", "0" }, { "unreal_ref", "1" }, See `Source/HoudiniEngine/Public/HoudiniEngineCommon.h`)
12. Mesh inputs are packed before transform.
13. All Input types support shared memory data transport, 8x faster than official shared memory session.

14. ... (And Much More)

**Output**:
01. Allow output special mesh and curves that could edit directly in unreal editor (See `Resources/houdini/otls/examples/he_example_quick_shape.hda`)
02. unreal_uproperty_* support dict and array attributes, almost everything could be set.
03. Mesh output support using s@unreal_split_attr (like instancer output, See `Resources/houdini/otls/examples/he_example_split_actors.hda`)
04. All output types support partial update, including meshes, instancers, landscapes, geometry collection (chaos), curves.
05. All output support generated to the split actors (i@unreal_split_actors = 1) rather than attach to parent Houdini Actors, and could set these split actors' properties by unreal_uproperty_* attributes, which means you could begin play in editor directly without baking output in a World-Partition level.
06. So Baking is deprecated, use i@unreal_split_actors = 1 instead (See `Resources/houdini/otls/examples/he_example_split_actors.hda`).
07. Support Texture output (volumevisualmode = "image", and as an HAPI bug, must add my sharedmemory_volumeoutput Sop to your HDA, See `Resources/houdini/otls/examples/he_example_texture_output.hda`).
08. "Alpha" heightfield layer now output as Landscape visibility layer, should look identical in houdini and unreal.
09. Landscape output support shared memory output (6x faster than official shared-memory session, need to add my sharedmemory_volumeoutput Sop to your HDA)
10. Class Instancer output is 40x faster than official plug-in, also support instantiate USceneComponent derived Classes(e.g. SplineMeshComponent, PointLightComponent etc.).
11. Geometry Collection (Chaos) output as instancers (s@unreal_output_instance_type = "GC"), all of the settings on UGeometryCollection could be set by unreal_uproperty_*, also support split and partial output (See `Resources/houdini/otls/examples/he_example_chaos_geometry_collection_output.hda`).
12. Support Static/AnimatedSparseVolumeTexture (VDBs) output
13. Support Dynamic Mesh (Geometry Script, s@unreal_output_mesh_type = "dynamic") Output

14. ... (And Much More)