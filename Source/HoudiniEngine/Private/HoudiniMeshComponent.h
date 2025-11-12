// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEditableGeometry.h"

#include "HoudiniMeshComponent.generated.h"


struct FHoudiniAttributeParameterSet;
class AHoudiniNode;


USTRUCT(BlueprintType)
struct HOUDINIENGINE_API FHoudiniMeshSection
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> TriangleIndices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HoudiniMesh)
	FName MaterialSlotName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HoudiniMesh)
	TObjectPtr<UMaterialInterface> Material;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniMeshUV
{
	GENERATED_BODY()

	UPROPERTY()
	EHoudiniAttributeOwner Owner = EHoudiniAttributeOwner::Invalid;

	UPROPERTY()
	TArray<FVector2f> Data;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniMeshPoly
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> PointIndices;

	UPROPERTY()
	TArray<FIntVector4> Triangles;  // XYZ is Local point indices of triangles within this poly, W is mesh triangle index
};

UCLASS()
class HOUDINIENGINE_API UHoudiniMeshComponent : public UHoudiniEditableGeometry
{
	GENERATED_BODY()
	
protected:
	// -------- Basic Data, must have values --------
	UPROPERTY()
	TArray<FVector3f> Positions;  // Point

	UPROPERTY()
	TArray<FIntVector> Triangles;  // Topology

	UPROPERTY()
	TArray<FHoudiniMeshSection> Sections;  // Section, for materials

	UPROPERTY()
	EHoudiniAttributeOwner NormalOwner = EHoudiniAttributeOwner::Invalid;

	UPROPERTY()
	TArray<FVector3f> NormalData;  // Must has Normals

	// -------- Attributes, optional, and memory saving --------
	UPROPERTY()
	EHoudiniAttributeOwner TangentUOwner = EHoudiniAttributeOwner::Invalid;

	UPROPERTY()
	TArray<FVector3f> TangentUData;

	UPROPERTY()
	EHoudiniAttributeOwner TangentVOwner = EHoudiniAttributeOwner::Invalid;

	UPROPERTY()
	TArray<FVector3f> TangentVData;

	UPROPERTY()
	EHoudiniAttributeOwner ColorOwner = EHoudiniAttributeOwner::Invalid;

	UPROPERTY()
	TArray<FColor> ColorData;

	UPROPERTY()
	TArray<FHoudiniMeshUV> UVs;

	// -------- Mesh Edit Data --------
	UPROPERTY()
	TArray<FHoudiniMeshPoly> Polys;

	UPROPERTY()
	TArray<FIntVector2> Edges;

	void GetPointsBounds(const TArray<int32>& PointIndices,  // PointIndices.Num() must >= 1
		FVector& OutMin, FVector& OutMax) const;

public:
	virtual int32 NumVertices() const override;

	virtual int32 NumPoints() const override { return Positions.Num(); }

	virtual int32 NumPrims() const override { return Polys.Num(); }

	void ResetMeshData();

	FORCEINLINE int32 AddPoint(const FVector3f& Position) { return Positions.Add(Position); }

	void AddTriangle(const FIntVector3& Triangle, const int32& SectionIdx);

	int32 AddSection(UMaterialInterface* Material);

	
	void SetNormalData(const EHoudiniAttributeOwner& Owner, const TArray<float>& Data,
		const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices);

	void ComputeNormal();  // We must call SetNormalData or ComputeNormal when Retrieve mesh attributes

	void SetTangentUData(const EHoudiniAttributeOwner& Owner, const TArray<float>& Data,
		const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices);

	void SetTangentVData(const EHoudiniAttributeOwner& Owner, const TArray<float>& Data,
		const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices);

	void SetColorData(const EHoudiniAttributeOwner& CdOwner, const TArray<float>& CdData,
		const EHoudiniAttributeOwner& AlphaOwner, const TArray<float>& AlphaData,
		const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices);

	void SetUVData(const TArray<EHoudiniAttributeOwner>& Owners, const TArray<TArray<float>>& Datas,
		const TArray<int32>& PointIndices, const TArray<int32>& TriangleIndices);

	// Will build polys and edges
	void Build(const TArray<int32>& PrimIds,  // Num() == HAPI_PartInfo::faceCount, are the attribValue of __primitive_id
		const TArray<int32>& TriangleIndices,  // Num() == UHoudiniMeshComponent::Triangles.Num(), are the Global indices of the traingles
		TArray<int32>& OutPrimIndices);  // Num() == UHoudiniMeshComponent::Polys.Num(), are the Global indices of all poly's first traingles

	virtual void ClearEditData() override;  // Will clear polys and edges, and attribs


	FORCEINLINE const TArray<FVector3f>& GetPositions() const { return Positions; }

	FORCEINLINE const TArray<FIntVector>& GetTriangles() const { return Triangles; }

	FORCEINLINE const TArray<FHoudiniMeshPoly>& GetPolys() const { return Polys; }

	FORCEINLINE const TArray<FIntVector2>& GetEdges() const { return Edges; }


	UFUNCTION(BlueprintCallable, Category = "HoudiniMeshComponent", BlueprintPure = false)
	UStaticMesh* SaveStaticMesh(const FString& StaticMeshName) const;

	UFUNCTION(BlueprintCallable, Category = "HoudiniMeshComponent", BlueprintPure = false)
	void SaveToStaticMesh(UStaticMesh* StaticMesh) const;

#if WITH_EDITOR
	virtual void PostEditUndo() override
	{
		Super::PostEditUndo();
		TriggerParentNodeToCook();
	}
#endif

	// -------- Selection --------
	virtual void Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
		const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FRay& ClickRay, const FModifierKeysState& ModifierKeys) override;

	virtual void ExpandSelection() override;  // Will first check attribute identifier, then check connectivity

	virtual void FrustumSelect(const FConvexVolume& Frustum) override;

	virtual void SphereSelect(const FVector& Centroid, const float& Radius, const bool& bAppend) override;

	virtual FTransform GetSelectionPivot() const override;  // Output world-coords(transfomed by component)

	virtual bool GetSelectionBoundingBox(FBox& OutBoundingBox) const override;  // return whether there is a bounding box on selection. e.g. if selection only has one point, then will return false

	// -------- Asset Drop --------
	virtual int32 RayCast(const FRay& Ray, FVector& OutRayCastPos) const override;

	// -------- Selection Transform --------
	void TranslateSelection(const FVector& DeltaTranslate);

	void RotateSelection(const FRotator& DeltaRotate, const FVector& Pivot);

	void ScaleSelection(const FVector& DeltaScale, const FVector& Pivot);


	static bool HapiUpload(const TArray<UHoudiniMeshComponent*>& HMCs,  // Make sure that HMCs.Num() >= 1, will empty all DeltaInfos of EditGeos
		const int32& SHMInputNodeId, size_t& InOutHandle);

	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool ShouldRenderSelected() const override { return false; }  // Disable orange profile when selected

	//virtual TArray<class UMaterialInterface*> GetMaterials() const override;
	//virtual int32 GetMaterialIndex(FName MaterialSlotName) const override;
	//virtual TArray<FName> GetMaterialSlotNames() const override;
	//virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const override { return GetMaterialIndex(MaterialSlotName) >= 0; }
	virtual int32 GetNumMaterials() const override { return Sections.Num(); }
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override { return Sections[ElementIndex].Material; }
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual void SetMaterialByName(FName MaterialSlotName, class UMaterialInterface* Material) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
};
