// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "HoudiniInput.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef HOUDINIENGINE_HoudiniInput_generated_h
#error "HoudiniInput.generated.h already included, missing '#pragma once' in HoudiniInput.h"
#endif
#define HOUDINIENGINE_HoudiniInput_generated_h

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_72_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FHoudiniInputSettings_Statics; \
	static class UScriptStruct* StaticStruct();


template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<struct FHoudiniInputSettings>();

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_159_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUHoudiniInput(); \
	friend struct Z_Construct_UClass_UHoudiniInput_Statics; \
public: \
	DECLARE_CLASS(UHoudiniInput, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/HoudiniEngine"), NO_API) \
	DECLARE_SERIALIZER(UHoudiniInput)


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_159_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UHoudiniInput(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UHoudiniInput(UHoudiniInput&&); \
	UHoudiniInput(const UHoudiniInput&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UHoudiniInput); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UHoudiniInput); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UHoudiniInput) \
	NO_API virtual ~UHoudiniInput();


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_156_PROLOG
#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_159_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_159_INCLASS_NO_PURE_DECLS \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_159_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> HOUDINIENGINE_API UClass* StaticClass<class UHoudiniInput>();

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_307_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUHoudiniInputHolder(); \
	friend struct Z_Construct_UClass_UHoudiniInputHolder_Statics; \
public: \
	DECLARE_CLASS(UHoudiniInputHolder, UObject, COMPILED_IN_FLAGS(CLASS_Abstract), CASTCLASS_None, TEXT("/Script/HoudiniEngine"), NO_API) \
	DECLARE_SERIALIZER(UHoudiniInputHolder)


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_307_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UHoudiniInputHolder(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UHoudiniInputHolder(UHoudiniInputHolder&&); \
	UHoudiniInputHolder(const UHoudiniInputHolder&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UHoudiniInputHolder); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UHoudiniInputHolder); \
	DEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UHoudiniInputHolder) \
	NO_API virtual ~UHoudiniInputHolder();


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_304_PROLOG
#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_307_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_307_INCLASS_NO_PURE_DECLS \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h_307_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> HOUDINIENGINE_API UClass* StaticClass<class UHoudiniInputHolder>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniInput_h


#define FOREACH_ENUM_EHOUDINIINPUTTYPE(op) \
	op(EHoudiniInputType::Content) \
	op(EHoudiniInputType::Curves) \
	op(EHoudiniInputType::World) \
	op(EHoudiniInputType::Node) \
	op(EHoudiniInputType::Mask) 

enum class EHoudiniInputType;
template<> struct TIsUEnumClass<EHoudiniInputType> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniInputType>();

#define FOREACH_ENUM_EHOUDINIACTORFILTERMETHOD(op) \
	op(EHoudiniActorFilterMethod::Selection) \
	op(EHoudiniActorFilterMethod::Class) \
	op(EHoudiniActorFilterMethod::Label) \
	op(EHoudiniActorFilterMethod::Tag) \
	op(EHoudiniActorFilterMethod::Folder) 

enum class EHoudiniActorFilterMethod;
template<> struct TIsUEnumClass<EHoudiniActorFilterMethod> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniActorFilterMethod>();

#define FOREACH_ENUM_EHOUDINIPAINTUPDATEMETHOD(op) \
	op(EHoudiniPaintUpdateMethod::Manual) \
	op(EHoudiniPaintUpdateMethod::EnterPressed) \
	op(EHoudiniPaintUpdateMethod::Brushed) \
	op(EHoudiniPaintUpdateMethod::EveryCook) 

enum class EHoudiniPaintUpdateMethod;
template<> struct TIsUEnumClass<EHoudiniPaintUpdateMethod> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniPaintUpdateMethod>();

#define FOREACH_ENUM_EHOUDINIMASKTYPE(op) \
	op(EHoudiniMaskType::Bit) \
	op(EHoudiniMaskType::Weight) \
	op(EHoudiniMaskType::Byte) 

enum class EHoudiniMaskType;
template<> struct TIsUEnumClass<EHoudiniMaskType> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniMaskType>();

#define FOREACH_ENUM_EHOUDINISTATICMESHLODIMPORTMETHOD(op) \
	op(EHoudiniStaticMeshLODImportMethod::FirstLOD) \
	op(EHoudiniStaticMeshLODImportMethod::LastLOD) \
	op(EHoudiniStaticMeshLODImportMethod::AllLODs) 

enum class EHoudiniStaticMeshLODImportMethod;
template<> struct TIsUEnumClass<EHoudiniStaticMeshLODImportMethod> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniStaticMeshLODImportMethod>();

#define FOREACH_ENUM_EHOUDINIMESHCOLLISIONIMPORTMETHOD(op) \
	op(EHoudiniMeshCollisionImportMethod::NoImportCollision) \
	op(EHoudiniMeshCollisionImportMethod::ImportWithMesh) \
	op(EHoudiniMeshCollisionImportMethod::ImportWithoutMesh) 

enum class EHoudiniMeshCollisionImportMethod;
template<> struct TIsUEnumClass<EHoudiniMeshCollisionImportMethod> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniMeshCollisionImportMethod>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
