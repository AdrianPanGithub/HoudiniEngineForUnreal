// Copyright (c) <2024> Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "HAPI/HAPI_Common.h"


class AHoudiniNode;
class UHoudiniAsset;
class IHoudiniContentInputBuilder;
class IHoudiniComponentInputBuilder;
class IHoudiniOutputBuilder;


class HOUDINIENGINE_API FHoudiniEngine : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FORCEINLINE static bool IsLoaded() { return HoudiniEngineInstance != nullptr; }

	FORCEINLINE static FHoudiniEngine& Get() { return *HoudiniEngineInstance; }

protected:
	static FHoudiniEngine* HoudiniEngineInstance;

	FString LoadedHoudiniBinDir;

	void* LoadedHAPILibraryHandle = nullptr;  // Free this when Houdini install path changed, and reload the new HAPILib

	FTSTicker::FDelegateHandle TickerHandle;

	FDelegateHandle OnWorldDestroyedHandle;

	TArray<TSharedPtr<IHoudiniContentInputBuilder>> ContentInputBuilders;

	TArray<TSharedPtr<IHoudiniComponentInputBuilder>> ComponentInputBuilders;

	TArray<TSharedPtr<IHoudiniOutputBuilder>> OutputBuilders;

	// -------- Current session exclusively --------
	HAPI_Session Session = { HAPI_SESSION_MAX, -1 };

	bool bSessionSync = false;
	
	float SessionCheckDeltaTime = 0.0f;  // Check is session valid per second

	TArray<TWeakObjectPtr<const UHoudiniAsset>> LoadedAssets;  // Should empty when session loss or restart session

	uint32 NumWorkingTasks = 0;  // For async cook/instantiate, disable all editing when async processing

	float EngineMaxFPS = 0.0f;  // For slow down engine tick while cooking, the MaxFPS should be backup for recover

	bool bEngineFPSLimited = false;

	FDelegateHandle OnNodeMovedHandle;

	// -------- Listening actor input changed --------
	FDelegateHandle OnActorChangedHandle;

	FDelegateHandle OnActorFolderChangedHandle;

	FDelegateHandle OnActorDeletedHandle;

	FDelegateHandle OnActorAddedHandle;

	// -------- Current world exclusively --------
	TWeakObjectPtr<UWorld> CurrWorld;

	TArray<TWeakObjectPtr<AHoudiniNode>> CurrNodes;

	TMap<FName, TPair<TWeakObjectPtr<AActor>, bool>> CachedNameActorLoadedMap;

	void CacheNameActorMap();  // Cache when cook triggered, so we can call GetActorByName


	void RegisterIntrinsicBuilders();  // Will register input and output builders

	void RegisterWorldDestroyedDelegate();

	void UnregisterWorldDestroyedDelegate();

	void RegisterNodeMovedDelegate();

	void UnregisterNodeMovedDelegate();

	void RegisterActorInputDelegates();

	void UnregisterActorInputDelegates();

	bool HapiInitializeSession();

	bool Tick(float DeltaTime);  // Will Start cook process

	void StartTick();

	void StopTick();
	
	FORCEINLINE void ResetSession() { Session.type = HAPI_SESSION_MAX; Session.id = -1; }

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHoudiniNodeRegisteredEvent, AHoudiniNode*)
	FOnHoudiniNodeRegisteredEvent OnHoudiniNodeRegisteredEvent;

	DECLARE_MULTICAST_DELEGATE_OneParam(FHoudiniAsyncTaskMessageEvent, const FText&)  // (Message), Empty means start fade out
	FHoudiniAsyncTaskMessageEvent HoudiniAsyncTaskMessageEvent;  // Should Only broadcast in GameThread

	FORCEINLINE void FinishHoudiniAsyncTaskMessage() const { HoudiniAsyncTaskMessageEvent.Broadcast(FText::GetEmpty()); }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FHoudiniMainTaskMessageEvent, const float&, const FText&)  // (Progress, Message), if progress < 0.0f, means close
	FHoudiniMainTaskMessageEvent HoudiniMainTaskMessageEvent;  // Should Only broadcast in GameThread

	FORCEINLINE void FinishHoudiniMainTaskMessage() const { HoudiniMainTaskMessageEvent.Broadcast(-1.0f, FText::GetEmpty()); }

	// -------- Input/Output Builder --------
	// The register order of houdini engine itself: StaticMesh < DataTable < Blueprint < FoliageType_InstancedStaticMesh < Texture
	FORCEINLINE void RegisterInputBuilder(const TSharedPtr<IHoudiniContentInputBuilder>& Builder) { ContentInputBuilders.AddUnique(Builder); }

	FORCEINLINE void UnregisterInputBuilder(const TSharedPtr<IHoudiniContentInputBuilder>& Builder) { ContentInputBuilders.Remove(Builder); }

	FORCEINLINE const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& GetContentInputBuilders() const { return ContentInputBuilders; }

	// The register order of houdini engine itself: ActorComponent < MeshComponent < SplineComponent < BrushComponent
	FORCEINLINE void RegisterInputBuilder(const TSharedPtr<IHoudiniComponentInputBuilder>& Builder) { ComponentInputBuilders.AddUnique(Builder); }

	FORCEINLINE void UnregisterInputBuilder(const TSharedPtr<IHoudiniComponentInputBuilder>& Builder) { ComponentInputBuilders.Remove(Builder); }

	FORCEINLINE const TArray<TSharedPtr<IHoudiniComponentInputBuilder>>& GetComponentInputBuilders() const { return ComponentInputBuilders; }

	// The register order of houdini engine itself: Landscape < Instancer < Curve < Mesh < Texture < DataTable
	FORCEINLINE void RegisterOutputBuilder(const TSharedPtr<IHoudiniOutputBuilder>& Builder) { OutputBuilders.AddUnique(Builder); }

	FORCEINLINE void UnregisterOutputBuilder(const TSharedPtr<IHoudiniOutputBuilder>& Builder) { OutputBuilders.Remove(Builder); }

	FORCEINLINE const TArray<TSharedPtr<IHoudiniOutputBuilder>>& GetOutputBuilders() const { return OutputBuilders; }

	void RegisterNode(AHoudiniNode* Node);  // This is where we register our GEngine delegates, and get current world, and if world changed, then CacheNameActorMap again

	void UnregisterNode(const TWeakObjectPtr<AHoudiniNode>& Node) { CurrNodes.Remove(Node); }

	void RegisterWorld(UWorld* World);

	FORCEINLINE bool IsNodeRegistered(const TWeakObjectPtr<AHoudiniNode>& Node) const { return CurrNodes.Contains(Node); }

	// Should only be used in cook processes
	void RegisterActor(AActor* Actor);

	AActor* GetActorByName(const FName& ActorName, const bool& bForceLoad = true);

	void DestroyActorByName(const FName& ActorName);

	FORCEINLINE const HAPI_Session* GetSession() const { return &Session; }

	FORCEINLINE bool IsNullSession() const { return (Session.id == -1 || Session.type == HAPI_SESSION_MAX); }

	FORCEINLINE const bool& IsSessionSync() const { return bSessionSync; }

	bool IsSessionValid() const;

	bool HapiStartSession();

	bool HapiStartSessionSync();

	bool HapiStopSession() const;  // Return true if successfully closed, so always call InvalidateSessionData() after this

	bool HapiOpenSceneInHoudini();

	void InvalidateSessionData();

	bool AllowEdit() const;  // No Task is running, and HAPI has been initialized

	// Should only call in game thread
	FORCEINLINE void StartHoudiniTask() { ++NumWorkingTasks; }

	// Should only call in game thread
	FORCEINLINE void FinishHoudiniTask() { if (NumWorkingTasks >= 1) --NumWorkingTasks; }

	void LimitEngineFrameRate();  // Useful when async execute houdini tasks, spare more compute resources for session tasks

	void RecoverEngineFrameRate();

	FORCEINLINE void RegisterLoadedAsset(const TWeakObjectPtr<const UHoudiniAsset>& InHoudiniAsset) { LoadedAssets.AddUnique(InHoudiniAsset); }

	FORCEINLINE bool IsAssetLoaded(const TWeakObjectPtr<const UHoudiniAsset>& InHoudiniAsset) { return LoadedAssets.Contains(InHoudiniAsset); }

	FORCEINLINE const TArray<TWeakObjectPtr<AHoudiniNode>>& GetCurrentNodes() { return CurrNodes; }

	static const FString& GetProcessIdentifier();  // For houdini named-pipe session create and shared memory data transport

	static const FString& GetPluginDir();

	FORCEINLINE const FString& GetHoudiniBinDir() const { return LoadedHoudiniBinDir; }

	static void PreStartSession();

	void InitializeHAPI();  // Will refresh LoadedHoudiniBinDir
};
