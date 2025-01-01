// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeHoudiniEngine_init() {}
	HOUDINIENGINE_API UFunction* Z_Construct_UDelegateFunction_AHoudiniNode_HoudiniNodeEvents__DelegateSignature();
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_HoudiniEngine;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_HoudiniEngine()
	{
		if (!Z_Registration_Info_UPackage__Script_HoudiniEngine.OuterSingleton)
		{
			static UObject* (*const SingletonFuncArray[])() = {
				(UObject* (*)())Z_Construct_UDelegateFunction_AHoudiniNode_HoudiniNodeEvents__DelegateSignature,
			};
			static const UECodeGen_Private::FPackageParams PackageParams = {
				"/Script/HoudiniEngine",
				SingletonFuncArray,
				UE_ARRAY_COUNT(SingletonFuncArray),
				PKG_CompiledIn | 0x00000040,
				0xD7E265C0,
				0xB058F6B5,
				METADATA_PARAMS(0, nullptr)
			};
			UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_HoudiniEngine.OuterSingleton, PackageParams);
		}
		return Z_Registration_Info_UPackage__Script_HoudiniEngine.OuterSingleton;
	}
	static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_HoudiniEngine(Z_Construct_UPackage__Script_HoudiniEngine, TEXT("/Script/HoudiniEngine"), Z_Registration_Info_UPackage__Script_HoudiniEngine, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xD7E265C0, 0xB058F6B5));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
