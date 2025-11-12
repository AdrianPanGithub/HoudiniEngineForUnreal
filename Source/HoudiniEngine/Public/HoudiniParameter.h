// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HoudiniParameter.generated.h"


class AHoudiniNode;
class UHoudiniInput;
struct HAPI_ParmInfo;
struct FHoudiniGenericParameter;
struct FHoudiniParameterPresetHelper;


UENUM()
enum class EHoudiniParameterType
{
	Invalid = -1,

	// None types
	Separator,  // HAPI_PARMTYPE_SEPARATOR
	Folder,  // HAPI_PARMTYPE_FOLDER
	Button,  // HAPI_PARMTYPE_BUTTON

	// Int types
	Int,  // HAPI_PARMTYPE_INT
	IntChoice,  // HAPI_PARMTYPE_INT with choices
	ButtonStrip,
	Toggle,  // HAPI_PARMTYPE_TOGGLE
	FolderList,  // HAPI_PARMTYPE_FOLDERLIST, HAPI_PARMTYPE_FOLDERLIST_RADIO

	// Float types
	Float,  // HAPI_PARMTYPE_FLOAT
	Color,  // HAPI_PARMTYPE_COLOR

	// String types
	String,  // HAPI_PARMTYPE_STRING, HAPI_PARMTYPE_NODE, HAPI_PARMTYPE_PATH_FILE, HAPI_PARMTYPE_PATH_FILE_GEO, HAPI_PARMTYPE_PATH_FILE_IMAGE
	StringChoice,  // HAPI_PARMTYPE_STRING with choices
	Asset,  // HAPI_PARMTYPE_STRING, with "unreal_ref" parm tag
	AssetChoice,  // HAPI_PARMTYPE_STRING, with "unreal_ref" parm tag and choices
	Input,  // HAPI_PARMTYPE_NODE

	Label,  // HAPI_PARMTYPE_LABEL

	// Extras
	MultiParm,  // HAPI_PARMTYPE_MULTIPARMLIST
	FloatRamp,  // HAPI_PARMTYPE_MULTIPARMLIST with rampType == HAPI_RAMPTYPE_FLOAT
	ColorRamp,  // HAPI_PARMTYPE_MULTIPARMLIST with rampType == HAPI_RAMPTYPE_COLOR
};

// Base class to record parms on nodes, outer must be a AHoudiniNode
UCLASS()
class HOUDINIENGINE_API UHoudiniParameter : public UObject
{
	GENERATED_BODY()

protected:
	// -------- From HAPI_ParmInfo --------
	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Label;

	UPROPERTY()
	FString Help;

	UPROPERTY()
	EHoudiniParameterType Type = EHoudiniParameterType::Invalid;

	UPROPERTY()
	int32 ValueIndex = -1;

	UPROPERTY()
	int32 Id = -1;

	UPROPERTY()
	int32 ParentId = -1;

	UPROPERTY()
	int32 Size = -1;

	UPROPERTY()
	int32 MultiParmInstanceIndex = -1;  // Maybe start from 0 or 1, and we just use it to call hapi to insert or remove instance

	UPROPERTY()
	bool bEnabled = true;

	UPROPERTY()
	bool bVisible = true;

	// -------- For UHoudiniParameter only --------
	UPROPERTY()
	FString BackupValueString;  // For ReDeltaInfo

	enum class EHoudiniParameterModification
	{
		// Common
		None = -1,
		SetValue,
		RevertToDefault,

		// For MultiParm
		InsertInst,
		RemoveInst,

		// For Asset with info
		Reimport
	};

	EHoudiniParameterModification Modification = EHoudiniParameterModification::None;

	FORCEINLINE void SetModification(const UHoudiniParameter* LegacyParm) { if (LegacyParm->HasChanged()) SetModification(LegacyParm->Modification); }

	void SetModification(const EHoudiniParameterModification& InModification);  // Will trigger hda update, so we must call this after value set

public:
	static bool GDisableAttributeActions;  // When attribute is brushing, will prevent attrib parm trigger node update, and prevent parm value update from attrib

	static bool HapiCreateOrUpdate(AHoudiniNode* Node,
		const HAPI_ParmInfo& InParmInfo, const FString& ParmName, const FString& ParmLabel, const FString& ParmHelp,
		const TArray<int32>& IntValues, const TArray<float>& FloatValues, TConstArrayView<FString> StringValues,
		TConstArrayView<FString> ChoiceValues, TConstArrayView<FString> ChoiceLabels,
		UHoudiniParameter*& InOutParm, const bool& bBeforeCook, const bool& bIsAttribute);

	static bool HapiGetUnit(const int32& NodeId, const int32& ParmId, FString& Unit);

	static EHoudiniParameterType GetParameterTypeFromInfo(const int32& NodeId, const HAPI_ParmInfo& InParmInfo);


	FORCEINLINE const EHoudiniParameterType& GetType() const { return Type; };

	FORCEINLINE bool IsParent() const { return Type == EHoudiniParameterType::Folder || Type == EHoudiniParameterType::FolderList || Type == EHoudiniParameterType::MultiParm; }


	FORCEINLINE const FString& GetParameterName() const { return Name; }

	FORCEINLINE const FString& GetLabel() const { return Label; }

	FORCEINLINE const FString& GetHelp() const { return Help; }

	FORCEINLINE const int32& GetNodeId() const;

	FORCEINLINE const int32& GetId() const { return Id; }

	FORCEINLINE const int32& GetParentId() const { return ParentId; }

	FORCEINLINE const int32& GetSize() const { return Size; }

	FORCEINLINE const bool& IsEnabled() const { return bEnabled; }

	FORCEINLINE const bool& IsVisible() const { return bVisible; }

	FORCEINLINE const int32& GetMultiParmInstanceIndex() const { return MultiParmInstanceIndex; }

	FORCEINLINE bool HasChanged() const { return Modification != EHoudiniParameterModification::None; }

	FORCEINLINE bool IsPendingRevert() const { return Modification == EHoudiniParameterModification::RevertToDefault; }

	FORCEINLINE void ResetModification() { Modification = EHoudiniParameterModification::None; }
 
	FORCEINLINE const FString& GetBackupValueString() const { return BackupValueString; }

	FORCEINLINE void UpdateBackupValueString() { BackupValueString = GetValueString(); }

	bool UploadGeneric(FHoudiniParameterPresetHelper& PresetHelper,
		const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault);  // Return true: has been upload or no need for load, false: need call HapiUpload later

#if WITH_EDITOR
	virtual void PostEditUndo() override
	{
		Super::PostEditUndo();

		SetModification(EHoudiniParameterModification::SetValue);
	}
#endif

	// -------- virtual methods --------
	virtual bool IsInDefault() const { return true; }

	virtual void RevertToDefault() {}

	virtual void ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm) {}  // Only execute it after node just instantiate

	virtual FString GetValueString() const { return FString(); }

	// The override methods MUST call UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags) firstly
	// And If has multiple value tuples, then need override this method to match size
	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags);  

	virtual bool CopySettingsFrom(const UHoudiniParameter* SrcParm);  // Will NOT copy the property called "Value", if classes are different, then return false

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const { return true; }  // Return true: has been upload, false: need call HapiUpload later

	virtual bool HapiUpload() { return true; }

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const {}  // Return false if this parameter does NOT has value(s)

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) { return false; }  // return whether the value(s) should upload to houdini

	// A parm that has value(s) will have methods like:
	// GetValue
	// GetDefaultValue(s)
	// SetValue (MUST check whether value is actually changed, and MUST call UHoudiniParameter::SetModification() at last)
	// UpdateValue(s)
	// UpdateDefaultValue(s)
};


UCLASS()
class HOUDINIENGINE_API UHoudiniMultiParameter : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	int32 DefaultCount = 0;  // Only for Attrib

	TArray<int32> ModificationIndices;  // Only for MultiParm

	// Only for AttribMultiParm
	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameter>> ChildAttribParms;

public:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHoudiniParameter>> ChildAttribParmInsts;  // Only for AttribMultiParm

	virtual FString GetValueString() const override { return FString::FromInt(Count); }

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override { return false; }  // Mark as need HapiUpload

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;  // Only for AttribMultiParm, parm has been uploaded

	// -------- Type only methods --------
	FORCEINLINE void UpdateInstanceCount(const int32& InstanceCount) { Count = InstanceCount; }

	FORCEINLINE void UpdateDefaultInstanceCount(const int32& InstanceCount) { DefaultCount = InstanceCount; }

	bool HapiSyncAttribute(const bool& bUpdateDefaultCount);

	FORCEINLINE const int32& GetDefaultInstanceCount() const { return DefaultCount; }

	FORCEINLINE const int32& GetInstanceCount() const { return Count; }

	void InsertInstance(const int32& InstIdx);

	void RemoveInstance(const int32& InstIdx);

	void SetInstanceCount(const int32& InNewCount, const bool& bNotifyChanged = true);

	// -------- Only for AttribMultiParm --------
	FORCEINLINE void ResetChildAttributeParameters() { ChildAttribParms.Empty(); }

	FORCEINLINE void AddChildAttributeParameter(UHoudiniParameter* ChildParm) { ChildAttribParms.Add(ChildParm); }

	FORCEINLINE const TArray<UHoudiniParameter*>& GetChildAttributeParameters() const { return ChildAttribParms; }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterInput : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString Value;

	UPROPERTY()
	TWeakObjectPtr<UHoudiniInput> Input;

public:
	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	// -------- Type only methods --------
	static void GetObjectInfo(const UObject* Object, FString& InOutInfoStr);

	FORCEINLINE void UpdateValue(TConstArrayView<FString> AllStringValues) { Value = AllStringValues[ValueIndex]; }

	FORCEINLINE void SetInput(const TWeakObjectPtr<UHoudiniInput>& InInput) { Input = InInput; }

	FORCEINLINE const TWeakObjectPtr<UHoudiniInput>& GetInput() const { return Input; }

	FORCEINLINE const FString& GetValue() const { return Value; }

	void TryLoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms);  // InputParm should load value in HapiUpdateParameter, before HapiUpdateInputs which will parse value into objects to input 

	void ConvertToGeneric(FHoudiniGenericParameter& OutGenericParm) const;  // Call in AHoudiniNode::GetParameterValue, it only has one GenericParm return, so we should convert holders input one single string
};
