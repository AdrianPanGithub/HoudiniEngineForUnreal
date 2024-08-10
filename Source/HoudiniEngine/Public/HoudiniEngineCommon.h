// Copyright (c) <2024> Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include <string>
#include "CoreMinimal.h"

#include "HoudiniEngineCommon.generated.h"


UENUM()
enum class EHoudiniGenericParameterType : uint8
{
	Int = 0,
	Float,
	String,
	Object,
	MultiParm,  // Need load first, Count stores on StringValue, to preserve value
};

USTRUCT(BlueprintType)
struct HOUDINIENGINE_API FHoudiniGenericParameter : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnyWhere, BlueprintReadWrite)
	EHoudiniGenericParameterType Type = EHoudiniGenericParameterType::Int;

	UPROPERTY(EditAnyWhere, BlueprintReadWrite)
	int32 Size = 0;

	UPROPERTY(EditAnyWhere, BlueprintReadWrite, meta = (EditCondition = "Type == EHoudiniGenericParameterType::Int || Type == EHoudiniGenericParameterType::Float", EditConditionHides))
	FVector4f NumericValues = FVector4f::Zero();

	UPROPERTY(EditAnyWhere, BlueprintReadWrite, meta = (EditCondition = "Type == EHoudiniGenericParameterType::String || Type == EHoudiniGenericParameterType::Object || Type == EHoudiniGenericParameterType::MultiParm", EditConditionHides))
	FString StringValue;  // If is input parm, then StringValue will be set to object infos, like bounds of StaticMesh, or landscape transforms

	UPROPERTY(EditAnyWhere, BlueprintReadWrite, meta = (EditCondition = "Type == EHoudiniGenericParameterType::Object", EditConditionHides))
	TSoftObjectPtr<UObject> ObjectValue;
};

UENUM()
enum class EHoudiniEditOptions : uint32
{
	None = 0X00000000,

	Hide = 0X00000001,
	Show = 0X00000002,

	Disabled = 0X00000010,
	Enabled = 0X00000020,

	CookOnSelect = 0X00001000,
};
ENUM_CLASS_FLAGS(EHoudiniEditOptions);

// All actor refs need to save as FName, as world partition could unload them, so we need actor name to re-load them
USTRUCT()
struct HOUDINIENGINE_API FHoudiniActorHolder
{
	GENERATED_BODY()

public:
	FHoudiniActorHolder() {}

	FHoudiniActorHolder(AActor* Actor);

protected:
	UPROPERTY()
	FName ActorName;

	mutable TWeakObjectPtr<AActor> Actor;

public:
	AActor* Load() const;

	void Destroy() const;
};

UENUM()
enum class EHoudiniCurveType
{
	Points = -1,
	Polygon,
	Subdiv,
	Bezier,
	Interpolate
};

UENUM()
enum class EHoudiniStorageType : int32
{
	Invalid = -1,

	Int = 0,
	Float = 1,
	String = 2,
};

UENUM()
enum class EHoudiniAttributeOwner  // Identical to HAPI_AttributeOwner
{
	Invalid = -1,

	Vertex,
	Point,
	Prim,
	Detail,
	
	Max
};
#define NUM_HOUDINI_ATTRIB_OWNERS   int(EHoudiniAttributeOwner::Max)


enum class EHoudiniVolumeConvertDataType : int
{
	Uint8 = 0,
	Uint16,
	Uint,
	Int,
	Int64,
	Float16,
	Float
};

enum class EHoudiniVolumeStorageType  // Identical to GEO_PrimVolume::StorageType
{
	Int = 0,
	Float,
	Vector2,
	Vector3,
	Vector4
};


UINTERFACE()
class HOUDINIENGINE_API UHoudiniPresetHandler : public UInterface
{
	GENERATED_BODY()
};

class HOUDINIENGINE_API IHoudiniPresetHandler
{
	GENERATED_BODY()

public:
	static TSharedPtr<FJsonObject> ConvertParametersToJson(const TMap<FName, FHoudiniGenericParameter>& Parms);

	static TSharedPtr<TMap<FName, FHoudiniGenericParameter>> ConvertJsonToParameters(const TSharedPtr<FJsonObject>& JsonParms);


	virtual bool GetGenericParameters(TMap<FName, FHoudiniGenericParameter>& OutParms) const = 0;

	virtual void SetGenericParameters(const TSharedPtr<const TMap<FName, FHoudiniGenericParameter>>& Parms) = 0;

	virtual FString GetPresetPathFilter() const = 0;  // NOT EndsWith(TEXT("/"))

	virtual const FString& GetPresetName() const = 0;

	virtual void SetPresetName(const FString& NewPresetName) = 0;


	FString GetPresetFolder() const;  // GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder + GetPresetPathFilter()

	void SavePreset(const FString& SavePresetName) const;

	void LoadPreset(const UDataTable* Preset);

	FORCEINLINE TSharedPtr<FJsonObject> ConvertToPresetJson() const
	{
		TMap<FName, FHoudiniGenericParameter> Parms;
		return GetGenericParameters(Parms) ? ConvertParametersToJson(Parms) : nullptr;
	}

	FORCEINLINE void SetFromPresetJson(const TSharedPtr<FJsonObject>& JsonParms) { SetGenericParameters(ConvertJsonToParameters(JsonParms)); }
};


HOUDINIENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogHoudiniEngine, Log, All)

// Return false only when session loss, otherwise true
#define HAPI_SESSION_INVALID_RESULT(HAPI_RESULT) HAPI_RESULT == HAPI_RESULT_INVALID_SESSION
#define HAPI_SESSION_FAIL_RETURN(HAPI_FUNC) { HAPI_Result HapiResult_ = HAPI_FUNC; if (HapiResult_ != HAPI_RESULT_SUCCESS) { FHoudiniEngineUtils::PrintFailedResult(UE_SOURCE_LOCATION, HapiResult_); if (HAPI_SESSION_INVALID_RESULT(HapiResult_)) { return false; } } }
#define HOUDINI_FAIL_RETURN(HOUDINI_FUNC) { if (!HOUDINI_FUNC) return false; }

// If Session lost, then we should invalidate session data
#define HOUDINI_FAIL_INVALIDATE(HOUDINI_FUNC) { if (!HOUDINI_FUNC) FHoudiniEngine::Get().InvalidateSessionData(); }
#define HOUDINI_FAIL_INVALIDATE_RETURN(HOUDINI_FUNC) { if (!HOUDINI_FUNC) { FHoudiniEngine::Get().InvalidateSessionData(); return; } }


#define IS_VALID_CHAR_IN_HOUDINI(CH) ((TCHAR('0') <= CH && CH <= TCHAR('9')) || (TCHAR('A') <= CH && CH <= TCHAR('Z')) || (CH == TCHAR('_')) || (TCHAR('a') <= CH && CH <= TCHAR('z')))


// ------- Constant Variable --------
#define HOUDINI_ENGINE                                      "HoudiniEngine"

#define HOUDINI_NODE_OUTLINER_FOLDER                        HOUDINI_ENGINE
#define HOUDINI_LOCTEXT_NAMESPACE                           HOUDINI_ENGINE

#define HOUDINI_ENGINE_BAKE_FOLDER_PATH                     TEXT("Bake/")
#define HOUDINI_ENGINE_COOK_FOLDER_PATH                     TEXT("Cook/")
#define HOUDINI_ENGINE_PRESET_FOLDER_PATH                   TEXT("Preset/")

#define HOUDINI_ENGINE_PDG_NODE_LABEL_PREFIX                TEXT("HE")
#define HOUDINI_ENGINE_PDG_OUTPUT_NODE_LABEL_PREFIX         TEXT("HE_OUT")

#if PLATFORM_WINDOWS
#define HAPI_LIB_OBJECT TEXT( "libHAPIL.dll" )
#elif PLATFORM_MAC
#define HAPI_LIB_OBJECT TEXT( "libHAPIL.dylib" )
#elif PLATFORM_LINUX
#define HAPI_LIB_OBJECT TEXT( "libHAPIL.so" )
#else
TEXT("");
#endif

#define POSITION_SCALE_TO_UNREAL_F                          100.0f
#define POSITION_SCALE_TO_HOUDINI_F                         0.01f
#define POSITION_SCALE_TO_UNREAL                            100.0
#define POSITION_SCALE_TO_HOUDINI                           0.01

// -------- DeltaInfo --------
#define HAPI_ATTRIB_DELTA_INFO                              "delta_info"

#define DELTAINFO_PARAMETER                                 TEXT("/parameter/")
#define DELTAINFO_INPUT                                     TEXT("/input/")
#define DELTAINFO_CLASS_POINT                               TEXT("point")
#define DELTAINFO_CLASS_PRIM                                TEXT("prim")

#define DELTAINFO_ACTION_SELECT                             TEXT("select")
#define DELTAINFO_ACTION_APPEND                             TEXT("append")
#define DELTAINFO_ACTION_REMOVE                             TEXT("remove")
#define DELTAINFO_ACTION_TRANSFORM                          TEXT("transform")
#define DELTAINFO_ACTION_SET_ALL_ATTRIBS                    TEXT("attribs")
#define DELTAINFO_ACTION_SPLIT                              TEXT("split")
#define DELTAINFO_ACTION_JOIN                               TEXT("join")

// --------- For copy and paste(append) editgeos --------
#define HOUDINI_JSON_KEY_POINTS                             TEXT("Points")
#define HOUDINI_JSON_KEY_PRIMS                              TEXT("Prims")
#define HOUDINI_JSON_KEY_PRIM_VERTEX_COUNT                  TEXT("PrimVertexCount")
#define HOUDINI_JSON_KEY_PRIM_VERTICES                      TEXT("PrimVertices")

// -------- Groups --------
#define HAPI_GROUP_CHANGED                                  "changed"
#define HAPI_GROUP_MAIN_GEO                                 "main_geo"
#define HAPI_GROUP_COLLISION_GEO                            "collision_geo"
#define HAPI_GROUP_PREFIX_LOD                               "lod_"


// -------- Common --------
#define HAPI_ATTRIB_ID				                        "id"
#define HAPI_ALPHA						                    "Alpha"
#define HAPI_ATTRIB_PRIMITIVE_ID                            "__primitive_id"
#define HAPI_ATTRIB_SHARED_MEMORY_PATH                      "__shared_memory_path__"
#define HAPI_ATTRIB_CURVE_POINT_ID                          "curve_point_id"  // When output curves, hapi will make all pts unique on curves, so this int attrib is using for fuse them
#define HAPI_ATTRIB_PARTIAL_OUTPUT_MODE			            "partial_output_mode"  // Could be both int or string
#define HAPI_PARTIAL_OUTPUT_MODE_REPLACE		            0  // Full update
#define HAPI_PARTIAL_OUTPUT_MODE_MODIFY		                1  // Partial update
#define HAPI_PARTIAL_OUTPUT_MODE_REMOVE		                2  // Partial remove

#define HAPI_ATTRIB_UNREAL_SPLIT_ACTORS                     "unreal_split_actors"  // Could be used for split meshes, curves, instancers, and landscape
#define HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY                 "unreal_uproperty_"
#define HAPI_ATTRIB_PREFIX_UNREAL_MATERIAL_PARAMETER        "unreal_material_parameter_"
#define HAPI_ATTRIB_UNREAL_ACTOR_PATH						"unreal_actor_path"
#define HAPI_ATTRIB_UNREAL_OBJECT_PATH						"unreal_object_path"
#define HAPI_ATTRIB_UNREAL_SPLIT_ATTR						"unreal_split_attr"  // Used for split meshes, curves, and instancers
#define HAPI_ATTRIB_UNREAL_MATERIAL							"unreal_material"
#define HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE				"unreal_material_instance"
#define HAPI_ATTRIB_UNREAL_OBJECT_METADATA                  "unreal_object_metadata"

// -------- Curve --------
#define HAPI_CURVE_TYPE                                     "curve_type"  // See EHoudiniCurveType
#define HAPI_CURVE_CLOSED                                   "curve_closed"
#define HAPI_PARM_TAG_CURVE_COLOR                           "curve_color"
#define HAPI_ATTRIB_COLLISION_NAME                          "collision_name"
#define HAPI_ATTRIB_COLLISION_NORMAL                        "collision_normal"

// -------- Mesh --------
#define HAPI_ATTRIB_LOD_SCREENSIZE                          "lod_screensize"
#define HAPI_ATTRIB_UNREAL_OUTPUT_MESH_TYPE		         	"unreal_output_mesh_type"  // Could be both int or string
#define HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH		        0  // Generate UStaticMesh and UStaticMeshComponent
#define HAPI_UNREAL_OUTPUT_MESH_TYPE_DYNAMICMESH		    1  // Generate UDynamicMeshComponent
#define HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH		    2  // Generate UHoudiniMeshComponent
#define HAPI_ATTRIB_UNREAL_NANITE_ENABLED                   "unreal_nanite_enabled"
// <Deprecated, Removed>, use d@unreal_uproperty_NaniteSettings = set("bEnabled", 1, "FallbackPercentTriangles", 0.5); instead
//#define HAPI_UNREAL_ATTRIB_NANITE_POSITION_PRECISION        "unreal_nanite_position_precision"
//#define HAPI_UNREAL_ATTRIB_NANITE_PERCENT_TRIANGLES         "unreal_nanite_percent_triangles"
//#define HAPI_UNREAL_ATTRIB_NANITE_FB_RELATIVE_ERROR         "unreal_nanite_fallback_relative_error"
//#define HAPI_UNREAL_ATTRIB_NANITE_TRIM_RELATIVE_ERROR       "unreal_nanite_trim_relative_error"

// -------- Instancer --------
#define HAPI_ATTRIB_UNREAL_INSTANCE				            "unreal_instance"
#define HAPI_ATTRIB_UNREAL_OUTPUT_INSTANCE_TYPE             "unreal_output_instance_type"  // Could be both int or string
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_AUTO               0   // Decided by asset to instantiate
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ISMC               1
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_HISMC              2
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_COMPONENTS         3
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ACTORS             4
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_FOLIAGE            5
#define HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_CHAOS              6  // Geometry Collection
#define HAPI_ATTRIB_UNREAL_INSTANCE_NUM_CUSTOM_FLOATS		"unreal_num_custom_floats"
#define HAPI_ATTRIB_UNREAL_INSTANCE_CUSTOM_DATA_PREFIX		"unreal_per_instance_custom_data"
#define HAPI_UNREAL_ATTRIB_FORCE_INSTANCER					"unreal_force_instancer"  // <Deprecated>, use "unreal_instance_output_mode" instead
#define HAPI_UNREAL_ATTRIB_HIERARCHICAL_INSTANCED_SM		"unreal_hierarchical_instancer"  // <Deprecated>, use i@unreal_instance_output_mode = 2 or s@unreal_instance_output_mode = \"hierarchical\" instead
#define HAPI_UNREAL_ATTRIB_FOLIAGE_INSTANCER				"unreal_foliage"  // <Deprecated>, use i@unreal_instance_output_mode = 2 or s@unreal_instance_output_mode = \"hierarchical\" instead

// -------- DataTable --------
#define HAPI_ATTRIB_PREFIX_UNREAL_DATA_TABLE				"unreal_data_table_"
#define HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWSTRUCT				"unreal_data_table_rowstruct"
#define HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWNAME               "unreal_data_table_rowname"

// -------- Landscape --------
#define HAPI_ATTRIB_UNREAL_OUTPUT_NAME			            "unreal_output_name"

#define HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND	"unreal_landscape_layer_noweightblend"  // <Deprecated>, use "unreal_instance_output_mode" instead

#define HAPI_ATTRIB_UNREAL_LANDSCAPE_HOLE_MATERIAL		    "unreal_landscape_hole_material"
#define HAPI_ATTRIB_UNREAL_LANDSCAPE_HOLE_MATERIAL_INSTANCE	"unreal_landscape_hole_material_instance"
#define HAPI_UNREAL_ATTRIB_MATERIAL_HOLE					"unreal_material_hole"  // <Deprecated>, use "unreal_landscape_hole_material" instead
#define HAPI_UNREAL_ATTRIB_MATERIAL_HOLE_INSTANCE			"unreal_material_hole_instance"  // <Deprecated>, use "unreal_landscape_hole_material_instance" instead

#define HAPI_ATTRIB_UNREAL_LANDSCAPE_OUTPUT_MODE			"unreal_landscape_output_mode"
#define HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_GENERATE			0  // Generate a new ALandscape
#define HAPI_UNREAL_LANDSCAPE_OUTPUT_MODE_MODIFY_LAYER		1  // Write back to exist landscape in current level

#define HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_NAME			"unreal_landscape_editlayer_name"
#define HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_CLEAR		"unreal_landscape_editlayer_clear"  // Clear the editlayer before blitting new data 
#define HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_AFTER		"unreal_landscape_editlayer_after"  // Place the output layer "after" the given layer

#define HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_TYPE			"unreal_landscape_editlayer_type"
#define HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_BASE			0  // Base layer: Values will be fit to the min/max height range in UE for optimal resolution.
#define HAPI_UNREAL_LANDSCAPE_EDITLAYER_TYPE_ADDITIVE		1  // 1 - Additive layer: Values will be scaled similar to the base layer but will NOT be offset
//     so that it will remain centered around the zero value.

// Subtractive mode for paint layers on landscape edit layers
#define HAPI_UNREAL_ATTRIB_LANDSCAPE_EDITLAYER_SUBTRACTIVE	"unreal_landscape_editlayer_subtractive"
#define HAPI_UNREAL_LANDSCAPE_EDITLAYER_SUBTRACTIVE_ON		0
#define HAPI_UNREAL_LANDSCAPE_EDITLAYER_SUBTRACTIVE_OFF		1

#define HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_INFO				"unreal_landscape_layer_info"

// -------- Texture --------
#define HAPI_ATTRIB_UNREAL_TEXTURE_STORAGE                  "unreal_texture_storage"
#define HAPI_UNREAL_TEXTURE_STORAGE_UINT8                   0
#define HAPI_UNREAL_TEXTURE_STORAGE_FLOAT16                 1
#define HAPI_UNREAL_TEXTURE_STORAGE_UINT16                  2
#define HAPI_UNREAL_TEXTURE_STORAGE_FLOAT                   3
#define HAPI_UNREAL_TEXTURE_STORAGE_STR_UINT8               "uint8"
#define HAPI_UNREAL_TEXTURE_STORAGE_STR_UINT16              "uint16"
#define HAPI_UNREAL_TEXTURE_STORAGE_STR_FLOAT16             "float16"
#define HAPI_UNREAL_TEXTURE_STORAGE_STR_FLOAT               "float"

// -------- Input --------
#define HAPI_ATTRIB_UNREAL_SPLINE_POINT_ARRIVE_TANGENT      "unreal_spline_point_arrive_tangent"
#define HAPI_ATTRIB_UNREAL_SPLINE_POINT_LEAVE_TANGENT       "unreal_spline_point_leave_tangent"
#define HAPI_ATTRIB_UNREAL_LANDSCAPE_SPLINE_TANGENT_LENGTH  "unreal_landscape_spline_tangent_length"
#define HAPI_ATTRIB_UNREAL_BRUSH_TYPE                       "unreal_brush_type"  // See EBrushType

// -------- ParmTags --------
#define HAPI_PARM_TAG_AS_TOGGLE                             "as_toggle"
#define HAPI_PARM_TAG_EDITABLE                              "editable"
#define HAPI_PARM_TAG_COOK_ON_SELECT                        "cook_on_select"
#define HAPI_PARM_TAG_IDENTIFIER_NAME                       "identifier_name"

#define HAPI_PARM_TAG_UNREAL_REF				        	"unreal_ref"
#define HAPI_PARM_TAG_UNREAL_REF_CLASS                      "unreal_ref_class"
#define HAPI_PARM_TAG_UNREAL_REF_FILTER                     "unreal_ref_filter"

#define HAPI_PARM_TAG_NUM_INPUT_OBJECTS			            "num_input_objects"  // For content input, <= 0 means dynamic num objects
#define HAPI_PARM_TAG_LOD_IMPORT_METHOD                     "lod_import_method"
#define HAPI_PARM_TAG_UNREAL_ACTOR_FILTER_METHOD            "unreal_actor_filter_method"
#define HAPI_PARM_TAG_PAINT_UPDATE_METHOD                   "paint_update_method"  // For landscape and mask input
#define HAPI_PARM_TAG_MASK_TYPE                             "mask_type"
#define HAPI_PARM_TAG_BYTE_MASK_VALUE_PARM_NAME	          	"byte_mask_value_parm_name"

#define HAPI_PARM_TAG_IMPORT_LANDSCAPE_SPLINES              "import_landscape_splines"
#define HAPI_PARM_TAG_LANDSCAPE_LAYER                       "landscape_layer"
#define HAPI_PARM_TAG_PREFIX_UNREAL_LANDSCAPE_EDITLAYER     "unreal_landscape_editlayer_"

// ------- Enviroment Variable --------
#define HAPI_ENV_CLIENT_PROJECT_DIR                         "CLIENT_PROJECT_DIR"
#define HAPI_ENV_CLIENT_SCENE_PATH                          "CLIENT_SCENE_PATH"
#define HAPI_ENV_HOUDINI_ENGINE_FOLDER                      "HOUDINI_ENGINE_FOLDER"

// ------- Notification --------
#define HAPI_MESSAGE_START_SESSION_SYNC                     "Open Houdini Session Sync..."
#define HAPI_MESSAGE_START_SESSION                          "Start HARS...\n(Houdini Engine API Remote Server)"
#define HAPI_MESSAGE_RESTART_SESSION                        "Restart HARS...\n(Houdini Engine API Remote Server)"


static const FName HoudiniHeightLayerName("height");
static const FName HoudiniAlphaLayerName("Alpha");
static const FName HoudiniMaskLayerName("mask");
static const FName HoudiniPartialOutputMaskName("partial_output_mask");
