// Copyright (c) <2024> Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEngineCommon.h"

#include "HoudiniOutput.generated.h"


struct HAPI_GeoInfo;
struct HAPI_PartInfo;

class AHoudiniNode;

UCLASS(Abstract)
class HOUDINIENGINE_API UHoudiniOutput : public UObject  // Outer must be AHoudiniNode
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString Name;

	const bool& CanHaveEditableOutput() const;

	FORCEINLINE AHoudiniNode* GetNode() const { return (AHoudiniNode*)GetOuter(); }

	bool HapiGetEditableGroups(TArray<FString>& OutEditableGroupNames, TArray<TArray<int32>>& OutEditableGroupships, bool& bOutIsMainGeoEditable,
		const int32& NodeId, const HAPI_PartInfo& PartInfo, const TArray<std::string>& PrimGroupNames);

	FString GetCookFolderPath() const;

public:
	FORCEINLINE const FString& GetOutputName() const { return Name; }

	virtual bool HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) { return true; }

	virtual void Destroy() const {}

	virtual void CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const {}  // Collect SplitValues after output update, for remove useless SplitActors

	virtual void OnNodeTransformed(const FMatrix& DeltaTransform) const {}

	virtual bool HasStandaloneActors() const { return false; }

	virtual void DestroyStandaloneActors() const {}
};

// Inherit from builder and register using FHoudiniEngine::RegisterOutputBuilder
// The register order of houdini engine itself: Landscape < Instancer < Curve < Mesh < Texture < DataTable
class HOUDINIENGINE_API IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) = 0;

	virtual TSubclassOf<UHoudiniOutput> GetClass() const { return nullptr; }  // Will be called when bOutShouldHoldByOutput == true

	virtual bool HapiRetrieve(AHoudiniNode* Node, const int32& NodeId, const TArray<HAPI_PartInfo>& PartInfos) { return true; }  // Will be called when bOutShouldHoldByOutput == false
};



// Basic struct for all SplitableOutput
USTRUCT()
struct HOUDINIENGINE_API FHoudiniSplitableOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString SplitValue;

	AActor* FindOrCreateSplitActor(const AHoudiniNode* Node, const bool& bIsEditableGeometry) const;  // Will FindOrCreate in AHoudiniNode::SplitActorMap or EditableSplitActotMap

public:
	FORCEINLINE const FString& GetSplitValue() const { return SplitValue; }

	AActor* FindSplitActor(const AHoudiniNode* Node, const bool& bIsEditableGeometry) const;  // Will Search in AHoudiniNode::SplitActorMap or EditableSplitActotMap
};

// Basic struct for all ComponentOutput
USTRUCT()
struct HOUDINIENGINE_API FHoudiniComponentOutput : public FHoudiniSplitableOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName ComponentName;

	UPROPERTY()
	bool bSplitActor = false;  // Mark for split to a standalone actor. If false, we should NOT call FHoudiniSplitableOutput::FindSplitActor

	UActorComponent* FindComponent(const AHoudiniNode* Node, AActor*& OutSplitFoundActor, const bool& bIsEditableGeometry) const;

	void CreateOrUpdateComponent(AHoudiniNode* Node, USceneComponent*& InOutSC, const TSubclassOf<USceneComponent>& Class,
		const FString& InSplitValue, const bool& bInSplitActor, const bool& bIsEditableGeometry);

	void DestroyComponent(const AHoudiniNode* Node, const TWeakObjectPtr<USceneComponent>& Component, const bool& bIsEditableGeometry) const;

public:
	FORCEINLINE bool IsSplitActor() const { return bSplitActor && (ComponentName != NAME_None); }

	FORCEINLINE bool CanReuse(const FString& InSplitValue, const bool& bInSplitActor) const
	{
		return ((bSplitActor == bInSplitActor) && (!bSplitActor || SplitValue == InSplitValue));
	}
};
