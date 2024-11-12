// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniEngine/Public/HoudiniInput.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeHoudiniInput() {}
// Cross Module References
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject_NoRegister();
	COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FColor();
	HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniInput();
	HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniInput_NoRegister();
	HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniInputHolder();
	HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniInputHolder_NoRegister();
	HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod();
	HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType();
	HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType();
	HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType();
	HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod();
	HOUDINIENGINE_API UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod();
	HOUDINIENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FHoudiniInputSettings();
	UPackage* Z_Construct_UPackage__Script_HoudiniEngine();
// End Cross Module References
	static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniInputType;
	static UEnum* EHoudiniInputType_StaticEnum()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniInputType.OuterSingleton)
		{
			Z_Registration_Info_UEnum_EHoudiniInputType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniInputType"));
		}
		return Z_Registration_Info_UEnum_EHoudiniInputType.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniInputType>()
	{
		return EHoudiniInputType_StaticEnum();
	}
	struct Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics
	{
		static const UECodeGen_Private::FEnumeratorParam Enumerators[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[];
#endif
		static const UECodeGen_Private::FEnumParams EnumParams;
	};
	const UECodeGen_Private::FEnumeratorParam Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::Enumerators[] = {
		{ "EHoudiniInputType::Content", (int64)EHoudiniInputType::Content },
		{ "EHoudiniInputType::Curves", (int64)EHoudiniInputType::Curves },
		{ "EHoudiniInputType::World", (int64)EHoudiniInputType::World },
		{ "EHoudiniInputType::Node", (int64)EHoudiniInputType::Node },
		{ "EHoudiniInputType::Mask", (int64)EHoudiniInputType::Mask },
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::Enum_MetaDataParams[] = {
		{ "Content.Name", "EHoudiniInputType::Content" },
		{ "Curves.Comment", "// StaticMesh, DataTable, Texture, Blueprint, FoliageType, maybe has null holder to be placeholders\n" },
		{ "Curves.Name", "EHoudiniInputType::Curves" },
		{ "Curves.ToolTip", "StaticMesh, DataTable, Texture, Blueprint, FoliageType, maybe has null holder to be placeholders" },
		{ "Mask.Comment", "// Only has one holder\n" },
		{ "Mask.Name", "EHoudiniInputType::Mask" },
		{ "Mask.ToolTip", "Only has one holder" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
		{ "Node.Comment", "// InputActors and InputLandscapes\n" },
		{ "Node.Name", "EHoudiniInputType::Node" },
		{ "Node.ToolTip", "InputActors and InputLandscapes" },
		{ "World.Comment", "// Only has one holder\n" },
		{ "World.Name", "EHoudiniInputType::World" },
		{ "World.ToolTip", "Only has one holder" },
	};
#endif
	const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
		nullptr,
		"EHoudiniInputType",
		"EHoudiniInputType",
		Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::Enumerators,
		RF_Public|RF_Transient|RF_MarkAsNative,
		UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::Enumerators),
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::Enum_MetaDataParams)
	};
	UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniInputType.InnerSingleton)
		{
			UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniInputType.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType_Statics::EnumParams);
		}
		return Z_Registration_Info_UEnum_EHoudiniInputType.InnerSingleton;
	}
	static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniActorFilterMethod;
	static UEnum* EHoudiniActorFilterMethod_StaticEnum()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniActorFilterMethod.OuterSingleton)
		{
			Z_Registration_Info_UEnum_EHoudiniActorFilterMethod.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniActorFilterMethod"));
		}
		return Z_Registration_Info_UEnum_EHoudiniActorFilterMethod.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniActorFilterMethod>()
	{
		return EHoudiniActorFilterMethod_StaticEnum();
	}
	struct Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics
	{
		static const UECodeGen_Private::FEnumeratorParam Enumerators[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[];
#endif
		static const UECodeGen_Private::FEnumParams EnumParams;
	};
	const UECodeGen_Private::FEnumeratorParam Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::Enumerators[] = {
		{ "EHoudiniActorFilterMethod::Selection", (int64)EHoudiniActorFilterMethod::Selection },
		{ "EHoudiniActorFilterMethod::Class", (int64)EHoudiniActorFilterMethod::Class },
		{ "EHoudiniActorFilterMethod::Label", (int64)EHoudiniActorFilterMethod::Label },
		{ "EHoudiniActorFilterMethod::Tag", (int64)EHoudiniActorFilterMethod::Tag },
		{ "EHoudiniActorFilterMethod::Folder", (int64)EHoudiniActorFilterMethod::Folder },
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::Enum_MetaDataParams[] = {
		{ "Class.Name", "EHoudiniActorFilterMethod::Class" },
		{ "Folder.Name", "EHoudiniActorFilterMethod::Folder" },
		{ "Label.Name", "EHoudiniActorFilterMethod::Label" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
		{ "Selection.Name", "EHoudiniActorFilterMethod::Selection" },
		{ "Tag.Name", "EHoudiniActorFilterMethod::Tag" },
	};
#endif
	const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
		nullptr,
		"EHoudiniActorFilterMethod",
		"EHoudiniActorFilterMethod",
		Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::Enumerators,
		RF_Public|RF_Transient|RF_MarkAsNative,
		UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::Enumerators),
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::Enum_MetaDataParams)
	};
	UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniActorFilterMethod.InnerSingleton)
		{
			UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniActorFilterMethod.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod_Statics::EnumParams);
		}
		return Z_Registration_Info_UEnum_EHoudiniActorFilterMethod.InnerSingleton;
	}
	static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniMaskType;
	static UEnum* EHoudiniMaskType_StaticEnum()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniMaskType.OuterSingleton)
		{
			Z_Registration_Info_UEnum_EHoudiniMaskType.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniMaskType"));
		}
		return Z_Registration_Info_UEnum_EHoudiniMaskType.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniMaskType>()
	{
		return EHoudiniMaskType_StaticEnum();
	}
	struct Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics
	{
		static const UECodeGen_Private::FEnumeratorParam Enumerators[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[];
#endif
		static const UECodeGen_Private::FEnumParams EnumParams;
	};
	const UECodeGen_Private::FEnumeratorParam Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::Enumerators[] = {
		{ "EHoudiniMaskType::Bit", (int64)EHoudiniMaskType::Bit },
		{ "EHoudiniMaskType::Weight", (int64)EHoudiniMaskType::Weight },
		{ "EHoudiniMaskType::Byte", (int64)EHoudiniMaskType::Byte },
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::Enum_MetaDataParams[] = {
		{ "Bit.Name", "EHoudiniMaskType::Bit" },
		{ "Byte.Name", "EHoudiniMaskType::Byte" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
		{ "Weight.Name", "EHoudiniMaskType::Weight" },
	};
#endif
	const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
		nullptr,
		"EHoudiniMaskType",
		"EHoudiniMaskType",
		Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::Enumerators,
		RF_Public|RF_Transient|RF_MarkAsNative,
		UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::Enumerators),
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::Enum_MetaDataParams)
	};
	UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniMaskType.InnerSingleton)
		{
			UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniMaskType.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType_Statics::EnumParams);
		}
		return Z_Registration_Info_UEnum_EHoudiniMaskType.InnerSingleton;
	}
	static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod;
	static UEnum* EHoudiniStaticMeshLODImportMethod_StaticEnum()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod.OuterSingleton)
		{
			Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniStaticMeshLODImportMethod"));
		}
		return Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniStaticMeshLODImportMethod>()
	{
		return EHoudiniStaticMeshLODImportMethod_StaticEnum();
	}
	struct Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics
	{
		static const UECodeGen_Private::FEnumeratorParam Enumerators[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[];
#endif
		static const UECodeGen_Private::FEnumParams EnumParams;
	};
	const UECodeGen_Private::FEnumeratorParam Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::Enumerators[] = {
		{ "EHoudiniStaticMeshLODImportMethod::FirstLOD", (int64)EHoudiniStaticMeshLODImportMethod::FirstLOD },
		{ "EHoudiniStaticMeshLODImportMethod::LastLOD", (int64)EHoudiniStaticMeshLODImportMethod::LastLOD },
		{ "EHoudiniStaticMeshLODImportMethod::AllLODs", (int64)EHoudiniStaticMeshLODImportMethod::AllLODs },
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::Enum_MetaDataParams[] = {
		{ "AllLODs.Name", "EHoudiniStaticMeshLODImportMethod::AllLODs" },
		{ "FirstLOD.Name", "EHoudiniStaticMeshLODImportMethod::FirstLOD" },
		{ "LastLOD.Name", "EHoudiniStaticMeshLODImportMethod::LastLOD" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
		nullptr,
		"EHoudiniStaticMeshLODImportMethod",
		"EHoudiniStaticMeshLODImportMethod",
		Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::Enumerators,
		RF_Public|RF_Transient|RF_MarkAsNative,
		UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::Enumerators),
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::Enum_MetaDataParams)
	};
	UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod.InnerSingleton)
		{
			UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod_Statics::EnumParams);
		}
		return Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod.InnerSingleton;
	}
	static FEnumRegistrationInfo Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod;
	static UEnum* EHoudiniMeshCollisionImportMethod_StaticEnum()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod.OuterSingleton)
		{
			Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("EHoudiniMeshCollisionImportMethod"));
		}
		return Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniMeshCollisionImportMethod>()
	{
		return EHoudiniMeshCollisionImportMethod_StaticEnum();
	}
	struct Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics
	{
		static const UECodeGen_Private::FEnumeratorParam Enumerators[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[];
#endif
		static const UECodeGen_Private::FEnumParams EnumParams;
	};
	const UECodeGen_Private::FEnumeratorParam Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::Enumerators[] = {
		{ "EHoudiniMeshCollisionImportMethod::NoImportCollision", (int64)EHoudiniMeshCollisionImportMethod::NoImportCollision },
		{ "EHoudiniMeshCollisionImportMethod::ImportWithMesh", (int64)EHoudiniMeshCollisionImportMethod::ImportWithMesh },
		{ "EHoudiniMeshCollisionImportMethod::ImportWithoutMesh", (int64)EHoudiniMeshCollisionImportMethod::ImportWithoutMesh },
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::Enum_MetaDataParams[] = {
		{ "ImportWithMesh.Name", "EHoudiniMeshCollisionImportMethod::ImportWithMesh" },
		{ "ImportWithoutMesh.Name", "EHoudiniMeshCollisionImportMethod::ImportWithoutMesh" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
		{ "NoImportCollision.Name", "EHoudiniMeshCollisionImportMethod::NoImportCollision" },
	};
#endif
	const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::EnumParams = {
		(UObject*(*)())Z_Construct_UPackage__Script_HoudiniEngine,
		nullptr,
		"EHoudiniMeshCollisionImportMethod",
		"EHoudiniMeshCollisionImportMethod",
		Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::Enumerators,
		RF_Public|RF_Transient|RF_MarkAsNative,
		UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::Enumerators),
		EEnumFlags::None,
		(uint8)UEnum::ECppForm::EnumClass,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::Enum_MetaDataParams)
	};
	UEnum* Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod()
	{
		if (!Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod.InnerSingleton)
		{
			UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod.InnerSingleton, Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod_Statics::EnumParams);
		}
		return Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod.InnerSingleton;
	}
	static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_HoudiniInputSettings;
class UScriptStruct* FHoudiniInputSettings::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniInputSettings.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_HoudiniInputSettings.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FHoudiniInputSettings, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("HoudiniInputSettings"));
	}
	return Z_Registration_Info_UScriptStruct_HoudiniInputSettings.OuterSingleton;
}
template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<FHoudiniInputSettings>()
{
	return FHoudiniInputSettings::StaticStruct();
}
	struct Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics
	{
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[];
#endif
		static void* NewStructOps();
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bImportAsReference_MetaData[];
#endif
		static void NewProp_bImportAsReference_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bImportAsReference;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bCheckChanged_MetaData[];
#endif
		static void NewProp_bCheckChanged_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bCheckChanged;
		static const UECodeGen_Private::FStrPropertyParams NewProp_Filters_Inner;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_Filters_MetaData[];
#endif
		static const UECodeGen_Private::FArrayPropertyParams NewProp_Filters;
		static const UECodeGen_Private::FStrPropertyParams NewProp_InvertedFilters_Inner;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_InvertedFilters_MetaData[];
#endif
		static const UECodeGen_Private::FArrayPropertyParams NewProp_InvertedFilters;
		static const UECodeGen_Private::FStrPropertyParams NewProp_AllowClassNames_Inner;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_AllowClassNames_MetaData[];
#endif
		static const UECodeGen_Private::FArrayPropertyParams NewProp_AllowClassNames;
		static const UECodeGen_Private::FStrPropertyParams NewProp_DisallowClassNames_Inner;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_DisallowClassNames_MetaData[];
#endif
		static const UECodeGen_Private::FArrayPropertyParams NewProp_DisallowClassNames;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bImportRenderData_MetaData[];
#endif
		static void NewProp_bImportRenderData_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bImportRenderData;
		static const UECodeGen_Private::FBytePropertyParams NewProp_LODImportMethod_Underlying;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_LODImportMethod_MetaData[];
#endif
		static const UECodeGen_Private::FEnumPropertyParams NewProp_LODImportMethod;
		static const UECodeGen_Private::FBytePropertyParams NewProp_CollisionImportMethod_Underlying;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_CollisionImportMethod_MetaData[];
#endif
		static const UECodeGen_Private::FEnumPropertyParams NewProp_CollisionImportMethod;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bDefaultCurveClosed_MetaData[];
#endif
		static void NewProp_bDefaultCurveClosed_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bDefaultCurveClosed;
		static const UECodeGen_Private::FInt8PropertyParams NewProp_DefaultCurveType_Underlying;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_DefaultCurveType_MetaData[];
#endif
		static const UECodeGen_Private::FEnumPropertyParams NewProp_DefaultCurveType;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_DefaultCurveColor_MetaData[];
#endif
		static const UECodeGen_Private::FStructPropertyParams NewProp_DefaultCurveColor;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bImportCollisionInfo_MetaData[];
#endif
		static void NewProp_bImportCollisionInfo_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bImportCollisionInfo;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bImportRotAndScale_MetaData[];
#endif
		static void NewProp_bImportRotAndScale_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bImportRotAndScale;
		static const UECodeGen_Private::FBytePropertyParams NewProp_ActorFilterMethod_Underlying;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_ActorFilterMethod_MetaData[];
#endif
		static const UECodeGen_Private::FEnumPropertyParams NewProp_ActorFilterMethod;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_NumHolders_MetaData[];
#endif
		static const UECodeGen_Private::FIntPropertyParams NewProp_NumHolders;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bImportLandscapeSplines_MetaData[];
#endif
		static void NewProp_bImportLandscapeSplines_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bImportLandscapeSplines;
		static const UECodeGen_Private::FStrPropertyParams NewProp_LandscapeLayerFilterMap_ValueProp;
		static const UECodeGen_Private::FNamePropertyParams NewProp_LandscapeLayerFilterMap_Key_KeyProp;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_LandscapeLayerFilterMap_MetaData[];
#endif
		static const UECodeGen_Private::FMapPropertyParams NewProp_LandscapeLayerFilterMap;
		static const UECodeGen_Private::FBytePropertyParams NewProp_MaskType_Underlying;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_MaskType_MetaData[];
#endif
		static const UECodeGen_Private::FEnumPropertyParams NewProp_MaskType;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const UECodeGen_Private::FStructParams ReturnStructParams;
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::Struct_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// All of the settings could set by parm tags\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "All of the settings could set by parm tags" },
#endif
	};
#endif
	void* Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHoudiniInputSettings>();
	}
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Parm Tags that could parse them in custom input holder or builder\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Parm Tags that could parse them in custom input holder or builder" },
#endif
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bImportAsReference = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference = { "bImportAsReference", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// for Content, World\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "for Content, World" },
#endif
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bCheckChanged = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged = { "bCheckChanged", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged_MetaData) };
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters_Inner = { "Filters", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// for Content, World\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "for Content, World" },
#endif
	};
#endif
	const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters = { "Filters", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, Filters), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters_MetaData) };
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters_Inner = { "InvertedFilters", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// for Content, Node, and World\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "for Content, Node, and World" },
#endif
	};
#endif
	const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters = { "InvertedFilters", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, InvertedFilters), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters_MetaData) };
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames_Inner = { "AllowClassNames", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// for Content, Node, and World\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "for Content, Node, and World" },
#endif
	};
#endif
	const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames = { "AllowClassNames", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, AllowClassNames), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames_MetaData) };
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames_Inner = { "DisallowClassNames", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// for Content, World\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "for Content, World" },
#endif
	};
#endif
	const UECodeGen_Private::FArrayPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames = { "DisallowClassNames", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, DisallowClassNames), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// -------- StaticMesh --------\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "-------- StaticMesh --------" },
#endif
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bImportRenderData = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData = { "bImportRenderData", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData_MetaData) };
	const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// If nanite enabled, then RenderData is nanite fallback mesh\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "If nanite enabled, then RenderData is nanite fallback mesh" },
#endif
	};
#endif
	const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod = { "LODImportMethod", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, LODImportMethod), Z_Construct_UEnum_HoudiniEngine_EHoudiniStaticMeshLODImportMethod, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod_MetaData) }; // 3967882074
	const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod = { "CollisionImportMethod", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, CollisionImportMethod), Z_Construct_UEnum_HoudiniEngine_EHoudiniMeshCollisionImportMethod, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod_MetaData) }; // 1472608697
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// -------- Curves --------\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "-------- Curves --------" },
#endif
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bDefaultCurveClosed = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed = { "bDefaultCurveClosed", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed_MetaData) };
	const UECodeGen_Private::FInt8PropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Int8, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType = { "DefaultCurveType", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, DefaultCurveType), Z_Construct_UEnum_HoudiniEngine_EHoudiniCurveType, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType_MetaData) }; // 581144134
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveColor_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FStructPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveColor = { "DefaultCurveColor", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, DefaultCurveColor), Z_Construct_UScriptStruct_FColor, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveColor_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveColor_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bImportCollisionInfo = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo = { "bImportCollisionInfo", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bImportRotAndScale = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale = { "bImportRotAndScale", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale_MetaData) };
	const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// -------- Actors --------\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "-------- Actors --------" },
#endif
	};
#endif
	const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod = { "ActorFilterMethod", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, ActorFilterMethod), Z_Construct_UEnum_HoudiniEngine_EHoudiniActorFilterMethod, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod_MetaData) }; // 1660427854
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_NumHolders_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// -------- Contents --------\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "-------- Contents --------" },
#endif
	};
#endif
	const UECodeGen_Private::FIntPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_NumHolders = { "NumHolders", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, NumHolders), METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_NumHolders_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_NumHolders_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	void Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines_SetBit(void* Obj)
	{
		((FHoudiniInputSettings*)Obj)->bImportLandscapeSplines = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines = { "bImportLandscapeSplines", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniInputSettings), &Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines_MetaData) };
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_ValueProp = { "LandscapeLayerFilterMap", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 1, METADATA_PARAMS(0, nullptr) };
	const UECodeGen_Private::FNamePropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_Key_KeyProp = { "LandscapeLayerFilterMap_Key", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FMapPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap = { "LandscapeLayerFilterMap", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Map, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, LandscapeLayerFilterMap), EMapPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_MetaData) };
	const UECodeGen_Private::FBytePropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// -------- Mask ---------\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "-------- Mask ---------" },
#endif
	};
#endif
	const UECodeGen_Private::FEnumPropertyParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType = { "MaskType", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniInputSettings, MaskType), Z_Construct_UEnum_HoudiniEngine_EHoudiniMaskType, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType_MetaData), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType_MetaData) }; // 4190280730
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportAsReference,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bCheckChanged,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters_Inner,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_Filters,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters_Inner,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_InvertedFilters,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames_Inner,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_AllowClassNames,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames_Inner,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DisallowClassNames,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRenderData,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod_Underlying,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LODImportMethod,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod_Underlying,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_CollisionImportMethod,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bDefaultCurveClosed,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType_Underlying,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveType,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_DefaultCurveColor,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportCollisionInfo,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportRotAndScale,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod_Underlying,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_ActorFilterMethod,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_NumHolders,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_bImportLandscapeSplines,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_ValueProp,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap_Key_KeyProp,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_LandscapeLayerFilterMap,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType_Underlying,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewProp_MaskType,
	};
	const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::ReturnStructParams = {
		(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
		nullptr,
		&NewStructOps,
		"HoudiniInputSettings",
		Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::PropPointers,
		UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::PropPointers),
		sizeof(FHoudiniInputSettings),
		alignof(FHoudiniInputSettings),
		RF_Public|RF_Transient|RF_MarkAsNative,
		EStructFlags(0x00000201),
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::Struct_MetaDataParams)
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::PropPointers) < 2048);
	UScriptStruct* Z_Construct_UScriptStruct_FHoudiniInputSettings()
	{
		if (!Z_Registration_Info_UScriptStruct_HoudiniInputSettings.InnerSingleton)
		{
			UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_HoudiniInputSettings.InnerSingleton, Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::ReturnStructParams);
		}
		return Z_Registration_Info_UScriptStruct_HoudiniInputSettings.InnerSingleton;
	}
	DEFINE_FUNCTION(UHoudiniInput::execImport)
	{
		P_GET_OBJECT(UObject,Z_Param_Object);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->Import(Z_Param_Object);
		P_NATIVE_END;
	}
	void UHoudiniInput::StaticRegisterNativesUHoudiniInput()
	{
		UClass* Class = UHoudiniInput::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "Import", &UHoudiniInput::execImport },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_UHoudiniInput_Import_Statics
	{
		struct HoudiniInput_eventImport_Parms
		{
			UObject* Object;
		};
		static const UECodeGen_Private::FObjectPropertyParams NewProp_Object;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[];
#endif
		static const UECodeGen_Private::FFunctionParams FuncParams;
	};
	const UECodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UHoudiniInput_Import_Statics::NewProp_Object = { "Object", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniInput_eventImport_Parms, Object), Z_Construct_UClass_UObject_NoRegister, METADATA_PARAMS(0, nullptr) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UHoudiniInput_Import_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UHoudiniInput_Import_Statics::NewProp_Object,
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UFunction_UHoudiniInput_Import_Statics::Function_MetaDataParams[] = {
		{ "Category", "HoudiniInput" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Could import Asset, Actor, Landscape or HoudiniNode for corresponding input type.\nIf this object has been imported, then will reimport it" },
#endif
	};
#endif
	const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UHoudiniInput_Import_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UHoudiniInput, nullptr, "Import", nullptr, nullptr, Z_Construct_UFunction_UHoudiniInput_Import_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInput_Import_Statics::PropPointers), sizeof(Z_Construct_UFunction_UHoudiniInput_Import_Statics::HoudiniInput_eventImport_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInput_Import_Statics::Function_MetaDataParams), Z_Construct_UFunction_UHoudiniInput_Import_Statics::Function_MetaDataParams) };
	static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInput_Import_Statics::PropPointers) < 2048);
	static_assert(sizeof(Z_Construct_UFunction_UHoudiniInput_Import_Statics::HoudiniInput_eventImport_Parms) < MAX_uint16);
	UFunction* Z_Construct_UFunction_UHoudiniInput_Import()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UHoudiniInput_Import_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UHoudiniInput);
	UClass* Z_Construct_UClass_UHoudiniInput_NoRegister()
	{
		return UHoudiniInput::StaticClass();
	}
	struct Z_Construct_UClass_UHoudiniInput_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
		static const UECodeGen_Private::FBytePropertyParams NewProp_Type_Underlying;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_Type_MetaData[];
#endif
		static const UECodeGen_Private::FEnumPropertyParams NewProp_Type;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_NodeInputIdx_MetaData[];
#endif
		static const UECodeGen_Private::FIntPropertyParams NewProp_NodeInputIdx;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_Name_MetaData[];
#endif
		static const UECodeGen_Private::FStrPropertyParams NewProp_Name;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_Settings_MetaData[];
#endif
		static const UECodeGen_Private::FStructPropertyParams NewProp_Settings;
		static const UECodeGen_Private::FObjectPtrPropertyParams NewProp_Holders_Inner;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_Holders_MetaData[];
#endif
		static const UECodeGen_Private::FArrayPropertyParams NewProp_Holders;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_bExpandSettings_MetaData[];
#endif
		static void NewProp_bExpandSettings_SetBit(void* Obj);
		static const UECodeGen_Private::FBoolPropertyParams NewProp_bExpandSettings;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UHoudiniInput_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::DependentSingletons) < 16);
	const FClassFunctionLinkInfo Z_Construct_UClass_UHoudiniInput_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_UHoudiniInput_Import, "Import" }, // 3988597629
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::FuncInfo) < 2048);
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Class to record node-inputs and operator-path-inputs, outer must be a AHouudiniNode\n" },
#endif
		{ "IncludePath", "HoudiniInput.h" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Class to record node-inputs and operator-path-inputs, outer must be a AHouudiniNode" },
#endif
	};
#endif
	const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type_MetaData[] = {
		{ "Category", "HoudiniInput" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type = { "Type", nullptr, (EPropertyFlags)0x0020080000000014, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniInput, Type), Z_Construct_UEnum_HoudiniEngine_EHoudiniInputType, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type_MetaData), Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type_MetaData) }; // 1015763725
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::NewProp_NodeInputIdx_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FIntPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_NodeInputIdx = { "NodeInputIdx", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Int, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniInput, NodeInputIdx), METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::NewProp_NodeInputIdx_MetaData), Z_Construct_UClass_UHoudiniInput_Statics::NewProp_NodeInputIdx_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Name_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Name = { "Name", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniInput, Name), METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Name_MetaData), Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Name_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Settings_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Settings = { "Settings", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniInput, Settings), Z_Construct_UScriptStruct_FHoudiniInputSettings, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Settings_MetaData), Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Settings_MetaData) }; // 3561868966
	const UECodeGen_Private::FObjectPtrPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders_Inner = { "Holders", nullptr, (EPropertyFlags)0x0004000000000000, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UClass_UHoudiniInputHolder_NoRegister, METADATA_PARAMS(0, nullptr) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders_MetaData[] = {
		{ "Category", "HoudiniInput" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders = { "Holders", nullptr, (EPropertyFlags)0x0014000000000014, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniInput, Holders), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders_MetaData), Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings_MetaData[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// May be contains nullptr when input type is EHoudiniInputType::Content\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "May be contains nullptr when input type is EHoudiniInputType::Content" },
#endif
	};
#endif
	void Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings_SetBit(void* Obj)
	{
		((UHoudiniInput*)Obj)->bExpandSettings = 1;
	}
	const UECodeGen_Private::FBoolPropertyParams Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings = { "bExpandSettings", nullptr, (EPropertyFlags)0x0010000000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(UHoudiniInput), &Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings_MetaData), Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings_MetaData) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UHoudiniInput_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type_Underlying,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Type,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_NodeInputIdx,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Name,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Settings,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders_Inner,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_Holders,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniInput_Statics::NewProp_bExpandSettings,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UHoudiniInput_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniInput>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_UHoudiniInput_Statics::ClassParams = {
		&UHoudiniInput::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		Z_Construct_UClass_UHoudiniInput_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::PropPointers),
		0,
		0x001000A0u,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::Class_MetaDataParams), Z_Construct_UClass_UHoudiniInput_Statics::Class_MetaDataParams)
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInput_Statics::PropPointers) < 2048);
	UClass* Z_Construct_UClass_UHoudiniInput()
	{
		if (!Z_Registration_Info_UClass_UHoudiniInput.OuterSingleton)
		{
			UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniInput.OuterSingleton, Z_Construct_UClass_UHoudiniInput_Statics::ClassParams);
		}
		return Z_Registration_Info_UClass_UHoudiniInput.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UClass* StaticClass<UHoudiniInput>()
	{
		return UHoudiniInput::StaticClass();
	}
	UHoudiniInput::UHoudiniInput(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UHoudiniInput);
	UHoudiniInput::~UHoudiniInput() {}
	DEFINE_FUNCTION(UHoudiniInputHolder::execRequestReimport)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->RequestReimport();
		P_NATIVE_END;
	}
	DEFINE_FUNCTION(UHoudiniInputHolder::execGetObject)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		*(TSoftObjectPtr<UObject>*)Z_Param__Result=P_THIS->GetObject();
		P_NATIVE_END;
	}
	void UHoudiniInputHolder::StaticRegisterNativesUHoudiniInputHolder()
	{
		UClass* Class = UHoudiniInputHolder::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "GetObject", &UHoudiniInputHolder::execGetObject },
			{ "RequestReimport", &UHoudiniInputHolder::execRequestReimport },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics
	{
		struct HoudiniInputHolder_eventGetObject_Parms
		{
			TSoftObjectPtr<UObject> ReturnValue;
		};
		static const UECodeGen_Private::FSoftObjectPropertyParams NewProp_ReturnValue;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[];
#endif
		static const UECodeGen_Private::FFunctionParams FuncParams;
	};
	const UECodeGen_Private::FSoftObjectPropertyParams Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0014000000000580, UECodeGen_Private::EPropertyGenFlags::SoftObject, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(HoudiniInputHolder_eventGetObject_Parms, ReturnValue), Z_Construct_UClass_UObject_NoRegister, METADATA_PARAMS(0, nullptr) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::NewProp_ReturnValue,
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::Function_MetaDataParams[] = {
		{ "Category", "HoudiniInputHolder" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UHoudiniInputHolder, nullptr, "GetObject", nullptr, nullptr, Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::PropPointers), sizeof(Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::HoudiniInputHolder_eventGetObject_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x54020400, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::Function_MetaDataParams), Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::Function_MetaDataParams) };
	static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::PropPointers) < 2048);
	static_assert(sizeof(Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::HoudiniInputHolder_eventGetObject_Parms) < MAX_uint16);
	UFunction* Z_Construct_UFunction_UHoudiniInputHolder_GetObject()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UHoudiniInputHolder_GetObject_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	struct Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport_Statics
	{
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[];
#endif
		static const UECodeGen_Private::FFunctionParams FuncParams;
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport_Statics::Function_MetaDataParams[] = {
		{ "Category", "HoudiniInputHolder" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
	};
#endif
	const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_UHoudiniInputHolder, nullptr, "RequestReimport", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020400, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport_Statics::Function_MetaDataParams), Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport_Statics::Function_MetaDataParams) };
	UFunction* Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UHoudiniInputHolder);
	UClass* Z_Construct_UClass_UHoudiniInputHolder_NoRegister()
	{
		return UHoudiniInputHolder::StaticClass();
	}
	struct Z_Construct_UClass_UHoudiniInputHolder_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UHoudiniInputHolder_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInputHolder_Statics::DependentSingletons) < 16);
	const FClassFunctionLinkInfo Z_Construct_UClass_UHoudiniInputHolder_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_UHoudiniInputHolder_GetObject, "GetObject" }, // 2714596590
		{ &Z_Construct_UFunction_UHoudiniInputHolder_RequestReimport, "RequestReimport" }, // 1064773108
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInputHolder_Statics::FuncInfo) < 2048);
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UHoudiniInputHolder_Statics::Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Base class to hold input objects, outer must be a UHouudiniInput\n" },
#endif
		{ "IncludePath", "HoudiniInput.h" },
		{ "ModuleRelativePath", "Public/HoudiniInput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Base class to hold input objects, outer must be a UHouudiniInput" },
#endif
	};
#endif
	const FCppClassTypeInfoStatic Z_Construct_UClass_UHoudiniInputHolder_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniInputHolder>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_UHoudiniInputHolder_Statics::ClassParams = {
		&UHoudiniInputHolder::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		nullptr,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		0,
		0,
		0x001000A1u,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniInputHolder_Statics::Class_MetaDataParams), Z_Construct_UClass_UHoudiniInputHolder_Statics::Class_MetaDataParams)
	};
	UClass* Z_Construct_UClass_UHoudiniInputHolder()
	{
		if (!Z_Registration_Info_UClass_UHoudiniInputHolder.OuterSingleton)
		{
			UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniInputHolder.OuterSingleton, Z_Construct_UClass_UHoudiniInputHolder_Statics::ClassParams);
		}
		return Z_Registration_Info_UClass_UHoudiniInputHolder.OuterSingleton;
	}
	template<> HOUDINIENGINE_API UClass* StaticClass<UHoudiniInputHolder>()
	{
		return UHoudiniInputHolder::StaticClass();
	}
	UHoudiniInputHolder::UHoudiniInputHolder(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UHoudiniInputHolder);
	UHoudiniInputHolder::~UHoudiniInputHolder() {}
	struct Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics
	{
		static const FEnumRegisterCompiledInInfo EnumInfo[];
		static const FStructRegisterCompiledInInfo ScriptStructInfo[];
		static const FClassRegisterCompiledInInfo ClassInfo[];
	};
	const FEnumRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::EnumInfo[] = {
		{ EHoudiniInputType_StaticEnum, TEXT("EHoudiniInputType"), &Z_Registration_Info_UEnum_EHoudiniInputType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1015763725U) },
		{ EHoudiniActorFilterMethod_StaticEnum, TEXT("EHoudiniActorFilterMethod"), &Z_Registration_Info_UEnum_EHoudiniActorFilterMethod, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1660427854U) },
		{ EHoudiniMaskType_StaticEnum, TEXT("EHoudiniMaskType"), &Z_Registration_Info_UEnum_EHoudiniMaskType, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 4190280730U) },
		{ EHoudiniStaticMeshLODImportMethod_StaticEnum, TEXT("EHoudiniStaticMeshLODImportMethod"), &Z_Registration_Info_UEnum_EHoudiniStaticMeshLODImportMethod, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 3967882074U) },
		{ EHoudiniMeshCollisionImportMethod_StaticEnum, TEXT("EHoudiniMeshCollisionImportMethod"), &Z_Registration_Info_UEnum_EHoudiniMeshCollisionImportMethod, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 1472608697U) },
	};
	const FStructRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::ScriptStructInfo[] = {
		{ FHoudiniInputSettings::StaticStruct, Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics::NewStructOps, TEXT("HoudiniInputSettings"), &Z_Registration_Info_UScriptStruct_HoudiniInputSettings, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FHoudiniInputSettings), 3561868966U) },
	};
	const FClassRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniInput, UHoudiniInput::StaticClass, TEXT("UHoudiniInput"), &Z_Registration_Info_UClass_UHoudiniInput, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniInput), 650862678U) },
		{ Z_Construct_UClass_UHoudiniInputHolder, UHoudiniInputHolder::StaticClass, TEXT("UHoudiniInputHolder"), &Z_Registration_Info_UClass_UHoudiniInputHolder, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniInputHolder), 2052388398U) },
	};
	static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_647017703(TEXT("/Script/HoudiniEngine"),
		Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::ClassInfo),
		Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::ScriptStructInfo),
		Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_3Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_Statics::EnumInfo));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
