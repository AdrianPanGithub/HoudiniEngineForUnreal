// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

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

	FORCEINLINE AHoudiniNode* GetNode() const { return (AHoudiniNode*)GetOuter(); }

	const bool& CanHaveEditableOutput() const;

	bool HapiGetEditableGroups(TArray<FString>& OutEditableGroupNames, TArray<TArray<int32>>& OutEditableGroupships, bool& bOutIsMainGeoEditable,
		const int32& NodeId, const HAPI_PartInfo& PartInfo, const TArray<std::string>& PrimGroupNames) const;

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
// The register order of houdini engine itself: Landscape < Instancer < Asset < Spline/Curve < Mesh < SkeletalMesh(KineFX) < MaterialInstance < Texture(Image and VDB) < DataTable
class HOUDINIENGINE_API IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) = 0;

	virtual TSubclassOf<UHoudiniOutput> GetClass() const { return nullptr; }  // Will be called when bOutShouldHoldByOutput == true

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) { return true; }  // Will be called when bOutShouldHoldByOutput == false


	virtual ~IHoudiniOutputBuilder() {}
};



// Basic struct for all SplittableOutput, all outputs held by UHoudiniOutput are splittable
USTRUCT()
struct HOUDINIENGINE_API FHoudiniSplittableOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString SplitValue;

	AActor* FindOrCreateSplitActor(AHoudiniNode* Node, const bool& bIsEditableGeometry) const;  // Will FindOrCreate in AHoudiniNode::SplitActorMap or EditableSplitActorMap

public:
	FORCEINLINE const FString& GetSplitValue() const { return SplitValue; }

	AActor* FindSplitActor(const AHoudiniNode* Node, const bool& bIsEditableGeometry) const;  // Will search in AHoudiniNode::SplitActorMap or EditableSplitActorMap
};

// Basic struct for all ComponentOutput
USTRUCT()
struct HOUDINIENGINE_API FHoudiniComponentOutput : public FHoudiniSplittableOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName ComponentName;

	UPROPERTY()
	bool bSplitActor = false;  // Mark for split to a standalone actor. If false, we should NOT call FHoudiniSplittableOutput::FindSplitActor

	UActorComponent* FindComponent(const AHoudiniNode* Node, AActor*& OutSplitFoundActor, const bool& bIsEditableGeometry) const;

	void CreateOrUpdateComponent(AHoudiniNode* Node, USceneComponent*& InOutSC, const TSubclassOf<USceneComponent>& Class,
		const FString& InSplitValue, const bool& bInSplitActor, const bool& bIsEditableGeometry);

	void DestroyComponent(const AHoudiniNode* Node, const TWeakObjectPtr<USceneComponent>& Component, const bool& bIsEditableGeometry) const;


	// mutable TWeakObjectPtr<TComponentClass> Component;

	template<bool bIsEditGeo, typename TComponentClass>
	FORCEINLINE TComponentClass* Find_Internal(TWeakObjectPtr<TComponentClass>& InOutComponent, const AHoudiniNode* Node) const
	{
		if (InOutComponent.IsValid())
			return InOutComponent.Get();

		AActor* FoundSplitActor = nullptr;
		TComponentClass* FoundComp = Cast<TComponentClass>(FindComponent(Node, FoundSplitActor, bIsEditGeo));
		InOutComponent = FoundComp;
		return FoundComp;
	}

	template<bool bIsEditGeo, typename TComponentClass>
	FORCEINLINE TComponentClass* CreateOrUpdate_Internal(TWeakObjectPtr<TComponentClass>& InOutComponent,
		AHoudiniNode* Node, const FString& InSplitValue, const bool& bInSplitActor)
	{
		USceneComponent* Comp = InOutComponent.IsValid() ? InOutComponent.Get() : nullptr;
		CreateOrUpdateComponent(Node, Comp, TComponentClass::StaticClass(), InSplitValue, bInSplitActor, bIsEditGeo);
		InOutComponent = (TComponentClass*)Comp;
		return (TComponentClass*)Comp;
	}

public:
	FORCEINLINE bool IsSplitActor() const { return bSplitActor && (ComponentName != NAME_None); }

	FORCEINLINE bool CanReuse(const FString& InSplitValue, const bool& bInSplitActor) const
	{
		return ((bSplitActor == bInSplitActor) && (!bSplitActor || SplitValue == InSplitValue));
	}
};
