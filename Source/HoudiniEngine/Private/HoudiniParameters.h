// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniParameter.h"
#include "HoudiniParameters.generated.h"


enum class EHoudiniEditOptions : uint32;


// None types
UCLASS()
class HOUDINIENGINE_API UHoudiniParameterSeparator : public UHoudiniParameter
{
	GENERATED_BODY()
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterFolder : public UHoudiniParameter
{
	GENERATED_BODY()
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterButton : public UHoudiniParameter
{
	GENERATED_BODY()

public:
	virtual FString GetValueString() const override { return FString(Modification == EHoudiniParameterModification::SetValue ? "0" : "1"); }

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override { return false; }  // Mark as need HapiUpload

	virtual bool HapiUpload() override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	FORCEINLINE void Press() { SetModification(EHoudiniParameterModification::SetValue); }
};

// Int types
UCLASS()
class HOUDINIENGINE_API UHoudiniParameterInt : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<int32> Values;

	UPROPERTY()
	TArray<int32> DefaultValues;

	UPROPERTY()
	int32 Min = TNumericLimits<int32>::Lowest();

	UPROPERTY()
	int32 Max = TNumericLimits<int32>::Max();

	UPROPERTY()
	int32 UIMin = TNumericLimits<int32>::Lowest();

	UPROPERTY()
	int32 UIMax = TNumericLimits<int32>::Max();

	UPROPERTY()
	FString Unit;

public:
	virtual bool IsInDefault() const override { return Values == DefaultValues; };

	virtual void RevertToDefault() override { Values = DefaultValues; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override;

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool CopySettingsFrom(const UHoudiniParameter* SrcParm) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	void UpdateValues(const TArray<int32>& AllValues)
	{
		for (int32 Idx = 0; Idx < Values.Num(); ++Idx)
			Values[Idx] = AllValues[ValueIndex + Idx];
	}

	void UpdateDefaultValues(const TArray<int32>& AllValues)
	{
		for (int32 Idx = 0; Idx < DefaultValues.Num(); ++Idx)
			DefaultValues[Idx] = AllValues[ValueIndex + Idx];
	}

	FORCEINLINE const TArray<int32>& GetDefaultValues() const { return DefaultValues; }

	FORCEINLINE const int32& GetValue(const int32& Idx) const { return Values[Idx]; }

	void SetValue(const int32& Idx, const int32& InNewValue)
	{
		if (Values[Idx] == InNewValue)
			return;

		Values[Idx] = InNewValue;
		SetModification(EHoudiniParameterModification::SetValue);
	}

	// Like float-parm
	FORCEINLINE const int32& GetMin() const { return Min; }

	FORCEINLINE const int32& GetMax() const { return Max; }

	FORCEINLINE const int32& GetUIMin() const { return UIMin; }

	FORCEINLINE const int32& GetUIMax() const { return UIMax; }

	FORCEINLINE const FString& GetUnit() const { return Unit; }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterIntChoice : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	int32 Value = -1;

	UPROPERTY()
	int32 DefaultValue = -1;

	UPROPERTY()
	TArray<int32> ChoiceValues;

	UPROPERTY()
	TArray<FString> ChoiceLabels;

	TArray<TSharedPtr<FString>> Options;  // Refresh when GetOptionSource()

public:
	virtual bool IsInDefault() const override { return Value == DefaultValue; }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override { return FString::FromInt(Value); }

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	FORCEINLINE void UpdateValue(const TArray<int32>& AllValues) { Value = AllValues[ValueIndex]; }

	FORCEINLINE void UpdateDefaultValue(const TArray<int32>& AllValues) { DefaultValue = AllValues[ValueIndex]; }

	void UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> ChoiceValues, TConstArrayView<FString> ChoiceLabels);

	void UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> ChoiceLabels);

	const TArray<TSharedPtr<FString>>* GetOptionsSource();

	TSharedPtr<FString> GetChosen() const;  // Must call after GetOptionSource()

	FORCEINLINE const int32& GetValue() const { return Value; }

	FORCEINLINE const int32& GetDefaultValue() const { return DefaultValue; }

	void SetChosen(const TSharedPtr<FString>& Chosen);  // Must call after GetOptionSource()

	void SetValue(const int32& NewValue) { if (Value != NewValue) { Value = NewValue; SetModification(EHoudiniParameterModification::SetValue); } }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterButtonStrip : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	int32 Value = 0;

	UPROPERTY()
	int32 DefaultValue = 0;

	UPROPERTY()
	TArray<FString> ChoiceLabels;

	UPROPERTY()
	bool bCanMultiSelect = true;

public:
	virtual bool IsInDefault() const override { return Value == DefaultValue; }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override { return FString::FromInt(Value); }

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	FORCEINLINE void UpdateValue(const TArray<int32>& AllValues) { Value = AllValues[ValueIndex]; }

	FORCEINLINE void UpdateDefaultValue(const TArray<int32>& AllValues) { DefaultValue = AllValues[ValueIndex]; }

	FORCEINLINE void UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> AllChoiceLabels)
	{
		ChoiceLabels.SetNum(ChoiceCount);
		for (int32 LocalChoiceIndex = 0; LocalChoiceIndex < ChoiceCount; ++LocalChoiceIndex)
			ChoiceLabels[LocalChoiceIndex] = AllChoiceLabels[ChoiceIndex + LocalChoiceIndex];
	}

	FORCEINLINE const int32& GetValue() const { return Value; }

	FORCEINLINE const int32& GetDefaultValue() const { return DefaultValue; }

	FORCEINLINE int32 NumButtons() const { return ChoiceLabels.Num(); }

	FORCEINLINE const FString& GetButtonLabel(const int32& ButtonIdx) const { return ChoiceLabels[ButtonIdx]; }

	FORCEINLINE bool IsPressed(const int32& ButtonIdx) const { return (bCanMultiSelect ? bool(Value & (1 << ButtonIdx)) : (Value == ButtonIdx)); }

	FORCEINLINE void Press(const int32& ButtonIdx) { if (bCanMultiSelect) { Value ^= (1 << ButtonIdx); } else { Value = ButtonIdx; } SetModification(EHoudiniParameterModification::SetValue); }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterToggle : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	bool Value = false;

	UPROPERTY()
	bool DefaultValue = false;

public:
	virtual bool IsInDefault() const override { return Value == DefaultValue; }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override { return Value ? TEXT("1") : TEXT("0"); }

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	FORCEINLINE void UpdateValue(const TArray<int32>& AllValues) { Value = bool(AllValues[ValueIndex]); }

	FORCEINLINE void UpdateDefaultValue(const TArray<int32>& AllValues) { DefaultValue = bool(AllValues[ValueIndex]); }

	FORCEINLINE const bool& GetDefaultValue() const { return DefaultValue; }

	FORCEINLINE const bool& GetValue() const { return Value; }

	void SetValue(const bool& InNewValue)
	{
		if (Value == InNewValue)
			return;

		Value = InNewValue;
		SetModification(EHoudiniParameterModification::SetValue);
	}
};

UENUM()
enum class EHoudiniFolderType
{
	Invalid = -1,
	Radio,  // HAPI_PRM_SCRIPT_TYPE_GROUPRADIO
	Toggle,  // HAPI_PRM_SCRIPT_TYPE_GROUPCOLLAPSIBLE, with "as_toggle" parm tag
	Collapsible,  // HAPI_PRM_SCRIPT_TYPE_GROUPCOLLAPSIBLE
	Simple,  // HAPI_PRM_SCRIPT_TYPE_GROUPSIMPLE
	Tabs,  // HAPI_PRM_SCRIPT_TYPE_GROUP
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterFolderList : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	int32 Value = 0;

	UPROPERTY()
	EHoudiniFolderType FolderType = EHoudiniFolderType::Invalid;

public:
	virtual FString GetValueString() const override { return HasValue() ? FString::FromInt(Value) : FString(); }

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	static bool HapiGetEditOptions(const int32& NodeId, const HAPI_ParmInfo& ParmInfo, EHoudiniEditOptions& OutEditOptions, FString& OutIdentifierName);

	FORCEINLINE void UpdateValue(const TArray<int32>& AllValues) { Value = AllValues[ValueIndex]; }

	FORCEINLINE const int32& GetFolderStateValue() const { return Value; }

	void SetFolderStateValue(const int32& InStateValue)
	{
		if (Value == InStateValue)
			return;

		Value = InStateValue;
		if (HasValue())
			SetModification(EHoudiniParameterModification::SetValue);
	}

	FORCEINLINE const EHoudiniFolderType& GetFolderType() const { return FolderType; }

	void SetFolderType(const HAPI_ParmInfo& FolderParmInfo);

	bool IsFolderExpanded(const int32& FolderIdx) const
	{
		if (FolderType == EHoudiniFolderType::Simple)
			return true;

		if ((FolderType == EHoudiniFolderType::Collapsible || FolderType == EHoudiniFolderType::Toggle) && Value)
			return true;

		if (IsTab() && Value == FolderIdx)
			return true;

		return false;
	}

	FORCEINLINE bool HasValue() const { return (FolderType == EHoudiniFolderType::Radio || FolderType == EHoudiniFolderType::Toggle); }

	FORCEINLINE bool IsTab() const { return (FolderType == EHoudiniFolderType::Radio || FolderType == EHoudiniFolderType::Tabs); }
};

// Float types
UCLASS()
class HOUDINIENGINE_API UHoudiniParameterFloat : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<float> Values;

	UPROPERTY()
	TArray<float> DefaultValues;

	UPROPERTY()
	float Min = TNumericLimits<float>::Lowest();

	UPROPERTY()
	float Max = TNumericLimits<float>::Max();

	UPROPERTY()
	float UIMin = TNumericLimits<float>::Lowest();

	UPROPERTY()
	float UIMax = TNumericLimits<float>::Max();

	UPROPERTY()
	FString Unit;

public:
	virtual bool IsInDefault() const override { return Values == DefaultValues; }

	virtual void RevertToDefault() override { Values = DefaultValues; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override;

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool CopySettingsFrom(const UHoudiniParameter* SrcParm) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	void UpdateValues(const TArray<float>& AllValues)
	{
		for (int32 Idx = 0; Idx < Values.Num(); ++Idx)
			Values[Idx] = AllValues[ValueIndex + Idx];
	};

	void UploadValues(TArray<float>& AllValues) const
	{
		for (int32 Idx = 0; Idx < DefaultValues.Num(); ++Idx)
			AllValues[ValueIndex + Idx] = Values[Idx];
	};

	void UpdateDefaultValues(const TArray<float>& AllValues)
	{
		for (int32 Idx = 0; Idx < DefaultValues.Num(); ++Idx)
			DefaultValues[Idx] = AllValues[ValueIndex + Idx];
	}

	FORCEINLINE const TArray<float>& GetDefaultValues() const { return DefaultValues; }

	FORCEINLINE const float& GetValue(const int32& Idx) const { return Values[Idx]; }

	void SetValue(const int32& Idx, const float& InNewValue)
	{
		if (Values[Idx] == InNewValue)
			return;

		Values[Idx] = InNewValue;
		SetModification(EHoudiniParameterModification::SetValue);
	}

	// Like int-parm
	FORCEINLINE const float& GetMin() const { return Min; }

	FORCEINLINE const float& GetMax() const { return Max; }

	FORCEINLINE const float& GetUIMin() const { return UIMin; }

	FORCEINLINE const float& GetUIMax() const { return UIMax; }

	FORCEINLINE const FString& GetUnit() const { return Unit; }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterColor : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FLinearColor Value = FLinearColor::White;

	UPROPERTY()
	FLinearColor DefaultValue = FLinearColor::White;

public:
	virtual bool IsInDefault() const override { return Value == DefaultValue; }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override
	{
		return (Size <= 3) ? FString::Printf(TEXT("%f,%f,%f"), Value.R, Value.G, Value.B) :
			FString::Printf(TEXT("%f,%f,%f,%f"), Value.R, Value.G, Value.B, Value.A);
	}

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	void UpdateValues(const TArray<float>& AllValues)
	{
		Value.R = AllValues[ValueIndex]; Value.G = AllValues[ValueIndex + 1]; Value.B = AllValues[ValueIndex + 2];
		Value.A = (Size == 4) ? AllValues[ValueIndex + 3] : 1.f;
	};

	void UploadValues(TArray<float>& AllValues) const
	{
		AllValues[ValueIndex] = Value.R; AllValues[ValueIndex + 1] = Value.G; AllValues[ValueIndex + 2] = Value.B;
		if (Size == 4) AllValues[ValueIndex + 3] = Value.A;
	};

	void UpdateDefaultValues(const TArray<float>& AllValues)
	{
		DefaultValue.R = AllValues[ValueIndex]; DefaultValue.G = AllValues[ValueIndex + 1]; DefaultValue.B = AllValues[ValueIndex + 2];
		DefaultValue.A = (Size == 4) ? AllValues[ValueIndex + 3] : 1.f;
	};

	FORCEINLINE const FLinearColor& GetDefaultValue() const { return DefaultValue; }

	FORCEINLINE const FLinearColor& GetValue() const { return Value; }

	void SetValue(const FLinearColor& InNewValue)
	{
		if (Value == InNewValue)
			return;

		Value = InNewValue;
		SetModification(EHoudiniParameterModification::SetValue);
	}
};

// String types
UENUM()
enum class EHoudiniPathParameterType
{
	Invalid, // HAPI_PARMTYPE_STRING
	File,  // HAPI_PARMTYPE_PATH_FILE
	Geo,  // HAPI_PARMTYPE_PATH_FILE_GEO
	Image,  // HAPI_PARMTYPE_PATH_FILE_IMAGE
	Directory  // HAPI_PARMTYPE_PATH_FILE_DIR
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterString : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString Value;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	EHoudiniPathParameterType PathType = EHoudiniPathParameterType::Invalid;

public:
	virtual bool IsInDefault() const override { return Value.Equals(DefaultValue, ESearchCase::CaseSensitive); }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual void ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm) override;

	virtual FString GetValueString() const override { return Value; }

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	void UpdateValue(TConstArrayView<FString> AllStringValues) { Value = AllStringValues[ValueIndex]; }

	void UpdateDefaultValue(TConstArrayView<FString> AllStringValues) { DefaultValue = AllStringValues[ValueIndex]; }

	FORCEINLINE const FString& GetDefaultValue() const { return DefaultValue; }

	FORCEINLINE const FString& GetValue() const { return Value; }

	void SetValue(const FString& NewValue)
	{
		if (Value.Equals(NewValue, ESearchCase::CaseSensitive))
			return;

		Value = NewValue;
		SetModification(EHoudiniParameterModification::SetValue);
	}

	FORCEINLINE const EHoudiniPathParameterType& GetPathType() const { return PathType; }
};

UENUM()
enum class EHoudiniChoiceListType
{
	Normal = 0,
	Replace,
	Toggle
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterStringChoice : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString Value;

	UPROPERTY()
	FString DefaultValue;

	UPROPERTY()
	TArray<FString> ChoiceValues;

	UPROPERTY()
	TArray<FString> ChoiceLabels;

	UPROPERTY()
	EHoudiniChoiceListType ChoiceListType = EHoudiniChoiceListType::Normal;

	TArray<TSharedPtr<FString>> Options;  // Refresh when GetOptionSource()

public:
	virtual bool IsInDefault() const override { return Value.Equals(DefaultValue, ESearchCase::CaseSensitive); }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual void ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm) override;

	virtual FString GetValueString() const override { return Value; }

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	FORCEINLINE void UpdateValue(TConstArrayView<FString> AllStringValues) { Value = AllStringValues[ValueIndex]; }

	FORCEINLINE void UpdateDefaultValue(TConstArrayView<FString> AllStringValues) { DefaultValue = AllStringValues[ValueIndex]; }

	FORCEINLINE const FString& GetDefaultValue() const { return DefaultValue; }

	FORCEINLINE const FString& GetValue() const { return Value; }

	void SetValue(const FString& NewValue)
	{
		if (Value.Equals(NewValue, ESearchCase::CaseSensitive))
			return;

		Value = NewValue;
		SetModification(EHoudiniParameterModification::SetValue);
	}

	FORCEINLINE const EHoudiniChoiceListType& GetChoiceListType() const { return ChoiceListType; }

	void UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> AllChoiceValues, TConstArrayView<FString> AllChoiceLabels);

	const TArray<TSharedPtr<FString>>* GetOptionsSource();

	TSharedPtr<FString> GetChosen() const;  // must call after GetOptionSource()

	void SetChosen(const TSharedPtr<FString>& Chosen);  // must call after GetOptionSource()
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterAsset : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FSoftObjectPath Value;

	UPROPERTY()
	FSoftObjectPath DefaultValue;

	// Asset filter settings
	UPROPERTY()
	TArray<FString> Filters;

	UPROPERTY()
	TArray<FString> InvertedFilters;

	UPROPERTY()
	TArray<FString> AllowClassNames;

	UPROPERTY()
	TArray<FString> DisallowClassNames;

	// Will import both asset ref and asset infos, like bounds, split by ";"
	UPROPERTY()
	bool bImportInfo = false;

public:
	virtual bool IsInDefault() const override { return Value == DefaultValue; }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual void ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm) override;

	virtual FString GetValueString() const override;

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	void UpdateValue(TConstArrayView<FString> AllStringValues);

	void UpdateDefaultValue(TConstArrayView<FString> AllStringValues);

	FORCEINLINE const FSoftObjectPath& GetAssetPath() const { return Value; }

	void SetAssetPath(const FSoftObjectPath& InNewAssetPath)
	{
		if (Value == InNewAssetPath)
			return;

		Value = InNewAssetPath;
		SetModification(EHoudiniParameterModification::SetValue);
	}

	FORCEINLINE bool HasAssetFilters() const { return !Filters.IsEmpty() || !InvertedFilters.IsEmpty(); }

	FORCEINLINE const TArray<FString>& GetAssetFilters() const { return Filters; }

	FORCEINLINE const TArray<FString>& GetAssetInvertedFilters() const { return InvertedFilters; }

	FORCEINLINE const TArray<FString>& GetAllowClassNames() const { return AllowClassNames; }

	FORCEINLINE const TArray<FString>& GetDisallowClassNames() const { return DisallowClassNames; }

	FORCEINLINE void SetShouldImportInfo(const bool& bInImportInfo) { bImportInfo = bInImportInfo; }

	FORCEINLINE const bool& ShouldImportInfo() const { return bImportInfo; }

	FORCEINLINE void Reimport() { SetModification(EHoudiniParameterModification::Reimport); }

	FORCEINLINE bool NeedReimport() const { return Modification == EHoudiniParameterModification::Reimport; }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterAssetChoice : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FSoftObjectPath Value;

	UPROPERTY()
	FSoftObjectPath DefaultValue;

	UPROPERTY()
	TArray<FString> ChoiceAssetPaths;

public:
	virtual bool IsInDefault() const override { return Value == DefaultValue; }

	virtual void RevertToDefault() override { Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual void ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm) override;

	virtual FString GetValueString() const override { return Value.ToString(); }

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Type only methods --------
	void UpdateValue(TConstArrayView<FString> AllStringValues);

	void UpdateDefaultValue(TConstArrayView<FString> AllStringValues);

	void UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> AllChoiceValues)
	{
		ChoiceAssetPaths.SetNum(ChoiceCount);
		for (int32 LocalChoiceIndex = 0; LocalChoiceIndex < ChoiceCount; ++LocalChoiceIndex)
			ChoiceAssetPaths[LocalChoiceIndex] = AllChoiceValues[ChoiceIndex + LocalChoiceIndex];
	}

	FORCEINLINE const TArray<FString>& GetChoiceAssetPaths() const { return ChoiceAssetPaths; }

	FORCEINLINE const FSoftObjectPath& GetAssetPath() const { return Value; }

	void SetAssetPath(const FSoftObjectPath& InNewAssetPath)
	{
		if (Value == InNewAssetPath)
			return;

		Value = InNewAssetPath;
		SetModification(EHoudiniParameterModification::SetValue);
	}
};

UCLASS()
class HOUDINIENGINE_API UHoudiniParameterLabel : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString Value;

public:
	void UpdateValue(TConstArrayView<FString> AllStringValues) { Value = AllStringValues[ValueIndex]; }

	FORCEINLINE const FString& GetValue() const { return Value; }
};

// Ramp types
UCLASS(Abstract)
class HOUDINIENGINE_API UHoudiniParameterRamp : public UHoudiniParameter
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	int32 StartFloatValueIndex = -1;

	UPROPERTY()
	bool bIsDefault = true;  // Judging ramp default is heavy, so should pre-judge default

public:
	virtual bool IsInDefault() const override { return bIsDefault; }

	//virtual FString GetValueString() const;

	virtual bool HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags) override;

	//virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool HapiUpload() override;

	virtual void SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const override;


	static ERichCurveInterpMode ToInterpMode(const int32& InBasis);

	static ERichCurveInterpMode ToInterpMode(const FString& InBasisStr);

	static int32 ToBasisEnum(const ERichCurveInterpMode& InInterpMode);

	static FString ToBasisString(const ERichCurveInterpMode& InInterpMode);

	// -------- Pure virtual methods, for ramps --------
	virtual void CheckDefault() {}

	virtual void UpdateValue(const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) {}

	virtual void UpdateDefaultValue(const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) {}

	virtual void ParseFromString(const FString& RampJsonString) {}  // Only for attribute

	virtual FString GetDefaultValueString() const { return FString(); }  // Only for attribute
};

// See UCurveFloat
UCLASS()
class HOUDINIENGINE_API UHoudiniParameterFloatRamp : public UHoudiniParameterRamp, public FCurveOwnerInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FRichCurve Value;

	UPROPERTY()
	FRichCurve DefaultValue;

	void UpdateKeys(FRichCurve& FloatCurve, const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) const;

	static FString ConvertRichCurve(const FRichCurve& FloatCurve);

	static bool bIsRampEditorDragging;

public:
	static void ParseString(FRichCurve& OutCurve, const FString& RampStr);

	FORCEINLINE bool IsDifferent(const FRichCurve& InCurve) const { return Value != InCurve; }

	FORCEINLINE void SetValue(const FRichCurve& InCurve)
	{
		if (IsDifferent(InCurve))
		{
			Value = InCurve;
			CheckDefault();
			SetModification(EHoudiniParameterModification::SetValue);
		}
	}

	virtual void RevertToDefault() override { bIsDefault = true; Value = DefaultValue; SetModification(EHoudiniParameterModification::RevertToDefault); }

	virtual FString GetValueString() const override { return ConvertRichCurve(Value); }  // for both parms and attribs

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	virtual bool LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault) override;

	// -------- Virtual methods of UHoudiniParameterRamp --------
	virtual void CheckDefault() override { bIsDefault = Value == DefaultValue; }

	virtual void UpdateValue(const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) override
	{
		UpdateKeys(Value, AllIntValues, AllFloatValues);
		CheckDefault();
	}

	virtual void UpdateDefaultValue(const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) override
	{
		UpdateKeys(DefaultValue, AllIntValues, AllFloatValues);
		CheckDefault();
	}

	virtual void ParseFromString(const FString& RampStr) override { ParseString(Value, RampStr); }  // Only for attribute

	virtual FString GetDefaultValueString() const override { return ConvertRichCurve(DefaultValue); }  // Only for attribute

	//  -------- Virtual methods of FCurveOwnerInterface --------
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override { return TArray<FRichCurveEditInfoConst>{ &Value }; }
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
	virtual void GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const { Curves.Add(FRichCurveEditInfoConst(&Value)); }
#endif
	virtual TArray<FRichCurveEditInfo> GetCurves() override { return TArray<FRichCurveEditInfo>{ &Value }; }

	virtual void ModifyOwner() override { Modify(); bIsRampEditorDragging = false; }

	virtual void ModifyOwnerChange() override { bIsRampEditorDragging = true; }

	virtual TArray<const UObject*> GetOwners() const override { return TArray<const UObject*>{ this }; }

	virtual void MakeTransactional() override { SetFlags(GetFlags() | RF_Transactional); }

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;

	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override { return IsValid(this); }
};

// See UCurveLinearColor
UCLASS()
class HOUDINIENGINE_API UHoudiniParameterColorRamp : public UHoudiniParameterRamp, public FCurveOwnerInterface
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FRichCurve Value[4];

	UPROPERTY()
	FRichCurve DefaultValue[4];

	void UpdateKeys(FRichCurve InOutCurves[4], const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) const;

	static FString ConvertRichCurve(const FRichCurve FloatCurves[4]);

public:
	static void ParseString(FRichCurve OutCurves[4], const FString& RampStr);

	FORCEINLINE bool IsDifferent(const FRichCurve InCurves[4]) const
	{
		if (Value[0] != InCurves[0]) return true;
		if (Value[1] != InCurves[1]) return true;
		if (Value[2] != InCurves[2]) return true;
		return false;
	}

	FORCEINLINE void SetValue(const FRichCurve InCurves[4])
	{
		if (IsDifferent(InCurves))
		{
			for (int32 ChannelIdx = 0; ChannelIdx < 3; ++ChannelIdx)
				Value[ChannelIdx] = InCurves[ChannelIdx];
			CheckDefault();
			SetModification(EHoudiniParameterModification::SetValue);
		}
	}

	FORCEINLINE void TriggerCookIfChanged() { if (HasChanged()) SetModification(EHoudiniParameterModification::SetValue); }

	virtual void RevertToDefault() override
	{ 
		bIsDefault = true;
		for (int32 CurveIdx = 0; CurveIdx < 3; ++CurveIdx)
			Value[CurveIdx] = DefaultValue[CurveIdx];
		SetModification(EHoudiniParameterModification::RevertToDefault);
	}

	virtual FString GetValueString() const override { return ConvertRichCurve(Value); }

	virtual bool Upload(FHoudiniParameterPresetHelper& PresetHelper) const override;

	// -------- Virtual methods of UHoudiniParameterRamp_Base --------
	virtual void CheckDefault() override
	{
		bIsDefault = true;
		if (Value[0] != DefaultValue[0]) { bIsDefault = false; return; }
		if (Value[1] != DefaultValue[1]) { bIsDefault = false; return; }
		if (Value[2] != DefaultValue[2]) { bIsDefault = false; return; }
	}

	virtual void UpdateValue(const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) override
	{
		UpdateKeys(Value, AllIntValues, AllFloatValues);
		CheckDefault();
	}

	virtual void UpdateDefaultValue(const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) override
	{
		UpdateKeys(DefaultValue, AllIntValues, AllFloatValues);
		CheckDefault();
	}

	virtual void ParseFromString(const FString& RampStr) override { ParseString(Value, RampStr); }  // Only for attribute

	virtual FString GetDefaultValueString() const override { return ConvertRichCurve(DefaultValue); }  // Only for attribute

	//  -------- Virtual methods of FCurveOwnerInterface --------
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
	virtual void GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const;
#endif
	virtual TArray<FRichCurveEditInfo> GetCurves() override;

	virtual void ModifyOwner() override { Modify(); }

	virtual TArray<const UObject*> GetOwners() const override { return TArray<const UObject*>{ this }; }

	virtual void MakeTransactional() override { SetFlags(GetFlags() | RF_Transactional); }

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;

	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override { return IsValid(this); }


	virtual bool IsLinearColorCurve() const override { return true; }

	virtual FLinearColor GetLinearColorValue(float InTime) const override;

	virtual FLinearColor GetClampedLinearColorValue(float InTime) const override { return GetLinearColorValue(InTime); }
};
