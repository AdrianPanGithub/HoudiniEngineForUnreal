// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


#define DEFAULT_POINT_SIZE 12.0f
#define DEFAULT_SHOW_EDGE_LENGTH false
#define DEFAULT_SHOW_POINT_POSITION false
#define DEFAULT_EDGE_THICKNESS 1.5f
#define DEFAULT_DISTANCE_CULLING false
#define DEFAULT_CULL_DISTANCE 102400.0


struct HOUDINIENGINEEDITOR_API FHoudiniEditableGeometryVisualSettings
{
protected:
	static FHoudiniEditableGeometryVisualSettings* Settings;

public:
	float PointSize = DEFAULT_POINT_SIZE;

	float EdgeThickness = DEFAULT_EDGE_THICKNESS;

	bool bDistanceCulling = DEFAULT_DISTANCE_CULLING;

	float CullDistance = DEFAULT_CULL_DISTANCE;

	bool bShowEdgeLength = DEFAULT_SHOW_EDGE_LENGTH;

	bool bShowPointCoordinate = DEFAULT_SHOW_POINT_POSITION;

	static FHoudiniEditableGeometryVisualSettings* Load();

	static void Save();
};

class IAssetViewport;

struct HOUDINIENGINEEDITOR_API FHoudiniEditableGeometryAssetDrop
{
protected:
	static TWeakPtr<IAssetViewport> AssetDropViewport;

	static TWeakPtr<SOverlay> AssetDropOverlay;

public:
	static void Register();

	static void Unregister();
};

// Some engine functions will Deactivate editing comp vis, so we need a scope to recover editing state after actor reselect calls
struct HOUDINIENGINEEDITOR_API FHoudiniEditableGeometryEditingScope
{
protected:
	static bool bAllowResetSelection;

public:
	FORCEINLINE static const bool& AllowResetSelection() { return bAllowResetSelection; }  // Used when FComponentVisualizer::EndEditing, check whether should retain the selection

	FHoudiniEditableGeometryEditingScope();

	~FHoudiniEditableGeometryEditingScope();  // Reactivate the activated component visualizer
};

#define HOUDINI_EDIT_GEO_SELECTED_COLOR FLinearColor(1.0f, 0.2f, 0.1f)
