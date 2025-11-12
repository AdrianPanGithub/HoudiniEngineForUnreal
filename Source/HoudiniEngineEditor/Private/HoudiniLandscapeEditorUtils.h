// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class FEdModeLandscape;
class UHoudiniInputLandscape;

struct HOUDINIENGINEEDITOR_API FHoudiniLandscapeEditorUtils
{
	friend class FHoudiniEngineEditor;

protected:
	static FName BrushingLandscapeName;  // If is NAME_None, then will NOT check brush update
	
	static FName BrushingEditLayerName;

	static TArray<FName> BrushingLayerNames;

	static TArray<UHoudiniInputLandscape*> BrushingLandscapeInputs;

	static void Reset();

	static void OnStartBrush(const FEdModeLandscape* LandscapeEd);

	static void OnBrushing(const FEdModeLandscape* LandscapeEd);

	static void OnEndBrush();
};
