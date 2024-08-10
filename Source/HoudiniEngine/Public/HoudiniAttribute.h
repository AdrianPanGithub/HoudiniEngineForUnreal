// Copyright (c) <2024> Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include <string>
#include "CoreMinimal.h"

#include "HAPI/HAPI_Common.h"


class FHoudiniSharedMemoryGeometryInput;


enum class EHoudiniDataType : uint8
{
	Int = 0, Int64, Uint, Uint64,
	Float, Double
};

class HOUDINIENGINE_API FHoudiniAttribute
{
protected:
	FString Name;

	HAPI_AttributeOwner Owner = HAPI_ATTROWNER_INVALID;

	HAPI_StorageType Storage = HAPI_STORAGETYPE_INVALID;

	int32 TupleSize = 0;

	TArray<int32> IntValues;

	TArray<float> FloatValues;

	TArray<std::string> StringValues;

	// For unreal_uproperty_*, in order to cache the found FProperties for faster property assign, <Struct, <PropertyName, <Property, Offset>>
	static TMap<const UStruct*, TMap<FString, TPair<TSharedPtr<FEditPropertyChain>, size_t>>> StructPropertiesMap;

	static bool FindPropertyRecursive(const UStruct* Struct, const FString& PropertyName,
		const TSharedPtr<FEditPropertyChain>& PropertyChain, size_t& OutOffset);

	void SetPropertyValues(void* TargetPtr, const UStruct* TargetStruct,
		const TSharedPtr<FEditPropertyChain>& PropertyChain, void* ContainerPtr, const int32& Index) const;

	FORCEINLINE bool IsRestrictString() const { return Storage == HAPI_STORAGETYPE_STRING || Storage == HAPI_STORAGETYPE_STRING_ARRAY; }

public:
	static const TArray<FName> SupportStructNames;

	static const uint8 SupportStructTupleSize[];

	static const EHoudiniDataType SupportStructElemType[];

	static void ResetPropertiesMaps();  // Call after hot-reload

	static void ClearBlueprintProperties();  // Will be called after output process finished

	static bool HapiRetrieveAttributes(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames,
		const int AttribCounts[HAPI_ATTROWNER_MAX], const char* AttributePrefix, TArray<TSharedPtr<FHoudiniAttribute>>& OutAttribs);

	static bool FindProperty(const UStruct* Struct, const FString& PropertyName,
		TSharedPtr<FEditPropertyChain>& OutFoundPropertyChain, size_t& OutOffset, const bool& bVerbose);

	bool SetStructPropertyValues(void* StructPtr, const UStruct* Struct, const int32& Index, const bool& bVerbose = false) const;

	bool SetObjectPropertyValues(UObject* Object, const int32& Index, const bool& bVerbose = false) const;


	FHoudiniAttribute(const FString& AttribName) : Name(AttribName) {}

	FORCEINLINE const FString& GetAttributeName() const { return Name; }

	FORCEINLINE const HAPI_AttributeOwner& GetOwner() const { return Owner; }

	FORCEINLINE const HAPI_StorageType& GetStorage() const { return Storage; }

	FORCEINLINE const int32& GetTupleSize() const { return TupleSize; }

	// For Inputs
	static void RetrieveAttributes(const TArray<const FProperty*>& Properties, const TArray<const uint8*>& Containers,
		const HAPI_AttributeOwner& Owner, TArray<TSharedPtr<FHoudiniAttribute>>& OutAttribs);

	FORCEINLINE TArray<int32>& InitializeInt(const HAPI_AttributeOwner& InOwner, const int32& InTupleSize)
	{
		Owner = InOwner; Storage = HAPI_STORAGETYPE_INT; TupleSize = InTupleSize; IntValues.Empty();
		return IntValues;
	}

	FORCEINLINE TArray<float>& InitializeFloat(const HAPI_AttributeOwner& InOwner, const int32& InTupleSize)
	{
		Owner = InOwner; Storage = HAPI_STORAGETYPE_FLOAT; TupleSize = InTupleSize; FloatValues.Empty();
		return FloatValues;
	}

	FORCEINLINE TArray<std::string>& InitializeString(const HAPI_AttributeOwner& InOwner)
	{
		Owner = InOwner; Storage = HAPI_STORAGETYPE_STRING; TupleSize = 1; StringValues.Empty();
		return StringValues;
	}

	FORCEINLINE TArray<std::string>& InitializeDictionary(const HAPI_AttributeOwner& InOwner)
	{
		Owner = InOwner; Storage = HAPI_STORAGETYPE_DICTIONARY; TupleSize = 1; StringValues.Empty();
		return StringValues;
	}
	
	virtual bool HapiRetrieveData(const int32& NodeId, const int32& PartId, const char* AttribName,  // AttribName may not be equals to Name, as Name has been removed prefix
		HAPI_AttributeInfo& AttribInfo);  // Could set AttribInfo.tupleSize = DesiredTupleSize

	virtual int32 GetDataCount(const int32& Index) const { return TupleSize; }

	virtual TArray<int32> GetIntData(const int32& Index) const;

	virtual TArray<float> GetFloatData(const int32& Index) const;

	virtual TArray<FString> GetStringData(const int32& Index) const;

	virtual void UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput, const char* AttributePrefix = nullptr) const;

	virtual void UploadData(float*& DataPtr) const;


	virtual ~FHoudiniAttribute() {};  // Useless, just avoid a warning of compilation.
};


class HOUDINIENGINE_API FHoudiniArrayAttribute : public FHoudiniAttribute
{
public:
	FHoudiniArrayAttribute(const FString& AttribName) : FHoudiniAttribute(AttribName) {}

	TArray<int32> Counts;  // Is the accumulated counts and is arrayLen * tupleSize when output, and abs counts when input

	virtual bool HapiRetrieveData(const int32& NodeId, const int32& PartId, const char* AttribName,
		HAPI_AttributeInfo& AttribInfo) override;

	virtual int32 GetDataCount(const int32& Index) const override;

	virtual TArray<int32> GetIntData(const int32& Index) const override;

	virtual TArray<float> GetFloatData(const int32& Index) const override;

	virtual TArray<FString> GetStringData(const int32& Index) const override;

	virtual void UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput, const char* AttributePrefix = nullptr) const override;

	virtual void UploadData(float*& DataPtr) const override;
};
