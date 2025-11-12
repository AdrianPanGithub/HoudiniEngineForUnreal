// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"


class FHoudiniEngineModeToolkit : public FModeToolkit
{
public:
	/** FModeToolkit interface */
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
};
