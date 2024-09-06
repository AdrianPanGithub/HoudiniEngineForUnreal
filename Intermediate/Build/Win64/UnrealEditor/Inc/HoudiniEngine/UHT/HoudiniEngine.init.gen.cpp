// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeHoudiniEngine_init() {}
	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_HoudiniEngine;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_HoudiniEngine()
	{
		if (!Z_Registration_Info_UPackage__Script_HoudiniEngine.OuterSingleton)
		{
			static const UECodeGen_Private::FPackageParams PackageParams = {
				"/Script/HoudiniEngine",
				nullptr,
				0,
				PKG_CompiledIn | 0x00000000,
				0xB0AA1A40,
				0xBCC69D11,
				METADATA_PARAMS(0, nullptr)
			};
			UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_HoudiniEngine.OuterSingleton, PackageParams);
		}
		return Z_Registration_Info_UPackage__Script_HoudiniEngine.OuterSingleton;
	}
	static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_HoudiniEngine(Z_Construct_UPackage__Script_HoudiniEngine, TEXT("/Script/HoudiniEngine"), Z_Registration_Info_UPackage__Script_HoudiniEngine, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0xB0AA1A40, 0xBCC69D11));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
