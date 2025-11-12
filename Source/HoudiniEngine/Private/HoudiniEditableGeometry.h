// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEngineCommon.h"

#include "HoudiniEditableGeometry.generated.h"


class AHoudiniNode;
class UHoudiniParameter;
class FHoudiniAttribute;
class UHoudiniParameterAttribute;


// Owner must be AHoudiniNode or AHoudiniNodeProxy
UCLASS(Abstract)
class HOUDINIENGINE_API UHoudiniEditableGeometry : public UPrimitiveComponent, public IHoudiniPresetHandler
{
	GENERATED_BODY()

public:
	UHoudiniEditableGeometry()
	{
		PrimaryComponentTick.bCanEverTick = false;
	}

protected:
	UPROPERTY()
	FString Name;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterAttribute>> PointParmAttribs;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameterAttribute>> PrimParmAttribs;

	UPROPERTY()
	EHoudiniEditOptions PointEditOptions = EHoudiniEditOptions::None;

	UPROPERTY()
	EHoudiniEditOptions PrimEditOptions = EHoudiniEditOptions::None;

	UPROPERTY()
	uint64 UpdateCycles = 0;  // Can judge which editgeo is the newest one, 0 means not editable

	UPROPERTY()
	FString PresetName;  // Record the last loaded PresetName, and used for save preset

	// -------- Selection --------
	UPROPERTY(Transient, DuplicateTransient)
	EHoudiniAttributeOwner SelectedClass = EHoudiniAttributeOwner::Invalid;

	UPROPERTY(Transient, DuplicateTransient)
	TArray<int32> SelectedIndices;  // Will Display the attrib panel based on Last selected, and may NOT be ordered!

	UPROPERTY(Transient, DuplicateTransient)
	FVector ClickPosition = FVector::ZeroVector;  // World coordinate

	// -------- DeltaInfo --------
	FString DeltaInfo;  // Should be empty after uploaded

	UPROPERTY(Transient, DuplicateTransient)
	FString ReDeltaInfo;  // Use for rebuild or hda reload

	UPROPERTY(Transient, DuplicateTransient)
	FString UnDeltaInfo;  // Use for undo, do NOT specify value before Transcation begin

	void TriggerParentNodeToCook() const;

public:
	AHoudiniNode* GetParentNode() const;

	FORCEINLINE const FString& GetGroupName() const { return Name; }

	FORCEINLINE const EHoudiniEditOptions& GetPointEditOptions() const { return PointEditOptions; }

	FORCEINLINE const EHoudiniEditOptions& GetPrimEditOptions() const { return PrimEditOptions; }

	FORCEINLINE bool HasChanged() const { return !DeltaInfo.IsEmpty(); }

	FORCEINLINE const uint64& GetUpdateCycles() const { return UpdateCycles; }

	TArray<UHoudiniParameter*> GetAttributeParameters(const bool& bSetParmValue = true) const;  // Use the last SelectedIdx to set parm values

	void ModifyAllAttributes();  // Will call each ParmAttribs' Modify()

	bool UpdateAttribute(const UHoudiniParameter* ChangedAttribParm, const EHoudiniAttributeOwner& ChangedClass);  // Return whether ParmAttrib been updated successfully

	// -------- IHoudiniPresetHandler --------
	virtual FString GetPresetPathFilter() const override;

	virtual bool GetGenericParameters(TMap<FName, FHoudiniGenericParameter>& OutParms) const override;

	virtual void SetGenericParameters(const TSharedPtr<const TMap<FName, FHoudiniGenericParameter>>& Parms) override;

	virtual const FString& GetPresetName() const override { return PresetName; }

	virtual void SetPresetName(const FString& NewPresetName) override { PresetName = NewPresetName; }

	// -------- IHoudiniEditableGeometry --------
	virtual void ClearEditData();  // Mark this edit geo NOT editable


	virtual int32 NumVertices() const { return 0; }

	virtual int32 NumPoints() const { return 0; }

	virtual int32 NumPrims() const { return 0; }

#if WITH_EDITOR
	virtual void PreEditUndo() override
	{
		Super::PreEditUndo();
		DeltaInfo = UnDeltaInfo;
	}

	virtual void PostEditUndo() override
	{
		Super::PostEditUndo();
		TriggerParentNodeToCook();
	}
#endif


	void Update(const AHoudiniNode* Node, const FString& InGroupName,  // Will update ParmAttribs, need align size or Retrieve values after
		const TArray<FString>& IgnorePointAttribNames, const TArray<FString>& IgnorePrimAttribNames);

	bool HapiUpdate(const AHoudiniNode* Node, const FString& InGroupName,
		const int32& NodeId, const int32& PartId, const TArray<int32>& PointIndices, const TArray<int32>& PrimIndices,  // PointIndices.Num() == NumPoints(), PrimIndices.Num() == NumPrims() 
		const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS],  // Judge attrib exists by names and counts, for less HAPI call
		TArray<UHoudiniParameterAttribute*>& InOutPointAttribs, TArray<UHoudiniParameterAttribute*>& InOutPrimAttribs,
		const TArray<FString>& IgnorePointAttribNames, const TArray<FString>& IgnorePrimAttribNames);


	// -------- Selection --------
	FORCEINLINE bool ShouldCookOnPointSelect() const { return bool(PointEditOptions & EHoudiniEditOptions::CookOnSelect); }

	FORCEINLINE bool ShouldCookOnPrimSelect() const { return bool(PrimEditOptions & EHoudiniEditOptions::CookOnSelect); }

	FORCEINLINE bool IsPointSelected(const int32& PointIdx) const
	{
		if (SelectedClass != EHoudiniAttributeOwner::Point)
			return false;
		return SelectedIndices.Contains(PointIdx);
	}

	FORCEINLINE bool IsPrimSelected(const int32& PrimIdx) const
	{
		if (SelectedClass != EHoudiniAttributeOwner::Prim)
			return false;
		return SelectedIndices.Contains(PrimIdx);
	}

	FORCEINLINE bool IsGeometrySelected() const
	{
		return (SelectedClass == EHoudiniAttributeOwner::Point || SelectedClass == EHoudiniAttributeOwner::Prim) && !SelectedIndices.IsEmpty();
	}

	FORCEINLINE const EHoudiniAttributeOwner& GetSelectedClass() const { return SelectedClass; }

	void Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
		const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FModifierKeysState& ModifierKeys);

	void SelectAll();

	FORCEINLINE void ResetSelection()
	{
		SelectedClass = EHoudiniAttributeOwner::Invalid;
		SelectedIndices.Empty();
	}

	bool SelectIdenticals();  // Return whether is handled, if not, then expand select by connectivity

	// Child classes Must override these methods:
	virtual void Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
		const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FRay& ClickRay, const FModifierKeysState& ModifierKeys) {}
	
	virtual void ExpandSelection() {}

	virtual void FrustumSelect(const FConvexVolume& Frustum) {}
	
	virtual void SphereSelect(const FVector& Centroid, const float& Radius, const bool& bAppend) {}

	virtual bool GetSelectionBoundingBox(FBox& OutBoundingBox) const { return false; }

	// Asset Drop
	FORCEINLINE bool CanDropAssets() const { return (bool(PointEditOptions & EHoudiniEditOptions::AllowAssetDrop) || bool(PrimEditOptions & EHoudiniEditOptions::AllowAssetDrop)); }

	virtual int32 RayCast(const FRay& Ray, FVector& OutRayCastPos) const { return -1; }  // Return elem idx, < 0 means not intersected.

	bool OnAssetsDropped(const TArray<UObject*>& Assets, const int32& ElemIdx, const FVector& RayCastPos, const FModifierKeysState& ModifierKeys);  // Return whether selected indices changed

	// -------- Selection Transform --------
	virtual FTransform GetSelectionPivot() const { return FTransform::Identity; }  // Output world-coords(transfomed by component)
	//void TranslateSelection(const FVector& DeltaTranslate);
	//void RotateSelection(const FRotator& DeltaRotate, const FVector& Pivot);
	//void ScaleSelection(const FVector& DeltaScale, const FVector& Pivot);
	void StartTransforming(const FVector& Pivot);
	void EndTransforming(const FTransform& Transform, const FVector& Pivot);

	// -------- Upload --------
	static const FString& ParseEditData(TConstArrayView<const UHoudiniEditableGeometry*> EditGeos,  // Make sure that EditGeos.Num() >= 1
		size_t& OutNumVertices, size_t& OutNumPoints, size_t& OutNumPrims,
		EHoudiniAttributeOwner& OutChangedClass,
		TArray<UHoudiniParameterAttribute*>& OutPointAttribs,
		TArray<UHoudiniParameterAttribute*>& OutPrimAttribs);

	FORCEINLINE void ResetDeltaInfo() { DeltaInfo.Empty(); }
};
