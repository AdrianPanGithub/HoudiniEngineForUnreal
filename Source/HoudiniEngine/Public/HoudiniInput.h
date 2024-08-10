// Copyright (c) <2024> Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEngineCommon.h"

#include "HoudiniInput.generated.h"


class UHoudiniParameterInput;
class AHoudiniNode;
class UHoudiniInputHolder;


UENUM()
enum class EHoudiniInputType
{
	Content = 0,  // StaticMesh, DataTable, Texture, Blueprint, FoliageType, maybe has null holder to be placeholders
	Curves,  // Only has one holder
	World,  // InputActors and InputLandscapes
	Node,  // Only has one holder
	Mask,  // Only has one holder
};


UENUM()
enum class EHoudiniActorFilterMethod
{
	Selection = 0,
	Class,
	Label,
	Tag,
	Folder
};

UENUM()
enum class EHoudiniPaintUpdateMethod
{
	Manual = 0,  // Import manually
	EnterPressed,  // "Enter" pressed or exit landscape edmode
	Brushed,  // While brushing and mouse released
	EveryCook  // While brushing, and when node cook triggered
};

UENUM()
enum class EHoudiniMaskType
{
	Bit = 0,
	Weight,
	Byte
};

UENUM()
enum class EHoudiniStaticMeshLODImportMethod
{
	FirstLOD = 0,
	LastLOD,
	AllLODs
};

UENUM()
enum class EHoudiniMeshCollisionImportMethod
{
	NoImportCollision = 0,
	ImportWithMesh,
	ImportWithoutMesh
};

USTRUCT()
struct HOUDINIENGINE_API FHoudiniInputSettings  // All of the settings could set by parm tags
{
	GENERATED_BODY()

	TArray<std::string> Tags;  // Parm Tags that could parse them in custom input holder or builder

	UPROPERTY()
	bool bImportAsReference = false;  // for Content, World

	UPROPERTY()
	bool bCheckChanged = true;  // for Content, Landscape

	UPROPERTY()
	TArray<FString> Filters;  // for Content, Node, and World

	UPROPERTY()
	TArray<FString> InvertedFilters;  // for Content, Node, and World

	UPROPERTY()
	TArray<FString> AllowClassNames;  // for Content, World

	UPROPERTY()
	TArray<FString> DisallowClassNames;  // for Content, World

	// -------- StaticMesh --------
	UPROPERTY()
	bool bImportRenderData = true;  // If nanite enabled, then RenderData is nanite fallback mesh

	UPROPERTY()
	EHoudiniStaticMeshLODImportMethod LODImportMethod = EHoudiniStaticMeshLODImportMethod::FirstLOD;

	UPROPERTY()
	EHoudiniMeshCollisionImportMethod CollisionImportMethod = EHoudiniMeshCollisionImportMethod::NoImportCollision;

	// -------- Curves --------
	UPROPERTY()
	bool bDefaultCurveClosed = false;

	UPROPERTY()
	EHoudiniCurveType DefaultCurveType = EHoudiniCurveType::Polygon;

	UPROPERTY()
	FColor DefaultCurveColor = FColor::White;

	UPROPERTY()
	bool bImportCollisionInfo = false;

	UPROPERTY()
	bool bImportRotAndScale = false;

	// -------- Actors --------
	UPROPERTY()
	EHoudiniActorFilterMethod ActorFilterMethod = EHoudiniActorFilterMethod::Selection;  // Filter by FHoudiniInputSettings::Filters

	// -------- Contents --------
	UPROPERTY()
	int32 NumHolders = 0;  // <= 0 means the num could be changed

	// -------- Landscape --------
	static const FName AllLandscapeLayerName;

	UPROPERTY()
	bool bImportLandscapeSplines = false;

	UPROPERTY()
	TMap<FName, FString> LandscapeLayerFilterMap;

	UPROPERTY()
	EHoudiniPaintUpdateMethod PaintUpdateMethod = EHoudiniPaintUpdateMethod::Manual;

	// -------- Mask ---------
	UPROPERTY()
	EHoudiniMaskType MaskType = EHoudiniMaskType::Bit;

	UPROPERTY()
	FString ByteMaskValueParmName;

	FORCEINLINE bool HasAssetFilters() const { return !Filters.IsEmpty() || !InvertedFilters.IsEmpty(); }

	bool FilterString(const FString& TargetStr) const;  // Return true if filter in, false if filter out

	bool HapiParseFromParameterTags(UHoudiniInput* Input, const int32& NodeId, const int32 ParmId);
};


// Class to record node-inputs and operator-path-inputs, outer must be a AHouudiniNode
UCLASS()
class HOUDINIENGINE_API UHoudiniInput : public UObject
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	EHoudiniInputType Type = EHoudiniInputType::Content;

	UPROPERTY()
	int32 NodeInputIdx = -1;

	UPROPERTY()
	FString Name;  // Used for find corresponding before cook

	FString Value;  // Used for find corresponding after cook, only for parm-inputs

	int32 GeoNodeId = -1;  // Parent "Geo", all input nodes are inside this node, so we just need to destory this node when destroy input

	int32 MergeNodeId = -1;  // For Content and World input, the node we should connect, this is a merge node, we should NOT destroy this node manually

	int32 MergedNodeCount = 0;  // For Content and World input, used to connect to merge node precisely

	bool bPendingClear = false;

	UPROPERTY()
	FHoudiniInputSettings Settings;

	FORCEINLINE AHoudiniNode* GetNode() const { return (AHoudiniNode*)GetOuter(); }

	bool HapiClear();  // Will reset parm value, call after RequestClear()

	bool HasValidHolders() const;

	static EHoudiniInputType ParseTypeFromString(const FString& InputName, bool& bOutTypeSpecified);

public:
	UPROPERTY()
	TArray<TObjectPtr<UHoudiniInputHolder>> Holders;  // May be contains nullptr when input type is EHoudiniInputType::Content

	UPROPERTY()
	bool bExpandSettings = false;

	static void ForAll(TFunctionRef<void(UHoudiniInput*)> Fn);

	const int32& GetParentNodeId() const;  // Get parent node's NodeId

	FORCEINLINE const int32& GetGeoNodeId() const { return GeoNodeId; }

	FORCEINLINE bool IsParameter() const { return NodeInputIdx < 0; }

	FORCEINLINE const FString& GetInputName() const { return Name; }

	FORCEINLINE const FString& GetValue() const { return Value; }

	FORCEINLINE const EHoudiniInputType& GetType() const { return Type; }

	FORCEINLINE const FHoudiniInputSettings& GetSettings() const { return Settings; }
	
	void SetType(const EHoudiniInputType& InNewType);


	void UpdateNodeInput(const int32& InputIdx, const FString& InputName);

	bool HapiUpdateFromParameter(UHoudiniParameterInput* Parm, const bool& bUpdateTags);  // If is a new input, then will parse the parm's value into objects to input 

	bool HapiUpload();

	bool HapiConnectToMergeNode(const int32& NodeId);  // For Content and World input, otherwise, set merge node id

	bool HapiDisconnectFromMergeNode(const int32& NodeId);  // For Content and World input, will query node id one by one, will call NotifyMergedNodeDestroyed() if successs

	FORCEINLINE void NotifyMergedNodeDestroyed() { if (MergedNodeCount >= 1) --MergedNodeCount; }

	void RequestClear();  // Clear input out of cook process of is unsafe, since this will reset parameter value, so we just mark this input pending clear

	bool HapiRemoveHolder(const int32& HolderIdx);  // Just remove holder out of cook process is safe

	void Invalidate();  // Should be called without HapiDestroy()

	bool HapiDestroy();  // Will NOT set parm value to empty


	// -------- Type specified methods --------
	void OnAssetChanged(const UObject* Object) const;  // Will mark input holder changed, but will NOT request cook

	bool CanNodeInput(const AHoudiniNode* InputNode) const;  // Check downstreams contains this node, we should use this method to avoid a loop

	// -------- World Input Only --------
	bool HapiImportActors(const TArray<AActor*>& Actors);  // Only construct holders, will NOT really import them, and will call HapiDestroy

	bool HapiReimportActorsByFilter();  // Only construct holders and HapiDestroy useless holders

	void OnActorFilterChanged(AActor* Actor, const EHoudiniActorFilterMethod& ChangeType);

	// -------- World And Mask Input --------
	void OnActorChanged(const AActor* Actor);

	void OnActorDeleted(const FName& DeletedActorName);

	void OnActorAdded(AActor* NewActor);


	// -------- Modify Settings --------
	void SetImportAsReference(const bool& bImportAsReference);

	void SetCheckChanged(const bool& bCheckChanged);

	void SetFilterString(const FString& FilterStr);

	void SetImportRenderData(const bool& bImportRenderData);

	void SetLODImportMethod(const EHoudiniStaticMeshLODImportMethod& LODImportMethod);

	void SetCollisionImportMethod(const EHoudiniMeshCollisionImportMethod& CollisionImportMethod);

	void SetActorFilterMethod(const EHoudiniActorFilterMethod& ActorFilterMethod);

	void SetImportRotAndScale(const bool& bImportRotAndScale);

	void SetImportCollisionInfo(const bool& bImportCollisionInfo);

	void ReimportAllMeshInputs() const;

	void SetImportLandscapeSplines(const bool& bImportLandscapeSplines);

	void SetPaintUpdateMethod(const EHoudiniPaintUpdateMethod& PaintUpdateMethod);

	void SetMaskType(const EHoudiniMaskType& MaskType);

	void SetByteMaskValueParmName(const FString& ValueParmName);
};

#define FOREACH_HOUDINI_INPUT(ACTION) for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())\
{\
	if (Node.IsValid())\
	{\
		for (UHoudiniInput* Input : Node->GetInputs())\
		{\
			ACTION\
		}\
	}\
}\

#define FOREACH_HOUDINI_INPUT_CHECK_CHANGED(ACTION)  FOREACH_HOUDINI_INPUT(if (Input->GetSettings().bCheckChanged) { ACTION } )


// Base class to hold input objects, outer must be a UHouudiniInput
UCLASS(Abstract)
class HOUDINIENGINE_API UHoudiniInputHolder : public UObject
{
	GENERATED_BODY()
	
protected:
	bool bHasChanged = true;  // Will always be true if not uploaded yet(force mark as changed in UHoudiniInput::Invalidate)

	FORCEINLINE UHoudiniInput* GetInput() const { return (UHoudiniInput*)GetOuter(); }

	FORCEINLINE const int32& GetGeoNodeId() const { return GetInput()->GetGeoNodeId(); }  // All input nodes should create inside this "geo" node

	FORCEINLINE const FHoudiniInputSettings& GetSettings() const { return GetInput()->GetSettings(); }

public:
	FORCEINLINE void MarkChanged(const bool& bChanged) { bHasChanged = bChanged; }

	virtual TSoftObjectPtr<UObject> GetAsset() const { return nullptr; }

	virtual void RequestReimport();  // Will mark this holder changed, and notify parent AHoudiniNode to cook

	virtual bool HapiUpload() { return true; }

	virtual bool HapiDestroy() { return true; }  // Will destory nodes and all houdini session data, but will NOT destroy unreal data

	virtual void Invalidate() {}  // Will invalidate houdini session data, but will NOT destroy unreal data

	virtual void Destroy() { Invalidate(); }  // Will destroy unreal data, and invalidate houdini session data. When input destory, this method will call

	virtual bool HasChanged() const { return bHasChanged; }
};

class HOUDINIENGINE_API IHoudiniContentInputBuilder
{
public:
	virtual void AppendAllowClasses(TArray<const UClass*>& InOutAllowclasses) = 0;

	virtual UHoudiniInputHolder* CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder) = 0;

	template<typename TAssetClass, typename THoudiniInputClass>
	FORCEINLINE static UHoudiniInputHolder* CreateOrUpdateHolder(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
	{
		if (TAssetClass* TypedAsset = Cast<TAssetClass>(Asset))
		{
			THoudiniInputClass* Holder = Cast<THoudiniInputClass>(OldHolder);
			if (!IsValid(Holder))
				Holder = NewObject<THoudiniInputClass>(Input, NAME_None, RF_Public | RF_Transactional);
			Holder->SetAsset(TypedAsset);
			return Holder;
		}
		return nullptr;
	}

	virtual bool GetInfo(const UObject* Asset, FString& OutInfoStr) { return false; }  // For string parameter with asset info, or when saving preset
};


struct HOUDINIENGINE_API FHoudiniComponentInputPoint
{
	FTransform Transform;

	std::string AssetRef;

	std::string MetaData;  // Should in json format

	bool bHasBeenCopied = false;
};

class HOUDINIENGINE_API FHoudiniComponentInput
{
public:
	virtual void Invalidate() const = 0;  // Will then delete this, so we need NOT to reset node ids to -1

	virtual bool HapiDestroy(UHoudiniInput* Input) const = 0;  // Will then delete this, so we need NOT to reset node ids to -1
};

class HOUDINIENGINE_API IHoudiniComponentInputBuilder
{
public:
	virtual bool IsValidInput(const UActorComponent* Component) = 0;

	virtual bool HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,  // Is there only one single valid component in the whole blueprint/actor
		const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,  // Components and Transforms are all of the components in blueprint/actor, and ComponentIndices are ref the valid indices from IsValidInput
		int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints) = 0;

	virtual void AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,  // See the comment upon
		const TSharedPtr<FJsonObject>& JsonObject) = 0;  // Append object info to JsonObject, keys are instance refs, values are JsonObjects that contain transoforms and meta data
};
