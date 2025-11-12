// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"


class FHoudiniEngineModeCommands : public TCommands<FHoudiniEngineModeCommands>
{
public:
	FHoudiniEngineModeCommands();

	virtual void RegisterCommands() override;
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	TSharedPtr<FUICommandInfo> ManageTool;
	TSharedPtr<FUICommandInfo> MaskTool;
	TSharedPtr<FUICommandInfo> EditTool;

protected:
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};
