// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEditableGeometry.h"

#include "HoudiniCurvesComponent.generated.h"


#define HOUDINI_RANDOM_ID FMath::Abs((int32)FPlatformTime::Cycles())

USTRUCT()
struct HOUDINIENGINE_API FHoudiniCurvePoint
{
	GENERATED_BODY()

	FHoudiniCurvePoint() : Id(-1) {};  // Should Never use default constructor

	FHoudiniCurvePoint(const int32& InId) : Id(InId) {}

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	int32 Id;  // Must initiate in constructors

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	FColor Color = FColor::White;

	UPROPERTY()
	FName CollisionActorName;

	UPROPERTY()
	FVector CollisionNormal = FVector::ZeroVector;
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniCurve
{
	GENERATED_BODY()

	FHoudiniCurve() : Id(-1) {};  // Should Never use default constructor

	FHoudiniCurve(const int32& InId) : Id(InId) {}

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	int32 Id;  // Must initiate in constructors

	UPROPERTY()
	bool bClosed = false;

	UPROPERTY()
	EHoudiniCurveType Type = EHoudiniCurveType::Polygon;

	UPROPERTY()
	TArray<int32> PointIndices;  // Local coordinate, need convert to world when display

	UPROPERTY()
	FColor Color = FColor::White;

	TArray<FVector> DisplayPoints;  // Points do Not have this

	TArray<int32> DisplayIndices;  // Align display points to vertices, Num() == PointIndices.Num(), only curvey curves have this

	FORCEINLINE bool NoNeedDisplay() const { return ((Type == EHoudiniCurveType::Points) || (PointIndices.Num() <= 1)); }

	static FVector Bezier(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const double& u);

	static FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const double& u);
};


UCLASS()
class HOUDINIENGINE_API UHoudiniCurvesComponent : public UHoudiniEditableGeometry
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<FHoudiniCurvePoint> Points;

	UPROPERTY()
	TArray<FHoudiniCurve> Curves;
	
	// -------- Defaults --------
	UPROPERTY()
	bool bDefaultCurveClosed = false;

	UPROPERTY()
	EHoudiniCurveType DefaultCurveType = EHoudiniCurveType::Polygon;

	UPROPERTY()
	FColor DefaultCurveColor = FColor::White;

	void GetPointsBounds(const TArray<int32>& PointIndices,  // PointIndices.Num() must >= 1
		FVector& OutMin, FVector& OutMax) const; 

public:
	virtual int32 NumVertices() const override;

	virtual int32 NumPoints() const override { return Points.Num(); }

	virtual int32 NumPrims() const override { return Curves.Num(); }

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	static const TArray<FString>& GetPointIntrinsicNames(const bool& bIsOutput);

	static const TArray<FString>& GetPrimIntrinsicNames(const bool& bIsOutput);

	FORCEINLINE bool IsDisabled() const { return uint32(PrimEditOptions & EHoudiniEditOptions::Disabled) >= 1; }

	FORCEINLINE const FColor& GetDefaultCurveColor() const { return DefaultCurveColor; }

	// -------- Display --------
	FORCEINLINE const TArray<FHoudiniCurve>& GetCurves() const { return Curves; }

	FORCEINLINE const TArray<FHoudiniCurvePoint>& GetPoints() const { return Points; }

	void RefreshCurveDisplayPoints(const int32& CurveIdx);


	// -------- Draw --------
	void AddNewCurve();  // if this has been called, then we need NOT call UHoudiniCurvesComponent::BeginDrawing()

	void BeginDrawing();  // will add a new curve and dup attrib-values when has curve selected

	void AddPoint(const FHoudiniCurvePoint& NewPoint, bool bDoBranch);  // Only when point selected, then could create branch

	void EndDrawing();


	// -------- Attributes --------
	void InputUpdate(AHoudiniNode* ParentNode, const FString& InGroupName,
		const bool& bInDefaultCurveClosed, const EHoudiniCurveType& InDefaultCurveType, const FColor& InDefaultCurveColor);
	

	bool GetSelectedCurveClosed() const;

	EHoudiniCurveType GetSelectedCurveType() const;

	void SetSelectedCurvesClosed(const bool& InNewCurveClosed);

	void SetSelectedCurvesType(const EHoudiniCurveType& InNewCurveType);

	// -------- Selection --------
	virtual void Select(const EHoudiniAttributeOwner& SelectClass, const int32& ElemIdx,
		const bool& bDeselectPrevious, const bool& bDeselectIfSelected, const FRay& ClickRay, const FModifierKeysState& ModifierKeys) override;  // We could record ClickPosition to DeltaInfo

	virtual void ExpandSelection() override;  // Will first check attribute identifier, then check connectivity

	virtual void FrustumSelect(const FConvexVolume& Frustum) override;

	virtual void SphereSelect(const FVector& Centroid, const float& Radius, const bool& bAppend) override;

	virtual FTransform GetSelectionPivot() const override;  // Output world-coords(transformed by component)
	
	virtual bool GetSelectionBoundingBox(FBox& OutBoundingBox) const override;  // return whether there is a bounding box on selection. e.g. if selection only has one point, then will return false
	

	TArray<int32> GetSelectedCurveIndices() const;

	bool GetInsertPosition(const FConvexVolume& Frustum, const FRay& MouseRay, FVector& OutInsertPos, int32& OutCurveIdx, int32& OutDisplayIdx) const;

	void InsertPoint(const FConvexVolume& Frustum, const FRay& MouseRay);


	bool IsSelectionSplittable() const;

	bool IsSelectionJoinable() const;

	bool SplitSelection();  // Return if truly split

	bool JoinSelection();  // Return if truly joined


	void RemoveSelection();

	void DuplicateSelection();

	void ProjectSelection();

	// -------- Selection Transform --------
	void TranslateSelection(const FVector& DeltaTranslate);

	void RotateSelection(const FRotator& DeltaRotate, const FVector& Pivot);

	void ScaleSelection(const FVector& DeltaScale, const FVector& Pivot);

	// -------- Asset Drop --------
	virtual int32 RayCast(const FRay& Ray, FVector& OutRayCastPos) const override;

	// -------- Retrieve From Output --------
	void ResetCurvesData();

	FHoudiniCurvePoint& AddPoint();  // Point.Id is the point index

	FHoudiniCurve& AddCurve();  // Curve.Id is the curve index

	// -------- Upload Data --------
	bool HapiInputUpload(const int32& SHMInputNodeId, const bool& bImportRotAndScale, const bool& bImportCollisionInfo, size_t& InOutHandle) const;

	static bool HapiUpload(const TArray<UHoudiniCurvesComponent*>& HCCs,  // Make sure that HCCs.Num() >= 1, will empty all DeltaInfos of EditGeos
		const int32& SHMInputNodeId, size_t& InOutHandle);

	// -------- Format --------
	TSharedPtr<FJsonObject> ConvertToJson() const;

	bool AppendFromJson(const TSharedPtr<FJsonObject>& JsonCurves);
};
