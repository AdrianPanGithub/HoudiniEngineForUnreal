// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Tools/UEdMode.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"

#include "HoudiniEngineEditorMode.generated.h"


UCLASS()
class UHoudiniEngineEditorMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_HoudiniEngineEditorModeId;

	static FString ManageToolName;
	static FString MaskToolName;
	static FString EditToolName;

	UHoudiniEngineEditorMode();

	/** UEdMode interface */
	virtual void Enter() override;
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
};
