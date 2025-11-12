// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class SNotificationItem;
class FUICommandList;
class UHoudiniEditableGeometry;
class FHoudiniAssetTypeActions;
class FHoudiniCurvesComponentVisualizer;
class FHoudiniMeshComponentVisualizer;

class HOUDINIENGINEEDITOR_API FHoudiniEngineEditor : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FORCEINLINE static bool IsLoaded() { return HoudiniEngineEditorInstance != nullptr; }

	FORCEINLINE static FHoudiniEngineEditor& Get() { return *HoudiniEngineEditorInstance; }

protected:
	static FHoudiniEngineEditor* HoudiniEngineEditorInstance;

	// -------- Asset And Actor ----------
	FHoudiniAssetTypeActions* HoudiniAssetTypeActions = nullptr;

	void RegisterAssetTypeActions();
	void UnregisterAssetTypeActions();

	void RegisterActorFactories();
	void UnregisterActorFactories();

	// -------- Delegates --------
	FDelegateHandle OnMapOpenedHandle;

	FDelegateHandle HoudiniNodeEventsHandle;

	TWeakPtr<SNotificationItem> Notification;

	FDelegateHandle HoudiniAsyncTaskMessageHandle;

	FScopedSlowTask* ScopedSlowTask = nullptr;

	FDelegateHandle HoudiniMainTaskMessageHandle;

	FDelegateHandle OnEndPIEHandle;

	void RegisterEditorDelegates();
	void UnregisterEditorDelegates();


	FDelegateHandle OnLandscapeEditorEnterOrExistHandle;

	FDelegateHandle OnLandscapeBrushingHandle;

	void RegisterLandscapeEditorDelegate();
	void UnregisterLandscapeEditorDelegate();

	// -------- UI --------
	void RegisterDetails();
	void UnregisterDetails();


	TSharedPtr<FUICommandList> HoudiniEngineCommandList;

	TSharedPtr<FExtender> MainMenuExtender;

	FDelegateHandle LevelViewportExtenderHandle;

	void RegisterCommands();
	void UnregisterCommands();

	// -------- Component Visualizers --------
	TSharedPtr<FHoudiniCurvesComponentVisualizer> CurvesComponentVisualizer;

	TSharedPtr<FHoudiniMeshComponentVisualizer> MeshComponentVisualizer;

	void RegisterComponentVisualizers();
	void UnregisterComponentVisualizers();

public:
	FORCEINLINE const TSharedPtr<FHoudiniCurvesComponentVisualizer>& GetCurvesComponentVisualizer() { return CurvesComponentVisualizer; }
	FORCEINLINE const TSharedPtr<FHoudiniMeshComponentVisualizer>& GetMeshComponentVisualizer() { return MeshComponentVisualizer; }

	UHoudiniEditableGeometry* GetSelectedGeometry() const;  // Get current selected geometry from ComponentVisualizers
	UHoudiniEditableGeometry* GetEditingGeometry() const;  // Get current editing geometry from ComponentVisualizers
};
