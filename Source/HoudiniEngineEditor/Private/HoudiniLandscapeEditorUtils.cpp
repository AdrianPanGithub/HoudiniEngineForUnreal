// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniLandscapeEditorUtils.h"

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
#include "LandscapeEditLayer.h"
#endif

#include "EditorModes.h"
#include "EditorModeManager.h"
#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"
#include "Editor/LandscapeEditor/Public/LandscapeToolInterface.h"
#include "Editor/LandscapeEditor/Public/LandscapeEditorObject.h"

#include "HoudiniEngine.h"
#include "HoudiniNode.h"
#include "HoudiniInputs.h"

#include "HoudiniEngineEditor.h"


FName FHoudiniLandscapeEditorUtils::BrushingLandscapeName = NAME_None;
FName FHoudiniLandscapeEditorUtils::BrushingEditLayerName = NAME_None;
TArray<FName> FHoudiniLandscapeEditorUtils::BrushingLayerNames;
TArray<UHoudiniInputLandscape*> FHoudiniLandscapeEditorUtils::BrushingLandscapeInputs;

void FHoudiniLandscapeEditorUtils::Reset()
{
	BrushingLandscapeName = NAME_None;
	BrushingEditLayerName = NAME_None;
	BrushingLayerNames.Empty();
	BrushingLandscapeInputs.Empty();
}

void FHoudiniLandscapeEditorUtils::OnStartBrush(const FEdModeLandscape* LandscapeEd)
{
	Reset();

	if (!LandscapeEd->CurrentBrush || !(FString(LandscapeEd->CurrentBrush->GetBrushName()).StartsWith(TEXT("Circle"))))
		return;

	const FLandscapeToolTarget& Target = LandscapeEd->CurrentToolTarget;
	if (!Target.LandscapeInfo.IsValid())
		return;

	// Update LandscapeName
	const ALandscape* Landscape = Target.LandscapeInfo->LandscapeActor.Get();
	BrushingLandscapeName = Landscape->GetFName();

	// Update EditLayerName
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
	if (Landscape->HasLayersContent())
#endif
	{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
		if (const ULandscapeEditLayerBase* EditingLayer = Landscape->GetEditLayerConst(Landscape->GetEditingLayer()))
			BrushingEditLayerName = EditingLayer->GetName();
#else
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
		if (const FLandscapeLayer* EditingLayer = Landscape->GetLayerConst(Landscape->GetEditingLayer()))
#else
		if (const FLandscapeLayer* EditingLayer = Landscape->GetLayer(Landscape->GetEditingLayer()))
#endif
			BrushingEditLayerName = EditingLayer->Name;
#endif
		else
		{
			Reset();
			return;
		}
	}

	// TODO: If import combined landscape layer, use ALandscapeProxy::OnComponentDataChanged / ULandscapeSubsystem::OnLandscapeProxyComponentDataChangedDelegate

	// Update LayerNames
	if (Target.TargetType == ELandscapeToolTargetType::Heightmap)
		BrushingLayerNames = TArray<FName>{ HoudiniHeightLayerName };
	else if (Target.TargetType == ELandscapeToolTargetType::Weightmap)
	{
		if (!Target.LayerInfo.IsValid())
			return;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
		if (Target.LayerInfo->GetBlendMethod() == ELandscapeTargetLayerBlendMethod::None)
#else
		if (Target.LayerInfo->bNoWeightBlend)
#endif
			BrushingLayerNames = TArray<FName>{ Target.LayerName };
		else  // If weight blend, means all weight blended layers are changing
		{
			for (const FLandscapeInfoLayerSettings& Layer : Target.LandscapeInfo->Layers)
			{
				if (IsValid(Layer.LayerInfoObj))
				{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
					if (Target.LayerInfo->GetBlendMethod() != ELandscapeTargetLayerBlendMethod::None)
#else
					if (!Layer.LayerInfoObj->bNoWeightBlend)
#endif
						BrushingLayerNames.Add(Layer.LayerName);
				}
			}
		}
	}
	else if (Target.TargetType == ELandscapeToolTargetType::Visibility)
		BrushingLayerNames = TArray<FName>{ HoudiniAlphaLayerName };

	// Update LandscapeInputs
	if (!BrushingLayerNames.IsEmpty())
	{
		FOREACH_HOUDINI_INPUT(if (Input->GetType() == EHoudiniInputType::World && Input->GetSettings().bCheckChanged)
		{
			for (UHoudiniInputHolder* Holder : Input->Holders)
			{
				if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
				{
					if (LandscapeInput->GetLandscapeName() == BrushingLandscapeName)
					{
						for (const FName& ChangedLayerName : BrushingLayerNames)
						{
							if (LandscapeInput->HasLayerImported(BrushingEditLayerName, ChangedLayerName))
								BrushingLandscapeInputs.AddUnique(LandscapeInput);
						}
						break;
					}
				}
			}
		}
		);
	}

	if (BrushingLandscapeInputs.IsEmpty())
		Reset();
}

void FHoudiniLandscapeEditorUtils::OnBrushing(const FEdModeLandscape* LandscapeEd)
{
	//#include "Editor/LandscapeEditor/Private/LandscapeEdModeBrushes.cpp"  // Many code should copy from this file
	class FHoudiniLandscapeBrushCircle : public FLandscapeBrush
	{
		using Super = FLandscapeBrush;

		// Components which previously we're under the area of the brush.
		TSet<TWeakObjectPtr<ULandscapeComponent>, TWeakObjectPtrSetKeyFuncs<TWeakObjectPtr<ULandscapeComponent>>> BrushMaterialComponents;

		// A Cache of previously created and now unused MIDs we can reuse if required.
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> BrushMaterialFreeInstances;

	protected:
		FVector2f LastMousePosition;
		TObjectPtr<UMaterialInterface> BrushMaterial;
		TMap<TWeakObjectPtr<ULandscapeComponent>, TWeakObjectPtr<UMaterialInstanceDynamic>, FDefaultSetAllocator, TWeakObjectPtrMapKeyFuncs<TWeakObjectPtr<ULandscapeComponent>, TWeakObjectPtr<UMaterialInstanceDynamic>>> BrushMaterialInstanceMap;

		bool bCanPaint;

		virtual float CalculateFalloff(float Distance, float Radius, float Falloff) = 0;

	public:
		FEdModeLandscape* EdMode;

		FORCEINLINE const FVector2f& GetLastPosition() const { return LastMousePosition; }
	};


	if (BrushingLandscapeName.IsNone() || BrushingLandscapeInputs.IsEmpty())
		return;

	const FVector2f& InteractorPosition = ((FHoudiniLandscapeBrushCircle*)LandscapeEd->CurrentBrush)->GetLastPosition();

	// Copy from FLandscapeBrushCircle::ApplyBrush
	const ULandscapeInfo* LandscapeInfo = LandscapeEd->CurrentToolTarget.LandscapeInfo.Get();
	const float ScaleXY = static_cast<float>(FMath::Abs(LandscapeInfo->DrawScale.X));
	//const float TotalRadius = EdMode->UISettings->GetCurrentToolBrushRadius() / ScaleXY;
	const ELandscapeToolTargetType& TargetType = LandscapeEd->CurrentToolTarget.TargetType;
	const float TotalRadius = ((TargetType == ELandscapeToolTargetType::Heightmap || TargetType == ELandscapeToolTargetType::Visibility) ?  // Copy from ULandscapeEditorObject::IsWeightmapTarget
		LandscapeEd->UISettings->BrushRadius : LandscapeEd->UISettings->PaintBrushRadius) / ScaleXY;

	FIntRect SpotBounds;
	SpotBounds.Min.X = FMath::FloorToInt32(InteractorPosition.X - TotalRadius);
	SpotBounds.Min.Y = FMath::FloorToInt32(InteractorPosition.Y - TotalRadius);
	SpotBounds.Max.X = FMath::CeilToInt32(InteractorPosition.X + TotalRadius);
	SpotBounds.Max.Y = FMath::CeilToInt32(InteractorPosition.Y + TotalRadius);
	
	FIntRect LandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(LandscapeExtent);
	if (LandscapeExtent.Intersect(SpotBounds))
		SpotBounds.Clip(LandscapeExtent);
	else
		return;

	// Notify region changed
	for (UHoudiniInputLandscape* LandscapeInput : BrushingLandscapeInputs)
	{
		for (const FName& ChangedLayerName : BrushingLayerNames)
			LandscapeInput->NotifyLayerChanged(BrushingEditLayerName, ChangedLayerName, SpotBounds);
	}
}

void FHoudiniLandscapeEditorUtils::OnEndBrush()
{
	for (UHoudiniInputLandscape* LandscapeInput : BrushingLandscapeInputs)
		LandscapeInput->RequestReimport();

	BrushingLandscapeName = NAME_None;
	BrushingEditLayerName = NAME_None;
	BrushingLayerNames.Empty();
	BrushingLandscapeInputs.Empty();
}

void FHoudiniEngineEditor::RegisterLandscapeEditorDelegate()
{
#if WITH_SLATE_DEBUGGING
	if (IsRunningCommandlet())
		return;

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
	if (OnLandscapeEditorEnterOrExistHandle.IsValid())
#else
	if (OnLandscapeEditorEnterOrExistHandle.IsValid() || !GLevelEditorModeToolsIsValid())  // If has been reister or edmode tools not valid, then we will NOT register again
#endif
		return;

	OnLandscapeEditorEnterOrExistHandle = GLevelEditorModeTools().OnEditorModeIDChanged().AddLambda(
		[](const FEditorModeID& ModeId, bool bEnter)
		{
			if (ModeId == FBuiltinEditorModes::EM_Landscape)
			{
				if (bEnter)
				{
					const FEdModeLandscape* LandscapeEd = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
					FHoudiniEngineEditor::Get().OnLandscapeBrushingHandle = FSlateDebugging::InputEvent.AddLambda(
						[LandscapeEd](const FSlateDebuggingInputEventArgs& EventArgs)
						{
							if (FHoudiniEngine::Get().GetCurrentNodes().IsEmpty())
								return;

							switch (EventArgs.InputEventType)
							{
							case ESlateDebuggingInputEvent::MouseButtonDown:
							if (EventArgs.InputEvent && EventArgs.InputEvent->IsPointerEvent() &&
								((const FPointerEvent*)EventArgs.InputEvent)->IsMouseButtonDown(EKeys::LeftMouseButton))
							{
								FHoudiniLandscapeEditorUtils::OnStartBrush(LandscapeEd);
								FHoudiniLandscapeEditorUtils::OnBrushing(LandscapeEd);
							}
							break;
							case ESlateDebuggingInputEvent::MouseMove:
								FHoudiniLandscapeEditorUtils::OnBrushing(LandscapeEd);
								break;
							case ESlateDebuggingInputEvent::MouseButtonUp:
								FHoudiniLandscapeEditorUtils::OnEndBrush();
								break;
							}
						});
				}
				else
				{
					if (FHoudiniEngineEditor::Get().OnLandscapeBrushingHandle.IsValid())
					{
						FSlateDebugging::InputEvent.Remove(FHoudiniEngineEditor::Get().OnLandscapeBrushingHandle);
						FHoudiniEngineEditor::Get().OnLandscapeBrushingHandle.Reset();
					}
				}
			}
		}
	);
#endif
}

void FHoudiniEngineEditor::UnregisterLandscapeEditorDelegate()
{
#if WITH_SLATE_DEBUGGING
	if (IsRunningCommandlet())
		return;

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
	if (!OnLandscapeEditorEnterOrExistHandle.IsValid() || !GIsRunning)
#else
	if (!OnLandscapeEditorEnterOrExistHandle.IsValid() || !GLevelEditorModeToolsIsValid())
#endif
		return;

	GLevelEditorModeTools().OnEditorModeIDChanged().Remove(OnLandscapeEditorEnterOrExistHandle);
	OnLandscapeEditorEnterOrExistHandle.Reset();
#endif
}
