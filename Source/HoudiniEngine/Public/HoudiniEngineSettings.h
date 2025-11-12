// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HoudiniEngineSettings.generated.h"


UENUM()
enum class EHoudiniSessionType : uint8
{
	NamedPipe = 0,
	SharedMemory
};

UCLASS(config = HoudiniEngine, defaultconfig)
class HOUDINIENGINE_API UHoudiniEngineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("HoudiniEngine"); }

	virtual FText GetSectionText() const override { return NSLOCTEXT("HoudiniEngine", "SettingsName", "Houdini Engine"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("HoudiniEngine", "SettingsDescription", "Configure the Houdini Engine plugin"); }

	UPROPERTY(config, EditAnywhere)
	FDirectoryPath CustomHoudiniLocation;

	UPROPERTY(config, EditAnywhere)
	EHoudiniSessionType SessionType = EHoudiniSessionType::NamedPipe;

	UPROPERTY(config, EditAnywhere, meta = (Units = "MB", EditCondition = "SessionType == EHoudiniSessionType::SharedMemory", ToolTip = "Only when Shared Memory Session Type. For reference, a 8k resolution landscape need at least 256 MB"))
	uint16 SharedMemoryBufferSize = 512;

	UPROPERTY(config, EditAnywhere, meta = (ToolTip = "Display detailed progress while cooking and instantiating, but will have a longer cook time"))
	bool bVerbose = false;

	UPROPERTY(config, EditAnywhere)
	FString HoudiniEngineFolder = TEXT("/Game/HoudiniEngine/");

	UPROPERTY(config, EditAnyWhere, meta = (InlineEditConditionToggle))
	bool bLimitFPSWhileCooking = true;

	UPROPERTY(config, EditAnyWhere, DisplayName = "Max FPS While Cooking", meta = (EditCondition = "bLimitFPSWhileCooking", ToolTip = "Save computing resources for Houdini"))
	float MaxFPSWhileCooking = 12.0f;

	UPROPERTY(config, EditAnyWhere, meta = (ToolTip = "(Global) Automatically trigger node cook after input changed"))
	bool bCookOnInputChanged = true;

	UPROPERTY(config, EditAnyWhere, meta = (ToolTip = "(Global) Defer Mesh Distance Field generation while editing"))
	bool bDeferMeshDistanceFieldGeneration = true;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
