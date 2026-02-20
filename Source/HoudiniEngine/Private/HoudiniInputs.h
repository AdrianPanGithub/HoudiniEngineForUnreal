// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniInput.h"
#include "HoudiniInputs.generated.h"


class UHoudiniCurvesComponent;
class ALandscape;
struct FLandscapeEditDataInterface;
class ULandscapeLayerInfoObject;
class UFoliageType_InstancedStaticMesh;


UCLASS()
class HOUDINIENGINE_API UHoudiniInputNode : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName NodeActorName;

	mutable TWeakObjectPtr<AHoudiniNode> Node;

public:
	static UHoudiniInputNode* Create(UHoudiniInput* Input, AHoudiniNode* Node);

	FORCEINLINE void MarkChanged() { bHasChanged = true; }

	FORCEINLINE const FName& GetNodeActorName() const { return NodeActorName; }

	void SetNode(AHoudiniNode* InputNode);

	AHoudiniNode* GetNode() const;

	virtual TSoftObjectPtr<UObject> GetObject() const override;
};


UCLASS()
class HOUDINIENGINE_API UHoudiniInputCurves : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UHoudiniCurvesComponent> CurvesComponent;  // Created along with holder, should Never be nullptr

	int32 NodeId = -1;

	size_t Handle = 0;

	FORCEINLINE const bool& ShouldImportRotAndScale() const { return GetSettings().bImportRotAndScale; }

	FORCEINLINE const bool& ShouldImportCollisionInfo() const { return GetSettings().bImportCollisionInfo; }

public:
	static UHoudiniInputCurves* Create(UHoudiniInput* Input);

	FORCEINLINE UHoudiniCurvesComponent* GetCurvesComponent() const { return CurvesComponent; }

	virtual bool HapiUpload() override;

	virtual void Invalidate() override;

	virtual void Destroy() override;

	virtual bool HapiDestroy() override;

	virtual bool HasChanged() const override;
};


UCLASS()
class UHoudiniInputTexture : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<UTexture2D> Texture;

	int32 NodeId = -1;

	size_t Handle = 0;

public:
	void SetAsset(UTexture2D* NewTexture);

	virtual TSoftObjectPtr<UObject> GetObject() const override { return Texture; }

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};

class FHoudiniTextureInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override { InOutAllowClasses.Add(UTexture2D::StaticClass()); }

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;
};


UCLASS()
class UHoudiniInputDataTable : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<UDataTable> DataTable;

	int32 NodeId = -1;  // Will be "null" if import as reference

	size_t Handle = 0;  // == false if import as reference

public:
	void SetAsset(UDataTable* NewDataTable);

	virtual TSoftObjectPtr<UObject> GetObject() const override { return DataTable; }

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};

class FHoudiniDataTableInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override { InOutAllowClasses.Add(UDataTable::StaticClass()); }

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;
};


UCLASS()
class UHoudiniInputDataAsset : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<UDataAsset> DataAsset;

	int32 NodeId = -1;

public:
	void SetAsset(UDataAsset* NewDataAsset);

	virtual TSoftObjectPtr<UObject> GetObject() const override { return DataAsset; }

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};

class FHoudiniDataAssetInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override { InOutAllowClasses.Add(UDataAsset::StaticClass()); }

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;

	virtual bool GetInfo(const UObject* Asset, FString& OutInfoStr) override;
};


UCLASS()
class UHoudiniInputStaticMesh : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	int32 SettingsNodeId = -1;  // Is a "null" if import as reference, else is "setupmeshinput" node

	int32 MeshNodeId = -1;  // < 0 if import as reference

	size_t MeshHandle = 0;

	static bool HapiImportMeshDescription(const UStaticMesh* SM, const UStaticMeshComponent* SMC,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle);

	static bool HapiImportRenderData(const UStaticMesh* SM, const UStaticMeshComponent* SMC,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle);

public:
	FORCEINLINE static bool HapiImport(const UStaticMesh* SM, const UStaticMeshComponent* SMC,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle)
	{
		return Settings.bImportRenderData ?
			HapiImportRenderData(SM, SMC, Settings, GeoNodeId, InOutSHMInputNodeId, InOutHandle) :
			HapiImportMeshDescription(SM, SMC, Settings, GeoNodeId, InOutSHMInputNodeId, InOutHandle);
	}

	static bool AppendBoundInfo(const UStaticMesh* InSM, FString& InOutInfoStr);

	void SetAsset(UStaticMesh* NewStaticMesh);

	virtual TSoftObjectPtr<UObject> GetObject() const override { return StaticMesh; }

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};

class FHoudiniStaticMeshInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override { InOutAllowClasses.Add(UStaticMesh::StaticClass()); }

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;

	virtual bool GetInfo(const UObject* Asset, FString& OutInfoStr) override;
};


UCLASS()
class UHoudiniInputSkeletalMesh : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	int32 SettingsNodeId = -1;  // Is a "null" if import as reference, else is "he_setup_mesh_input" node

	int32 MeshNodeId = -1;  // < 0 if import as reference

	size_t MeshHandle = 0;

	int32 SkeletonNodeId = -1;

	size_t SkeletonHandle = 0;

	static bool HapiImportMeshDescription(const USkeletalMesh* SM, const USkinnedMeshComponent* SMC,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle);

	static bool HapiImportRenderData(const USkeletalMesh* SM, const USkinnedMeshComponent* SMC,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle);

	static bool HapiImportSkeleton(const USkeletalMesh* SM,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle);

public:
	static bool HapiImport(const USkeletalMesh* SM, const USkinnedMeshComponent* SMC,
		const FHoudiniInputSettings& Settings, const int32& GeoNodeId,
		int32& InOutMeshInputNodeId, size_t& InOutMeshHandle, int32& InOutSkeletonInputNodeId, size_t& InOutSkeletonHandle);

	void SetAsset(USkeletalMesh* NewSkeletalMesh);

	virtual TSoftObjectPtr<UObject> GetObject() const override { return SkeletalMesh; }

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};

class FHoudiniSkeletalMeshInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override { InOutAllowClasses.Add(USkeletalMesh::StaticClass()); }

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;

	virtual bool GetInfo(const UObject* Asset, FString& OutInfoStr) override;
};


// Copy from UHoudiniInputStaticMesh
UCLASS()
class UHoudiniInputFoliageType : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<UFoliageType_InstancedStaticMesh> FoliageType;

	int32 SettingsNodeId = -1;  // Is a "null" if import as reference, else is "setupmeshinput" node

	int32 MeshNodeId = -1;  // < 0 if import as reference

	size_t MeshHandle = 0;

public:
	void SetAsset(UFoliageType_InstancedStaticMesh* NewFoliageType);

	virtual TSoftObjectPtr<UObject> GetObject() const override;

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};

class FHoudiniFoliageTypeInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override;

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;

	virtual bool GetInfo(const UObject* Asset, FString& OutInfoStr) override;
};


// -------- Component Inputs --------
class FHoudiniActorComponentInputBuilder : public IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) override;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) override;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
		const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject) override;
};

class FHoudiniStaticMeshComponentInput : public FHoudiniComponentInput
{
public:
	enum class EHoudiniSettingsNodeType
	{
		null = 0,  // Reference only
		copytopoints,
		he_setup_mesh_input,  // UStaticMesh, UModel(BSP), UDynamicMesh
	};

	FHoudiniStaticMeshComponentInput(const EHoudiniSettingsNodeType& InType) : Type(InType) {}

	int32 MeshNodeId = -1;  // Must be a geo input node
	size_t MeshHandle = 0;

	EHoudiniSettingsNodeType Type = EHoudiniSettingsNodeType::null;
	int32 SettingsNodeId = -1;

	virtual bool HapiDestroy(UHoudiniInput* Input) const override;

	virtual void Invalidate() const override;
};

class FHoudiniStaticMeshComponentInputBuilder : public IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) override;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) override;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
		const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject) override;
};

class FHoudiniSkeletalMeshComponentInput : public FHoudiniComponentInput
{
public:
	enum class EHoudiniSettingsNodeType
	{
		null = 0,  // Reference only
		he_setup_kinefx_inputs,
		he_setup_kinefx_input,  // USkeletalMesh
	};

	FHoudiniSkeletalMeshComponentInput(const EHoudiniSettingsNodeType& InType) : Type(InType) {}

	int32 MeshNodeId = -1;
	size_t MeshHandle = 0;

	int32 SkeletonNodeId = -1;
	size_t SkeletonHandle = 0;

	EHoudiniSettingsNodeType Type = EHoudiniSettingsNodeType::null;
	int32 SettingsNodeId = -1;

	virtual bool HapiDestroy(UHoudiniInput* Input) const override;

	virtual void Invalidate() const override;
};

class FHoudiniSkinnedMeshComponentInputBuilder : public IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) override;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) override;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
		const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject) override;
};

class FHoudiniGeometryComponentInput : public FHoudiniComponentInput
{
public:
	int32 NodeId = -1;

	size_t Handle = 0;

	virtual bool HapiDestroy(UHoudiniInput* Input) const override;

	virtual void Invalidate() const override;
};

class FHoudiniSplineComponentInputBuilder : public IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) override;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) override;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
		const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject) override;
};

class FHoudiniBrushComponentInputBuilder : public IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) override;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) override;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
		const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject) override;
};

class FHoudiniDynamicMeshComponentInputBuilder : public IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) override;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) override;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
		const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject) override;
};


UCLASS(Abstract)
class HOUDINIENGINE_API UHoudiniInputComponents : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	int32 InstancerNodeId = -1;

	size_t InstanceHandle = 0;

	bool bHasStandaloneInstancers = false;

	int32 SettingsNodeId = -1;  // A "blast" node, to get pure ref points, or "null" when Blueprint or Actor has NO valid component

	TMap<TWeakPtr<IHoudiniComponentInputBuilder>, TArray<TSharedPtr<FHoudiniComponentInput>>> ComponentInputs;  // If RefNode directly connect to merge, then means SMCInputs is Empty

	bool HapiUploadComponents(const UObject* Asset, const TArray<const UActorComponent*>& Components, const TArray<FTransform>& ComponentTransforms);  // Components should be filtered by ShouldImport()

	bool HapiDestroyNodes();

public:
	static void GetComponentsInfo(const AActor* Actor,  // if Actor == nullptr, means this is a blueprint
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, FString& OutInfoStr);

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};


UCLASS()
class UHoudiniInputBlueprint : public UHoudiniInputComponents
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TSoftObjectPtr<UBlueprint> Blueprint;

public:
	static void GetComponents(const UBlueprint* BP, TArray<const UActorComponent*>& OutComponents, TArray<FTransform>& OutTransforms);

	void SetAsset(UBlueprint* NewBlueprint);

	virtual TSoftObjectPtr<UObject> GetObject() const override { return Blueprint; }

	virtual bool IsObjectExists() const override;

	virtual bool HapiUpload() override;
};

class FHoudiniBlueprintInputBuilder : public IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses) override { InOutAllowClasses.Add(UBlueprint::StaticClass()); }

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) override;

	virtual bool GetInfo(const UObject* Asset, FString& InOutInfoStr) override;
};


UCLASS()
class HOUDINIENGINE_API UHoudiniInputActor : public UHoudiniInputComponents
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName ActorName;

	mutable TWeakObjectPtr<AActor> Actor;

public:
	static UHoudiniInputActor* Create(UHoudiniInput* Input, AActor* Actor);

	FORCEINLINE const FName& GetActorName() const { return ActorName; }

	void SetActor(AActor* NewActor);

	AActor* GetActor() const;

	virtual TSoftObjectPtr<UObject> GetObject() const override { return GetActor(); }

	virtual bool HapiUpload() override;
};


#define INVALID_LANDSCAPE_EXTENT FIntRect(MAX_int32, MAX_int32, MIN_int32, MIN_int32)  // See ULandscapeInfo::GetLandscapeExtent

USTRUCT()
struct HOUDINIENGINE_API FHoudiniLayerImportInfo
{
	GENERATED_BODY()

	int32 NodeId = -1;

	size_t Handle = 0;

	FIntRect ChangedExtent = INVALID_LANDSCAPE_EXTENT;  // Absolute extent, if ChangedExtent is valid, means we should upload partially

	bool HapiUpload(FLandscapeEditDataInterface& LandscapeEdit,
		const FName& EditLayerName, ULandscapeLayerInfoObject* LayerInfo,  // LayerInfo == nullptr, means we should import height
		const float& ZScale, const FIntRect& LandscapeExtent,
		const int32& GeoNodeId);

	FORCEINLINE bool NeedReimport() const { return ((NodeId < 0) || (ChangedExtent != INVALID_LANDSCAPE_EXTENT)); }

	bool HapiDestroy();

	void Invalidate();
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniEditLayerImportInfo
{
	GENERATED_BODY()

	int32 MergeNodeId = -1;

	UPROPERTY()
	TMap<FName, FHoudiniLayerImportInfo> LayerImportInfoMap;

	bool HapiDestroy();

	void Invalidate();
};

UCLASS()
class HOUDINIENGINE_API UHoudiniInputLandscape : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName LandscapeName;

	mutable TWeakObjectPtr<ALandscape> Landscape;

	FIntRect ImportedExtent = INVALID_LANDSCAPE_EXTENT;

	UPROPERTY()
	TMap<FName, FHoudiniEditLayerImportInfo> EditLayerImportInfoMap;  // Hierarchy can only modified in HapiUpload()

	int32 MergeNodeId = -1;  // Merge all EditLayers, Could be -1 if EditLayerImportInfoMap.IsEmpty(), which means we just need a reference volume to represent landscape

	int32 SettingsNodeId = -1;  // Always be "he_setup_heightfield_input" node, and represent landscape when MergeNodeId = -1


	int32 SplinesNodeId = -1;

	size_t SplinesHandle = 0;

	void InvalidateSplinesInput();

	bool HapiDestroySplinesInput();

public:
	static ALandscape* TryCast(AActor* InLandscape);  // If is ALandscapeProxy, then will return the parent ALandscape of it

	static bool IsLandscape(const AActor* Actor);

	static UHoudiniInputLandscape* Create(UHoudiniInput* Input, ALandscape* Landscape);

	FORCEINLINE const FName& GetLandscapeName() const { return LandscapeName; }

	void SetLandscape(ALandscape* NewLandscape);

	ALandscape* GetLandscape() const;

	bool HasLayerImported(const FName& EditLayerName, const FName& LayerName) const;

	bool NotifyLayerChanged(const FName& EditLayerName, const FName& LayerName, const FIntRect& ChangedExtent);  // Return layer found


	virtual TSoftObjectPtr<UObject> GetObject() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;
};


USTRUCT()
struct HOUDINIENGINE_API FHoudiniMaskTile
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UTexture2D> Texture;

	mutable uint8* Data = nullptr;

public:
	void Init(UObject* Outer, const int32& TileIdX, const int32& TileIdY,
		const int32& TileSize, const uint8* TileData);

	FORCEINLINE uint8* GetData() const { if (!Data) Data = Texture->Source.LockMip(0); return Data; }

	FORCEINLINE void Commit() const { if (Data) { Texture->Source.UnlockMip(0); Data = nullptr; } }

	FORCEINLINE void Modify() const { Texture->Modify(); }

	FORCEINLINE const uint8* GetConstData() const { return Data ? Data : Texture->Source.LockMipReadOnly(0); }

	FORCEINLINE bool HasMutableData() const { return Data != nullptr; }

	FORCEINLINE void ForceCommit() const { Data = nullptr; Texture->Source.UnlockMip(0); }
};

UCLASS()
class HOUDINIENGINE_API UHoudiniInputMask : public UHoudiniInputHolder
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FName LandscapeName;

	mutable TWeakObjectPtr<ALandscape> Landscape;

	UPROPERTY()
	TMap<FIntVector2, FHoudiniMaskTile> Tiles;

	// -------- Only for byte mask --------
	UPROPERTY()
	TArray<uint8> ByteValues;  // Num() == ByteColors.Num()

	UPROPERTY()
	TArray<FColor> ByteColors;  // Num() == ByteValues.Num()

	// -------- HAPI Session data --------
	int32 SettingsNodeId = -1;  // Always be "he_setup_heightfield_input" node, and represent landscape when MaskNodeId = -1

	int32 MaskNodeId = -1;  // Could be -1 if Data.IsEmpty(), which means we just need a reference volume to represent landscape

	size_t MaskHandle = 0;

	FIntRect ChangedExtent = INVALID_LANDSCAPE_EXTENT;  // Absolute extent

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHoudiniInputMaskChangedDelegate, const bool&)  // Is pending destroy
	FOnHoudiniInputMaskChangedDelegate OnChangedDelegate;

	static UHoudiniInputMask* Create(UHoudiniInput* Input, ALandscape* Landscape);

	FORCEINLINE const FName& GetLandscapeName() const { return LandscapeName; }

	ALandscape* GetLandscape() const;

	void SetNewLandscape(ALandscape* Landscape);


	FORCEINLINE const EHoudiniMaskType& GetMaskType() const { return GetSettings().MaskType; }

	FORCEINLINE bool HasData() const { return !Tiles.IsEmpty(); }

	// -------- Brush --------
	FORCEINLINE void ModifyAll() { Modify(); for (const auto& Tile : Tiles) Tile.Value.Modify(); }  // When Brush Begin

	FIntRect UpdateData(const FVector& BrushPosition, const float& BrushSize, const float& BrushFallOff,
		const uint8& Value, const bool& bInversed);  // MUST CommitData() later. Return changed absolute extent, return invalid if not changed

	// Return LandscapeExtent if INVALID_LANDSCAPE_EXTENT, else return local extent of Extent
	FIntRect GetColorData(TArray<FColor>& OutColorData, const FIntRect& Extent = INVALID_LANDSCAPE_EXTENT) const;  // Extent is absolute extent

	FIntRect UpdateData(const FVector& BrushPosition, const float& BrushSize, const float& BrushFallOff,
		const uint8& Value, const bool& bInversed, TArray<FColor>& OutColorData);  // MUST CommitData() later. Return changed local extent

	FIntRect UpdateData(TConstArrayView<uint8> Data, const FIntRect& Extent);  // MUST CommitData() later

	FORCEINLINE void CommitData() { for (const auto& Tile : Tiles) { Tile.Value.Commit(); } }  // When Brush End

	FORCEINLINE bool HasDataChanged() const { return ChangedExtent != INVALID_LANDSCAPE_EXTENT; }

	// -------- Only for byte mask --------
	FORCEINLINE int32 NumByteLayers() const { return ByteValues.Num(); }

	FORCEINLINE const uint8& GetByteValue(const int32& Index) const { return ByteValues[Index]; }

	FORCEINLINE const FColor& GetByteColor(const int32& Index) const { return ByteColors[Index]; }

	FORCEINLINE void SetByteColor(const int32& Index, const FColor& NewColor) { ByteColors[Index] = NewColor; }

	FORCEINLINE int32 FindByteLayerIndex(const uint8& ByteValue) const { return ByteValues.IndexOfByKey(ByteValue); }

	FIntRect GetValueExtent(const uint8& Value) const;  // Return absolute extent, should manually call CommitData() after

	
	bool HapiUploadData();  // Where actually upload mask data and set/get byte mask parm value


#if WITH_EDITOR
	virtual void PostEditUndo() override
	{
		Super::PostEditUndo();

		if (OnChangedDelegate.IsBound()) OnChangedDelegate.Broadcast(false);
		RequestReimport();
	}
#endif

	virtual TSoftObjectPtr<UObject> GetObject() const override;

	virtual bool HapiUpload() override;

	virtual bool HapiDestroy() override;

	virtual void Invalidate() override;

	virtual void Destroy() override
	{
		Invalidate();

		if (OnChangedDelegate.IsBound())
		{
			OnChangedDelegate.Broadcast(true);  // Broadcast this mask input is pending destroy
			OnChangedDelegate.Clear();
		}
	}
};
