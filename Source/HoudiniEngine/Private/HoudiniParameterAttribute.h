// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEngineCommon.h"

#include "HoudiniParameterAttribute.generated.h"


class UHoudiniParameter;
class FHoudiniSharedMemoryGeometryInput;


UCLASS()
class HOUDINIENGINE_API UHoudiniParameterAttribute : public UObject
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	EHoudiniAttributeOwner Owner = EHoudiniAttributeOwner::Invalid;

	UPROPERTY()
	EHoudiniStorageType Storage = EHoudiniStorageType::Invalid;

	UPROPERTY()
	int32 TupleSize = 0;

	UPROPERTY()
	TArray<int32> IntValues;

	UPROPERTY()
	TArray<float> FloatValues;

	UPROPERTY()
	TArray<FString> StringValues;

	UPROPERTY()
	TArray<FSoftObjectPath> ObjectValues;  // Should be unique values, indices is record in IntValues

	FORCEINLINE const FSoftObjectPath& GetObjectValue(const int32& ElemIdx) const { return (IntValues[ElemIdx] >= 0) ? ObjectValues[IntValues[ElemIdx]] : DefaultObjectValue; }

	void ShrinkObjectValues(TArray<int32>& RemovedObjectIndices);  // Assume that ObjectIdx >= 0

	void ConvertToObjectStorage();

	FORCEINLINE const FString& GetIndexingStringValue(const int32& ElemIdx) const { return (IntValues[ElemIdx] >= 0) ? StringValues[IntValues[ElemIdx]] : DefaultStringValue; }

	void ShrinkIndexingStringValues(TArray<int32>& RemovedObjectIndices);  // Assume that ObjectIdx >= 0

	void UniqueStringStorage();


	UPROPERTY()
	TObjectPtr<UHoudiniParameter> Parm;  // No need for weak-ptr, if orig parm destroyed, then we still remain it here.

	UPROPERTY()
	TArray<int32> DefaultIntValues;

	UPROPERTY()
	TArray<float> DefaultFloatValues;

	UPROPERTY()
	FString DefaultStringValue;

	UPROPERTY()
	FSoftObjectPath DefaultObjectValue;

public:
	virtual void PostLoad() override;  // Only for compatibility

	static UHoudiniParameterAttribute* Create(UObject* Outer, UHoudiniParameter* Parm, const EHoudiniAttributeOwner& Owner);

	const FString& GetParameterName() const;

	FORCEINLINE UHoudiniParameter* GetParameter() const { return Parm; }

	FORCEINLINE bool IsParameterHolder() const { return Storage == EHoudiniStorageType::Invalid; }

	// -------- Common --------
	virtual void SetNum(const int32& NumElems);
	
	virtual void Update(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner);

	virtual bool IsParameterMatched(const UHoudiniParameter* InParm) const;  // Used when attribute need update from parameter

	virtual bool Correspond(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner);

	// -------- Editing --------
	virtual void DuplicateAppend(const TArray<int32>& DupFromIndices);

	virtual void RemoveIndices(const TArray<int32>& RemoveSortedIndices);  // Make sure that Indices has been sorted!

	virtual UHoudiniParameter* GetParameter(const int32& SrcElemIdx) const;

	virtual FString GetValuesString(TArray<int32> ElemIndices) const;

	virtual void UpdateFromParameter(const TArray<int32>& DstElemIndices);

	virtual void SelectIdenticals(TArray<int32>& InOutSelectedIndices) const;

	// -------- I/O --------
	virtual bool HapiDuplicateRetrieve(UHoudiniParameterAttribute*& OutNewAttrib, const int32& NodeId, const int32& PartId,  // OutNewAttrib may be nullptr if a valid
		const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS]) const;

	virtual void SetDataFrom(const UHoudiniParameterAttribute* SrcAttrib, const TArray<int32>& SrcIndices);  // Will SetNum if failed

	virtual UHoudiniParameterAttribute* DuplicateNew() const;  // Will return nullptr if this ParmAttrib does not have values

	virtual bool Join(const UHoudiniParameterAttribute* OtherAttrib);  // Return false if attrib info does not match, please call SetNum() later

	void UploadInfo_Internal(FHoudiniSharedMemoryGeometryInput& SHMGeoInput, const int32& ArrayCount) const;  // ArrayCount >= 1 means this is an array attribute

	virtual void UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput) const { UploadInfo_Internal(SHMGeoInput, 0); }

	virtual void UploadData(float*& DataPtr) const;

	// -------- Format --------
	virtual TSharedPtr<FJsonObject> ConvertToJson() const;

	virtual bool AppendFromJson(const TSharedPtr<FJsonObject>& JsonAttrib);
};

UCLASS()
class HOUDINIENGINE_API UHoudiniMultiParameterAttribute : public UHoudiniParameterAttribute
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterAttribute>> ChildParmAttribs;

	void ConvertToChildAttributeIndices(const TArray<int32>& InIndices, TArray<int32>& OutChildAttribIndices) const;

	// IntValues will be the counts of individual elements

public:
	// -------- Common --------
	virtual void SetNum(const int32& NumElems) override;

	virtual void Update(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner) override;

	virtual bool IsParameterMatched(const UHoudiniParameter* InParm) const override;  // Used when attribute need update from parameter

	virtual bool Correspond(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner) override;

	// -------- Editing --------
	virtual void DuplicateAppend(const TArray<int32>& DupFromIndices) override;

	virtual void RemoveIndices(const TArray<int32>& RemoveSortedIndices) override;  // Make sure that Indices has been sorted!

	virtual UHoudiniParameter* GetParameter(const int32& SrcElemIdx) const override;

	virtual FString GetValuesString(TArray<int32> ElemIndices) const override;

	virtual void UpdateFromParameter(const TArray<int32>& DstElemIndices) override;

	//virtual void SelectIdenticals(TArray<int32>& InOutSelectedIndices) const override;

	// -------- I/O --------
	virtual bool HapiDuplicateRetrieve(UHoudiniParameterAttribute*& OutNewAttrib, const int32& NodeId, const int32& PartId,  // OutNewAttrib may be nullptr if a valid
		const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS]) const override;

	virtual void SetDataFrom(const UHoudiniParameterAttribute* SrcAttrib, const TArray<int32>& SrcIndices) override;  // Will SetNum if failed

	virtual UHoudiniParameterAttribute* DuplicateNew() const override;  // Will return nullptr if this ParmAttrib does not have values

	virtual bool Join(const UHoudiniParameterAttribute* OtherAttrib) override;  // Return false if attrib info does not match, please call SetNum() later

	virtual void UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput) const override;

	virtual void UploadData(float*& DataPtr) const override;

	// -------- Format --------
	virtual TSharedPtr<FJsonObject> ConvertToJson() const override;

	virtual bool AppendFromJson(const TSharedPtr<FJsonObject>& JsonAttrib) override;

#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
#endif
};
