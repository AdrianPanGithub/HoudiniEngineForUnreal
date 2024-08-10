// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "HoudiniEngineCommon.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef HOUDINIENGINE_HoudiniEngineCommon_generated_h
#error "HoudiniEngineCommon.generated.h already included, missing '#pragma once' in HoudiniEngineCommon.h"
#endif
#define HOUDINIENGINE_HoudiniEngineCommon_generated_h

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_24_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FHoudiniGenericParameter_Statics; \
	static class UScriptStruct* StaticStruct(); \
	typedef FTableRowBase Super;


template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<struct FHoudiniGenericParameter>();

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_61_GENERATED_BODY \
	friend struct Z_Construct_UScriptStruct_FHoudiniActorHolder_Statics; \
	static class UScriptStruct* StaticStruct();


template<> HOUDINIENGINE_API UScriptStruct* StaticStruct<struct FHoudiniActorHolder>();

#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UHoudiniPresetHandler(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	UHoudiniPresetHandler(UHoudiniPresetHandler&&); \
	UHoudiniPresetHandler(const UHoudiniPresetHandler&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UHoudiniPresetHandler); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UHoudiniPresetHandler); \
	DEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UHoudiniPresetHandler) \
	NO_API virtual ~UHoudiniPresetHandler();


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_GENERATED_UINTERFACE_BODY() \
private: \
	static void StaticRegisterNativesUHoudiniPresetHandler(); \
	friend struct Z_Construct_UClass_UHoudiniPresetHandler_Statics; \
public: \
	DECLARE_CLASS(UHoudiniPresetHandler, UInterface, COMPILED_IN_FLAGS(CLASS_Abstract | CLASS_Interface), CASTCLASS_None, TEXT("/Script/HoudiniEngine"), NO_API) \
	DECLARE_SERIALIZER(UHoudiniPresetHandler)


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_GENERATED_BODY \
	PRAGMA_DISABLE_DEPRECATION_WARNINGS \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_GENERATED_UINTERFACE_BODY() \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_ENHANCED_CONSTRUCTORS \
private: \
	PRAGMA_ENABLE_DEPRECATION_WARNINGS


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_INCLASS_IINTERFACE_NO_PURE_DECLS \
protected: \
	virtual ~IHoudiniPresetHandler() {} \
public: \
	typedef UHoudiniPresetHandler UClassType; \
	typedef IHoudiniPresetHandler ThisClass; \
	virtual UObject* _getUObject() const { return nullptr; }


#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_136_PROLOG
#define FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_144_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h_139_INCLASS_IINTERFACE_NO_PURE_DECLS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> HOUDINIENGINE_API UClass* StaticClass<class UHoudiniPresetHandler>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_UE5_4Test_Plugins_HoudiniEngine_Source_HoudiniEngine_Public_HoudiniEngineCommon_h


#define FOREACH_ENUM_EHOUDINIGENERICPARAMETERTYPE(op) \
	op(EHoudiniGenericParameterType::Int) \
	op(EHoudiniGenericParameterType::Float) \
	op(EHoudiniGenericParameterType::String) \
	op(EHoudiniGenericParameterType::Object) \
	op(EHoudiniGenericParameterType::MultiParm) 

enum class EHoudiniGenericParameterType : uint8;
template<> struct TIsUEnumClass<EHoudiniGenericParameterType> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniGenericParameterType>();

#define FOREACH_ENUM_EHOUDINIEDITOPTIONS(op) \
	op(EHoudiniEditOptions::None) \
	op(EHoudiniEditOptions::Hide) \
	op(EHoudiniEditOptions::Show) \
	op(EHoudiniEditOptions::Disabled) \
	op(EHoudiniEditOptions::Enabled) \
	op(EHoudiniEditOptions::CookOnSelect) 

enum class EHoudiniEditOptions : uint32;
template<> struct TIsUEnumClass<EHoudiniEditOptions> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniEditOptions>();

#define FOREACH_ENUM_EHOUDINICURVETYPE(op) \
	op(EHoudiniCurveType::Points) \
	op(EHoudiniCurveType::Polygon) \
	op(EHoudiniCurveType::Subdiv) \
	op(EHoudiniCurveType::Bezier) \
	op(EHoudiniCurveType::Interpolate) 

enum class EHoudiniCurveType;
template<> struct TIsUEnumClass<EHoudiniCurveType> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniCurveType>();

#define FOREACH_ENUM_EHOUDINISTORAGETYPE(op) \
	op(EHoudiniStorageType::Invalid) \
	op(EHoudiniStorageType::Int) \
	op(EHoudiniStorageType::Float) \
	op(EHoudiniStorageType::String) 

enum class EHoudiniStorageType : int32;
template<> struct TIsUEnumClass<EHoudiniStorageType> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniStorageType>();

#define FOREACH_ENUM_EHOUDINIATTRIBUTEOWNER(op) \
	op(EHoudiniAttributeOwner::Invalid) \
	op(EHoudiniAttributeOwner::Vertex) \
	op(EHoudiniAttributeOwner::Point) \
	op(EHoudiniAttributeOwner::Prim) \
	op(EHoudiniAttributeOwner::Detail) \
	op(EHoudiniAttributeOwner::Max) 

enum class EHoudiniAttributeOwner;
template<> struct TIsUEnumClass<EHoudiniAttributeOwner> { enum { Value = true }; };
template<> HOUDINIENGINE_API UEnum* StaticEnum<EHoudiniAttributeOwner>();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
