// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEngineCommon.h"

#include "HoudiniNode.generated.h"


struct HAPI_GeoInfo;
struct HAPI_PartInfo;
class UHoudiniParameter;
class UHoudiniInput;
class UHoudiniOutput;
class UHoudiniAsset;
class AHoudiniNode;
class UHoudiniEditableGeometry;


UENUM()
enum class EHoudiniNodeEvent : uint8
{
	RefreshEditorOnly,  // e.g. Input type switched but not trigger cook, then will trigger this event
	StartCook,
	FinishInstantiate,
	FinishCook,
	Destroy
};


// These parameters won't record in AHoudiniNode::Parms
USTRUCT()
struct FHoudiniAttributeParameterSet
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameter>> PointAttribParms;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameter>> PrimAttribParms;

	UPROPERTY()
	EHoudiniEditOptions PointEditOptions = EHoudiniEditOptions::None;

	UPROPERTY()
	EHoudiniEditOptions PrimEditOptions = EHoudiniEditOptions::None;

	UPROPERTY()
	FString PointIdentifierName;

	UPROPERTY()
	FString PrimIdentifierName;
};


USTRUCT(BlueprintType)
struct HOUDINIENGINE_API FHoudiniTopNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HoudiniTopNode")
	FString Path;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniOutput>> Outputs;

	int32 NodeId = -1;

	int32 NumErrorWorkItems = 0, NumCompletedWorkItems = 0, NumRunningWorkItems = 0, NumWaitingWorkItems = 0;

	enum class EPDGTaskType
	{
		None = -1,
		PendingCook,
		Cooking,
		Pause,
		Cancel
	};

	mutable EPDGTaskType Task = EPDGTaskType::None;

	FORCEINLINE void ResetStates() { NumErrorWorkItems = 0; NumCompletedWorkItems = 0; NumRunningWorkItems = 0; NumWaitingWorkItems = 0; }

	FORCEINLINE bool NeedCook() const { return Task == EPDGTaskType::PendingCook; }

	bool IsOutput() const;

	bool HapiCook(AHoudiniNode* Node);

	bool HapiUpdateOutputs(AHoudiniNode* Node, const FString& PDGOutputIdentifier, const std::string& FilePath, int32& InOutFileInputNodeId);

	bool HapiDirty() const;

	bool HapiDirtyAll() const;

	void Invalidate();  // Just reset NodeId and Task, will NOT destroy and empty outputs

	void Destroy();  // Just destroy and empty outputs, will NOT reset node id
};


UCLASS()
class HOUDINIENGINE_API AHoudiniNode : public AActor, public IHoudiniPresetHandler
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UHoudiniAsset> Asset;

	UPROPERTY()
	FString SelectedOpName;  // As one .hda file may contain multiple ops.

	UPROPERTY()
	TArray<FString> AvailableOpNames;  // As one .hda file may contain multiple ops.

	// -------- Get by HAPI_CreateNode --------
	int32 NodeId = -1;

	// -------- From HAPI_AssetInfo --------
	UPROPERTY()
	FString Label;

	UPROPERTY()
	FString HelpText;

	UPROPERTY()
	FString HelpURL;

	// -------- From HAPI_NodeInfo --------
	int32 GeoNodeId = -1;  // When the asset is a sop, then GeoNodeId is the parent node id of NodeId, otherwise, -1

	int32 InputCount = 0;

	// -------- For editable outputs and delta-infos --------
	UPROPERTY()
	bool bOutputEditable = false;

	int32 DeltaInfoNodeId = -1;

	int32 MergeNodeId = -1;

	TArray<TPair<int32, size_t>> EditGeoNodeIdHandles;  // <NodeId, Handle>, for editgeos' feedback

	FString DeltaInfo;

	UPROPERTY()
	FString ReDeltaInfo;  // For rebuild or on just instantiated

	// -------- Preset --------
	UPROPERTY()
	FString PresetName;  // Record the last loaded PresetName, and used for save preset

	TSharedPtr<const TMap<FName, FHoudiniGenericParameter>> Preset;  // If is valid, then means we need to set node parameters based on this preset

	// -------- Cook settings --------
	bool bRebuildBeforeCook = true;  // Will be true if manual rebuild or before instantiate, thus we should NOT check this when ticking

	enum class EHoudiniNodeRequestCookMethod
	{
		None = -1,
		Changed,
		Force  // Ignore bCookOnParameterChanged, force to trigger HapiCook
	};

	EHoudiniNodeRequestCookMethod RequestCookMethod = EHoudiniNodeRequestCookMethod::None;

	mutable FTransform LastTransform = FTransform::Identity;

	UPROPERTY()
	bool bCookOnParameterChanged = true;

	UPROPERTY()
	bool bCookOnUpstreamChanged = true;


	UPROPERTY()
	TArray<TObjectPtr<UHoudiniParameter>> Parms;

	UPROPERTY()
	TMap<FString, FHoudiniAttributeParameterSet> GroupParmSetMap;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniInput>> Inputs;

	UPROPERTY()
	TArray<TObjectPtr<UHoudiniOutput>> Outputs;

	UPROPERTY(BlueprintReadOnly, Category = "HoudiniNode")
	TArray<FHoudiniTopNode> TopNodes;

	UPROPERTY()
	TMap<FString, FHoudiniActorHolder> SplitActorMap;

	UPROPERTY()
	TMap<FString, FHoudiniActorHolder> SplitEditableActorMap;  // For edit-curve and edit-mesh


	bool NeedInstantiate();

	bool HapiSyncAttributeMultiParameters(const bool& bUpdateDefaultCount) const;

	bool HapiCookUpstream(bool& bOutHasUpstreamCooking) const;
	
	bool HapiUpdatePDG();


	bool HapiInstantiate();  // INSTANTIATE

	bool HapiUploadInputsAndParameters();  // Upload all inputs and parameter values, can be based on preset if is valid

	bool HapiUploadEditableOutputs();


	bool HapiCook(  // COOK
		TArray<FString>& OutGeoNames, TArray<HAPI_GeoInfo>& OutGeoInfos, TArray<TArray<HAPI_PartInfo>>& OutGeoPartInfos);

	bool HasTopNodePendingCook() const;

	void AsyncCookPDG();


	bool HapiUpdateParameters(const bool& bBeforeCook);  // Will update node info, and sync MultiParms before cook or loading preset

	bool HapiUpdateInputs(const bool& bBeforeCook, bool& bOutHasNewInputsPendingUpload);

	bool HapiUpdateOutputs(  // Receive the geos Retrieved from HapiCookNodes
		const TArray<FString>& GeoNames, const TArray<HAPI_GeoInfo>& GeoInfos, const TArray<TArray<HAPI_PartInfo>>& GeoPartInfos);


	void NotifyDownstreamCookFinish();

	void CleanupSplitActors();

	void FinishCook();


	void InvalidateEditableGeometryFeedback();  // Will reset merge and DeltaInfo node ids, clear GroupEditableNodeIdHandleMap and close shm handles


	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHoudiniNodeEvents, const EHoudiniNodeEvent, InNodeEvent);

	UPROPERTY(BlueprintAssignable, Transient, DuplicateTransient, NonTransactional, Category = "HoudiniNode")
	FHoudiniNodeEvents HoudiniNodeEvents;  // For Blueprint Only. please use FHoudiniEngine::HoudiniNodeEvents in C++

public:
	FORCEINLINE UHoudiniAsset* GetAsset() const { return Asset; }

	FORCEINLINE const TArray<FString>& GetAvailableOpNames() const { return AvailableOpNames; }

	FORCEINLINE const FString& GetOpName() const { return SelectedOpName; }

	FORCEINLINE void SelectOpName(const FString& InOpName) { SelectedOpName = InOpName; RequestRebuild(); }

	FORCEINLINE const int32& GetNodeId() const { return NodeId; }

	FORCEINLINE const FString& GetHelpText() const { return HelpText; }

	FORCEINLINE const TArray<UHoudiniParameter*>& GetParameters() const { return Parms; }

	FORCEINLINE const TMap<FString, FHoudiniAttributeParameterSet>& GetGroupParameterSetMap() const { return GroupParmSetMap; }

	FORCEINLINE const TArray<UHoudiniInput*>& GetInputs() const { return Inputs; }

	FORCEINLINE const TArray<UHoudiniOutput*>& GetOutputs() const { return Outputs; }

	FORCEINLINE const TArray<FHoudiniTopNode>& GetTopNodes() const { return TopNodes; }

	FORCEINLINE bool NeedCook() const { return RequestCookMethod != EHoudiniNodeRequestCookMethod::None; }

	FORCEINLINE const bool& CookOnParameterChanged() const { return bCookOnParameterChanged; }

	FORCEINLINE const bool& CookOnUpstreamChanged() const { return bCookOnUpstreamChanged; }

	FORCEINLINE bool DeferredCook() const { return !bCookOnParameterChanged || !bCookOnUpstreamChanged; }

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	void SetCookOnParameterChanged(const bool bInCookOnParameterChanged);  // May broadcast refresh editor event if DeferredCook() changed

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	void SetCookOnUpstreamChanged(const bool bInCookOnUpstreamChanged);  // May broadcast refresh editor event if DeferredCook() changed

	FORCEINLINE FString GetIdentifier() const { return FString::Printf(TEXT("%08X"), GetTypeHash(ActorGuid)); }  // As cook folder

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (ToolTip = "Where assets generated by this node will be placed in. Endswith '/'"))
	FString GetCookFolderPath() const;  // EndsWith '/'

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (ToolTip = "Where assets baked by this node will be placed in. Endswith '/'"))
	FString GetBakeFolderPath() const;  // EndsWith '/'

	// ------- IHoudiniPresetHandler --------
	virtual bool GetGenericParameters(TMap<FName, FHoudiniGenericParameter>& OutParms) const override;

	virtual void SetGenericParameters(const TSharedPtr<const TMap<FName, FHoudiniGenericParameter>>& InPreset) override { Preset = InPreset; RequestCook(); }  // We should actually load preset in other place, because we may need to instantiate node first

	virtual FString GetPresetPathFilter() const override;  // NOT EndsWith(TEXT("/"))

	virtual const FString& GetPresetName() const override { return PresetName; }

	virtual void SetPresetName(const FString& NewPresetName) override { PresetName = NewPresetName; }

	// -------- Trigger Cook --------
	FORCEINLINE void RequestCook() { RequestCookMethod = EHoudiniNodeRequestCookMethod::Changed; }  // Should call by TriggerCookBy(*) methods in most situation, as they will update DeltaInfos

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (ToolTip = "Cooking is async, so please bind your post cook functions to HoudiniNodeEvents"))
	FORCEINLINE void ForceCook() { RequestCookMethod = EHoudiniNodeRequestCookMethod::Force; }

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	FORCEINLINE void RequestRebuild() { bRebuildBeforeCook = true; RequestCookMethod = EHoudiniNodeRequestCookMethod::Changed; }

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	FORCEINLINE void RequestPDGCook(const int32 TopNodeIdx) { if (TopNodes.IsValidIndex(TopNodeIdx)) { TopNodes[TopNodeIdx].Task = FHoudiniTopNode::EPDGTaskType::PendingCook; ForceCook(); } }

	void TriggerCookByParameter(const UHoudiniParameter* ChangedParm);

	void TriggerCookByInput(const UHoudiniInput* ChangedInput);

	void TriggerCookByEditableGeometry(const UHoudiniEditableGeometry* ChangedEditGeo);

	void AppendDeltaInfos(const FString& NewDeltaInfo, const FString& NewReDeltaInfo);

	bool NeedWaitUpstreamCookFinish() const;

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	void Initialize(UHoudiniAsset* HoudiniAsset);  // Will call RequestRebuild()

	static AHoudiniNode* Create(UWorld* World, UHoudiniAsset* Asset, const bool bTemporary = false);


	void Invalidate();

	bool HapiAsyncProcess(const bool& bAfterInstantiating);  // Do NOT call this, Should only trigger by FHoudiniEngine::Tick

	bool HapiDestroy();

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", BlueprintPure = false, meta = (ToolTip = "Recommend to use i@unreal_split_actors = 1, instead of Baking"))
	AActor* Bake() const;


	virtual void PostLoad() override;

	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	virtual void Destroyed() override;  // Call when only actor destroyed, if world destroyed, then we could only destroy this actor in FHoudiniEngine::OnWordDestroyedHandle

	virtual bool CanDeleteSelectedActor(FText& OutReason) const override;

	bool HapiOnTransformed() const;  // As many output actors cannot attach to node due to world partition, we need to apply node transform to these actor when transformed

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	void BroadcastEvent(const EHoudiniNodeEvent NodeEvent);

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (WorldContext = "WorldContextObject"))
	static AHoudiniNode* InstantiateHoudiniAsset(const UObject* WorldContextObject, UHoudiniAsset* HoudiniAsset, const bool bTemporary = false);

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	void LoadParameterPreset(const UDataTable* ParameterPreset) { LoadPreset(ParameterPreset); }

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	UHoudiniInput* GetInputByName(const FString& Name, const bool bIsOperatorPathParameter = true) const;

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode")
	FHoudiniGenericParameter GetParameterValue(const FString& ParameterName) const;

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (ToolTip = "Press Button by int; Set Multi-parm by string; Set Operator-Path by string. Will truly set parameters on next tick"))
	void SetParameterValues(const TMap<FName, FHoudiniGenericParameter>& Parameters);

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (ToolTip = "Should only call before destroy HoudiniNode if you want to retain the generated split actors"))
	void UnlinkSplitActors() { SplitActorMap.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "HoudiniNode", meta = (ToolTip = "Return (not linked), SplitActor == nullptr means unlink"))
	bool LinkSplitActor(const FString& SplitValue, AActor* SplitActor);
};

// Must attach to AHoudiniNode
UCLASS()
class HOUDINIENGINE_API AHoudiniNodeProxy : public AActor
{
	GENERATED_UCLASS_BODY()
	
public:
	FORCEINLINE AHoudiniNode* GetParentNode() const { return Cast<AHoudiniNode>(GetAttachParentActor()); }

	virtual bool EditorCanAttachTo(const AActor* InParent, FText& OutReason) const override { return false; }
};
