// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HoudiniMaskGizmoActiveActor.generated.h"


class ALandscape;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class UHoudiniInputMask;


UCLASS(Transient)
class HOUDINIENGINEEDITOR_API AHoudiniMaskGizmoActiveActor : public AActor
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(NonTransactional)
	TObjectPtr<UDecalComponent> BrushDecalComponent;

	UPROPERTY(NonTransactional)
	TObjectPtr<UMaterialInstanceDynamic> BrushMaterialInstance;

	UPROPERTY(NonTransactional)
	TObjectPtr<UDecalComponent> MaskDecalComponent;

	UPROPERTY(NonTransactional)
	TObjectPtr<UMaterialInstanceDynamic> MaskMaterialInstance;

	UPROPERTY(NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> MaskColorRenderTarget;


	TWeakObjectPtr<UHoudiniInputMask> MaskInput;
	
	bool bVisualizeMask = true;
	float BrushSize = 10000.0f;  // Brush Radius
	float BrushValue = 1.0f;  // Only for weight mask
	float BrushFallOff = 0.5f;  // Only for weight mask
	uint8 ByteValue = 0;  // Only for byte mask


	void InitializeBrushDisplay();

	void DeactivateBrushDisplay();

	void DeactivateMaskDisplay() const;

	void UpdateMaskTransform(const FTransform& LandscapeTransform, const FIntRect& MaskExtent) const;


	void OnActorsStreamed(const TArray<AActor*>& LoadedActors);  // On Actors Loaded and Unloaded, If happen on ALandscapeStreamingProxy, we may need to refresh mask

public:
	AHoudiniMaskGizmoActiveActor();

	FORCEINLINE const TWeakObjectPtr<UHoudiniInputMask>& GetMaskInput() const { return MaskInput; }

	FORCEINLINE const bool& ShouldVisualizeMask() const { return bVisualizeMask; }
	FORCEINLINE void DisableMaskVisualization() { bVisualizeMask = false; DeactivateMaskDisplay(); }

	FORCEINLINE const float& GetBrushSize() const { return BrushSize; }
	void SetBrushSize(const float& InBrushSize);

	FORCEINLINE const float& GetBrushValue() const { return BrushValue; }
	void SetBrushValue(const float& InBrushValue);

	FORCEINLINE const float& GetBrushFallOff() const { return BrushFallOff; }
	void SetBrushFallOff(const float& InBrushFallOff);

	FORCEINLINE const uint8& GetByteValue() const { return ByteValue; }
	void OnTargetByteValueChanged(const int32& ByteLayerIdx);  // This method is a toggle, will activate inactive, and deactivate active

	void SetByteColor(const int32& Index, const FColor& NewColor);


	static AHoudiniMaskGizmoActiveActor* FindOrCreate(UWorld* World, UHoudiniInputMask* PendingMaskInput = nullptr);  // Will return nullptr if no MaskInput in this world
	
	void Enter(UHoudiniInputMask* PendingMaskInput);

	bool CanBrush() const;

	void UpdateBrushPosition(const FRay& MouseRay);

	void OnBrush(const bool& bBrushInversed);

	void EndBrush();

	void Exit();

	void OnMaskChanged(const bool& bIsPendingDestroy);

	void VisualizeMask();


	virtual bool CanDeleteSelectedActor(FText& OutReason) const override { return false; }  // Do NOT allow to delete this actor
};
