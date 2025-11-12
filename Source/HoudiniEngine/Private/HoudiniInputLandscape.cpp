// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputs.h"

#include "Landscape.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeEdit.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplinesComponent.h"
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
#include "LandscapeEditLayer.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"


ALandscape* UHoudiniInputLandscape::TryCast(AActor* InLandscape)
{
	ALandscape* Landscape = Cast<ALandscape>(InLandscape);
	if (!IsValid(Landscape))
	{
		ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InLandscape);
		if (IsValid(LandscapeProxy))
			Landscape = LandscapeProxy->GetLandscapeActor();
	}
	return Landscape;
}

bool UHoudiniInputLandscape::IsLandscape(const AActor* Actor)
{
	return IsValid(Actor) && Actor->IsA<ALandscape>();
}

UHoudiniInputLandscape* UHoudiniInputLandscape::Create(UHoudiniInput* Input, ALandscape* Landscape)
{
	UHoudiniInputLandscape* LandscapeInput = NewObject<UHoudiniInputLandscape>(Input, NAME_None, RF_Public | RF_Transactional);

	LandscapeInput->SetLandscape(Landscape);

	return LandscapeInput;
}

void UHoudiniInputLandscape::SetLandscape(ALandscape* NewLandscape)
{
	if (GetLandscape() != NewLandscape)
	{
		Landscape = NewLandscape;
		LandscapeName = NewLandscape->GetFName();
		RequestReimport();
	}
}

ALandscape* UHoudiniInputLandscape::GetLandscape() const
{
	if (Landscape.IsValid())
		return Landscape.Get();

	Landscape = Cast<ALandscape>(FHoudiniEngine::Get().GetActorByName(LandscapeName, false));
	return Landscape.IsValid() ? Landscape.Get() : nullptr;
}

TSoftObjectPtr<UObject> UHoudiniInputLandscape::GetObject() const
{
	return GetLandscape();
}

bool UHoudiniInputLandscape::HasLayerImported(const FName& EditLayerName, const FName& LayerName) const
{
	const FHoudiniEditLayerImportInfo* FoundEditLayerImportInfoPtr = EditLayerName.IsNone() ? nullptr : EditLayerImportInfoMap.Find(EditLayerName);
	if (!FoundEditLayerImportInfoPtr)  // Check whether we have imported all Editlayers
		FoundEditLayerImportInfoPtr = EditLayerImportInfoMap.Find(FHoudiniInputSettings::AllLandscapeLayerName);

	return FoundEditLayerImportInfoPtr ?
		FoundEditLayerImportInfoPtr->LayerImportInfoMap.Contains(LayerName) : false;
}

bool UHoudiniInputLandscape::NotifyLayerChanged(const FName& EditLayerName, const FName& LayerName, const FIntRect& ChangedRegion)
{
	FHoudiniEditLayerImportInfo* FoundEditLayerImportInfoPtr = EditLayerName.IsNone() ? nullptr : EditLayerImportInfoMap.Find(EditLayerName);
	if (!FoundEditLayerImportInfoPtr)  // Check whether we have imported all Editlayers
		FoundEditLayerImportInfoPtr = EditLayerImportInfoMap.Find(FHoudiniInputSettings::AllLandscapeLayerName);

	if (FoundEditLayerImportInfoPtr)
	{
		if (FHoudiniLayerImportInfo* FoundLayerImportInfoPtr = FoundEditLayerImportInfoPtr->LayerImportInfoMap.Find(LayerName))
		{
			if (FoundLayerImportInfoPtr->ChangedExtent == INVALID_LANDSCAPE_EXTENT)
				FoundLayerImportInfoPtr->ChangedExtent = ChangedRegion;
			else
				FoundLayerImportInfoPtr->ChangedExtent.Union(ChangedRegion);
			bHasChanged = true;

			return true;
		}
	}

	return false;
}

bool HapiUploadLandscapeInfo(const int32& SettingsNodeId, const FString& DeltaInfo,
	const ALandscape* Landscape, const ULandscapeInfo* LandscapeInfo, const FIntRect& LandscapeExtent);

bool HapiUploadLandscapeSplines(const ALandscape* Landscape, const TArray<const ULandscapeSplinesComponent*>& LSCs,
	UHoudiniInput* Input, int32& InOutNodeId, size_t& InOutHandle)
{
	size_t NumPoints = 0;
	size_t NumPrims = 0;  // NumSegments
	for (const ULandscapeSplinesComponent* LSC : LSCs)
	{
		NumPoints += LSC->GetControlPoints().Num();
		NumPrims += LSC->GetSegments().Num();
	}
	const size_t NumVertices = NumPrims * 2;

	FHoudiniSharedMemoryGeometryInput GeoInput(NumPoints, NumPrims, NumVertices);
	GeoInput.AppendAttribute(HAPI_ATTRIB_ROT, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 4, NumPoints * 4);
	GeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_LANDSCAPE_SPLINE_TANGENT_LENGTH, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 1, NumVertices);

	float* const SHM = GeoInput.GetSharedMemory(FString::Printf(TEXT("splines_%08X"), (size_t)Landscape), InOutHandle);

	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	float* RotationDataPtr = (float*)(VertexDataPtr + NumVertices + NumPrims);
	float* TanLenDataPtr = RotationDataPtr + NumPoints * 4;

	int32 GlobalPointIdx = 0;
	for (const ULandscapeSplinesComponent* LSC : LSCs)
	{
		const FTransform ComponentTransform = LSC->GetComponentTransform();
		TMap<const ULandscapeSplineControlPoint*, int32> PointIdxMap;
		for (const ULandscapeSplineControlPoint* Point : LSC->GetControlPoints())
		{
			const FVector3f Position = FVector3f(ComponentTransform.TransformPosition(Point->Location) * POSITION_SCALE_TO_HOUDINI);
			*PositionDataPtr = Position.X;
			*(PositionDataPtr + 1) = Position.Z;
			*(PositionDataPtr + 2) = Position.Y;
			PositionDataPtr += 3;

			const FQuat4f Rotation = FQuat4f(Point->Rotation.Quaternion());
			*RotationDataPtr = Rotation.X;
			*(RotationDataPtr + 1) = Rotation.Z;
			*(RotationDataPtr + 2) = Rotation.Y;
			*(RotationDataPtr + 3) = -Rotation.W;
			RotationDataPtr += 4;

			PointIdxMap.Add(Point, GlobalPointIdx);
			++GlobalPointIdx;
		}

		for (const ULandscapeSplineSegment* Seg : LSC->GetSegments())
		{
			*VertexDataPtr = PointIdxMap[Seg->Connections[0].ControlPoint];
			*(VertexDataPtr + 1) = PointIdxMap[Seg->Connections[1].ControlPoint];
			*(VertexDataPtr + 2) = -2;
			VertexDataPtr += 3;

			*TanLenDataPtr = Seg->Connections[0].TangentLen;
			*(TanLenDataPtr + 1) = Seg->Connections[1].TangentLen;
			TanLenDataPtr += 2;
		}

		NumPoints += LSC->GetControlPoints().Num();
		NumPrims += LSC->GetSegments().Num();
	}

	if (InOutNodeId < 0)
	{
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(Input->GetGeoNodeId(),
			FString::Printf(TEXT("%s_landscape_splines_%08X"), *(Landscape->GetName()), FPlatformTime::Cycles()), InOutNodeId));

		HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(InOutNodeId));
	}

	return GeoInput.HapiUpload(InOutNodeId, SHM);
}

void UHoudiniInputLandscape::InvalidateSplinesInput()
{
	SplinesNodeId = -1;
	if (SplinesHandle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(SplinesHandle);
		SplinesHandle = 0;
	}
}

bool UHoudiniInputLandscape::HapiDestroySplinesInput()
{
	if (SplinesNodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SplinesNodeId));
		GetInput()->NotifyMergedNodeDestroyed();
	}

	InvalidateSplinesInput();
	return true;
}

// Houdini volume X and Y is fliped, and we should convert abs extent to local extent so that hda could use it properly
#define EXPAND_EXTENT_TO_HOUDINI(ChangedExtent, LandscapeExtent) \
	FMath::Max(ChangedExtent.Min.Y - LandscapeExtent.Min.Y, 0),\
	FMath::Max(ChangedExtent.Min.X - LandscapeExtent.Min.X, 0),\
	FMath::Min(ChangedExtent.Max.Y - LandscapeExtent.Min.Y, LandscapeExtent.Height() + 1),\
	FMath::Min(ChangedExtent.Max.X - LandscapeExtent.Min.X, LandscapeExtent.Width() + 1)

bool UHoudiniInputLandscape::HapiUpload()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniInputLandscape);

	ALandscape* LandscapeActor = GetLandscape();
	ULandscapeInfo* LandscapeInfo = IsValid(LandscapeActor) ? LandscapeActor->GetLandscapeInfo() : nullptr;
	if (!IsValid(LandscapeInfo))
	{
		// TODO:
		return HapiDestroy();
	}

	if (GetSettings().bImportLandscapeSplines)
	{
		TArray<const ULandscapeSplinesComponent*> LSCs;
		LandscapeInfo->ForAllSplineActors([&LSCs](TScriptInterface<ILandscapeSplineInterface> SplineInterface)
			{
				const ULandscapeSplinesComponent* LSC = SplineInterface->GetSplinesComponent();
				if (IsValid(LSC))
					LSCs.Add(LSC);
			});

		if (LSCs.IsEmpty())
			HOUDINI_FAIL_RETURN(HapiDestroySplinesInput())
		else
			HOUDINI_FAIL_RETURN(HapiUploadLandscapeSplines(LandscapeActor, LSCs, GetInput(), SplinesNodeId, SplinesHandle));
		//for (const ULandscapeSplinesComponent* LSC : LSCs)
		//	UE_LOG(LogTemp, Warning, TEXT("%s |"), *LSC->GetName());
	}
	else
		HOUDINI_FAIL_RETURN(HapiDestroySplinesInput());

	// Check whether landscape extent has changed
	FIntRect NewLandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(NewLandscapeExtent);

	// -------- Record delta info parts, and check whether there has layers changed by the way --------
	// DeltaInfo format is { InputName }/{ EditLayers, split by " " }/{ Layers, split by " ", diff-editlayer split by "|" }/{ ChangedExtents, split by "|" }
	FString ChangedEditLayersStr;
	FString ChangedLayersStr;
	FString ChangedExtentsStr;
	for (const auto& EditLayerImportInfo : EditLayerImportInfoMap)
	{
		bool bEditLayerChanged = false;
		for (const auto& LayerImportInfo : EditLayerImportInfo.Value.LayerImportInfoMap)
		{
			if (LayerImportInfo.Value.ChangedExtent != INVALID_LANDSCAPE_EXTENT)
			{
				if (bEditLayerChanged)  // Means previous has layer changed in the same editlayer
					ChangedLayersStr += TEXT(" ") + LayerImportInfo.Key.ToString();
				else
				{
					if (!ChangedLayersStr.IsEmpty())  // Layers in different EditLayers split by "|"
						ChangedLayersStr += TEXT("|");
					ChangedLayersStr += LayerImportInfo.Key.ToString();
					bEditLayerChanged = true;
				}

				const FIntRect& ChangedExtent = LayerImportInfo.Value.ChangedExtent;
				ChangedExtentsStr += ChangedExtentsStr.IsEmpty() ?
					FString::Printf(TEXT("%d,%d-%d,%d"), EXPAND_EXTENT_TO_HOUDINI(ChangedExtent, NewLandscapeExtent)) :
					FString::Printf(TEXT("|%d,%d-%d,%d"), EXPAND_EXTENT_TO_HOUDINI(ChangedExtent, NewLandscapeExtent));
			}
		}

		if (bEditLayerChanged)
		{
			if (!ChangedEditLayersStr.IsEmpty())
				ChangedEditLayersStr += TEXT(" ");
			ChangedEditLayersStr += EditLayerImportInfo.Key.ToString();
		}
	}

	bool bForceReimport = ChangedEditLayersStr.IsEmpty();  // if there are no layer changed, means this cook is triggered manually, we should ForceReimport

	if (NewLandscapeExtent != ImportedExtent)
		bForceReimport = true;


	// -------- Import layer data --------
	const FTransform& LandscapeTransform = LandscapeActor->GetActorTransform();
	const bool bHasVisibilityLayer = IsValid(LandscapeActor->LandscapeHoleMaterial);
	const bool bPreviousNoLayersImported = EditLayerImportInfoMap.IsEmpty();

	bool bEditlayersNeedReconnect = false;
	TMap<FName, FHoudiniEditLayerImportInfo> NewEditLayerImportInfoMap;  // If Empty, then means we should import as ref

	if (!GetSettings().bImportAsReference)
	{
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);  // As LandscapeEdit will restore the uncompressed textures, which we could reuse when import multiple layers
	LandscapeEdit.SetShouldDirtyPackage(false);  // Should NOT call any modify()
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
	for (int32 EditLayerIdx = -1; EditLayerIdx < LandscapeActor->GetLayersConst().Num(); ++EditLayerIdx)
	{
		const ULandscapeEditLayerBase* EditLayer = (EditLayerIdx <= -1) ? nullptr : LandscapeActor->GetEditLayerConst(EditLayerIdx);
		const FName EditLayerName = EditLayer ? EditLayer->GetName() : FHoudiniInputSettings::AllLandscapeLayerName;
		if ((EditLayerIdx <= -1) && !GetSettings().LandscapeLayerFilterMap.Contains(EditLayerName))  // EditLayerIdx == -1 means all edit layers to import
			continue;

		const FGuid EditLayerGuid = EditLayer ? EditLayer->GetGuid() : FGuid();
#else
	for (int32 EditLayerIdx = -1; EditLayerIdx < int32(LandscapeActor->GetLayerCount()); ++EditLayerIdx)
	{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
		const FLandscapeLayer* EditLayer = (EditLayerIdx <= -1) ? nullptr : LandscapeActor->GetLayerConst(EditLayerIdx);
#else
		const FLandscapeLayer* EditLayer = (EditLayerIdx <= -1) ? nullptr : LandscapeActor->GetLayer(EditLayerIdx);
#endif
		const FName EditLayerName = EditLayer ? EditLayer->Name : FHoudiniInputSettings::AllLandscapeLayerName;
		if ((EditLayerIdx <= -1) && !GetSettings().LandscapeLayerFilterMap.Contains(EditLayerName))  // EditLayerIdx == -1 means all edit layers to import
			continue;

		const FGuid EditLayerGuid = EditLayer ? EditLayer->Guid : FGuid();
#endif
		
		
		TArray<FString> LayersToImport;
		TArray<FString> LayersNoImport;
		if (!GetSettings().LandscapeLayerFilterMap.IsEmpty())  // if No filters, then all layers will import
		{
			const FString* LayerFilterPtr = GetSettings().LandscapeLayerFilterMap.Find(EditLayerName);
			if (!LayerFilterPtr)
				continue;

			FHoudiniEngineUtils::ParseFilterPattern(*LayerFilterPtr, LayersToImport, LayersNoImport);
		}


		FScopedSetLandscapeEditingLayer Scope(LandscapeActor, EditLayerGuid); // Scope landscape access to the current layer


		FHoudiniEditLayerImportInfo& NewEditLayerImportInfo = NewEditLayerImportInfoMap.Add(EditLayerName);
		FHoudiniEditLayerImportInfo* OldEditLayerImportInfoPtr = EditLayerImportInfoMap.Find(EditLayerName);
		bool bNewLayerImported = false;  // Means we need to reconnect layer import nodes to "merge" node of NewEditLayerImportInfo
		for (int32 LayerIdx = -2; LayerIdx < LandscapeInfo->Layers.Num(); ++LayerIdx)
		{
			// Get valid LayerInfo and LayerName
			ULandscapeLayerInfoObject* LayerInfo = nullptr;
			FName LayerName;
			if (LayerIdx == -2)  // height: Heightmap
				LayerName = HoudiniHeightLayerName;
			else if (LayerIdx == -1)  // Alpha: Visibility layer
			{
				if (bHasVisibilityLayer)
				{
					LayerInfo = ALandscapeProxy::VisibilityLayer;
					LayerName = HoudiniAlphaLayerName;
				}
				else
					continue;
			}
			else  // Weightmaps
			{
				LayerInfo = LandscapeInfo->Layers[LayerIdx].LayerInfoObj;
				if (!IsValid(LayerInfo))
					continue;

				LayerName = LandscapeInfo->Layers[LayerIdx].LayerName;
			}

			// Judge whether this layer should import, by filters
			if (!LayersToImport.IsEmpty())
			{
				if (!LayersToImport.Contains(LayerName.ToString()))
					continue;
			}
			else if (!LayersNoImport.IsEmpty())
			{
				if (LayersNoImport.Contains(LayerName.ToString()))
					continue;
			}  // if both LayersToImport and LayersNoImport are empty, we will import all layers

			// Now this layer must import, so we must ensure that the NewEditLayerImportInfo has a "merge" node to connect
			if (NewEditLayerImportInfo.MergeNodeId < 0)
			{
				if (OldEditLayerImportInfoPtr && OldEditLayerImportInfoPtr->MergeNodeId >= 0)  // Try to reuse the previous merge node
				{
					NewEditLayerImportInfo.MergeNodeId = OldEditLayerImportInfoPtr->MergeNodeId;
					OldEditLayerImportInfoPtr->MergeNodeId = -1;   // Reset old MergeNodeId so that this "merge" node won't be destroyed
				}
				
				if (NewEditLayerImportInfo.MergeNodeId < 0)
				{
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), GetGeoNodeId(), "merge",
						TCHAR_TO_UTF8(*FString::Printf(TEXT("%s_%08X"), *FHoudiniEngineUtils::GetValidatedString(EditLayerName.ToString()), FPlatformTime::Cycles())),
						false, &NewEditLayerImportInfo.MergeNodeId));

					bEditlayersNeedReconnect = true;  // Means we need to reconnect editlayer-import-nodes to "merge" node if UHoudiniInputLandscape
				}
			}

			FHoudiniLayerImportInfo& NewLayerImportInfo = NewEditLayerImportInfo.LayerImportInfoMap.Add(LayerName);
			if (OldEditLayerImportInfoPtr)  // Try to reuse the previous volumeinput node
			{
				if (const FHoudiniLayerImportInfo* FoundLayerImportInfoPtr = OldEditLayerImportInfoPtr->LayerImportInfoMap.Find(LayerName))
				{
					NewLayerImportInfo = *FoundLayerImportInfoPtr;
					OldEditLayerImportInfoPtr->LayerImportInfoMap.Remove(LayerName);  // Remove from old map, as the old ImportInfos will be destroyed
					
					if (bForceReimport)  // Reset ChangedExtent, mark that we should force reimport this layer in FHoudiniLayerImportInfo::HapiUpload
						NewLayerImportInfo.ChangedExtent = INVALID_LANDSCAPE_EXTENT;
					else if (!NewLayerImportInfo.NeedReimport())  // if !bForceReimport and this layer need NOT import, then we should skip import this layer
						continue;
				}
			}

			if (NewLayerImportInfo.NodeId < 0)
				bNewLayerImported = true;  // Means we need to reconnect layer import nodes to "merge" node of NewEditLayerImportInfo

			HOUDINI_FAIL_RETURN(NewLayerImportInfo.HapiUpload(LandscapeEdit, EditLayerName, LayerInfo,
				LandscapeTransform.GetScale3D().Z, NewLandscapeExtent, GetGeoNodeId()));
		}

		// Connect to new nodes
		if (bNewLayerImported)
		{
			const int32& LayerMergeNodeId = NewEditLayerImportInfo.MergeNodeId;
			int32 MergeInputIdx = 0;
			for (const auto& NewLayerImportInfo : NewEditLayerImportInfo.LayerImportInfoMap)
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
					LayerMergeNodeId, MergeInputIdx, NewLayerImportInfo.Value.NodeId, 0));
				++MergeInputIdx;
			}
		}
	}
	}

	// -------- Cleanup old nodes --------
	for (auto& OldEditLayerImportInfo : EditLayerImportInfoMap)
		HOUDINI_FAIL_RETURN(OldEditLayerImportInfo.Value.HapiDestroy());


	// -------- Create or destroy "merge" node, depend on NewEditLayerImportInfoMap.IsEmpty() --------
	if (NewEditLayerImportInfoMap.IsEmpty())  // No layers to import, then we should just create a volume to represent landscape
	{
		if (MergeNodeId >= 0)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MergeNodeId));
			MergeNodeId = -1;
		}
	}
	else  // We should have a "merge" node to merge all imported editlayers
	{
		if (MergeNodeId < 0)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), GetGeoNodeId(),
				"merge", nullptr, false, &MergeNodeId));
			
			if (SettingsNodeId >= 0)
				HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiConnectInput(SettingsNodeId, MergeNodeId));

			bEditlayersNeedReconnect = true;
		}

		if (bEditlayersNeedReconnect)
		{
			int32 MergeInputIdx = 0;
			for (const auto& NewEditLayerImportInfo : NewEditLayerImportInfoMap)
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
					MergeNodeId, MergeInputIdx, NewEditLayerImportInfo.Value.MergeNodeId, 0));
				++MergeInputIdx;
			}
		}
	}

	// -------- Create "he_setup_heightfield_input" node --------
	const bool bCreateNewSettingsNode = (SettingsNodeId < 0);
	if (bCreateNewSettingsNode)
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiCreateNode(GetGeoNodeId(),
			FString::Printf(TEXT("%s_%08X"), *LandscapeActor->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
		
		if (MergeNodeId >= 0)
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiConnectInput(SettingsNodeId, MergeNodeId));
	}

	// -------- Setup landscape info ---------
	if (MergeNodeId < 0)  // Means we should just import a volume reference
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiSetResolution(SettingsNodeId,
			false, FIntVector2(NewLandscapeExtent.Width() + 1, NewLandscapeExtent.Height() + 1)));
	}
	
	HOUDINI_FAIL_RETURN(HapiUploadLandscapeInfo(SettingsNodeId, GetInput()->GetInputName() /
		(ChangedEditLayersStr.IsEmpty() ? FString("*") : ChangedEditLayersStr) /
		(ChangedLayersStr.IsEmpty() ? FString("*") : ChangedLayersStr) /
		(ChangedExtentsStr.IsEmpty() ? FString("*") : ChangedExtentsStr), LandscapeActor, LandscapeInfo, NewLandscapeExtent));

	HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiSetLandscapeTransform(SettingsNodeId,
		LandscapeTransform, NewLandscapeExtent));

	if (bCreateNewSettingsNode)
		HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));

	// -------- Finalize --------
	EditLayerImportInfoMap = NewEditLayerImportInfoMap;
	ImportedExtent = NewLandscapeExtent;

	bHasChanged = false;

	return true;
}

FORCEINLINE static float GetLandscapeVisibilityScaleToAlpha(const ALandscape* Landscape)
{
	if (IsValid(Landscape->LandscapeHoleMaterial))
	{
		if (const UMaterial* Material = Landscape->LandscapeHoleMaterial->GetMaterial())
			return (0.5f / Material->OpacityMaskClipValue);
	}
	return 1.5f;
}

bool FHoudiniLayerImportInfo::HapiUpload(FLandscapeEditDataInterface& LandscapeEdit,
	const FName& EditLayerName, ULandscapeLayerInfoObject* LayerInfo,  // LayerInfo == nullptr, means we should import height
	const float& ZScale, const FIntRect& LandscapeExtent,
	const int32& GeoNodeId)
{
	const bool bIsAlphaLayer = (LayerInfo == ALandscapeProxy::VisibilityLayer);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
	const FString LayerName = LayerInfo ? (bIsAlphaLayer ? TEXT("Alpha") : LayerInfo->GetLayerName().ToString()) : TEXT("height");
#else
	const FString LayerName = LayerInfo ? (bIsAlphaLayer ? TEXT("Alpha") : LayerInfo->LayerName.ToString()) : TEXT("height");
#endif
	FHoudiniSharedMemoryVolumeInput SHMVolumeInput(LayerInfo ? EHoudiniVolumeConvertDataType::Uint8 : EHoudiniVolumeConvertDataType::Uint16,
		EHoudiniVolumeStorageType::Float, FIntVector3(LandscapeExtent.Height() + 1, LandscapeExtent.Width() + 1, 1));  // X and Y is swaped in houdini

	const ALandscape* Landscape = LandscapeEdit.GetTargetLandscape();

	bool bSHMExists = false;
	float* const SHM = SHMVolumeInput.GetSharedMemory(
		FString::Printf(TEXT("%08X_%s_%s"), (size_t)Landscape,
		*FHoudiniEngineUtils::GetValidatedString(EditLayerName.ToString()), *LayerName), Handle, bSHMExists);

	const bool bPartialUpdate = (NodeId >= 0 && bSHMExists && ChangedExtent != INVALID_LANDSCAPE_EXTENT);  // Check whether we could upload data partially
	const FIntRect& Extent = bPartialUpdate ? ChangedExtent : LandscapeExtent;

	const int32 LandscapeSizeY = LandscapeExtent.Height() + 1;
	const int32 ExtentSizeX = Extent.Width() + 1;
	const int32 ExtentSizeY = Extent.Height() + 1;
	const int32 XStart = Extent.Min.X - LandscapeExtent.Min.X;
	const int32 YStart = Extent.Min.Y - LandscapeExtent.Min.Y;

	if (LayerInfo)
	{
		TArray64<uint8> Data;
		Data.SetNumUninitialized(ExtentSizeX * ExtentSizeY);
		LandscapeEdit.GetWeightDataFast(LayerInfo, Extent.Min.X, Extent.Min.Y, Extent.Max.X, Extent.Max.Y, Data.GetData(), 0);
		uint8* DataPtr = (uint8*)SHM;
		ParallelFor(ExtentSizeY, [&](int32 nY)
			{
				for (int32 nX = 0; nX < ExtentSizeX; ++nX)
					DataPtr[(nX + XStart) * LandscapeSizeY + (nY + YStart)] = Data[nY * ExtentSizeX + nX];
			});
	}
	else
	{
		TArray64<uint16> Data;
		Data.SetNumUninitialized(ExtentSizeX * ExtentSizeY);
		LandscapeEdit.GetHeightDataFast(Extent.Min.X, Extent.Min.Y, Extent.Max.X, Extent.Max.Y, Data.GetData(), 0);
		uint16* DataPtr = (uint16*)SHM;
		ParallelFor(ExtentSizeY, [&](int32 nY)
			{
				for (int32 nX = 0; nX < ExtentSizeX; ++nX)
					DataPtr[(nX + XStart) * LandscapeSizeY + (nY + YStart)] = Data[nY * ExtentSizeX + nX];
			});
	}
	

	if (NodeId < 0)
	{
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryVolumeInput::HapiCreateNode(GeoNodeId,
			FString::Printf(TEXT("%s_%08X"), *LayerName, FPlatformTime::Cycles()), NodeId));
	}

	if (bPartialUpdate)
	{
		HOUDINI_FAIL_RETURN(SHMVolumeInput.HapiPartialUpload(NodeId, SHM, true,
			FIntVector3(ChangedExtent.Min.Y, ChangedExtent.Min.X, 0), FIntVector3(ChangedExtent.Max.Y, ChangedExtent.Max.X, 0)));  // X and Y is swaped in houdini
	}
	else
	{
		SHMVolumeInput.AppendAttribute(HAPI_ATTRIB_UNREAL_LANDSCAPE_EDITLAYER_NAME,
			EHoudiniAttributeOwner::Prim, false, EditLayerName.ToString());
		HOUDINI_FAIL_RETURN(SHMVolumeInput.HapiUpload(NodeId, SHM, LayerInfo ?
				(bIsAlphaLayer ? FVector2f(GetLandscapeVisibilityScaleToAlpha(Landscape), 0.0f) : FVector2f(0.0f, 1.0f)) : FVector2f(-256.0f, 256.0f) * ZScale * POSITION_SCALE_TO_HOUDINI_F,
			LayerName, false, FIntVector3::ZeroValue, FIntVector3::ZeroValue, false));
	}

	ChangedExtent = INVALID_LANDSCAPE_EXTENT;

	return true;
}

bool FHoudiniLayerImportInfo::HapiDestroy()
{
	if (NodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeId));

	Invalidate();

	return true;
}

void FHoudiniLayerImportInfo::Invalidate()
{
	NodeId = -1;
	if (Handle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(Handle);
		Handle = 0;
	}
}

bool FHoudiniEditLayerImportInfo::HapiDestroy()
{
	if (MergeNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MergeNodeId));

	MergeNodeId = -1;

	for (auto& LayerImportInfo : LayerImportInfoMap)
		HOUDINI_FAIL_RETURN(LayerImportInfo.Value.HapiDestroy());

	return true;
}

void FHoudiniEditLayerImportInfo::Invalidate()
{
	MergeNodeId = -1;
	for (auto& LayerImportInfo : LayerImportInfoMap)
		LayerImportInfo.Value.Invalidate();
}

bool UHoudiniInputLandscape::HapiDestroy()
{
	if (MergeNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MergeNodeId));

	if (SettingsNodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
		GetInput()->NotifyMergedNodeDestroyed();
	}

	for (auto& EditLayerImportInfo : EditLayerImportInfoMap)
		HOUDINI_FAIL_RETURN(EditLayerImportInfo.Value.HapiDestroy());

	if (SplinesNodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SplinesNodeId));
		GetInput()->NotifyMergedNodeDestroyed();
	}

	Invalidate();

	return true;
}

void UHoudiniInputLandscape::Invalidate()
{
	MergeNodeId = -1;
	SettingsNodeId = -1;

	ImportedExtent = INVALID_LANDSCAPE_EXTENT;
	
	for (auto& EditLayerImportInfo : EditLayerImportInfoMap)
		EditLayerImportInfo.Value.Invalidate();

	InvalidateSplinesInput();
}


bool HapiUploadLandscapeInfo(const int32& SettingsNodeId, const FString& DeltaInfo, const ALandscape* Landscape,
	const ULandscapeInfo* LandscapeInfo, const FIntRect& LandscapeExtent)  // Though we could get LandscapeInfo and LandscapeExtent from Landscape, we just use the ones gotten before
{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
	static const FString LandscapeInfoFormat = TEXT(R"""(s@delta_info = "{0}";
i@unreal_landscape_subsection_size_quads = {1};
i@unreal_landscape_component_num_sections = {2};
p@unreal_landscape_extent = {3};
s@unreal_material = "{4}";
s@unreal_landscape_hole_material = "{5}";
s@unreal_actor_path = "{6}";
s[]@unreal_landscape_layers = {7};
i[]@unreal_landscape_layers_blend_method = {8};
s[]@unreal_landscape_editlayers = {9};
f[]@unreal_landscape_editlayers_heightmap_alpha = {10};
f[]@unreal_landscape_editlayers_weightmap_alpha = {11};)""");
#else
	static const FString LandscapeInfoFormat = TEXT(R"""(s@delta_info = "{0}";
i@unreal_landscape_subsection_size_quads = {1};
i@unreal_landscape_component_num_sections = {2};
p@unreal_landscape_extent = {3};
s@unreal_material = "{4}";
s@unreal_landscape_hole_material = "{5}";
s@unreal_actor_path = "{6}";
s[]@unreal_landscape_layers = {7};
i[]@unreal_landscape_layers_noweightblend = {8};
s[]@unreal_landscape_editlayers = {9};
f[]@unreal_landscape_editlayers_heightmap_alpha = {10};
f[]@unreal_landscape_editlayers_weightmap_alpha = {11};)""");
#endif

	FString LayerArrayStr("{");
	FString LayerNoWeighBlendArrayStr("{");
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
	TSet<FName> ExistLayers;
	for (const FLandscapeInfoLayerSettings& LayerInfoSettings : LandscapeInfo->Layers)
	{
		ExistLayers.Add(LayerInfoSettings.LayerName);
		LayerArrayStr += TEXT(" \"") + LayerInfoSettings.LayerName.ToString() + TEXT("\",");

		LayerNoWeighBlendArrayStr += (IsValid(LayerInfoSettings.LayerInfoObj) ?
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			FString::Printf(TEXT(" %d,"), int32(LayerInfoSettings.LayerInfoObj->GetBlendMethod())) : TEXT(" -1,"));
#else
			(LayerInfoSettings.LayerInfoObj->bNoWeightBlend ? TEXT(" 1,") : TEXT(" 0,")) : TEXT(" -1,"));
#endif
	}
	if (IsValid(Landscape->LandscapeMaterial))  // After UE5.5, Layers will NOT automatically create from material, so we need get layer names manually
	{
		const TArray<FName>& LayersInMat = ALandscapeProxy::GetLayersFromMaterial(Landscape->LandscapeMaterial);
		for (const FName& LayerName : LayersInMat)
		{
			if (!ExistLayers.Contains(LayerName))
			{
				LayerArrayStr += TEXT(" \"") + LayerName.ToString() + TEXT("\",");
				LayerNoWeighBlendArrayStr += TEXT(" -1,");
			}
		}
	}
#else
	for (const FLandscapeInfoLayerSettings& LayerInfoSettings : LandscapeInfo->Layers)
	{
		LayerArrayStr += TEXT(" \"") + LayerInfoSettings.LayerName.ToString() + TEXT("\",");

		LayerNoWeighBlendArrayStr += (IsValid(LayerInfoSettings.LayerInfoObj) ?
			(LayerInfoSettings.LayerInfoObj->bNoWeightBlend ? TEXT(" 1,") : TEXT(" 0,")) : TEXT(" -1,"));
	}
#endif
	LayerArrayStr.RemoveFromEnd(TEXT(","));
	LayerArrayStr += TEXT(" }");
	LayerNoWeighBlendArrayStr.RemoveFromEnd(TEXT(","));
	LayerNoWeighBlendArrayStr += TEXT(" }");


	FString EditLayerArrayStr("{");
	FString EditLayerHeightmapAlphaArrayStr("{");
	FString EditLayerWeightmapAlphaArrayStr("{");

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
	for (const FLandscapeLayer& EditLayer : Landscape->GetLayersConst())
	{
		EditLayerArrayStr += TEXT(" \"") + EditLayer.EditLayer->GetName().ToString() + TEXT("\",");
		EditLayerHeightmapAlphaArrayStr += TEXT(" ") + FString::SanitizeFloat(EditLayer.EditLayer->GetAlphaForTargetType(ELandscapeToolTargetType::Heightmap)) + TEXT(",");
		EditLayerWeightmapAlphaArrayStr += TEXT(" ") + FString::SanitizeFloat(EditLayer.EditLayer->GetAlphaForTargetType(ELandscapeToolTargetType::Weightmap)) + TEXT(",");
	}
#else
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
	for (const FLandscapeLayer& EditLayer : Landscape->GetLayers())
#else
	for (const FLandscapeLayer& EditLayer : Landscape->LandscapeLayers)
#endif
	{
		EditLayerArrayStr += TEXT(" \"") + EditLayer.Name.ToString() + TEXT("\",");
		EditLayerHeightmapAlphaArrayStr += TEXT(" ") + FString::SanitizeFloat(EditLayer.HeightmapAlpha) + TEXT(",");
		EditLayerWeightmapAlphaArrayStr += TEXT(" ") + FString::SanitizeFloat(EditLayer.WeightmapAlpha) + TEXT(",");
	}
#endif
	EditLayerArrayStr.RemoveFromEnd(TEXT(","));
	EditLayerArrayStr += TEXT(" }");
	EditLayerHeightmapAlphaArrayStr.RemoveFromEnd(TEXT(","));
	EditLayerHeightmapAlphaArrayStr += TEXT(" }");
	EditLayerWeightmapAlphaArrayStr.RemoveFromEnd(TEXT(","));
	EditLayerWeightmapAlphaArrayStr += TEXT(" }");


	const UMaterialInterface* LandscapeMaterial = Landscape->GetLandscapeMaterial();
	const UMaterialInterface* LandscapeHoleMaterial = Landscape->GetLandscapeHoleMaterial();

	TArray<FStringFormatArg> Args
	{
		DeltaInfo,
		FString::FromInt(LandscapeInfo->SubsectionSizeQuads),
		FString::FromInt(LandscapeInfo->ComponentNumSubsections),
		FString::Printf(TEXT("{ %d, %d, %d, %d }"), LandscapeExtent.Min.Y, LandscapeExtent.Min.X, LandscapeExtent.Max.Y, LandscapeExtent.Max.X),
		IsValid(LandscapeMaterial) ? FHoudiniEngineUtils::GetAssetReference(LandscapeMaterial) : FString(),
		IsValid(LandscapeHoleMaterial) ? FHoudiniEngineUtils::GetAssetReference(LandscapeHoleMaterial) : FString(),
		FHoudiniEngineUtils::GetAssetReference(Landscape),
		LayerArrayStr,
		LayerNoWeighBlendArrayStr,
		EditLayerArrayStr,
		EditLayerHeightmapAlphaArrayStr,
		EditLayerWeightmapAlphaArrayStr
	};

	return FHoudiniEngineSetupHeightfieldInput::HapiSetVexScript(SettingsNodeId, FString::Format(*LandscapeInfoFormat, Args));
}



UHoudiniInputMask* UHoudiniInputMask::Create(UHoudiniInput* Input, ALandscape* Landscape)
{
	UHoudiniInputMask* MaskInput = NewObject<UHoudiniInputMask>(Input, NAME_None, RF_Public | RF_Transactional);

	MaskInput->Landscape = Landscape;
	MaskInput->LandscapeName = Landscape->GetFName();
	MaskInput->RequestReimport();

	return MaskInput;
}

ALandscape* UHoudiniInputMask::GetLandscape() const
{
	if (Landscape.IsValid())
		return Landscape.Get();

	Landscape = Cast<ALandscape>(FHoudiniEngine::Get().GetActorByName(LandscapeName, false));
	return Landscape.IsValid() ? Landscape.Get() : nullptr;
}

TSoftObjectPtr<UObject> UHoudiniInputMask::GetObject() const
{
	return GetLandscape();
}

void UHoudiniInputMask::SetNewLandscape(ALandscape* InLandscape)
{
	if (!IsValid(InLandscape))
		return;

	Landscape = InLandscape;
	LandscapeName = InLandscape->GetFName();
	Tiles.Empty();
	if (OnChangedDelegate.IsBound())
		OnChangedDelegate.Broadcast(false);

	RequestReimport();
}


FIntRect UHoudiniInputMask::GetColorData(TArray<FColor>& OutColorData, const FIntRect& Extent) const
{
	const ALandscape* LandscapeActor = GetLandscape();
	const ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();

	FIntRect LandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(LandscapeExtent);
	if (LandscapeExtent == INVALID_LANDSCAPE_EXTENT)
		return INVALID_LANDSCAPE_EXTENT;

	const bool bIsPartialUpdating = Extent != INVALID_LANDSCAPE_EXTENT;
	const FIntRect& TargetExtent = bIsPartialUpdating ? Extent : LandscapeExtent;

	const FIntVector2 Resolution(TargetExtent.Width() + 1, TargetExtent.Height() + 1);
	OutColorData.SetNumUninitialized(Resolution.X * Resolution.Y);
	FMemory::Memset(OutColorData.GetData(), 255, OutColorData.Num() * sizeof(FColor));
	if (!Tiles.IsEmpty())
	{
		const int32 TileSize = LandscapeInfo->SubsectionSizeQuads * LandscapeInfo->ComponentNumSubsections * 2 + 1;  // An LandscapeProxy always has 2x2 components

		const int32& StartX = TargetExtent.Min.X;
		const int32& StartY = TargetExtent.Min.Y;
		const int32& EndX = TargetExtent.Max.X;
		const int32& EndY = TargetExtent.Max.Y;

		const int32 StartTileIdX = FMath::Max(FMath::FloorToInt((StartX - 1.0) / (TileSize - 1.0)), 0);
		const int32 StartTileIdY = FMath::Max(FMath::FloorToInt((StartY - 1.0) / (TileSize - 1.0)), 0);
		const int32 EndTileIdX = FMath::Min(EndX / (TileSize - 1), LandscapeExtent.Max.X / (TileSize - 1) - 1);
		const int32 EndTileIdY = FMath::Min(EndY / (TileSize - 1), LandscapeExtent.Max.Y / (TileSize - 1) - 1);

		const EHoudiniMaskType& MaskType = GetMaskType();
		FColor ByteMaskColorStack[256];
		if (MaskType == EHoudiniMaskType::Byte)
		{
			FMemory::Memset(ByteMaskColorStack, 255, 256 * sizeof(FColor));
			for (int32 LayerIdx = 0; LayerIdx < ByteValues.Num(); ++LayerIdx)
				ByteMaskColorStack[ByteValues[LayerIdx]] = ByteColors[LayerIdx];
		}
		
		for (int32 TileIdX = StartTileIdX; TileIdX <= EndTileIdX; ++TileIdX)
		{
			for (int32 TileIdY = StartTileIdY; TileIdY <= EndTileIdY; ++TileIdY)
			{
				if (const FHoudiniMaskTile* FoundTilePtr = Tiles.Find(FIntVector2(TileIdX, TileIdY)))
				{
					const uint8* TileData = FoundTilePtr->GetConstData();

					const int32 TileStartX = TileIdX * (TileSize - 1);
					const int32 TileStartY = TileIdY * (TileSize - 1);
					const int32 StartAbsX = FMath::Max(TileStartX, StartX);
					const int32 StartAbsY = FMath::Max(TileStartY, StartY);
					const int32 EndAbsX = FMath::Min((TileIdX + 1) * (TileSize - 1), EndX);
					const int32 EndAbsY = FMath::Min((TileIdY + 1) * (TileSize - 1), EndY);

					for (int32 Y = StartAbsY; Y <= EndAbsY; ++Y)
					{
						const int32 TileYOffset = ((Y - TileStartY) * TileSize) - TileStartX;
						const int32 DataYOffset = ((Y - StartY) * Resolution.X) - StartX;
						for (int32 X = StartAbsX; X <= EndAbsX; ++X)
						{
							const uint8& PixelValue = TileData[TileYOffset + X];
							FColor& ColorValue = OutColorData[DataYOffset + X];
							switch (MaskType)
							{
							case EHoudiniMaskType::Bit: ColorValue = PixelValue ? FColor::Red : FColor::White; break;
							case EHoudiniMaskType::Weight: ColorValue = FColor(255, 255 - PixelValue, 255 - PixelValue, 255); break;
							case EHoudiniMaskType::Byte: ColorValue = ByteMaskColorStack[PixelValue]; break;
							default: break;
							}
						}
					}

					if (!FoundTilePtr->HasMutableData())  // We should NOT lock source when brushing, as they will be locked on brush end
						FoundTilePtr->ForceCommit();
				}
			}
		}
	}

	if (!bIsPartialUpdating)
		return LandscapeExtent;

	// Convert to local extent
	const int32& LandscapeMinX = LandscapeExtent.Min.X;
	const int32& LandscapeMinY = LandscapeExtent.Min.Y;
	return FIntRect(TargetExtent.Min.X - LandscapeMinX, TargetExtent.Min.Y - LandscapeMinY,
					TargetExtent.Max.X - LandscapeMinX, TargetExtent.Max.Y - LandscapeMinY);
}

void FHoudiniMaskTile::Init(UObject* Outer, const int32& TileIdX, const int32& TileIdY,
	const int32& TileSize, const uint8* TileData)
{
	Texture = NewObject<UTexture2D>(Outer, FName(*FString::Printf(TEXT("Tile_%d_%d"), TileIdX, TileIdY)), RF_Public | RF_Transactional);
	Texture->Source.Init(TileSize, TileSize, 1, 1, TSF_G8, TileData);
}

FIntRect UHoudiniInputMask::UpdateData(const FVector& BrushPosition, const float& BrushSize, const float& BrushFallOff,
	const uint8& Value, const bool& bInversed)
{
	const ALandscape* LandscapeActor = GetLandscape();
	
	const FTransform LandscapeTransform = LandscapeActor->GetTransform();
	const FVector BrushPixelPosition = LandscapeTransform.InverseTransformPosition(BrushPosition);
	const FVector BrushPixelRadius = FVector(BrushSize) / LandscapeTransform.GetScale3D() * 0.5;

	const ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();
	const int32 TileSize = LandscapeInfo->SubsectionSizeQuads * LandscapeInfo->ComponentNumSubsections * 2 + 1;  // A LandscapeProxy always has 2x2 components
	FIntRect LandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(LandscapeExtent);

	// Absolute extent to update
	const int32 StartX = FMath::Max(FMath::FloorToInt(BrushPixelPosition.X - BrushPixelRadius.X), LandscapeExtent.Min.X);
	const int32 StartY = FMath::Max(FMath::FloorToInt(BrushPixelPosition.Y - BrushPixelRadius.Y), LandscapeExtent.Min.Y);
	const int32 EndX = FMath::Min(FMath::CeilToInt(BrushPixelPosition.X + BrushPixelRadius.X), LandscapeExtent.Max.X);
	const int32 EndY = FMath::Min(FMath::CeilToInt(BrushPixelPosition.Y + BrushPixelRadius.Y), LandscapeExtent.Max.Y);

	const int32 StartTileIdX = FMath::Max(FMath::FloorToInt((StartX - 1.0) / (TileSize - 1.0)), 0);
	const int32 StartTileIdY = FMath::Max(FMath::FloorToInt((StartY - 1.0) / (TileSize - 1.0)), 0);
	const int32 EndTileIdX = FMath::Min(EndX / (TileSize - 1), LandscapeExtent.Max.X / (TileSize - 1) - 1);
	const int32 EndTileIdY = FMath::Min(EndY / (TileSize - 1), LandscapeExtent.Max.Y / (TileSize - 1) - 1);
	
	const float DistScaleX = 1.0f / BrushPixelRadius.X;
	const float DistScaleY = 1.0f / BrushPixelRadius.Y;
	FIntRect UpdatedExtent = INVALID_LANDSCAPE_EXTENT;  // Absolute extent
	for (int32 TileIdX = StartTileIdX; TileIdX <= EndTileIdX; ++TileIdX)
	{
		for (int32 TileIdY = StartTileIdY; TileIdY <= EndTileIdY; ++TileIdY)
		{
			const int32 TileStartX = TileIdX * (TileSize - 1);
			const int32 TileStartY = TileIdY * (TileSize - 1);
			const int32 StartAbsX = FMath::Max(TileStartX, StartX);
			const int32 StartAbsY = FMath::Max(TileStartY, StartY);
			const int32 EndAbsX = FMath::Min((TileIdX + 1) * (TileSize - 1), EndX);
			const int32 EndAbsY = FMath::Min((TileIdY + 1) * (TileSize - 1), EndY);

			const FIntVector2 TileId(TileIdX, TileIdY);

			uint8* TileData = nullptr;
			TArray<uint8> TileDataArray;
			FHoudiniMaskTile* TilePtr = Tiles.Find(TileId);
			if (TilePtr)
				TileData = TilePtr->GetData();
			else
			{
				TileDataArray.SetNumZeroed(TileSize * TileSize);
				TileData = TileDataArray.GetData();
			}

			for (int32 X = StartAbsX; X <= EndAbsX; ++X)
			{
				const float DistRatioX = (X - BrushPixelPosition.X) * DistScaleX;
				const int32 LocalX = X - TileStartX;
				for (int32 Y = StartAbsY; Y <= EndAbsY; ++Y)
				{
					const float DistRatioY = (Y - BrushPixelPosition.Y) * DistScaleY;
					const int32 LocalY = Y - TileStartY;
					
					const size_t PixelIdx = LocalY * TileSize + LocalX;
					uint8& OldValue = TileData[PixelIdx];
					uint8 NewValue = OldValue;
					if (BrushFallOff <= 0.0001f)  // Hard Brush
					{
						if (DistRatioX * DistRatioX + DistRatioY * DistRatioY < 1.0f)
							NewValue = bInversed ? 0 : Value;
					}
					else
					{
						const float Bias = (1.0f - FMath::Sqrt(DistRatioX * DistRatioX + DistRatioY * DistRatioY)) / BrushFallOff;  // 0: edge, 1: center
						if (Bias > 0.0f)
						{
							if (Bias >= 1.0f)
								NewValue = bInversed ? 0 : Value;
							else
							{
								NewValue = bInversed ? FMath::Lerp(Value, 0, Bias) : FMath::Lerp(0, Value, Bias);
								NewValue = bInversed ? FMath::Min(NewValue, OldValue) : FMath::Max(NewValue, OldValue);
							}
						}
					}

					if (OldValue != NewValue)
					{
						OldValue = NewValue;
						if (UpdatedExtent == INVALID_LANDSCAPE_EXTENT)
							UpdatedExtent = FIntRect(X, Y, X, Y);
						else
							UpdatedExtent.Include(FInt32Point(X, Y));
					}
				}
			}

			bool bAllZero = true;
			for (int32 PixelIdx = TileSize * TileSize - 1; PixelIdx >= 0; --PixelIdx)
			{
				if (TileData[PixelIdx] >= 1)
				{
					bAllZero = false;
					break;
				}
			}

			if (bAllZero)
			{
				if (TilePtr)
				{
					TilePtr->Commit();
					Tiles.Remove(TileId);
				}
			}
			else if (!TilePtr)
				Tiles.Add(TileId).Init(this, TileIdX, TileIdY, TileSize, TileData);
		}
	}

	if (UpdatedExtent != INVALID_LANDSCAPE_EXTENT)
	{
		bHasChanged = true;
		if (ChangedExtent == INVALID_LANDSCAPE_EXTENT)
			ChangedExtent = UpdatedExtent;
		else
			ChangedExtent.Union(UpdatedExtent);
	}

	return UpdatedExtent;
}

FIntRect UHoudiniInputMask::GetValueExtent(const uint8& Value) const
{
	const ALandscape* LandscapeActor = GetLandscape();
	const ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();

	FIntRect LandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(LandscapeExtent);

	const int32 TileSize = LandscapeInfo->SubsectionSizeQuads * LandscapeInfo->ComponentNumSubsections * 2 + 1;  // An LandscapeProxy always has 2x2 components

	const int32& StartX = LandscapeExtent.Min.X;
	const int32& StartY = LandscapeExtent.Min.Y;
	const int32& EndX = LandscapeExtent.Max.X;
	const int32& EndY = LandscapeExtent.Max.Y;

	const int32 StartTileIdX = FMath::Max(FMath::FloorToInt((StartX - 1.0) / (TileSize - 1.0)), 0);
	const int32 StartTileIdY = FMath::Max(FMath::FloorToInt((StartY - 1.0) / (TileSize - 1.0)), 0);
	const int32 EndTileIdX = FMath::Min(EndX / (TileSize - 1), LandscapeExtent.Max.X / (TileSize - 1) - 1);
	const int32 EndTileIdY = FMath::Min(EndY / (TileSize - 1), LandscapeExtent.Max.Y / (TileSize - 1) - 1);

	FIntRect ValueExtent = INVALID_LANDSCAPE_EXTENT;
	for (int32 TileIdX = StartTileIdX; TileIdX <= EndTileIdX; ++TileIdX)
	{
		for (int32 TileIdY = StartTileIdY; TileIdY <= EndTileIdY; ++TileIdY)
		{
			if (const FHoudiniMaskTile* FoundTilePtr = Tiles.Find(FIntVector2(TileIdX, TileIdY)))
			{
				const uint8* TileData = FoundTilePtr->GetConstData();

				const int32 TileStartX = TileIdX * (TileSize - 1);
				const int32 TileStartY = TileIdY * (TileSize - 1);
				const int32 StartAbsX = FMath::Max(TileStartX, StartX);
				const int32 StartAbsY = FMath::Max(TileStartY, StartY);
				const int32 EndAbsX = FMath::Min((TileIdX + 1) * (TileSize - 1), EndX);
				const int32 EndAbsY = FMath::Min((TileIdY + 1) * (TileSize - 1), EndY);

				for (int32 Y = StartAbsY; Y <= EndAbsY; ++Y)
				{
					const int32 TileYOffset = ((Y - TileStartY) * TileSize) - TileStartX;
					for (int32 X = StartAbsX; X <= EndAbsX; ++X)
					{
						if (TileData[TileYOffset + X] == Value)
						{
							if (ValueExtent == INVALID_LANDSCAPE_EXTENT)
								ValueExtent = FIntRect(X, Y, X, Y);
							else
								ValueExtent.Include(FInt32Point(X, Y));
						}
					}
				}

				FoundTilePtr->ForceCommit();
			}
		}
	}

	return ValueExtent;
}

FIntRect UHoudiniInputMask::UpdateData(const FVector& BrushPosition, const float& BrushSize, const float& BrushFallOff,
	const uint8& Value, const bool& bInversed, TArray<FColor>& OutColorData)  // Return changed local extent
{
	const FIntRect UpdatedExtent = UpdateData(BrushPosition, BrushSize, BrushFallOff, Value, bInversed);
	if (UpdatedExtent == INVALID_LANDSCAPE_EXTENT)
		return INVALID_LANDSCAPE_EXTENT;

	return GetColorData(OutColorData, UpdatedExtent);
}

FIntRect UHoudiniInputMask::UpdateData(TConstArrayView<uint8> Data, const FIntRect& Extent)
{
	const ALandscape* LandscapeActor = GetLandscape();
	const ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();
	const int32 TileSize = LandscapeInfo->SubsectionSizeQuads * LandscapeInfo->ComponentNumSubsections * 2 + 1;  // An LandscapeProxy always has 2x2 components
	FIntRect LandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(LandscapeExtent);

	// Absolute extent to update
	const int32 StartX = FMath::Max(Extent.Min.X, LandscapeExtent.Min.X);
	const int32 StartY = FMath::Max(Extent.Min.Y, LandscapeExtent.Min.Y);
	const int32 EndX = FMath::Min(Extent.Max.X, LandscapeExtent.Max.X);
	const int32 EndY = FMath::Min(Extent.Max.Y, LandscapeExtent.Max.Y);

	const int32 StartTileIdX = FMath::Max(FMath::FloorToInt((StartX - 1.0) / (TileSize - 1.0)), 0);
	const int32 StartTileIdY = FMath::Max(FMath::FloorToInt((StartY - 1.0) / (TileSize - 1.0)), 0);
	const int32 EndTileIdX = FMath::Min(EndX / (TileSize - 1), LandscapeExtent.Max.X / (TileSize - 1) - 1);
	const int32 EndTileIdY = FMath::Min(EndY / (TileSize - 1), LandscapeExtent.Max.Y / (TileSize - 1) - 1);

	FIntRect UpdatedExtent = INVALID_LANDSCAPE_EXTENT;  // Absolute extent
	const int32 DataSizeX = Extent.Width() + 1;
	for (int32 TileIdX = StartTileIdX; TileIdX <= EndTileIdX; ++TileIdX)
	{
		for (int32 TileIdY = StartTileIdY; TileIdY <= EndTileIdY; ++TileIdY)
		{
			const int32 TileStartX = TileIdX * (TileSize - 1);
			const int32 TileStartY = TileIdY * (TileSize - 1);
			const int32 StartAbsX = FMath::Max(TileStartX, StartX);
			const int32 StartAbsY = FMath::Max(TileStartY, StartY);
			const int32 EndAbsX = FMath::Min((TileIdX + 1) * (TileSize - 1), EndX);
			const int32 EndAbsY = FMath::Min((TileIdY + 1) * (TileSize - 1), EndY);

			const FIntVector2 TileId(TileIdX, TileIdY);

			uint8* TileData = nullptr;
			TArray<uint8> TileDataArray;
			FHoudiniMaskTile* TilePtr = Tiles.Find(TileId);
			if (TilePtr)
				TileData = TilePtr->GetData();
			else
			{
				TileDataArray.SetNumZeroed(TileSize * TileSize);
				TileData = TileDataArray.GetData();
			}

			for (int32 Y = StartAbsY; Y <= EndAbsY; ++Y)
			{
				const int32 TileYOffset = ((Y - TileStartY) * TileSize) - TileStartX;
				const int32 DataYOffset = ((Y - Extent.Min.Y) * DataSizeX) - Extent.Min.X;
				for (int32 X = StartAbsX; X <= EndAbsX; ++X)
				{
					uint8& OldValue = TileData[TileYOffset + X];
					const uint8 NewValue = Data[DataYOffset + X];
					if (OldValue != NewValue)
					{
						OldValue = NewValue;
						if (UpdatedExtent == INVALID_LANDSCAPE_EXTENT)
							UpdatedExtent = FIntRect(X, Y, X, Y);
						else
							UpdatedExtent.Include(FInt32Point(X, Y));
					}
				}
			}

			bool bAllZero = true;
			for (int32 PixelIdx = TileSize * TileSize - 1; PixelIdx >= 0; --PixelIdx)
			{
				if (TileData[PixelIdx] >= 1)
				{
					bAllZero = false;
					break;
				}
			}

			if (bAllZero)
			{
				if (TilePtr)
				{
					TilePtr->Commit();
					Tiles.Remove(TileId);
				}
			}
			else if (!TilePtr)
				Tiles.Add(TileId).Init(this, TileIdX, TileIdY, TileSize, TileData);
		}
	}

	if (UpdatedExtent != INVALID_LANDSCAPE_EXTENT)
	{
		bHasChanged = true;
		if (ChangedExtent == INVALID_LANDSCAPE_EXTENT)
		{
			ChangedExtent = UpdatedExtent;
			Modify();
		}
		else
			ChangedExtent.Union(UpdatedExtent);
	}

	return UpdatedExtent;
}

bool UHoudiniInputMask::HapiUploadData()
{
	bool bNeedReimport = bHasChanged;
	bHasChanged = false;

	// -------- Process byte mask value parms --------
	const EHoudiniMaskType& MaskType = GetMaskType();
	if (MaskType == EHoudiniMaskType::Byte)
	{
		const int32& NodeId = GetInput()->GetParentNodeId();

		bool bByteMaskValueExists[256] = { false };
		TArray<std::string> ByteValueParms;
		TArray<uint8> NewByteValues;
		for (int32 ParmInstIdx = 0; ParmInstIdx < 256; ++ParmInstIdx)
		{
			const std::string ParmName = TCHAR_TO_UTF8(*(GetInput()->GetInputName() +
				TEXT(HAPI_PARM_SUFFIX_BYTE_MASK_VALUE) + FString::FromInt(ParmInstIdx)));
			int32 ByteParmValue;
			HAPI_Result Result = FHoudiniApi::GetParmIntValue(FHoudiniEngine::Get().GetSession(), NodeId,
				ParmName.c_str(), 0, &ByteParmValue);
			if (Result != HAPI_RESULT_SUCCESS)
			{
				if (ParmInstIdx == 0)  // Maybe multiparm offset is 1
					continue;

				break;
			}

			ByteValueParms.Add(ParmName);
			NewByteValues.Add(ByteParmValue);
			if (ByteParmValue < 256)
				bByteMaskValueExists[ByteParmValue] = true;
		}

		// Set all invalid byte value >= 1
		for (int32 ByteLayerIdx = 0; ByteLayerIdx < NewByteValues.Num(); ++ByteLayerIdx)
		{
			uint8& NewByteValue = NewByteValues[ByteLayerIdx];
			if (NewByteValue <= 0)  // Invalid byte value, must be [1 - 255]
			{
				for (NewByteValue = 1; NewByteValue < 255; ++NewByteValue)  // Find a byte value that hasn't been used
				{
					if (!bByteMaskValueExists[NewByteValue])
						break;
				}

				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), NodeId,
					ByteValueParms[ByteLayerIdx].c_str(), 0, int(NewByteValue)));

				bByteMaskValueExists[NewByteValue] = true;
			}
		}

		TArray<FColor> NewByteColors;
		for (const uint8& NewByteValue : NewByteValues)
		{
			const int32 FoundOldIdx = ByteValues.IndexOfByKey(NewByteValue);
			NewByteColors.Add(ByteColors.IsValidIndex(FoundOldIdx) ? ByteColors[FoundOldIdx] : FColor::MakeRandomColor());
		}

		const bool bByteParmChanged = (NewByteValues != ByteValues);
		if (bByteParmChanged)
			bNeedReimport = true;

		if (OnChangedDelegate.IsBound() && bByteParmChanged)
		{
			// Before broadcast changed, we should update ByteValues so that the object bind to this will update mask visualization properly
			ByteValues = NewByteValues;  
			ByteColors = NewByteColors;

			OnChangedDelegate.Broadcast(false);
		}
		else
		{
			ByteValues = NewByteValues;
			ByteColors = NewByteColors;
		}
	}

	if (!bNeedReimport)
		return true;

	// -------- Upload data --------
	const ALandscape* LandscapeActor = GetLandscape();
	
	const ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();
	FIntRect LandscapeExtent;
	LandscapeInfo->GetLandscapeExtent(LandscapeExtent);
	if (HasData())
	{
		const int32 TileSize = LandscapeInfo->SubsectionSizeQuads * LandscapeInfo->ComponentNumSubsections * 2 + 1;  // An LandscapeProxy always has 2x2 components

		const FIntVector2 Resolution(LandscapeExtent.Width() + 1, LandscapeExtent.Height() + 1);
		FHoudiniSharedMemoryVolumeInput SHMVolumeInput(EHoudiniVolumeConvertDataType::Uint8, EHoudiniVolumeStorageType::Float,
			FIntVector3(Resolution.Y, Resolution.X, 1));  // X and Y is swaped in houdini
		bool bSHMExists = false;
		float* const SHM = SHMVolumeInput.GetSharedMemory(FString::Printf(TEXT("%08X_%d_%d_%d_%d"), (size_t)this,
				LandscapeExtent.Min.X, LandscapeExtent.Min.Y, LandscapeExtent.Max.X, LandscapeExtent.Max.Y), MaskHandle, bSHMExists);

		const bool bPartialUpdate = bSHMExists && (ChangedExtent != INVALID_LANDSCAPE_EXTENT);
		const FIntRect Extent = bPartialUpdate ? ChangedExtent : LandscapeExtent;

		uint8* DataPtr = (uint8*)SHM;

		bool bByteValueExists[256] = { false };
		if (MaskType == EHoudiniMaskType::Byte)
		{
			for (const uint8& ByteValue : ByteValues)
				bByteValueExists[ByteValue] = true;
		}

		const int32& LandscapeStartX = LandscapeExtent.Min.X;
		const int32& LandscapeStartY = LandscapeExtent.Min.Y;

		// Absolute extent to update
		const int32& StartX = Extent.Min.X;
		const int32& StartY = Extent.Min.Y;
		const int32& EndX = Extent.Max.X;
		const int32& EndY = Extent.Max.Y;

		const int32 StartTileIdX = FMath::Max(FMath::FloorToInt((StartX - 1.0) / (TileSize - 1.0)), 0);
		const int32 StartTileIdY = FMath::Max(FMath::FloorToInt((StartY - 1.0) / (TileSize - 1.0)), 0);
		const int32 EndTileIdX = FMath::Min(EndX / (TileSize - 1), LandscapeExtent.Max.X / (TileSize - 1) - 1);
		const int32 EndTileIdY = FMath::Min(EndY / (TileSize - 1), LandscapeExtent.Max.Y / (TileSize - 1) - 1);


		if (bPartialUpdate)
		{
			const int32 LengthY = Extent.Max.Y - Extent.Min.Y + 1;
			if (LengthY >= 1)
			{
				for (int32 X = Extent.Min.X; X <= Extent.Max.X; ++X)
					FMemory::Memzero(DataPtr + ((X - LandscapeStartX) * Resolution.Y) - LandscapeStartY + Extent.Min.Y, LengthY);
			}
		}
		else
			FMemory::Memzero(SHM, Resolution.Y * Resolution.X);

		for (int32 TileIdX = StartTileIdX; TileIdX <= EndTileIdX; ++TileIdX)
		{
			for (int32 TileIdY = StartTileIdY; TileIdY <= EndTileIdY; ++TileIdY)
			{
				if (const FHoudiniMaskTile* FoundTilePtr = Tiles.Find(FIntVector2(TileIdX, TileIdY)))
				{
					const uint8* TileDataPtr = FoundTilePtr->GetConstData();

					const int32 TileStartX = TileIdX * (TileSize - 1);
					const int32 TileStartY = TileIdY * (TileSize - 1);
					const int32 StartAbsX = FMath::Max(TileStartX, StartX);
					const int32 StartAbsY = FMath::Max(TileStartY, StartY);
					const int32 EndAbsX = FMath::Min((TileIdX + 1) * (TileSize - 1), EndX);
					const int32 EndAbsY = FMath::Min((TileIdY + 1) * (TileSize - 1), EndY);

					for (int32 X = StartAbsX; X <= EndAbsX; ++X)
					{
						const int32 LocalX = X - TileStartX;
						for (int32 Y = StartAbsY; Y <= EndAbsY; ++Y)
						{
							const int32 LocalY = Y - TileStartY;
							const uint8& PixelValue = TileDataPtr[LocalY * TileSize + LocalX];
							uint8& DataValue = DataPtr[(Y - LandscapeStartY) + (X - LandscapeStartX) * Resolution.Y];
							switch (MaskType)
							{
							case EHoudiniMaskType::Bit: DataValue = (PixelValue == 0) ? 0 : 255; break;
							case EHoudiniMaskType::Weight: DataValue = PixelValue; break;
							case EHoudiniMaskType::Byte: DataValue = bByteValueExists[PixelValue] ? PixelValue : 0; break;
							default: break;
							}
						}
					}

					FoundTilePtr->ForceCommit();
				}
			}
		}


		if (bPartialUpdate)
		{
			HOUDINI_FAIL_RETURN(SHMVolumeInput.HapiPartialUpload(MaskNodeId, SHM, true,
				FIntVector3(ChangedExtent.Min.Y, ChangedExtent.Min.X, 0), FIntVector3(ChangedExtent.Max.Y, ChangedExtent.Max.X, 1)));  // X and Y is swaped in houdini
		}
		else
		{
			HOUDINI_FAIL_RETURN(SHMVolumeInput.HapiUpload(MaskNodeId, SHM, FVector2f(0.0f, 1.0f),
				TEXT("mask"), false, FIntVector3::ZeroValue, FIntVector3::ZeroValue, false));
		}

		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiSetLandscapeTransform(SettingsNodeId,
			LandscapeActor->GetActorTransform(), LandscapeExtent));
	}
	else  // Data hasn't been initialized, so we just create a empty volume
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiSetResolution(SettingsNodeId,
			true, FIntVector2(LandscapeExtent.Height() + 1, LandscapeExtent.Width() + 1)));  // X and Y is swaped in houdini

		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiSetLandscapeTransform(SettingsNodeId,
			LandscapeActor->GetActorTransform(), LandscapeExtent));
	}

	HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiSetVexScript(SettingsNodeId,
		TEXT("s@delta_info = \"") + (GetInput()->GetInputName()) / TEXT("mask") / TEXT("bit") /
		(ChangedExtent == INVALID_LANDSCAPE_EXTENT ? FString("*") : FString::Printf(TEXT("%d,%d-%d,%d"),
			EXPAND_EXTENT_TO_HOUDINI(ChangedExtent, LandscapeExtent))) + TEXT("\";")));

	ChangedExtent = INVALID_LANDSCAPE_EXTENT;

	return true;
}

bool UHoudiniInputMask::HapiUpload()
{
	if (SettingsNodeId < 0)
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiCreateNode(GetGeoNodeId(),
			FString::Printf(TEXT("landscape_mask_%08X"), FPlatformTime::Cycles()), SettingsNodeId));

		HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}

	if (MaskNodeId < 0 && !Tiles.IsEmpty())
	{
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryVolumeInput::HapiCreateNode(GetGeoNodeId(),
			FString::Printf(TEXT("mask_%08X"), FPlatformTime::Cycles()), MaskNodeId));

		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupHeightfieldInput::HapiConnectInput(SettingsNodeId, MaskNodeId));
	}
	else if (MaskNodeId >= 0 && Tiles.IsEmpty())
	{
		if (MaskHandle)
		{
			FHoudiniEngineUtils::CloseSharedMemoryHandle(MaskHandle);
			MaskHandle = 0;
		}

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MaskNodeId));
		MaskNodeId = -1;
	}

	return true;
}

bool UHoudiniInputMask::HapiDestroy()
{
	if (MaskNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MaskNodeId));

	if (SettingsNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));

	Invalidate();

	return true;
}

void UHoudiniInputMask::Invalidate()
{
	MaskNodeId = -1;
	SettingsNodeId = -1;
	if (MaskHandle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(MaskHandle);
		MaskHandle = 0;
	}
}
