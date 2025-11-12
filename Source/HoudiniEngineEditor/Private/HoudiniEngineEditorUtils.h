// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class AHoudiniNode;
class UHoudiniEditableGeometry;
class IHoudiniPresetHandler;
class FLevelEditorViewportClient;
class FEditorViewportClient;

struct HOUDINIENGINEEDITOR_API FHoudiniEngineEditorUtils
{
	static FEditorViewportClient* FindEditorViewportClient(const UWorld* CurrWorld);

	static void DisableOverrideEngineShowFlags();

	static void ReselectSelectedActors();

	static void RefreshNodeEditor(const AHoudiniNode* Node);

	static TSharedPtr<SWidget> GetAttributePanel(UHoudiniEditableGeometry* EditGeo);
	
	static TSharedRef<SWidget> CreateTitleSeparatorWidget(const FText& Text);

	static TSharedRef<SWidget> MakePresetWidget(const TWeakInterfacePtr<IHoudiniPresetHandler> PresetHandler);

	static TSharedRef<SWidget> MakeCopyButton(const TWeakInterfacePtr<IHoudiniPresetHandler> PresetHandler);

	static TSharedRef<SWidget> MakePasteButton(const TWeakInterfacePtr<IHoudiniPresetHandler> PresetHandler);
};
