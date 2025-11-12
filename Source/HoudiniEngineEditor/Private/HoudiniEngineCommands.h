// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"


class HOUDINIENGINEEDITOR_API FHoudiniEngineCommands : public TCommands<FHoudiniEngineCommands>
{
public:
	FHoudiniEngineCommands();

	// TCommand<> interface
	virtual void RegisterCommands() override;

	// Menu
	TSharedPtr<FUICommandInfo> RestartSession;

	TSharedPtr<FUICommandInfo> OpenSessionSync;

	TSharedPtr<FUICommandInfo> StopSession;

	TSharedPtr<FUICommandInfo> SaveHoudiniScene;

	TSharedPtr<FUICommandInfo> OpenSceneInHoudini;

	TSharedPtr<FUICommandInfo> CleanUpCookFolder;

	TSharedPtr<FUICommandInfo> Settings;

	TSharedPtr<FUICommandInfo> About;

	// Shortcut
	TSharedPtr<FUICommandInfo> IncreasePointSize;

	TSharedPtr<FUICommandInfo> DecreasePointSize;

	TSharedPtr<FUICommandInfo> TogglePointCoordinateShown;

	TSharedPtr<FUICommandInfo> ToggleEdgeLengthShown;

	TSharedPtr<FUICommandInfo> ToggleDistanceCulling;

	TSharedPtr<FUICommandInfo> IncreaseCullDistance;

	TSharedPtr<FUICommandInfo> DecreaseCullDistance;


	static void AsyncRestartSession();

	static void AsyncOpenSessionSync();

	static void SyncStopSession();

	static void AsyncOpenSceneInHoudini();

	static void AsyncSaveHoudiniScene();

	static void SyncCleanupCookFolder();

	static void NavigateToSettings();

	static void ShowInfo();
};
