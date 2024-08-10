// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "HoudiniEngine/Public/HoudiniOutput.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeHoudiniOutput() {}

// Begin Cross Module References
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniOutput();
HOUDINIENGINE_API UClass* Z_Construct_UClass_UHoudiniOutput_NoRegister();
HOUDINIENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FHoudiniComponentOutput();
HOUDINIENGINE_API UScriptStruct* Z_Construct_UScriptStruct_FHoudiniSplitableOutput();
UPackage* Z_Construct_UPackage__Script_HoudiniEngine();
// End Cross Module References

// Begin Class UHoudiniOutput
void UHoudiniOutput::StaticRegisterNativesUHoudiniOutput()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UHoudiniOutput);
UClass* Z_Construct_UClass_UHoudiniOutput_NoRegister()
{
	return UHoudiniOutput::StaticClass();
}
struct Z_Construct_UClass_UHoudiniOutput_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "HoudiniOutput.h" },
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_Name_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_Name;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UHoudiniOutput>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_UHoudiniOutput_Statics::NewProp_Name = { "Name", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UHoudiniOutput, Name), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_Name_MetaData), NewProp_Name_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UHoudiniOutput_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UHoudiniOutput_Statics::NewProp_Name,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniOutput_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UHoudiniOutput_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniOutput_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UHoudiniOutput_Statics::ClassParams = {
	&UHoudiniOutput::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	Z_Construct_UClass_UHoudiniOutput_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniOutput_Statics::PropPointers),
	0,
	0x001000A1u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UHoudiniOutput_Statics::Class_MetaDataParams), Z_Construct_UClass_UHoudiniOutput_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UHoudiniOutput()
{
	if (!Z_Registration_Info_UClass_UHoudiniOutput.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UHoudiniOutput.OuterSingleton, Z_Construct_UClass_UHoudiniOutput_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UHoudiniOutput.OuterSingleton;
}
template<> HOUDINIENGINE_API UClass* StaticClass<UHoudiniOutput>()
{
	return UHoudiniOutput::StaticClass();
}
UHoudiniOutput::UHoudiniOutput(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(UHoudiniOutput);
UHoudiniOutput::~UHoudiniOutput() {}
// End Class UHoudiniOutput

// Begin ScriptStruct FHoudiniSplitableOutput
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput;
class UScriptStruct* FHoudiniSplitableOutput::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FHoudiniSplitableOutput, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("HoudiniSplitableOutput"));
	}
	return Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput.OuterSingleton;
}
template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<FHoudiniSplitableOutput>()
{
	return FHoudiniSplitableOutput::StaticStruct();
}
struct Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Basic struct for all SplitableOutput\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Basic struct for all SplitableOutput" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_SplitValue_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_SplitValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHoudiniSplitableOutput>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::NewProp_SplitValue = { "SplitValue", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniSplitableOutput, SplitValue), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_SplitValue_MetaData), NewProp_SplitValue_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::NewProp_SplitValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
	nullptr,
	&NewStructOps,
	"HoudiniSplitableOutput",
	Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::PropPointers),
	sizeof(FHoudiniSplitableOutput),
	alignof(FHoudiniSplitableOutput),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FHoudiniSplitableOutput()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput.InnerSingleton, Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput.InnerSingleton;
}
// End ScriptStruct FHoudiniSplitableOutput

// Begin ScriptStruct FHoudiniComponentOutput
static_assert(std::is_polymorphic<FHoudiniComponentOutput>() == std::is_polymorphic<FHoudiniSplitableOutput>(), "USTRUCT FHoudiniComponentOutput cannot be polymorphic unless super FHoudiniSplitableOutput is polymorphic");
static FStructRegistrationInfo Z_Registration_Info_UScriptStruct_HoudiniComponentOutput;
class UScriptStruct* FHoudiniComponentOutput::StaticStruct()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniComponentOutput.OuterSingleton)
	{
		Z_Registration_Info_UScriptStruct_HoudiniComponentOutput.OuterSingleton = GetStaticStruct(Z_Construct_UScriptStruct_FHoudiniComponentOutput, (UObject*)Z_Construct_UPackage__Script_HoudiniEngine(), TEXT("HoudiniComponentOutput"));
	}
	return Z_Registration_Info_UScriptStruct_HoudiniComponentOutput.OuterSingleton;
}
template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<FHoudiniComponentOutput>()
{
	return FHoudiniComponentOutput::StaticStruct();
}
struct Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Struct_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// Basic struct for all ComponentOutput\n" },
#endif
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Basic struct for all ComponentOutput" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ComponentName_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_bSplitActor_MetaData[] = {
		{ "ModuleRelativePath", "Public/HoudiniOutput.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FNamePropertyParams NewProp_ComponentName;
	static void NewProp_bSplitActor_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_bSplitActor;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static void* NewStructOps()
	{
		return (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<FHoudiniComponentOutput>();
	}
	static const UECodeGen_Private::FStructParams StructParams;
};
const UECodeGen_Private::FNamePropertyParams Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewProp_ComponentName = { "ComponentName", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Name, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(FHoudiniComponentOutput, ComponentName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ComponentName_MetaData), NewProp_ComponentName_MetaData) };
void Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewProp_bSplitActor_SetBit(void* Obj)
{
	((FHoudiniComponentOutput*)Obj)->bSplitActor = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewProp_bSplitActor = { "bSplitActor", nullptr, (EPropertyFlags)0x0020080000000000, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(FHoudiniComponentOutput), &Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewProp_bSplitActor_SetBit, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_bSplitActor_MetaData), NewProp_bSplitActor_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewProp_ComponentName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewProp_bSplitActor,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::PropPointers) < 2048);
const UECodeGen_Private::FStructParams Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::StructParams = {
	(UObject* (*)())Z_Construct_UPackage__Script_HoudiniEngine,
	Z_Construct_UScriptStruct_FHoudiniSplitableOutput,
	&NewStructOps,
	"HoudiniComponentOutput",
	Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::PropPointers,
	UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::PropPointers),
	sizeof(FHoudiniComponentOutput),
	alignof(FHoudiniComponentOutput),
	RF_Public|RF_Transient|RF_MarkAsNative,
	EStructFlags(0x00000201),
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::Struct_MetaDataParams), Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::Struct_MetaDataParams)
};
UScriptStruct* Z_Construct_UScriptStruct_FHoudiniComponentOutput()
{
	if (!Z_Registration_Info_UScriptStruct_HoudiniComponentOutput.InnerSingleton)
	{
		UECodeGen_Private::ConstructUScriptStruct(Z_Registration_Info_UScriptStruct_HoudiniComponentOutput.InnerSingleton, Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::StructParams);
	}
	return Z_Registration_Info_UScriptStruct_HoudiniComponentOutput.InnerSingleton;
}
// End ScriptStruct FHoudiniComponentOutput

// Begin Registration
struct Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_Statics
{
	static constexpr FStructRegisterCompiledInInfo ScriptStructInfo[] = {
		{ FHoudiniSplitableOutput::StaticStruct, Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics::NewStructOps, TEXT("HoudiniSplitableOutput"), &Z_Registration_Info_UScriptStruct_HoudiniSplitableOutput, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FHoudiniSplitableOutput), 602188274U) },
		{ FHoudiniComponentOutput::StaticStruct, Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics::NewStructOps, TEXT("HoudiniComponentOutput"), &Z_Registration_Info_UScriptStruct_HoudiniComponentOutput, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(FHoudiniComponentOutput), 2922129982U) },
	};
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UHoudiniOutput, UHoudiniOutput::StaticClass, TEXT("UHoudiniOutput"), &Z_Registration_Info_UClass_UHoudiniOutput, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UHoudiniOutput), 213378977U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_2338050462(TEXT("/Script/HoudiniEngine"),
	Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_Statics::ClassInfo),
	Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_Statics::ScriptStructInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_Statics::ScriptStructInfo),
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
