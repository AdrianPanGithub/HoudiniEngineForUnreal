// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniMaskGizmoActiveActor.h"

#include "Landscape.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/DecalComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"

#include "HoudiniEngine.h"
#include "HoudiniNode.h"
#include "HoudiniInputs.h"

// Sets default values
AHoudiniMaskGizmoActiveActor::AHoudiniMaskGizmoActiveActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	SetIsSpatiallyLoaded(false);
	bIsEditorOnlyActor = true;
	bListedInSceneOutliner = false;
}

AHoudiniMaskGizmoActiveActor* AHoudiniMaskGizmoActiveActor::FindOrCreate(UWorld* World, UHoudiniInputMask* PendingMaskInput)
{
	AHoudiniMaskGizmoActiveActor* MaskActor = nullptr;
	for (TActorIterator<AHoudiniMaskGizmoActiveActor> MaskActorIter(World); MaskActorIter; ++MaskActorIter)
	{
		MaskActor = *MaskActorIter;
		if (IsValid(MaskActor))
			break;
		else
			MaskActor = nullptr;
	}

	if (MaskActor)
	{
		if (PendingMaskInput)  // If has PendingMaskInput, then we set current Mask Input to PendingMaskInput;
		{
			MaskActor->Enter(PendingMaskInput);
			return MaskActor;
		}
		else if (MaskActor->MaskInput.IsValid())  // If already has MaskInput, then parsing finished, just return
		{
			MaskActor->Enter(MaskActor->MaskInput.Get());
			return MaskActor;
		}
	}

	if (!PendingMaskInput)  // If no mask input held, then try to find a mask input in current level
	{
		for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())
		{
			for (UHoudiniInput* Input : Node->GetInputs())
			{
				if ((Input->GetType() == EHoudiniInputType::Mask) && Input->Holders.IsValidIndex(0))
				{
					PendingMaskInput = Cast<UHoudiniInputMask>(Input->Holders[0]);
					break;
				}
			}

			if (PendingMaskInput)
				break;
		}
	}

	if (PendingMaskInput && !MaskActor)
	{
		// Spawn a new gizmo actor
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.bTemporaryEditorActor = true;
		SpawnParameters.ObjectFlags |= RF_Transient;
		MaskActor = World->SpawnActor<AHoudiniMaskGizmoActiveActor>(SpawnParameters);
		MaskActor->MaskInput = PendingMaskInput;

		// Register actor loaded/unloaded event
		ULevel* CurrLevel = World->GetCurrentLevel();
		CurrLevel->OnLoadedActorAddedToLevelPostEvent.AddUObject(MaskActor, &AHoudiniMaskGizmoActiveActor::OnActorsStreamed);
		CurrLevel->OnLoadedActorRemovedFromLevelPostEvent.AddUObject(MaskActor, &AHoudiniMaskGizmoActiveActor::OnActorsStreamed);
	}

	if (MaskActor && MaskActor->MaskInput.IsValid())
		MaskActor->Enter(MaskActor->MaskInput.Get());

	return MaskActor;
}

void AHoudiniMaskGizmoActiveActor::OnActorsStreamed(const TArray<AActor*>& LoadedActors)
{
	if (!bVisualizeMask || !MaskDecalComponent || !MaskDecalComponent->IsVisible() || !MaskInput.IsValid())
		return;

	ALandscape* Landscape = MaskInput->GetLandscape();
	if (!IsValid(Landscape))
		return;

	if (LoadedActors.ContainsByPredicate([Landscape](const AActor* Actor)
		{
			if (const ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor))
			{
				if (LandscapeProxy->GetLandscapeActor() == Landscape)
					return true;
			}

			return false;
		}))
		VisualizeMask();
}

void AHoudiniMaskGizmoActiveActor::SetBrushSize(const float& InBrushSize)
{
	BrushSize = InBrushSize;
	if (BrushDecalComponent)
		BrushDecalComponent->SetWorldScale3D(FVector(BrushSize, BrushSize, BrushSize));
}

static const FName HoudiniBrushHardnessName("Hardness");
static const FHashedMaterialParameterInfo ColorInfo(FName("Color"));
static const FHashedMaterialParameterInfo HardnessInfo(HoudiniBrushHardnessName);

void AHoudiniMaskGizmoActiveActor::SetBrushValue(const float& InBrushValue)
{
	BrushValue = InBrushValue;
	if (BrushMaterialInstance && MaskInput.IsValid() &&
		(MaskInput->GetMaskType() == EHoudiniMaskType::Weight))
	{
		const FLinearColor CurrColor(1.0f, 0.0f, 0.0f, BrushValue);
		FLinearColor PrevColor;
		BrushMaterialInstance->GetVectorParameterValue(ColorInfo, PrevColor);
		if (PrevColor != CurrColor)
			BrushMaterialInstance->SetVectorParameterValue(NAME_Color, CurrColor);
	}
}

void AHoudiniMaskGizmoActiveActor::SetBrushFallOff(const float& InBrushFallOff)
{
	BrushFallOff = InBrushFallOff;
	if (BrushMaterialInstance && MaskInput.IsValid() &&
		(MaskInput->GetMaskType() == EHoudiniMaskType::Weight))
	{
		const float CurrHardness = 1.f - BrushFallOff;
		float PrevHardness;
		BrushMaterialInstance->GetScalarParameterValue(HardnessInfo, PrevHardness);
		if (FMath::Abs(PrevHardness - CurrHardness) > 0.001)
			BrushMaterialInstance->SetScalarParameterValue(HoudiniBrushHardnessName, CurrHardness);
	}
}

void AHoudiniMaskGizmoActiveActor::OnTargetByteValueChanged(const int32& ByteLayerIdx)
{
	if (MaskInput.IsValid() && (MaskInput->GetMaskType() == EHoudiniMaskType::Byte))
	{
		const uint8& TargetByteValue = MaskInput->GetByteValue(ByteLayerIdx);
		if (ByteValue == TargetByteValue)  // Deactivate this ByteValue Brush
		{
			ByteValue = 0;
			DeactivateBrushDisplay();
		}
		else
		{
			ByteValue = TargetByteValue;
			InitializeBrushDisplay();
		}
	}
}

void AHoudiniMaskGizmoActiveActor::SetByteColor(const int32& Index, const FColor& NewColor)
{
	MaskInput->SetByteColor(Index, NewColor);

	// Also update brush color if this ByteValue is painting
	if (BrushMaterialInstance && (ByteValue == MaskInput->GetByteValue(Index)))
	{
		const FLinearColor CurrColor(NewColor);
		FLinearColor PrevColor;
		BrushMaterialInstance->GetVectorParameterValue(ColorInfo, PrevColor);
		if (PrevColor != CurrColor)
			BrushMaterialInstance->SetVectorParameterValue(NAME_Color, CurrColor);
	}

	// if Mask is visualizing, then we should also update mask display
	if (MaskMaterialInstance && MaskColorRenderTarget && MaskDecalComponent && MaskDecalComponent->IsVisible())
	{
		const FIntRect ValueLocalExtent = MaskInput->GetValueExtent(MaskInput->GetByteValue(Index));
		if (ValueLocalExtent != INVALID_LANDSCAPE_EXTENT)
		{
			TArray<FColor> ChangedColorData;
			const FIntRect ChangedLocalExtent = MaskInput->GetColorData(ChangedColorData, ValueLocalExtent);

			const FColor* ColorDataPtr = ChangedColorData.GetData();

			FTextureRenderTargetResource* Resource = MaskColorRenderTarget->GameThread_GetRenderTargetResource();
			ENQUEUE_RENDER_COMMAND(UpdateHoudiniMaskColor)(
				[&](FRHICommandListImmediate& RHICmdList)
				{
					RHIUpdateTexture2D(Resource->GetTexture2DRHI(), 0, FUpdateTextureRegion2D(
						ChangedLocalExtent.Min.X, ChangedLocalExtent.Min.Y, 0, 0, ChangedLocalExtent.Width() + 1, ChangedLocalExtent.Height() + 1),
						(ChangedLocalExtent.Width() + 1) * sizeof(FColor), (const uint8*)ColorDataPtr);
				});

			FlushRenderingCommands();
		}
	}
}


void AHoudiniMaskGizmoActiveActor::Enter(UHoudiniInputMask* PendingMaskInput)
{
	if (MaskInput.IsValid())
		MaskInput->OnChangedDelegate.RemoveAll(this);  // Remove the previous binding

	MaskInput = PendingMaskInput;
	if (!MaskInput->OnChangedDelegate.IsBoundToObject(this))
		MaskInput->OnChangedDelegate.AddUObject(this, &AHoudiniMaskGizmoActiveActor::OnMaskChanged);

	if (CanBrush())
		InitializeBrushDisplay();

	if (bVisualizeMask)
		VisualizeMask();
}

void AHoudiniMaskGizmoActiveActor::OnMaskChanged(const bool& bIsPendingDestroy)
{
	if (bIsPendingDestroy)
	{
		if (MaskInput.IsValid())
			MaskInput->OnChangedDelegate.RemoveAll(this);
		MaskInput.Reset();
		DeactivateMaskDisplay();
		DeactivateBrushDisplay();
	}
	else
	{
		if (MaskInput.IsValid() && (MaskInput->GetMaskType() == EHoudiniMaskType::Byte) &&
			MaskInput->FindByteLayerIndex(ByteValue) < 0)  // Maybe this painting ByteValue has been removed, so brush decal need be deactivate
		{
			DeactivateBrushDisplay();
			ByteValue = 0;
		}
		else
			InitializeBrushDisplay();  // Force refresh brush display, as mask type may changed

		if (bVisualizeMask)  // Refresh mask visualization
			VisualizeMask();
	}
}


bool AHoudiniMaskGizmoActiveActor::CanBrush() const
{
	if (!MaskInput.IsValid())
		return false;

	if (!IsValid(MaskInput->GetLandscape()))
		return false;

	if ((MaskInput->GetMaskType() == EHoudiniMaskType::Byte) && (MaskInput->FindByteLayerIndex(ByteValue) < 0))
		return false;

	return true;
}

void AHoudiniMaskGizmoActiveActor::InitializeBrushDisplay()
{
	if (!IsValid(RootComponent))
	{
		RootComponent = NewObject<USceneComponent>(this, USceneComponent::GetDefaultSceneRootVariableName(), RF_Public | RF_Transactional);
		SetRootComponent(RootComponent);
		RootComponent->RegisterComponent();
	}

	if (!IsValid(BrushDecalComponent))
	{
		BrushDecalComponent = NewObject<UDecalComponent>(this, FName("BrushDecalComponent"), RF_Public | RF_Transactional);
		BrushDecalComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		BrushDecalComponent->RegisterComponent();
		AddInstanceComponent(BrushDecalComponent);

		BrushDecalComponent->DecalSize = FVector(2.0, 0.5, 0.5);
	}

	BrushDecalComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0).Quaternion());
	BrushDecalComponent->SetWorldScale3D(FVector(BrushSize, BrushSize, BrushSize));

	if (!IsValid(BrushMaterialInstance))
	{
		UMaterial* M_Brush = LoadObject<UMaterial>(nullptr,
			TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Materials/M_Brush.M_Brush'"));

		BrushMaterialInstance = UMaterialInstanceDynamic::Create(M_Brush, this);

		BrushDecalComponent->SetDecalMaterial(BrushMaterialInstance);
		
	}

	switch (MaskInput->GetMaskType())
	{
	case EHoudiniMaskType::Bit:
	{
		BrushMaterialInstance->SetVectorParameterValue(NAME_Color, FLinearColor::Red);
		BrushMaterialInstance->SetScalarParameterValue(HoudiniBrushHardnessName, 1.0f);
	}
	break;
	case EHoudiniMaskType::Weight:
	{
		BrushMaterialInstance->SetVectorParameterValue(NAME_Color, FLinearColor(1.0f, 0.0f, 0.0f, BrushValue));
		BrushMaterialInstance->SetScalarParameterValue(HoudiniBrushHardnessName, 1.f - BrushFallOff);
	}
	break;
	case EHoudiniMaskType::Byte:
	{
		const int32 FoundByteLayerIdx = (ByteValue == 0) ? -1 : MaskInput->FindByteLayerIndex(ByteValue);
		if (FoundByteLayerIdx < 0)  // Make sure that this ByteValue is exists
			ByteValue = 0;
		else
		{
			BrushMaterialInstance->SetVectorParameterValue(NAME_Color, MaskInput->GetByteColor(FoundByteLayerIdx));
			BrushMaterialInstance->SetScalarParameterValue(HoudiniBrushHardnessName, 1.f);
		}
	}
	break;
	}

	if (!BrushDecalComponent->IsVisible())
		BrushDecalComponent->SetVisibility(true);
}

void AHoudiniMaskGizmoActiveActor::UpdateBrushPosition(const FRay& MouseRay)
{
	if (!BrushDecalComponent || !BrushMaterialInstance || !BrushDecalComponent->IsVisible())  // Make Sure the Brush is valid
		return;

	// Only trace the landscape proxies
	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
	{
		const AActor* Actor = *ActorIter;
		if (IsValid(Actor)) if (!Actor->IsA<ALandscapeProxy>())
			CollisionQueryParams.AddIgnoredActor(Actor);
	}

	FHitResult Hit(ForceInit);
	if (GetWorld()->LineTraceSingleByChannel(Hit, MouseRay.Origin,
		MouseRay.Origin + MouseRay.Direction * 999999.0, ECC_Visibility, CollisionQueryParams))
		BrushDecalComponent->SetWorldTransform(FTransform(FRotator(90.0, 0.0, 0.0), Hit.Location, FVector(BrushSize, BrushSize, BrushSize)));
}

void AHoudiniMaskGizmoActiveActor::DeactivateBrushDisplay()
{
	if (IsValid(BrushDecalComponent) && BrushDecalComponent->IsVisible())
		BrushDecalComponent->SetVisibility(false);
}

void AHoudiniMaskGizmoActiveActor::VisualizeMask()
{
	if (!MaskInput.IsValid())
		return;

	const ALandscape* Landscape = MaskInput->GetLandscape();
	if (!IsValid(Landscape))
		return;

	bVisualizeMask = true;


	TArray<FColor> ColorData;
	const FIntRect LandscapeExtent = MaskInput->GetColorData(ColorData);
	if (LandscapeExtent == INVALID_LANDSCAPE_EXTENT)  // No LandscapeProxy loaded
		return;

	if (!IsValid(MaskColorRenderTarget))
	{
		MaskColorRenderTarget = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass(),
			TEXT("MaskColorRenderTarget"), RF_Public | RF_Transient);

		MaskColorRenderTarget->RenderTargetFormat = RTF_RGBA8;
		MaskColorRenderTarget->ClearColor = FLinearColor::White;
		MaskColorRenderTarget->bAutoGenerateMips = false;
		MaskColorRenderTarget->bCanCreateUAV = true;
	}

	const FIntVector2 Resolution(LandscapeExtent.Width() + 1, LandscapeExtent.Height() + 1);

	if ((MaskColorRenderTarget->SizeX != Resolution.X) || (MaskColorRenderTarget->SizeY != Resolution.Y))
	{
		MaskColorRenderTarget->InitAutoFormat(Resolution.X, Resolution.Y);
		MaskColorRenderTarget->UpdateResourceImmediate(true);
	}

	const FColor* ColorDataPtr = ColorData.GetData();
	FTextureRenderTargetResource* Resource = MaskColorRenderTarget->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(VisualizeMaskColor)(
		[Resource, Resolution, ColorDataPtr](FRHICommandListImmediate& RHICmdList)
		{
			RHIUpdateTexture2D(Resource->GetTexture2DRHI(), 0, FUpdateTextureRegion2D(0, 0, 0, 0, Resolution.X, Resolution.Y),
			Resolution.X * sizeof(FColor), (const uint8*)ColorDataPtr);
		});

	FlushRenderingCommands();
	

	if (!IsValid(RootComponent))
	{
		RootComponent = NewObject<USceneComponent>(this, USceneComponent::GetDefaultSceneRootVariableName(), RF_Public | RF_Transactional);
		SetRootComponent(RootComponent);
		RootComponent->RegisterComponent();
	}

	if (!IsValid(MaskDecalComponent))
	{
		MaskDecalComponent = NewObject<UDecalComponent>(this, FName("MaskDecalComponent"), RF_Public | RF_Transactional);
		MaskDecalComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		MaskDecalComponent->RegisterComponent();
		AddInstanceComponent(MaskDecalComponent);
	}

	if (!IsValid(MaskMaterialInstance))
	{
		UMaterial* M_TextureVisualization = LoadObject<UMaterial>(nullptr,
			TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Materials/M_TextureVisualization.M_TextureVisualization'"));

		MaskMaterialInstance = UMaterialInstanceDynamic::Create(M_TextureVisualization, this);
		MaskMaterialInstance->SetTextureParameterValue(FName("Texture"), MaskColorRenderTarget);

		MaskDecalComponent->SetDecalMaterial(MaskMaterialInstance);
		MaskDecalComponent->DecalSize = FVector(2.0, 0.5, 0.5);
	}

	if (!MaskDecalComponent->IsVisible())
		MaskDecalComponent->SetVisibility(true);

	UpdateMaskTransform(Landscape->GetActorTransform(), LandscapeExtent);
}

void AHoudiniMaskGizmoActiveActor::OnBrush(const bool& bBrushInversed)
{
	if (!CanBrush())
		return;

	const EHoudiniMaskType& MaskType = MaskInput->GetMaskType();
	if (!bVisualizeMask)
	{
		MaskInput->UpdateData(BrushDecalComponent->GetComponentTransform().GetLocation(),
			BrushSize, (MaskType == EHoudiniMaskType::Weight) ? BrushFallOff : 0.0f,
			(MaskType == EHoudiniMaskType::Byte) ? ByteValue : ((MaskType == EHoudiniMaskType::Weight) ? FMath::RoundToInt(255.0f * BrushValue) : 255),
			bBrushInversed);
		
		return;
	}

	TArray<FColor> ChangedColorData;
	const FIntRect ChangedLocalExtent = MaskInput->UpdateData(BrushDecalComponent->GetComponentTransform().GetLocation(),
			BrushSize, (MaskType == EHoudiniMaskType::Weight) ? BrushFallOff : 0.0f,
			(MaskType == EHoudiniMaskType::Byte) ? ByteValue : ((MaskType == EHoudiniMaskType::Weight) ? FMath::RoundToInt(255.0f * BrushValue) : 255),
			bBrushInversed, ChangedColorData);

	if (ChangedLocalExtent == INVALID_LANDSCAPE_EXTENT)
		return;

	const FColor* ColorDataPtr = ChangedColorData.GetData();

	FTextureRenderTargetResource* Resource = MaskColorRenderTarget->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(UpdateHoudiniMaskColor)(
		[&](FRHICommandListImmediate& RHICmdList)
		{
			RHIUpdateTexture2D(Resource->GetTexture2DRHI(), 0, FUpdateTextureRegion2D(
				ChangedLocalExtent.Min.X, ChangedLocalExtent.Min.Y, 0, 0, ChangedLocalExtent.Width() + 1, ChangedLocalExtent.Height() + 1),
				(ChangedLocalExtent.Width() + 1) * sizeof(FColor), (const uint8*)ColorDataPtr);
		});

	FlushRenderingCommands();
}

void AHoudiniMaskGizmoActiveActor::EndBrush()
{
	if (CanBrush())
	{
		MaskInput->CommitData();
		if (MaskInput->ShouldCheckChanged() && MaskInput->HasDataChanged())
			MaskInput->RequestReimport();
	}
}

void AHoudiniMaskGizmoActiveActor::Exit()
{
	if (MaskInput.IsValid())
		MaskInput->OnChangedDelegate.RemoveAll(this);

	DeactivateBrushDisplay();
	DeactivateMaskDisplay();
}


void AHoudiniMaskGizmoActiveActor::DeactivateMaskDisplay() const
{
	if (MaskInput.IsValid())
		MaskInput->OnChangedDelegate.RemoveAll(this);  // We only need this delegate to refresh mask visualization

	if (IsValid(MaskDecalComponent) && MaskDecalComponent->IsVisible())
		MaskDecalComponent->SetVisibility(false);
}

UE_DISABLE_OPTIMIZATION
void AHoudiniMaskGizmoActiveActor::UpdateMaskTransform(const FTransform& LandscapeTransform, const FIntRect& MaskExtent) const
{
	FTransform OutTransform = LandscapeTransform;

	// The final landscape transform that should go into Houdini consist of the following two components:
	// - Shared Landscape Transform
	// - Extents of all the loaded landscape components

	// The houdini transform will always be in the center of the currently loaded landscape components.
	// HF are centered, Landscape aren't
	// Calculate the offset needed to properly represent the Landscape in H	
	const FVector3d CenterOffset(
		(double)(MaskExtent.Max.X + MaskExtent.Min.X) * 0.5,
		(double)(MaskExtent.Max.Y + MaskExtent.Min.Y) * 0.5,
		1.0);

	// Extract the Landscape rotation/scale and apply them to the offset 
	FTransform TransformWithRot = FTransform::Identity;
	TransformWithRot.CopyRotation(OutTransform);
	FVector3d LandscapeScale = OutTransform.GetScale3D();

	const FVector RotScaledOffset = TransformWithRot.TransformPosition(FVector(CenterOffset.X * LandscapeScale.X, CenterOffset.Y * LandscapeScale.Y, 0));

	// Apply the rotated offset to the transform's position
	FVector3d Loc = OutTransform.GetLocation() + RotScaledOffset;
	OutTransform.SetLocation(Loc);

	OutTransform.SetScale3D(FVector(LandscapeScale.Z, LandscapeScale.Y, LandscapeScale.X));  // Swap axis, as decal is align to X originally

	FTransform MaskTransform(FRotator(90.0, 0.0, 0.0), FVector(0.0, 0.0, 0.0),
		FVector(MaskExtent.Width() + MaskExtent.Height() + 2, MaskExtent.Height(), -MaskExtent.Width()));
	MaskTransform.Accumulate(OutTransform);

	MaskDecalComponent->SetWorldTransform(MaskTransform);
	/*
	FIntRect LandscapeRegion;
	LandscapeInfo->GetLandscapeExtent(LandscapeRegion);
	FVector DecalSize = LandscapeTransform.TransformVector(FVector(LandscapeRegion.Width(), -LandscapeRegion.Height(), LandscapeRegion.Width() + LandscapeRegion.Height() + 2));
	MaskDecalComponent->SetWorldTransform(FTransform(FRotator(90.0, 0.0, 0.0), FVector(0.0, 0.0, 0.0), FVector(DecalSize.Z, DecalSize.X, DecalSize.Y)));  // TODO: calculate formal transform
	*/
}
UE_ENABLE_OPTIMIZATION
