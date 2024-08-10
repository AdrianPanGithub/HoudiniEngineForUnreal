// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniEngine/Public/HoudiniEngineCommon.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeHoudiniEngineCommon() {}

// Begin Cross Module References
COREUOBJECT_API UClass* Z_Construct_UClass_UInterface();
COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister();
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FVector4f();
ENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FTableRowBase();
HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniPresetHandler();
HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniPresetHandler_NoRegister();
HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner();
HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType();
HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions();
HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType();
HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType();
HOUDINIENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FHoudiniActorHolder();
HOUDINIENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FHoudiniGenericParameter();
UPackage* Z_Construct_UPackage__Script_HoudiniEngine();
// End Cross Module References

// Begin Enum EHoudiniGenericParameterType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniGenericParameterType;
static UEnum* EHoudiniGenericParameterType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EHoudiniGenericParameterType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EHoudiniGenericParameterType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniGenericParameterType"));
	}
	return Z_Registration_Info_UEnum_EHoudiniGenericParameterType.OuterSingleton;
}
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniGenericParameterType>()
{
	return EHoudiniGenericParameterType_StaticEnum();
}
struct Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "Float.Name", "EHoudiniGenericParameterType::Float" },
		{ "Int.Name", "EHoudiniGenericParameterType::Int" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
		{ "MultiParm.Name", "EHoudiniGenericParameterType::MultiParm" },
		{ "Object.Name", "EHoudiniGenericParameterType::Object" },
		{ "String.Name", "EHoudiniGenericParameterType::String" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EHoudiniGenericParameterType::Int", (int64)EHoudiniGenericParameterType::Int },
		{ "EHoudiniGenericParameterType::Float", (int64)EHoudiniGenericParameterType::Float },
		{ "EHoudiniGenericParameterType::String", (int64)EHoudiniGenericParameterType::String },
		{ "EHoudiniGenericParameterType::Object", (int64)EHoudiniGenericParameterType::Object },
		{ "EHoudiniGenericParameterType::MultiParm", (int64)EHoudiniGenericParameterType::MultiParm },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	"EHoudiniGenericParameterType",
	"EHoudiniGenericParameterType",
	Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType()
{
	if (!Z_Registration_Info_UEnum_EHoudiniGenericParameterType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniGenericParameterType.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EHoudiniGenericParameterType.InnerSingleton;
}
// End Enum EHoudiniGenericParameterType

// Begin ScriptStruct FHoudiniGenericParameter
static_assert(std::is_polymorphic<FHoudiniGenericParameter>() == std::is_polymorphic<FTableRowBase>(), "USTRUCT FHoudiniGenericParameter cannot be polymorphic unless super FTableRowBase is polymorphic");
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_HoudiniGenericParameter;
class UScriptStruct* FHoudiniGenericParameter::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniGenericParameter.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_HoudiniGenericParameter.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FHoudiniGenericParameter, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("HoudiniGenericParameter"));
	}
	return Z_Registration_Info_UScriptStruct_HoudiniGenericParameter.OuterSingleton;
}
template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<FHoudiniGenericParameter>()
{
	return FHoudiniGenericParameter::StaticStruct();
}
struct Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
		{ "BlueprintType", "true" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Type_MetaData[] = {
		{ "Category", "HoudiniGenericParameter" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Size_MetaData[] = {
		{ "Category", "HoudiniGenericParameter" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_NumericValues_MetaData[] = {
		{ "Category", "HoudiniGenericParameter" },
		{ "EditCondition", "Type == EHoudiniGenericParameterType::Int || Type == EHoudiniGenericParameterType::Float" },
		{ "EditConditionHides", "" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StringValue_MetaData[] = {
		{ "Category", "HoudiniGenericParameter" },
		{ "EditCondition", "Type == EHoudiniGenericParameterType::String || Type == EHoudiniGenericParameterType::Object || Type == EHoudiniGenericParameterType::MultiParm" },
		{ "EditConditionHides", "" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ObjectValue_MetaData[] = {
		{ "Category", "HoudiniGenericParameter" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "// If is input parm, then StringValue will be set to object infos, like bounds of StaticMesh, or landscape transforms\n" },
#endif
		{ "EditCondition", "Type == EHoudiniGenericParameterType::Object" },
		{ "EditConditionHides", "" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "If is input parm, then StringValue will be set to object infos, like bounds of StaticMesh, or landscape transforms" },
#endif
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FBytePropertyParams NewProp_Type_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_Type;
	static const UECodeGen_Private::FIntPropertyParams NewProp_Size;
	static const UECodeGen_Private::FStructPropertyParams NewProp_NumericValues;
	static const UECodeGen_Private::FStrPropertyParams NewProp_StringValue;
	static const UECodeGen_Private::FSoftObjectPropertyParams NewProp_ObjectValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHoudiniGenericParameter>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_Type_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_Type = { "Type", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniGenericParameter, Type), Z_Construct_UEnum_HoudiniEngine_EHoudiniGenericParameterType, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Type_MetaData), NewProp_Type_MetaData) }; // 2036901813
const UECodeGen_Private::FIntPropertyParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_Size = { "Size", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniGenericParameter, Size), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Size_MetaData), NewProp_Size_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_NumericValues = { "NumericValues", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniGenericParameter, NumericValues), Z_Construct_UScriptStruct_FVector4f, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_NumericValues_MetaData), NewProp_NumericValues_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_StringValue = { "StringValue", nullptr, (EPropertyFlags)0x0010000000000005, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniGenericParameter, StringValue), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StringValue_MetaData), NewProp_StringValue_MetaData) };
const UECodeGen_Private::FSoftObjectPropertyParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_ObjectValue = { "ObjectValue", nullptr, (EPropertyFlags)0x0014000000000005, UECodeGen_Private::EPropertyGenFlags::SoftObject, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniGenericParameter, ObjectValue), Z_Construct_UClass_UObject_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ObjectValue_MetaData), NewProp_ObjectValue_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_Type_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_Type,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_Size,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_NumericValues,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_StringValue,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewProp_ObjectValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
	Z_Construct_UScriptStruct_FTableRowBase,
	&NewStructOps,
	"HoudiniGenericParameter",
	Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::PropPointers),
	sizeof(FHoudiniGenericParameter),
	alignof(FHoudiniGenericParameter),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FHoudiniGenericParameter()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniGenericParameter.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_HoudiniGenericParameter.InnerSingleton, Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_HoudiniGenericParameter.InnerSingleton;
}
// End ScriptStruct FHoudiniGenericParameter

// Begin Enum EHoudiniEditOptions
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniEditOptions;
static UEnum* EHoudiniEditOptions_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EHoudiniEditOptions.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EHoudiniEditOptions.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniEditOptions"));
	}
	return Z_Registration_Info_UEnum_EHoudiniEditOptions.OuterSingleton;
}
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniEditOptions>()
{
	return EHoudiniEditOptions_StaticEnum();
}
struct Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "CookOnSelect.Name", "EHoudiniEditOptions::CookOnSelect" },
		{ "Disabled.Name", "EHoudiniEditOptions::Disabled" },
		{ "Enabled.Name", "EHoudiniEditOptions::Enabled" },
		{ "Hide.Name", "EHoudiniEditOptions::Hide" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
		{ "None.Name", "EHoudiniEditOptions::None" },
		{ "Show.Name", "EHoudiniEditOptions::Show" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EHoudiniEditOptions::None", (int64)EHoudiniEditOptions::None },
		{ "EHoudiniEditOptions::Hide", (int64)EHoudiniEditOptions::Hide },
		{ "EHoudiniEditOptions::Show", (int64)EHoudiniEditOptions::Show },
		{ "EHoudiniEditOptions::Disabled", (int64)EHoudiniEditOptions::Disabled },
		{ "EHoudiniEditOptions::Enabled", (int64)EHoudiniEditOptions::Enabled },
		{ "EHoudiniEditOptions::CookOnSelect", (int64)EHoudiniEditOptions::CookOnSelect },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	"EHoudiniEditOptions",
	"EHoudiniEditOptions",
	Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions()
{
	if (!Z_Registration_Info_UEnum_EHoudiniEditOptions.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniEditOptions.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniEditOptions_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EHoudiniEditOptions.InnerSingleton;
}
// End Enum EHoudiniEditOptions

// Begin ScriptStruct FHoudiniActorHolder
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_HoudiniActorHolder;
class UScriptStruct* FHoudiniActorHolder::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniActorHolder.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_HoudiniActorHolder.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FHoudiniActorHolder, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("HoudiniActorHolder"));
	}
	return Z_Registration_Info_UScriptStruct_HoudiniActorHolder.OuterSingleton;
}
template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<FHoudiniActorHolder>()
{
	return FHoudiniActorHolder::StaticStruct();
}
struct Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// All actor refs need to save as FName, as world partition could unload them, so we need actor name to re-load them\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "All actor refs need to save as FName, as world partition could unload them, so we need actor name to re-load them" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ActorName_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FNamePropertyParams NewProp_ActorName;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHoudiniActorHolder>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FNamePropertyParams Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::NewProp_ActorName = { "ActorName", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniActorHolder, ActorName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ActorName_MetaData), NewProp_ActorName_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::NewProp_ActorName,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	&NewStructOps,
	"HoudiniActorHolder",
	Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::PropPointers),
	sizeof(FHoudiniActorHolder),
	alignof(FHoudiniActorHolder),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FHoudiniActorHolder()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniActorHolder.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_HoudiniActorHolder.InnerSingleton, Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_HoudiniActorHolder.InnerSingleton;
}
// End ScriptStruct FHoudiniActorHolder

// Begin Enum EHoudiniCurveType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniCurveType;
static UEnum* EHoudiniCurveType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EHoudiniCurveType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EHoudiniCurveType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniCurveType"));
	}
	return Z_Registration_Info_UEnum_EHoudiniCurveType.OuterSingleton;
}
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniCurveType>()
{
	return EHoudiniCurveType_StaticEnum();
}
struct Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "Bezier.Name", "EHoudiniCurveType::Bezier" },
		{ "Interpolate.Name", "EHoudiniCurveType::Interpolate" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
		{ "Points.Name", "EHoudiniCurveType::Points" },
		{ "Polygon.Name", "EHoudiniCurveType::Polygon" },
		{ "Subdiv.Name", "EHoudiniCurveType::Subdiv" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EHoudiniCurveType::Points", (int64)EHoudiniCurveType::Points },
		{ "EHoudiniCurveType::Polygon", (int64)EHoudiniCurveType::Polygon },
		{ "EHoudiniCurveType::Subdiv", (int64)EHoudiniCurveType::Subdiv },
		{ "EHoudiniCurveType::Bezier", (int64)EHoudiniCurveType::Bezier },
		{ "EHoudiniCurveType::Interpolate", (int64)EHoudiniCurveType::Interpolate },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	"EHoudiniCurveType",
	"EHoudiniCurveType",
	Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType()
{
	if (!Z_Registration_Info_UEnum_EHoudiniCurveType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniCurveType.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EHoudiniCurveType.InnerSingleton;
}
// End Enum EHoudiniCurveType

// Begin Enum EHoudiniStorageType
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniStorageType;
static UEnum* EHoudiniStorageType_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EHoudiniStorageType.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EHoudiniStorageType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniStorageType"));
	}
	return Z_Registration_Info_UEnum_EHoudiniStorageType.OuterSingleton;
}
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniStorageType>()
{
	return EHoudiniStorageType_StaticEnum();
}
struct Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
		{ "Float.Name", "EHoudiniStorageType::Float" },
		{ "Int.Name", "EHoudiniStorageType::Int" },
		{ "Invalid.Name", "EHoudiniStorageType::Invalid" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
		{ "String.Name", "EHoudiniStorageType::String" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EHoudiniStorageType::Invalid", (int64)EHoudiniStorageType::Invalid },
		{ "EHoudiniStorageType::Int", (int64)EHoudiniStorageType::Int },
		{ "EHoudiniStorageType::Float", (int64)EHoudiniStorageType::Float },
		{ "EHoudiniStorageType::String", (int64)EHoudiniStorageType::String },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	"EHoudiniStorageType",
	"EHoudiniStorageType",
	Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType()
{
	if (!Z_Registration_Info_UEnum_EHoudiniStorageType.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniStorageType.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniStorageType_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EHoudiniStorageType.InnerSingleton;
}
// End Enum EHoudiniStorageType

// Begin Enum EHoudiniAttributeOwner
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniAttributeOwner;
static UEnum* EHoudiniAttributeOwner_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EHoudiniAttributeOwner.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EHoudiniAttributeOwner.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniAttributeOwner"));
	}
	return Z_Registration_Info_UEnum_EHoudiniAttributeOwner.OuterSingleton;
}
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniAttributeOwner>()
{
	return EHoudiniAttributeOwner_StaticEnum();
}
struct Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Identical to HAPI_AttributeOwner\n" },
#endif
		{ "Detail.Name", "EHoudiniAttributeOwner::Detail" },
		{ "Invalid.Name", "EHoudiniAttributeOwner::Invalid" },
		{ "Max.Name", "EHoudiniAttributeOwner::Max" },
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
		{ "Point.Name", "EHoudiniAttributeOwner::Point" },
		{ "Prim.Name", "EHoudiniAttributeOwner::Prim" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Identical to HAPI_AttributeOwner" },
#endif
		{ "Vertex.Name", "EHoudiniAttributeOwner::Vertex" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EHoudiniAttributeOwner::Invalid", (int64)EHoudiniAttributeOwner::Invalid },
		{ "EHoudiniAttributeOwner::Vertex", (int64)EHoudiniAttributeOwner::Vertex },
		{ "EHoudiniAttributeOwner::Point", (int64)EHoudiniAttributeOwner::Point },
		{ "EHoudiniAttributeOwner::Prim", (int64)EHoudiniAttributeOwner::Prim },
		{ "EHoudiniAttributeOwner::Detail", (int64)EHoudiniAttributeOwner::Detail },
		{ "EHoudiniAttributeOwner::Max", (int64)EHoudiniAttributeOwner::Max },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
};
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	"EHoudiniAttributeOwner",
	"EHoudiniAttributeOwner",
	Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner()
{
	if (!Z_Registration_Info_UEnum_EHoudiniAttributeOwner.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniAttributeOwner.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniAttributeOwner_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EHoudiniAttributeOwner.InnerSingleton;
}
// End Enum EHoudiniAttributeOwner

// Begin Interface UHoudiniPresetHandler
void UHoudiniPresetHandler::StaticRegisterNativesUHoudiniPresetHandler()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UHoudiniPresetHandler);
UClass* Z_Construct_UClass_UHoudiniPresetHandler_NoRegister()
{
	return UHoudiniPresetHandler::StaticClass();
}
struct Z_Construct_UClass_UHoudiniPresetHandler_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/HoudiniEngineCommon.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<IHoudiniPresetHandler>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_UHoudiniPresetHandler_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UInterface,
	(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniPresetHandler_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UHoudiniPresetHandler_Statics::ClassParams = {
	&UHoudiniPresetHandler::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x001040A1u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniPresetHandler_Statics::Class_MetaDataParams), Z_Construct_UClass_UHoudiniPresetHandler_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UHoudiniPresetHandler()
{
	if (!Z_Registration_Info_UClass_UHoudiniPresetHandler.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniPresetHandler.OuterSingleton, Z_Construct_UClass_UHoudiniPresetHandler_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UHoudiniPresetHandler.OuterSingleton;
}
template<> HOUDINIENGINE_API UClass* StaticClass<UHoudiniPresetHandler>()
{
	return UHoudiniPresetHandler::StaticClass();
}
UHoudiniPresetHandler::UHoudiniPresetHandler(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(UHoudiniPresetHandler);
UHoudiniPresetHandler::~UHoudiniPresetHandler() {}
// End Interface UHoudiniPresetHandler

// Begin Registration
struct Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EHoudiniGenericParameterType_StaticEnum, TEXT("EHoudiniGenericParameterType"), &Z_Registration_Info_UEnum_EHoudiniGenericParameterType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2036901813U) },
		{ EHoudiniEditOptions_StaticEnum, TEXT("EHoudiniEditOptions"), &Z_Registration_Info_UEnum_EHoudiniEditOptions, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2391968578U) },
		{ EHoudiniCurveType_StaticEnum, TEXT("EHoudiniCurveType"), &Z_Registration_Info_UEnum_EHoudiniCurveType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3009086690U) },
		{ EHoudiniStorageType_StaticEnum, TEXT("EHoudiniStorageType"), &Z_Registration_Info_UEnum_EHoudiniStorageType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2516412674U) },
		{ EHoudiniAttributeOwner_StaticEnum, TEXT("EHoudiniAttributeOwner"), &Z_Registration_Info_UEnum_EHoudiniAttributeOwner, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 2857751349U) },
	};
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FHoudiniGenericParameter::StaticStruct, Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics::NewStructOps, TEXT("HoudiniGenericParameter"), &Z_Registration_Info_UScriptStruct_HoudiniGenericParameter, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FHoudiniGenericParameter), 4122214735U) },
		{ FHoudiniActorHolder::StaticStruct, Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics::NewStructOps, TEXT("HoudiniActorHolder"), &Z_Registration_Info_UScriptStruct_HoudiniActorHolder, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FHoudiniActorHolder), 2130876777U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniPresetHandler, UHoudiniPresetHandler::StaticClass, TEXT("UHoudiniPresetHandler"), &Z_Registration_Info_UClass_UHoudiniPresetHandler, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniPresetHandler), 3842740422U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_101360685(TEXT("/Script/HoudiniEngine"),
	Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics::ClassInfo),
	Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics::ScriptStructInfo),
	Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_Statics::EnumInfo));
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
