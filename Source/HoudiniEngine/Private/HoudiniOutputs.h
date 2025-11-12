// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniOutput.h"

#include "HoudiniOutputs.generated.h"


class ALandscape;
class AGeometryCollectionActor;
class UGeometryCollection;
class UDynamicMeshComponent;
class UHoudiniCurvesComponent;
class UHoudiniMeshComponent;


USTRUCT()
struct FHoudiniStaticMeshOutput : public FHoudiniComponentOutput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	int32 PartId = -1;  // if is a packed mesh, then instancer part could find it by NodeId and PartId

protected:
	mutable TWeakObjectPtr<UStaticMeshComponent> Component;

	static IConsoleVariable* MeshDistanceFieldCVar;
	static bool GShouldRecoverMeshDistanceField;

public:
	UStaticMeshComponent* Find(const AHoudiniNode* Node) const;

	UStaticMeshComponent* Commit(AHoudiniNode* Node, const int32& PartId, const FString& InSplitValue, const bool& bSplitActors);  // PartId >= 0 means this is a packed mesh part, we should destroy SMC, otherwise, find or create a SMC

	void Destroy(const AHoudiniNode* Node) const;  // Will also delete the StaticMesh asset

	FORCEINLINE bool IsInstanced() const { return ComponentName.IsNone(); }


	HOUDINIENGINE_API static void OnStaticMeshBuild();  // Will Temporarily disable MeshDistanceField building
	HOUDINIENGINE_API static void FinishCompilation(const bool& bForce);
};

USTRUCT()
struct FHoudiniDynamicMeshOutput : public FHoudiniComponentOutput
{
	GENERATED_BODY()

protected:
	mutable TWeakObjectPtr<UDynamicMeshComponent> Component;

public:
	UDynamicMeshComponent* Find(const AHoudiniNode* Node) const;

	UDynamicMeshComponent* CreateOrUpdate(AHoudiniNode* Node, const FString& InSplitValue, const bool& bSplitToActors);

	void Destroy(const AHoudiniNode* Node) const;
};

USTRUCT()
struct FHoudiniMeshOutput : public FHoudiniComponentOutput
{
	GENERATED_BODY()

protected:
	mutable TWeakObjectPtr<UHoudiniMeshComponent> Component;

public:
	UHoudiniMeshComponent* Find(const AHoudiniNode* Node) const;

	UHoudiniMeshComponent* CreateOrUpdate(AHoudiniNode* Node, const FString& InSplitValue, const bool& bSplitToActors);

	void Destroy(const AHoudiniNode* Node) const;
};

UCLASS()
class UHoudiniOutputMesh : public UHoudiniOutput
{
	GENERATED_BODY()

protected:
	int32 NodeId = -1;  // If is a packed mesh, then instancer part could find it by NodeId and PartId

	UPROPERTY()
	TArray<FHoudiniStaticMeshOutput> StaticMeshOutputs;

	UPROPERTY()
	TArray<FHoudiniDynamicMeshOutput> DynamicMeshOutputs;

	UPROPERTY()
	TArray<FHoudiniMeshOutput> HoudiniMeshOutputs;

public:
	virtual bool HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;

	virtual void Destroy() const override;

	virtual void CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const override;

	void CollectEditMeshes(TMap<FString, TArray<UHoudiniMeshComponent*>>& InOutGroupMeshesMap) const;

	void GetInstancedStaticMeshes(const TArray<int32>& InstancedPartIds, TArray<UStaticMesh*>& OutSMs) const;  // For packed mesh parts
};

class FHoudiniMeshOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual TSubclassOf<UHoudiniOutput> GetClass() const override { return UHoudiniOutputMesh::StaticClass(); }
};

class FHoudiniSkeletalMeshOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;
};


enum class EHoudiniInstancerOutputMode : int8
{
	Auto = 0,  // Depend on instance count, When just a single point without custom floats
	ISMC,  // Force to generate InstancedStaticMeshComponent
	HISMC,  // Force to generate HierarchicalInstancedStaticMeshComponent
	Components, // From USceneComponent classes, such as Blueprint, PointLightComponent etc.
	Actors,  // From Blueprint, DecalMaterial, PointLight etc.
	Foliage,
	GeometryCollection
};

USTRUCT()
struct FHoudiniInstancedStaticMeshOutput : public FHoudiniComponentOutput
{
	GENERATED_BODY()

protected:
	mutable TWeakObjectPtr<UStaticMeshComponent> Component;

public:
	UStaticMeshComponent* Find(const AHoudiniNode* Node) const;

	UStaticMeshComponent* CreateOrUpdate(AHoudiniNode* Node,
		const EHoudiniInstancerOutputMode& InstancedType, const FString& InSplitValue, const bool& bSplitToActors);

	void Destroy(const AHoudiniNode* Node) const;
};

USTRUCT()
struct FHoudiniInstancedActorOutput : public FHoudiniSplittableOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FSoftObjectPath Reference;

	UPROPERTY()
	TArray<FHoudiniActorHolder> ActorHolders;

public:
	bool HasValidAndCleanup();

	int32 GetMatchScore(const UObject* Instance, const int32& NumInsts) const;  // Smaller is better, Minus means not matched

	bool Update(const AHoudiniNode* Node, const FString& InSplitValue, UObject* Instance, AActor*& InOutRefActor,
		const TArray<int32>& PointIndices, const TArray<FTransform>& Transforms, TFunctionRef<void(AActor*, const int32& ElemIdx)> PostFunc, const bool& bCustomFolderPath);

	void TransformActors(const FMatrix& DeltaXform) const;

	void Destroy() const;
};

USTRUCT()
struct FHoudiniInstancedComponentHolder
{
	GENERATED_BODY()

	UPROPERTY()
	FName ComponentName;

	mutable TWeakObjectPtr<USceneComponent> Component;
};

USTRUCT()
struct FHoudiniInstancedComponentOutput : public FHoudiniSplittableOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSubclassOf<USceneComponent> ComponentClass;

	UPROPERTY()
	TArray<FHoudiniInstancedComponentHolder> ComponentHolders;

	UPROPERTY()
	bool bSplitActor = false;

	const AActor* GetParentActorNameComponentMap(const AHoudiniNode* Node, TMap<FName, USceneComponent*>& OutNameComponentMap) const;

public:
	bool HasValidAndCleanup(const AHoudiniNode* Node);

	int32 GetMatchScore(const TSubclassOf<USceneComponent>& InComponentClass, const int32& NumInsts) const;  // Smaller is better, Zero means perfect, Minus means not matched

	bool Update(const TSubclassOf<USceneComponent>& InComponentClass, AHoudiniNode* Node,
		const FString& InSplitValue, const bool& bSplitActors, const int32& NumComponents, TArray<USceneComponent*>& OutComponents);

	void Destroy(const AHoudiniNode* Node) const;

	FORCEINLINE bool IsSplitActor() const { return bSplitActor && !ComponentHolders.IsEmpty(); }

	FORCEINLINE bool CanReuse(const FString& InSplitValue, const bool& bInSplitActor) const
	{
		return ((bSplitActor == bInSplitActor) && (!bSplitActor || SplitValue == InSplitValue));
	}
};

USTRUCT()
struct FHoudiniFoliageOutput : public FHoudiniComponentOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<FHoudiniActorHolder> FoliageActorHolders;  // Record InstancedFoliageActors, for delete FoliageInstances

	mutable TWeakObjectPtr<UActorComponent> Component;  // Just create an empty component to bind foliage instances

public:
	bool IsComponentValid(const AHoudiniNode* Node) const;

	bool Create(AHoudiniNode* Node, UObject* Instance,
		const FString& GeoName, const FString& InSplitValue, const bool& bInSplitActor,
		const TArray<int32>& PointIndices, const TArray<FTransform>& Transforms);

	void ClearFoliageInstances(const AHoudiniNode* Node) const;

	void Destroy(AHoudiniNode* Node) const;
};

USTRUCT()
struct FHoudiniGeometryCollectionOutput : public FHoudiniSplittableOutput
{
	GENERATED_BODY()

public:
	FHoudiniGeometryCollectionOutput() {}

	FHoudiniGeometryCollectionOutput(AGeometryCollectionActor* Actor);

protected:
	UPROPERTY()
	FName ActorName;

	mutable TWeakObjectPtr<AGeometryCollectionActor> Actor;

public:
	AGeometryCollectionActor* Load() const;

	AGeometryCollectionActor* Update(const FString& InSplitValue, const FString& OutputName, const AHoudiniNode* Node, UGeometryCollection*& OutGC);

	void TransformActor(const FMatrix& DeltaXform) const;

	void Destroy() const;
};

UCLASS()
class UHoudiniOutputInstancer : public UHoudiniOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<FHoudiniInstancedStaticMeshOutput> InstancedStaticMeshOutputs;

	UPROPERTY()
	TArray<FHoudiniInstancedComponentOutput> InstancedComponentOutputs;

	UPROPERTY()
	TArray<FHoudiniInstancedActorOutput> InstancedActorOutputs;

	UPROPERTY()
	TArray<FHoudiniFoliageOutput> FoliageOutputs;

	UPROPERTY()
	TArray<FHoudiniGeometryCollectionOutput> GeometryCollectionOutputs;

public:
	virtual bool HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;

	virtual void Destroy() const override;

	virtual void CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const override;

	virtual void OnNodeTransformed(const FMatrix& DeltaXform) const override;

	virtual bool HasStandaloneActors() const override { return !InstancedActorOutputs.IsEmpty() || !GeometryCollectionOutputs.IsEmpty(); }

	virtual void DestroyStandaloneActors() const override;
};

class FHoudiniInstancerOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual TSubclassOf<UHoudiniOutput> GetClass() const override { return UHoudiniOutputInstancer::StaticClass(); }
};


USTRUCT()
struct FHoudiniCurvesOutput : public FHoudiniComponentOutput
{
	GENERATED_BODY()

protected:
	mutable TWeakObjectPtr<UHoudiniCurvesComponent> Component;

public:
	UHoudiniCurvesComponent* Find(const AHoudiniNode* Node) const;

	UHoudiniCurvesComponent* CreateOrUpdate(AHoudiniNode* Node, const FString& InSplitValue, const bool& bSplitToActors);

	void Destroy(const AHoudiniNode* Node) const;
};

UCLASS()
class UHoudiniOutputCurve : public UHoudiniOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<FHoudiniInstancedComponentOutput> SplineComponentOutputs;

	UPROPERTY()
	TArray<FHoudiniInstancedActorOutput> SplineActorOutputs;

	UPROPERTY()
	TArray<FHoudiniCurvesOutput> HoudiniCurvesOutputs;

public:
	virtual bool HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;

	virtual void Destroy() const override;

	virtual void CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const override;

	virtual void OnNodeTransformed(const FMatrix& DeltaXform) const override;

	virtual bool HasStandaloneActors() const override { return !SplineActorOutputs.IsEmpty(); }

	virtual void DestroyStandaloneActors() const override;

	void CollectEditCurves(TMap<FString, TArray<UHoudiniCurvesComponent*>>& InOutGroupMeshesMap) const;
};

class FHoudiniCurveOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual TSubclassOf<UHoudiniOutput> GetClass() const override { return UHoudiniOutputCurve::StaticClass(); }
};


USTRUCT()
struct FHoudiniLandscapeOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName LandscapeName;

	mutable TWeakObjectPtr<ALandscape> Landscape;

	UPROPERTY()
	TArray<FHoudiniActorHolder> LandscapeProxyHolders;

public:
	FHoudiniLandscapeOutput() {}

	FHoudiniLandscapeOutput(ALandscape* Landscape);

	ALandscape* Load() const;

	void Destroy() const;
};

UCLASS()
class UHoudiniOutputLandscape : public UHoudiniOutput
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<FHoudiniLandscapeOutput> LandscapeOutputs;

public:
	virtual bool HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;

	virtual void Destroy() const override;

	virtual void OnNodeTransformed(const FMatrix& DeltaXform) const override;
};

class FHoudiniLandscapeOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual TSubclassOf<UHoudiniOutput> GetClass() const override { return UHoudiniOutputLandscape::StaticClass(); }

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;
};



class FHoudiniTextureOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;
};

class FHoudiniDataTableOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;
};

class FHoudiniMaterialInstanceOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;
};

class FHoudiniAssetOutputBuilder : public IHoudiniOutputBuilder
{
public:
	virtual bool HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput) override;

	virtual bool HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos) override;
};
