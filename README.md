# Houdini Engine For Unreal

Welcome to the [repository](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal) for the Houdini Engine For Unreal Plugin. 

This plug-in is the completely remaster of the official Houdini Engine For Unreal from zero, with similar usages but up to 2x - 40x faster data I/O performance compare to the lastest official plug-in, native World-Partition support and much more functionalities optimized for making procedural landscape and city toolset. Moreover, this plug-in provides a set of C++ API to define your own unreal classes and assets I/O translators

As the usage is similar, [Official Documentation](https://www.sidefx.com/docs/houdini/unreal/) is also available for this plug-in . But there are still lot of things are distinct. For concrete usage of this plug-in, please see `Source/HoudiniEngine/Public/HoudiniEngineCommon.h` and `Resources/houdini/otls/examples`


# Compatibility

Support Houdini >= 20.5.278, Unreal Engine >= 5.3.

NOT compatible with official plug-in, and can NOT work together with official one.

# Installation
01. In this GitHub [repository](https://github.com/AdrianPanGithub/HoudiniEngineForUnreal), click **Releases** on the right side. 
02. Download the Houdini Engine zip file that matches your Unreal Engine Version.  
03. Extract the **HoudiniEngine** folder to the **Plugins** of your Unreal Project Directory.

    e.g. `C:/Unreal Projects/MyGameProject/Plugins/HoudiniEngine`

This Plugin will automatically find the correct houdini version on your computer, or you can specify a custom Houdini installation in the plug-in settings.

# Usage Brief

Here's a list of extra functionalities than official plug-in:

N.B. This list is NOT completed, for details please see `Source/HoudiniEngine/Public/HoudiniEngineCommon.h`

**Common**:
01. Provides a set of C++ API, allow writing custom I/O translator for your own unreal classes or assets

    See `Source/HoudiniEngine/Public/HoudiniInput.h` and `Source/HoudiniEngine/Public/HoudiniOutput.h`
02. Light weight, compact usage and panel widgets.
03. ... (And Much More)

**Paramerter**:
01. Much better nested parameter support.
02. All parameters support copy and paste.
03. Fully Menu support, support menu script (dynamic menu), support "normal", "replace", "toggle" menu types.

04. ... (And Much More)

**Input**:
01. Much more intutive way to draw input curves, support fuse/split points and join/detach curves, support free-hand curve drawing.
02. Support Landscape Mask brush input with mask types of Bit, Byte, and Weight
03. Landscape layers input could be specified for individual editlayer and layer;
04. Landscape layers input support update while brushing.
05. Unreal spline input support import custom properties on your Blueprint.
06. Texture input support.
07. All component type input support.
08. All settings in the parameter input panel could set by parameter tags
09. Mesh inputs are packed before transform.
10. All Input types support shared memory data transport, 8x faster than official shared memory session.

11. ... (And Much More)

**Output**:
01. unreal_uproperty_* support dict and array attributes, almost everything could be set.
02. Mesh output support using s@unreal_split_attr (like instancer output)
02. All output types support partial update, including meshes, instancers, landscapes, geometry collection (chaos), curves.
03. All output support generated to the split actors (i@unreal_split_actors = 1) rather than attach to parent Houdini Actors, and could set these split actors' properties by unreal_uproperty_* attributes, which means you could begin play in editor directly without baking output in a World-Partition level.
04. So Baking is deprecated, use i@unreal_split_actors = 1 instead.
06. Support Texture output (As a HAPI bug, must add my sharedmemory_volumeoutput Sop to your HDA)
07. Landscape output support shared memory output (6x faster than official shared-memory session, need to add my sharedmemory_volumeoutput Sop to your HDA)
08. Class Instancer output is 40x faster than official plug-in, also support instantiate USceneComponent derived Classes
09. Geometry Collection (Chaos) output as instancers (s@unreal_output_instance_type = "GC"), all of the settings on UGeometryCollection could be set by unreal_uproperty_*, also support split and partial output

09. ... (And Much More)