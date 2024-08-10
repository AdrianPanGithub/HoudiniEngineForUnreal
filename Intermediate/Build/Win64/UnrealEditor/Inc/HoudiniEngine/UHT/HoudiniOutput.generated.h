// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "HoudiniOutput.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef HOUDINIENGINE_HoudiniOutput_generated_h
#error "HoudiniOutput.generated.h already included, missing '#pragma once' in HoudiniOutput.h"
#endif
#define HOUDINIENGINE_HoudiniOutput_generated_h

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_18_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUHoudiniOutput(); \
	friend struct Z_Construct_UClass_UHoudiniOutput_Statics; \
public: \
	DECLARE_CLASS(UHoudiniOutput, UObject, COMPILED_IN_FLAGS(CLASS_Abstract), CASTCLASS_None, TEXT("/Script/HoudiniEngine"), NO_API) \
	DECLARE_SERIALIZER(UHoudiniOutput)


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_18_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UHoudiniOutput(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UHoudiniOutput(UHoudiniOutput&&); \
	UHoudiniOutput(const UHoudiniOutput&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UHoudiniOutput); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UHoudiniOutput); \
	DEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UHoudiniOutput) \
	NO_API virtual ~UHoudiniOutput();


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_15_PROLOG
#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_18_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_18_INCLASS_NO_PURE_DECLS \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_18_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> HOUDINIENGINE_API UClass* StaticClass<class UHoudiniOutput>();

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_67_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FHoudiniSplitableOutput_Statics; \
	static class UScriptStruct* StaticStruct();


template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<struct FHoudiniSplitableOutput>();

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h_85_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FHoudiniComponentOutput_Statics; \
	static class UScriptStruct* StaticStruct(); \
	typedef FHoudiniSplitableOutput Super;


template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<struct FHoudiniComponentOutput>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniOutput_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
