// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeEdit.h"
#include "LandscapeConfigHelper.h"
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
#include "LandscapeEditLayer.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAttribute.h"
#include "HoudiniNode.h"


#define INVALID_HEIGHTFIELD_EXTENT FIntRect(MAX_int32, MAX_int32, MIN_int32, MIN_int32)  // See ULandscapeInfo::GetLandscapeExtent

bool FHoudiniLandscapeOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = false;
	bOutShouldHoldByOutput = true;
	if (PartInfo.type == HAPI_PARTTYPE_VOLUME)
	{
		bOutIsValid = true;

		const int32& PartId = PartInfo.id;
		int32 AttribValue = 0;
		HAPI_AttributeInfo AttribInfo;
		for (int32 OwnerIdx = 0; OwnerIdx < HAPI_ATTROWNER_MAX; ++OwnerIdx)
		{
			const HAPI_AttributeOwner Owner = HAPI_AttributeOwner((OwnerIdx + HAPI_ATTROWNER_PRIM) % HAPI_ATTROWNER_MAX);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_LANDSCAPE_OUTPUT_MODE, Owner, &AttribInfo));

			if (AttribInfo.exists && FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int)
				break;
			else
				AttribInfo.exists = false;
		}

		if (AttribInfo.exists)
		{
			AttribInfo.tupleSize = 1;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_LANDSCAPE_OUTPUT_MODE, &AttribInfo, 0, &AttribValue, 0, 1));
			if (AttribInfo.exists && AttribValue >= 1)
			{
				bOutShouldHoldByOutput = false;
				return true;
			}
		}
	}

	return true;
}

struct FHoudiniLandscapeOutputHelper
{
protected:
	TArray<FLandscapeEditDataInterface*> LandscapeEdits;  // As LandscapeEdit will restore the uncompressed textures, which we could reuse when output multiple layers

	static void DeleteLayerAllocation(ULandscapeComponent* Component, const FGuid& InEditLayerGuid, int32 InLayerAllocationIdx, bool bInShouldDirtyPackage);

	static bool DeleteLayerIfAllZero(ULandscapeComponent* const Component, const uint8* const TexDataPtr, int32 TexSize, int32 LayerIdx, bool bShouldDirtyPackage);

	FORCEINLINE FLandscapeEditDataInterface* FindOrCreateLandscapeEdit(ULandscapeInfo* LandscapeInfo)
	{
		int32 FoundEditIdx = LandscapeEdits.IndexOfByPredicate(
			[LandscapeInfo](const FLandscapeEditDataInterface* LandscapeEdit) { return LandscapeEdit->GetTargetLandscape() == LandscapeInfo->LandscapeActor; });
		if (!LandscapeEdits.IsValidIndex(FoundEditIdx))
		{
			FLandscapeEditDataInterface* NewLandscapeEdit = new FLandscapeEditDataInterface(LandscapeInfo);
			NewLandscapeEdit->SetShouldDirtyPackage(false);  // We should modify the LandscapeComponents ourselves
			FoundEditIdx = LandscapeEdits.Add(NewLandscapeEdit);
		}
		
		return LandscapeEdits[FoundEditIdx];
	}

public:
	void SetWeightData(ULandscapeInfo* LandscapeInfo, ULandscapeLayerInfoObject* const LayerInfo,
		const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint8* Data, const uint8* MaskData, const bool& bForceModify);

	void SetHeightData(ULandscapeInfo* LandscapeInfo,
		const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint16* InData, const uint8* MaskData);

	FORCEINLINE void Release()
	{
		for (FLandscapeEditDataInterface* LandscapeEdit : LandscapeEdits)
			delete LandscapeEdit;

		LandscapeEdits.Empty();
	}

	~FHoudiniLandscapeOutputHelper()
	{
		Release();
	}
};

// See ULandscapeComponent::DeleteLayerAllocation
void FHoudiniLandscapeOutputHelper::DeleteLayerAllocation(ULandscapeComponent* Component, const FGuid& InEditLayerGuid, int32 InLayerAllocationIdx, bool bInShouldDirtyPackage)
{
	TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = Component->GetWeightmapLayerAllocations(InEditLayerGuid);
	TArray<TObjectPtr<UTexture2D>>& ComponentWeightmapTextures = Component->GetWeightmapTextures(InEditLayerGuid);
	TArray<TObjectPtr<ULandscapeWeightmapUsage>>& ComponentWeightmapTexturesUsage = Component->GetWeightmapTexturesUsage(InEditLayerGuid);

	const FWeightmapLayerAllocationInfo& LayerAllocation = ComponentWeightmapLayerAllocations[InLayerAllocationIdx];
	const int32 DeleteLayerWeightmapTextureIndex = LayerAllocation.WeightmapTextureIndex;

	ALandscapeProxy* Proxy = Component->GetLandscapeProxy();
	Component->Modify(bInShouldDirtyPackage);
	Proxy->Modify(bInShouldDirtyPackage);

	// Mark the weightmap channel as unallocated, so we can reuse it later
	ULandscapeWeightmapUsage* Usage = ComponentWeightmapTexturesUsage.IsValidIndex(DeleteLayerWeightmapTextureIndex) ? ComponentWeightmapTexturesUsage[DeleteLayerWeightmapTextureIndex] : nullptr;
	if (Usage) // can be null if WeightmapUsageMap hasn't been built yet
	{
		Usage->ChannelUsage[LayerAllocation.WeightmapTextureChannel] = nullptr;
	}

	// Remove the layer:
	ComponentWeightmapLayerAllocations.RemoveAt(InLayerAllocationIdx);

	// Check if the weightmap texture used by the material layer we just removed is used by any other material layer -- if not then we can remove the texture from the local list (as it's not used locally)
	bool bCanRemoveLayerTexture = !ComponentWeightmapLayerAllocations.ContainsByPredicate([DeleteLayerWeightmapTextureIndex](const FWeightmapLayerAllocationInfo& Allocation) { return Allocation.WeightmapTextureIndex == DeleteLayerWeightmapTextureIndex; });
	if (bCanRemoveLayerTexture)
	{
		// Make sure the texture can be garbage collected, if necessary
		ComponentWeightmapTextures[DeleteLayerWeightmapTextureIndex]->ClearFlags(RF_Standalone);

		// Remove from our local list of textures and usages
		ComponentWeightmapTextures.RemoveAt(DeleteLayerWeightmapTextureIndex);
		if (Usage)
		{
			ComponentWeightmapTexturesUsage.RemoveAt(DeleteLayerWeightmapTextureIndex);
		}

		// Adjust WeightmapTextureIndex for other allocations (as we just reordered the Weightmap list with the deletions above)
		for (FWeightmapLayerAllocationInfo& Allocation : ComponentWeightmapLayerAllocations)
		{
			if (Allocation.WeightmapTextureIndex > DeleteLayerWeightmapTextureIndex)
			{
				Allocation.WeightmapTextureIndex--;
			}
			check(Allocation.WeightmapTextureIndex < ComponentWeightmapTextures.Num());
		}
	}

	//Proxy->ValidateProxyLayersWeightmapUsage();
}

// See DeleteLayerIfAllZero in "LandscapeEditInterface.cpp"
bool FHoudiniLandscapeOutputHelper::DeleteLayerIfAllZero(ULandscapeComponent* const Component, const uint8* const TexDataPtr, int32 TexSize, int32 LayerIdx, bool bShouldDirtyPackage)
{
	// Check the data for the entire component and to see if it's all zero
	for (int32 TexY = 0; TexY < TexSize; TexY++)
	{
		for (int32 TexX = 0; TexX < TexSize; TexX++)
		{
			const int32 TexDataIndex = 4 * (TexX + TexY * TexSize);

			// Stop the first time we see any non-zero data
			uint8 Weight = TexDataPtr[TexDataIndex];
			if (Weight != 0)
			{
				return false;
			}
		}
	}

	DeleteLayerAllocation(Component, Component->GetLandscapeActor()->EditingLayer, LayerIdx, bShouldDirtyPackage);

	return true;
}

// See FLandscapeEditDataInterface::SetAlphaData
void FHoudiniLandscapeOutputHelper::SetWeightData(ULandscapeInfo* LandscapeInfo, ULandscapeLayerInfoObject* const LayerInfo,
	const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint8* Data, const uint8* MaskData, const bool& bForceModify)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputLandscape_SetWeightData);
	
	FLandscapeEditDataInterface& LandscapeEdit = *FindOrCreateLandscapeEdit(LandscapeInfo);
	//FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);

	const bool bNeedUpdateCollision = LayerInfo == ALandscapeProxy::VisibilityLayer;

	static const size_t ChannelOffsets[4] = { STRUCT_OFFSET(FColor,R), STRUCT_OFFSET(FColor,G), STRUCT_OFFSET(FColor,B), STRUCT_OFFSET(FColor,A) };
	const int32 Stride = (1 + X2 - X1);
	const int32& SubsectionSizeQuads = LandscapeInfo->SubsectionSizeQuads;
	const int32& ComponentSizeQuads = LandscapeInfo->ComponentSizeQuads;
	const int32& ComponentNumSubsections = LandscapeInfo->ComponentNumSubsections;

	check(ComponentSizeQuads > 0);
	// Find component range for this block of data
	int32 ComponentIndexX1 = (X1 - 1 >= 0) ? (X1 - 1) / ComponentSizeQuads : (X1) / ComponentSizeQuads - 1;	// -1 because we need to pick up vertices shared between components
	int32 ComponentIndexY1 = (Y1 - 1 >= 0) ? (Y1 - 1) / ComponentSizeQuads : (Y1) / ComponentSizeQuads - 1;
	int32 ComponentIndexX2 = (X2 >= 0) ? X2 / ComponentSizeQuads : (X2 + 1) / ComponentSizeQuads - 1;
	int32 ComponentIndexY2 = (Y2 >= 0) ? Y2 / ComponentSizeQuads : (Y2 + 1) / ComponentSizeQuads - 1;
	TMap<FIntPoint, TMap<const ULandscapeLayerInfoObject*, uint32>> LayerInfluenceCache;

	for (int32 ComponentIndexY = ComponentIndexY1; ComponentIndexY <= ComponentIndexY2; ComponentIndexY++)
	{
		for (int32 ComponentIndexX = ComponentIndexX1; ComponentIndexX <= ComponentIndexX2; ComponentIndexX++)
		{
			FIntPoint ComponentKey(ComponentIndexX, ComponentIndexY);
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ComponentKey);

			// if NULL, there is no component at this location
			if (Component == NULL)
			{
				continue;
			}

			if (LayerInfo == ALandscape::VisibilityLayer && !Component->IsLandscapeHoleMaterialValid())
			{
				continue;
			}

			//Component->Modify(LandscapeEdit.GetShouldDirtyPackage());

			TArray<FWeightmapLayerAllocationInfo>& ComponentWeightmapLayerAllocations = Component->GetWeightmapLayerAllocations(true);
			int32 UpdateLayerIdx = ComponentWeightmapLayerAllocations.IndexOfByPredicate([LayerInfo](const FWeightmapLayerAllocationInfo& Allocation) { return Allocation.LayerInfo == LayerInfo; });

			// Need allocation for weightmap
			if (UpdateLayerIdx == INDEX_NONE)
			{
				const int32 LayerLimit = Component->GetLandscapeProxy()->MaxPaintedLayersPerComponent;

				auto IsDataAllZeroForComponent = [&]() -> bool
					{
						// Find coordinates of box that lies inside component
						const int32 ComponentX1 = FMath::Clamp<int32>(X1 - ComponentIndexX * ComponentSizeQuads, 0, ComponentSizeQuads);
						const int32 ComponentY1 = FMath::Clamp<int32>(Y1 - ComponentIndexY * ComponentSizeQuads, 0, ComponentSizeQuads);
						const int32 ComponentX2 = FMath::Clamp<int32>(X2 - ComponentIndexX * ComponentSizeQuads, 0, ComponentSizeQuads);
						const int32 ComponentY2 = FMath::Clamp<int32>(Y2 - ComponentIndexY * ComponentSizeQuads, 0, ComponentSizeQuads);

						// Find subsection range for this box
						const int32 SubIndexX1 = FMath::Clamp<int32>((ComponentX1 - 1) / SubsectionSizeQuads, 0, ComponentNumSubsections - 1); // -1 because we need to pick up vertices shared between subsections
						const int32 SubIndexY1 = FMath::Clamp<int32>((ComponentY1 - 1) / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);
						const int32 SubIndexX2 = FMath::Clamp<int32>(ComponentX2 / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);
						const int32 SubIndexY2 = FMath::Clamp<int32>(ComponentY2 / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);

						for (int32 SubIndexY = SubIndexY1; SubIndexY <= SubIndexY2; SubIndexY++)
						{
							for (int32 SubIndexX = SubIndexX1; SubIndexX <= SubIndexX2; SubIndexX++)
							{
								// Find coordinates of box that lies inside subsection
								const int32 SubX1 = FMath::Clamp<int32>(ComponentX1 - SubsectionSizeQuads * SubIndexX, 0, SubsectionSizeQuads);
								const int32 SubY1 = FMath::Clamp<int32>(ComponentY1 - SubsectionSizeQuads * SubIndexY, 0, SubsectionSizeQuads);
								const int32 SubX2 = FMath::Clamp<int32>(ComponentX2 - SubsectionSizeQuads * SubIndexX, 0, SubsectionSizeQuads);
								const int32 SubY2 = FMath::Clamp<int32>(ComponentY2 - SubsectionSizeQuads * SubIndexY, 0, SubsectionSizeQuads);

								// Update texture data for the box that lies inside subsection
								for (int32 SubY = SubY1; SubY <= SubY2; SubY++)
								{
									for (int32 SubX = SubX1; SubX <= SubX2; SubX++)
									{
										const int32 LandscapeX = SubIndexX * SubsectionSizeQuads + ComponentIndexX * ComponentSizeQuads + SubX;
										const int32 LandscapeY = SubIndexY * SubsectionSizeQuads + ComponentIndexY * ComponentSizeQuads + SubY;
										checkSlow(LandscapeX >= X1 && LandscapeX <= X2);
										checkSlow(LandscapeY >= Y1 && LandscapeY <= Y2);

										// Find the input data corresponding to this vertex
										const int32 DataIndex = (LandscapeX - X1) + Stride * (LandscapeY - Y1);
										if (Data[DataIndex] && (MaskData ? (MaskData[DataIndex] >= 1) : true))
										{
											return false;
										}
									}
								}
							}
						}
						return true;
					};

				// Avoid reallocating and update material instances if we're about to write all zeros and we don't have an allocated channel for this LayerInfo
				if (IsDataAllZeroForComponent())
				{
					continue;
				}

				UpdateLayerIdx = ComponentWeightmapLayerAllocations.Num();
				new (ComponentWeightmapLayerAllocations) FWeightmapLayerAllocationInfo(LayerInfo);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
				Component->ReallocateWeightmaps(&LandscapeEdit, LandscapeEdit.GetEditLayer(), /*bInSaveToTransactionBuffer = */true, /*bool bInForceReallocate = */false, /*InTargetProxy = */nullptr, /*InRestrictSharingToComponents = */nullptr);
#else
				Component->ReallocateWeightmaps(&LandscapeEdit);  // Component->ReallocateWeightmapsInternal(this, GetEditLayer());
#endif
				Component->RequestWeightmapUpdate(false, bNeedUpdateCollision);
				Component->Modify();
			}

			int32 WeightmapIdx = ComponentWeightmapLayerAllocations[UpdateLayerIdx].WeightmapTextureIndex;
			int32 WeightmapChannel = ComponentWeightmapLayerAllocations[UpdateLayerIdx].WeightmapTextureChannel;

			// Lock data for all the weightmaps
			const TArray<UTexture2D*>& ComponentWeightmapTextures = Component->GetWeightmapTextures(true);
			FLandscapeTextureDataInfo* TexDataInfo = LandscapeEdit.GetTextureDataInfo(ComponentWeightmapTextures[WeightmapIdx]);
			uint8* LayerDataPtr = (uint8*)TexDataInfo->GetMipData(0) + ChannelOffsets[WeightmapChannel];
			bool bValueChanged = false;

			// Find the texture data corresponding to this vertex
			const int32 TexSize = (SubsectionSizeQuads + 1) * ComponentNumSubsections;

			// Find coordinates of box that lies inside component
			const int32 ComponentX1 = FMath::Clamp<int32>(X1 - ComponentIndexX * ComponentSizeQuads, 0, ComponentSizeQuads);
			const int32 ComponentY1 = FMath::Clamp<int32>(Y1 - ComponentIndexY * ComponentSizeQuads, 0, ComponentSizeQuads);
			const int32 ComponentX2 = FMath::Clamp<int32>(X2 - ComponentIndexX * ComponentSizeQuads, 0, ComponentSizeQuads);
			const int32 ComponentY2 = FMath::Clamp<int32>(Y2 - ComponentIndexY * ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			const int32 SubIndexX1 = FMath::Clamp<int32>((ComponentX1 - 1) / SubsectionSizeQuads, 0, ComponentNumSubsections - 1); // -1 because we need to pick up vertices shared between subsections
			const int32 SubIndexY1 = FMath::Clamp<int32>((ComponentY1 - 1) / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);
			const int32 SubIndexX2 = FMath::Clamp<int32>(ComponentX2 / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);
			const int32 SubIndexY2 = FMath::Clamp<int32>(ComponentY2 / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);

			for (int32 SubIndexY = SubIndexY1; SubIndexY <= SubIndexY2; SubIndexY++)
			{
				for (int32 SubIndexX = SubIndexX1; SubIndexX <= SubIndexX2; SubIndexX++)
				{
					// Find coordinates of box that lies inside subsection
					const int32 SubX1 = FMath::Clamp<int32>(ComponentX1 - SubsectionSizeQuads * SubIndexX, 0, SubsectionSizeQuads);
					const int32 SubY1 = FMath::Clamp<int32>(ComponentY1 - SubsectionSizeQuads * SubIndexY, 0, SubsectionSizeQuads);
					const int32 SubX2 = FMath::Clamp<int32>(ComponentX2 - SubsectionSizeQuads * SubIndexX, 0, SubsectionSizeQuads);
					const int32 SubY2 = FMath::Clamp<int32>(ComponentY2 - SubsectionSizeQuads * SubIndexY, 0, SubsectionSizeQuads);

					bool bSubValueChanged = false;
					// Update texture data for the box that lies inside subsection
					for (int32 SubY = SubY1; SubY <= SubY2; SubY++)
					{
						for (int32 SubX = SubX1; SubX <= SubX2; SubX++)
						{
							const int32 LandscapeX = SubIndexX * SubsectionSizeQuads + ComponentIndexX * ComponentSizeQuads + SubX;
							const int32 LandscapeY = SubIndexY * SubsectionSizeQuads + ComponentIndexY * ComponentSizeQuads + SubY;
							checkSlow(LandscapeX >= X1 && LandscapeX <= X2);
							checkSlow(LandscapeY >= Y1 && LandscapeY <= Y2);

							// Find the input data corresponding to this vertex
							const int32 DataIndex = (LandscapeX - X1) + Stride * (LandscapeY - Y1);
							if (MaskData)
							{
								if (MaskData[DataIndex] == 0)
									continue;
							}
							uint8 NewWeight = Data[DataIndex];

							const int32 TexX = (SubsectionSizeQuads + 1) * SubIndexX + SubX;
							const int32 TexY = (SubsectionSizeQuads + 1) * SubIndexY + SubY;
							const int32 TexDataIndex = 4 * (TexX + TexY * TexSize);

							uint8& CurrentWeight = LayerDataPtr[TexDataIndex];
							if (NewWeight != CurrentWeight)
							{
								bSubValueChanged = true;
								CurrentWeight = NewWeight;
							}
						}
					}

					if (bSubValueChanged)
					{
					// Record the areas of the texture we need to re-upload
					const int32 TexX1 = (SubsectionSizeQuads + 1) * SubIndexX + SubX1;
					const int32 TexY1 = (SubsectionSizeQuads + 1) * SubIndexY + SubY1;
					const int32 TexX2 = (SubsectionSizeQuads + 1) * SubIndexX + SubX2;
					const int32 TexY2 = (SubsectionSizeQuads + 1) * SubIndexY + SubY2;
					TexDataInfo->AddMipUpdateRegion(0, TexX1, TexY1, TexX2, TexY2);
					bValueChanged = true;
					}
				}
			}

			if (DeleteLayerIfAllZero(Component, LayerDataPtr, TexSize, UpdateLayerIdx, true))
			{
				Component->RequestWeightmapUpdate(false, bNeedUpdateCollision);
				Component->Modify();
			}
			else if (bValueChanged || bForceModify)
			{
				Component->RequestWeightmapUpdate(false, bNeedUpdateCollision);
				Component->Modify();
			}
			//Component->GetLandscapeProxy()->ValidateProxyLayersWeightmapUsage();
		}
	}
}

// See FLandscapeEditDataInterface::SetHeightData
void FHoudiniLandscapeOutputHelper::SetHeightData(ULandscapeInfo* LandscapeInfo,
	const int32 X1, const int32 Y1, const int32 X2, const int32 Y2, const uint16* InData, const uint8* MaskData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputLandscape_SetHeightData);
	
	FLandscapeEditDataInterface& LandscapeEdit = *FindOrCreateLandscapeEdit(LandscapeInfo);
	//FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);

	const int32 NumVertsX = 1 + X2 - X1;
	const int32 NumVertsY = 1 + Y2 - Y1;
	const int32 InStride = (1 + X2 - X1);
	const int32& SubsectionSizeQuads = LandscapeInfo->SubsectionSizeQuads;
	const int32& ComponentSizeQuads = LandscapeInfo->ComponentSizeQuads;
	const int32& ComponentNumSubsections = LandscapeInfo->ComponentNumSubsections;


	bool InUpdateBounds = true, InUpdateCollision = true, InGenerateMips = true;


	check(ComponentSizeQuads > 0);
	// Find component range for this block of data
	int32 ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	for (int32 ComponentIndexY = ComponentIndexY1; ComponentIndexY <= ComponentIndexY2; ComponentIndexY++)
	{
		for (int32 ComponentIndexX = ComponentIndexX1; ComponentIndexX <= ComponentIndexX2; ComponentIndexX++)
		{
			FIntPoint ComponentKey(ComponentIndexX, ComponentIndexY);
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ComponentKey);

			// if nullptr, it was painted away
			if (Component == nullptr)
				continue;

			UTexture2D* Heightmap = Component->GetHeightmap(LandscapeEdit.GetEditLayer());
			//UTexture2D* XYOffsetmapTexture = ToRawPtr(Component->XYOffsetmapTexture);

			//Component->Modify(LandscapeEdit.GetShouldDirtyPackage());

			FLandscapeTextureDataInfo* TexDataInfo = LandscapeEdit.GetTextureDataInfo(Heightmap);
			FColor* HeightmapTextureData = (FColor*)TexDataInfo->GetMipData(0);

			// Find the texture data corresponding to this vertex
			int32 SizeU = Heightmap->Source.GetSizeX();
			int32 SizeV = Heightmap->Source.GetSizeY();
			int32 HeightmapOffsetX = Component->HeightmapScaleBias.Z * (float)SizeU;
			int32 HeightmapOffsetY = Component->HeightmapScaleBias.W * (float)SizeV;

			// Find coordinates of box that lies inside component
			int32 ComponentX1 = FMath::Clamp<int32>(X1 - ComponentIndexX * ComponentSizeQuads, 0, ComponentSizeQuads);
			int32 ComponentY1 = FMath::Clamp<int32>(Y1 - ComponentIndexY * ComponentSizeQuads, 0, ComponentSizeQuads);
			int32 ComponentX2 = FMath::Clamp<int32>(X2 - ComponentIndexX * ComponentSizeQuads, 0, ComponentSizeQuads);
			int32 ComponentY2 = FMath::Clamp<int32>(Y2 - ComponentIndexY * ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			int32 SubIndexX1 = FMath::Clamp<int32>((ComponentX1 - 1) / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);	// -1 because we need to pick up vertices shared between subsections
			int32 SubIndexY1 = FMath::Clamp<int32>((ComponentY1 - 1) / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);
			int32 SubIndexX2 = FMath::Clamp<int32>(ComponentX2 / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);
			int32 SubIndexY2 = FMath::Clamp<int32>(ComponentY2 / SubsectionSizeQuads, 0, ComponentNumSubsections - 1);

			bool bValueChanged = false;
			for (int32 SubIndexY = SubIndexY1; SubIndexY <= SubIndexY2; SubIndexY++)
			{
				for (int32 SubIndexX = SubIndexX1; SubIndexX <= SubIndexX2; SubIndexX++)
				{
					// Find coordinates of box that lies inside subsection
					int32 SubX1 = FMath::Clamp<int32>(ComponentX1 - SubsectionSizeQuads * SubIndexX, 0, SubsectionSizeQuads);
					int32 SubY1 = FMath::Clamp<int32>(ComponentY1 - SubsectionSizeQuads * SubIndexY, 0, SubsectionSizeQuads);
					int32 SubX2 = FMath::Clamp<int32>(ComponentX2 - SubsectionSizeQuads * SubIndexX, 0, SubsectionSizeQuads);
					int32 SubY2 = FMath::Clamp<int32>(ComponentY2 - SubsectionSizeQuads * SubIndexY, 0, SubsectionSizeQuads);

					bool bSubValueChanged = false;
					// Update texture data for the box that lies inside subsection
					for (int32 SubY = SubY1; SubY <= SubY2; SubY++)
					{
						for (int32 SubX = SubX1; SubX <= SubX2; SubX++)
						{
							int32 LandscapeX = SubIndexX * SubsectionSizeQuads + ComponentIndexX * ComponentSizeQuads + SubX;
							int32 LandscapeY = SubIndexY * SubsectionSizeQuads + ComponentIndexY * ComponentSizeQuads + SubY;
							checkSlow(LandscapeX >= X1 && LandscapeX <= X2);
							checkSlow(LandscapeY >= Y1 && LandscapeY <= Y2);

							// Find the input data corresponding to this vertex
							int32 DataIndex = (LandscapeX - X1) + InStride * (LandscapeY - Y1);
							if (MaskData)
							{
								if (MaskData[DataIndex] == 0)
									continue;
							}

							uint16 Height = InData[DataIndex];

							int32 TexX = HeightmapOffsetX + (SubsectionSizeQuads + 1) * SubIndexX + SubX;
							int32 TexY = HeightmapOffsetY + (SubsectionSizeQuads + 1) * SubIndexY + SubY;
							FColor& TexData = HeightmapTextureData[TexX + TexY * SizeU];

							uint8 R = Height >> 8;
							uint8 G = Height & 255;
							if (TexData.R != R || TexData.G != G)
								bSubValueChanged = true;

							// Update the texture
							TexData.R = R;
							TexData.G = G;
						}
					}

					if (bSubValueChanged)
					{
					// Record the areas of the texture we need to re-upload
					int32 TexX1 = HeightmapOffsetX + (SubsectionSizeQuads + 1) * SubIndexX + SubX1;
					int32 TexY1 = HeightmapOffsetY + (SubsectionSizeQuads + 1) * SubIndexY + SubY1;
					int32 TexX2 = HeightmapOffsetX + (SubsectionSizeQuads + 1) * SubIndexX + SubX2;
					int32 TexY2 = HeightmapOffsetY + (SubsectionSizeQuads + 1) * SubIndexY + SubY2;
					TexDataInfo->AddMipUpdateRegion(0, TexX1, TexY1, TexX2, TexY2);
					bValueChanged = true;
					}
				}
			}

			if (bValueChanged)
			{
				Component->RequestHeightmapUpdate();
				Component->Modify();

				// In Layer, cumulate dirty collision region (will be used next time UpdateCollisionHeightData is called)
				//Component->UpdateDirtyCollisionHeightData(FIntRect(ComponentX1, ComponentY1, ComponentX2, ComponentY2));

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 5)) || (ENGINE_MAJOR_VERSION < 5)
				// Update GUID for Platform Data
				FPlatformMisc::CreateGuid(Component->StateId);
#endif
			}
		}
	}
}

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
static const ULandscapeEditLayerBase* FindOrCreateEditLayer(ALandscape* Landscape, const FName& EditLayerName)
{
	if (EditLayerName.IsNone())
		return Landscape->GetEditLayerConst(0);

	if (const ULandscapeEditLayerBase* FoundLandscapeLayerPtr = Landscape->GetEditLayerConst(EditLayerName))
		return FoundLandscapeLayerPtr;

	const int32 TargetEditLayerIdx = Landscape->CreateLayer(EditLayerName);
	return Landscape->GetEditLayerConst(TargetEditLayerIdx);
}
#else
static const FLandscapeLayer* FindOrCreateEditLayer(ALandscape* Landscape, const FName& EditLayerName)
{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
	if (EditLayerName.IsNone())
		return Landscape->GetLayerConst(0);
	
	if (const FLandscapeLayer* FoundLandscapeLayerPtr = Landscape->GetLayerConst(EditLayerName))
		return FoundLandscapeLayerPtr;
	
	const int32 TargetEditLayerIdx = Landscape->CreateLayer(EditLayerName);
	return Landscape->GetLayerConst(TargetEditLayerIdx);
#else
	if (EditLayerName.IsNone())
		return Landscape->GetLayer(0);

	if (const FLandscapeLayer* FoundLandscapeLayerPtr = Landscape->GetLayer(EditLayerName))
		return FoundLandscapeLayerPtr;

	const int32 TargetEditLayerIdx = Landscape->CreateLayer(EditLayerName);
	return &(Landscape->LandscapeLayers[TargetEditLayerIdx]);
#endif
}
#endif
UE_DISABLE_OPTIMIZATION
static void GetHeightfieldOutputExtent(FTransform LandscapeTransform, const FIntRect& LandscapeExtent, const HAPI_VolumeInfo& VolumeInfo,
	FIntRect& OutTargetLandscapeExtent, FIntRect& OutHeightfieldExtent)  // OutHeightfieldExtent is the actual extent on houdini volume that we need to output
{
	FTransform HeightfieldTransform;
	{
		const HAPI_Transform& HapiTransform = VolumeInfo.transform;
		HeightfieldTransform.SetLocation(FVector(HapiTransform.position[2], HapiTransform.position[0], HapiTransform.position[1]));
		FRotator Rotator = FQuat(HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[1], -HapiTransform.rotationQuaternion[3]).Rotator();
		Rotator.Yaw -= 90.0;
		Rotator.Roll += 90.0;
		HeightfieldTransform.SetRotation(Rotator.Quaternion());
		HeightfieldTransform.SetScale3D(FVector(HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2]) * 2.0);
	}

	// Convert heightfield min index and max index to unreal world coordinate
	FVector HeightfieldStart = HeightfieldTransform.TransformPosition(FVector::ZeroVector);
	FVector HeightfieldEnd = HeightfieldTransform.TransformPosition(
		FVector(double(VolumeInfo.xLength) - 1.0, double(VolumeInfo.yLength) - 1.0, 0.0));
	Swap(HeightfieldStart.X, HeightfieldStart.Y);
	Swap(HeightfieldEnd.X, HeightfieldEnd.Y);

	// Transform the world coordinate to landscape index
	FVector LandscapeStart = LandscapeTransform.InverseTransformPosition(HeightfieldStart * POSITION_SCALE_TO_UNREAL);
	FVector LandscapeEnd = LandscapeTransform.InverseTransformPosition(HeightfieldEnd * POSITION_SCALE_TO_UNREAL);
	
	FIntRect RawTargetLandscapeExtent(FMath::RoundToInt(LandscapeStart.X), FMath::RoundToInt(LandscapeStart.Y),
		FMath::RoundToInt(LandscapeEnd.X), FMath::RoundToInt(LandscapeEnd.Y));

	OutTargetLandscapeExtent = RawTargetLandscapeExtent;
	if (!OutTargetLandscapeExtent.Intersect(LandscapeExtent))  // Heightfield extent not in landscape extent, will not retrieve data
	{
		OutTargetLandscapeExtent = INVALID_HEIGHTFIELD_EXTENT;
		return;
	}

	OutTargetLandscapeExtent.Clip(LandscapeExtent);
	if (RawTargetLandscapeExtent != OutTargetLandscapeExtent)  // Means the RawTargetLandscapeExtent out of the LandscapeExtent, we need clip the OutHeightfieldExtent
	{
		// Clip Landscape start and end by LandscapeExtent
		LandscapeStart = FVector(FMath::Clamp(LandscapeStart.X, LandscapeExtent.Min.X, LandscapeExtent.Max.X),
			FMath::Clamp(LandscapeStart.Y, LandscapeExtent.Min.Y, LandscapeExtent.Max.Y), 0.0);
		LandscapeEnd = FVector(FMath::Clamp(LandscapeEnd.X, LandscapeExtent.Min.X, LandscapeExtent.Max.X),
			FMath::Clamp(LandscapeEnd.Y, LandscapeExtent.Min.Y, LandscapeExtent.Max.Y), 0.0);

		// Convert landscape min index and max index to unreal world coordinate
		HeightfieldStart = LandscapeTransform.TransformPosition(LandscapeStart) * POSITION_SCALE_TO_HOUDINI;
		HeightfieldEnd =LandscapeTransform.TransformPosition(LandscapeEnd) * POSITION_SCALE_TO_HOUDINI;
		Swap(HeightfieldStart.X, HeightfieldStart.Y);
		Swap(HeightfieldEnd.X, HeightfieldEnd.Y);

		// Transform the world coordinate to heightfield index
		HeightfieldStart = HeightfieldTransform.InverseTransformPosition(HeightfieldStart);
		HeightfieldEnd = HeightfieldTransform.InverseTransformPosition(HeightfieldEnd);

		OutHeightfieldExtent = FIntRect(FMath::RoundToInt(HeightfieldStart.Y), FMath::RoundToInt(HeightfieldStart.X),
			FMath::RoundToInt(HeightfieldEnd.Y), FMath::RoundToInt(HeightfieldEnd.X));
		OutHeightfieldExtent.Clip(FIntRect(0, 0, VolumeInfo.yLength - 1, VolumeInfo.xLength - 1));
	}
	else
		OutHeightfieldExtent = FIntRect(0, 0, VolumeInfo.yLength - 1, VolumeInfo.xLength - 1);
}
UE_ENABLE_OPTIMIZATION

static void AlignDataExtent(const uint8* OldData, const FIntRect& OldExtent, TArray<uint8>& OutData, const FIntRect& NewExtent)
{
	if (OldExtent == NewExtent)
		return;

	const int32 OldSizeX = OldExtent.Width() + 1;
	const int32 OldSizeY = OldExtent.Height() + 1;
	const int32 NewSizeX = NewExtent.Width() + 1;
	const int32 NewSizeY = NewExtent.Height() + 1;
	const int32 OffsetX = OldExtent.Min.X - NewExtent.Min.X;
	const int32 OffsetY = OldExtent.Min.Y - NewExtent.Min.Y;
	OutData.SetNumZeroed(NewSizeX * NewSizeY);

	for (int32 NewX = 0; NewX < NewSizeX; ++NewX)
	{
		const int32 OldX = NewX + OffsetX;
		if (0 <= OldX || OldX < OldSizeX)
		{
			for (int32 NewY = 0; NewY < NewSizeY; ++NewY)
			{
				const int32 OldY = NewY + OffsetY;
				if (0 <= OldY || OldY < OldSizeY)
					OutData[NewY * NewSizeX + NewX] = OldData[OldY * OldSizeX + OldX];
			}
		}
	}
}

static bool HapiGetHeightfieldData(const float*& OutFloatData, size_t& OutHandle,  // 0 means not a SHM
	const int32& NodeId, const int32& PartId, const HAPI_VolumeInfo& VolumeInfo)
{
	const size_t NumPixels = size_t(VolumeInfo.xLength) * size_t(VolumeInfo.yLength);

	OutHandle = 0;
	HAPI_AttributeInfo AttribInfo;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		HAPI_ATTRIB_SHARED_MEMORY_PATH, HAPI_ATTROWNER_PRIM, &AttribInfo));
	if (AttribInfo.exists && AttribInfo.storage == HAPI_STORAGETYPE_STRING)
	{
		HAPI_StringHandle SHMPathSH;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_SHARED_MEMORY_PATH, &AttribInfo, &SHMPathSH, 0, 1));
		FString SHMPath;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(SHMPathSH, SHMPath));
		if (!SHMPath.IsEmpty())
			OutFloatData = FHoudiniEngineUtils::GetSharedMemory(*SHMPath, NumPixels, OutHandle);
	}

	if (!OutHandle)
	{
		OutFloatData = (const float*)FMemory::Malloc(NumPixels * sizeof(float));
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetHeightFieldData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			(float*)OutFloatData, 0, NumPixels));
		UE_LOG(LogHoudiniEngine, Warning, TEXT("Please add \"sharedmemory_volumeoutput\" sop [at last]/[on the top of output node] in your HDA, for Faster and Robust volume data transport.\n                  See \"Resources/houdini/otls/examples/he_example_terrain_stamp.hda\"\n                  Since HAPI_GetHeightFieldData has serious bugs, especially when transport volumes with various resolutions"))
	}

	return true;
}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
static void SetPropertiesOnEditLayer(const TArray<TSharedPtr<FHoudiniAttribute>>& PropAttribs, ULandscapeEditLayerBase* EditLayer, ALandscape* Landscape)
{
	// Set uproperty on current EditLayer
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
	if (EditLayer && Landscape->HasLayersContent())
#endif
	{
		for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
			PropAttrib->SetObjectPropertyValues(EditLayer, 0);
	}
}
#else
static void SetPropertiesOnEditLayer(const TArray<TSharedPtr<FHoudiniAttribute>>& PropAttribs, FLandscapeLayer* EditLayer, ALandscape* Landscape,
	TArray<FGuid>& InOutSetEditLayerGuids, bool& bInOutHasLandscapeChanged)
{
	// Set uproperty on current EditLayer
	if (!PropAttribs.IsEmpty() && EditLayer && Landscape->HasLayersContent() && !InOutSetEditLayerGuids.Contains(EditLayer->Guid))
	{
		InOutSetEditLayerGuids.Add(EditLayer->Guid);

		if (!PropAttribs.IsEmpty())
		{
			const FLandscapeLayer BackupEditLayer = *EditLayer;
			bool bFoundProperty = false;
			for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
			{
				const FString& PropertyName = PropAttrib->GetAttributeName();
				if (PropertyName != TEXT("Name") && PropertyName != TEXT("Guid"))  // We should NOT allow EditLayer name to be set by houdini
				{
					if (PropAttrib->SetStructPropertyValues((void*)EditLayer, FLandscapeLayer::StaticStruct(), 0))
						bFoundProperty = true;
				}
			}

			if (bFoundProperty && !bInOutHasLandscapeChanged)
			{
				// We should judge whether landscape layer is truly changed
				for (TFieldIterator<FProperty> PropIter(FLandscapeLayer::StaticStruct(), EFieldIteratorFlags::IncludeSuper); PropIter; ++PropIter)
				{
					if (const FProperty* Prop = *PropIter)
					{
						if (Prop->HasAnyFlags(RF_Transient))
							continue;

						if (!Prop->Identical_InContainer(EditLayer, &BackupEditLayer))
						{
							bInOutHasLandscapeChanged = true;
							break;
						}
					}
				}
			}
		}
	}
}
#endif
static bool HapiFindOrCreateLayerInfo(ULandscapeLayerInfoObject*& OutLayerInfo, ALandscape* TargetLandscape, ULandscapeInfo* LandscapeInfo,
	const FName& LayerName, const FString& CookFolder,
	const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
	bool& bOutHasTempLayerAdded)
{
	if (LayerName == HoudiniAlphaLayerName)
	{
		if (IsValid(TargetLandscape->LandscapeHoleMaterial))
			OutLayerInfo = ALandscapeProxy::VisibilityLayer;
	}
	else
	{
		// First, find LayerInfo by specified s@unreal_landscape_layer_info
		FString LayerInfoPath;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
			AttribNames, AttribCounts, HAPI_ATTRIB_UNREAL_LANDSCAPE_LAYER_INFO, LayerInfoPath));

		if (!LayerInfoPath.IsEmpty())
			OutLayerInfo = LoadObject<ULandscapeLayerInfoObject>(nullptr, *LayerInfoPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
		if (IsValid(OutLayerInfo) && OutLayerInfo->GetLayerName() != LayerName)  // ensure that we have the corresponding LayerName
#else
		if (IsValid(OutLayerInfo) && OutLayerInfo->LayerName != LayerName)  // ensure that we have the corresponding LayerName
#endif
			OutLayerInfo = nullptr;

		// Second, if not found, then get exists LayerInfo from LandscapeInfo
		if (!IsValid(OutLayerInfo))
			OutLayerInfo = LandscapeInfo->GetLayerInfoByName(LayerName);

		// Check if we have this LayerName in landscape
		const int32 FoundLayerIdx = LandscapeInfo->GetLayerInfoIndex(LayerName);

		// Finally, if not found, then create a LayerInfo for
		if (!IsValid(OutLayerInfo))
		{
			OutLayerInfo = FHoudiniEngineUtils::FindOrCreateAsset<ULandscapeLayerInfoObject>(
				FString::Printf(TEXT("%s%s_%s_LayerInfo"), *CookFolder, *TargetLandscape->GetFName().ToString(), *LayerName.ToString()));
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			OutLayerInfo->SetLayerName(LayerName, false);
			OutLayerInfo->SetBlendMethod((FoundLayerIdx == INDEX_NONE) ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
			OutLayerInfo->LayerName = LayerName;
			OutLayerInfo->bNoWeightBlend = FoundLayerIdx == INDEX_NONE;  // If Not exists in layers, then probably be a NoWeightBlend layer
#endif
			// Trigger update of the Layer Info
			OutLayerInfo->PreEditChange(nullptr);
			OutLayerInfo->PostEditChange();
			OutLayerInfo->MarkPackageDirty();
		}

		// Check if landscape layer is set to NoWeightBlend
		HAPI_AttributeOwner NoWeightBlendOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, AttribCounts, HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND);
		TArray<int8> NoWeightBlend;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
			HAPI_UNREAL_ATTRIB_LANDSCAPE_LAYER_NOWEIGHTBLEND, NoWeightBlend, NoWeightBlendOwner));
		if (NoWeightBlend.IsValidIndex(0))
		{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			const bool bNoWeightBlend = (NoWeightBlend[0] >= 1);
			if ((OutLayerInfo->GetBlendMethod() == ELandscapeTargetLayerBlendMethod::None) != bNoWeightBlend)
				OutLayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, true);
#else
			const bool bNoWeightBlend = (NoWeightBlend[0] >= 1);
			if (OutLayerInfo->bNoWeightBlend != bNoWeightBlend)
			{
				OutLayerInfo->bNoWeightBlend = bNoWeightBlend;
				OutLayerInfo->Modify();
			}
#endif
		}

		auto CreateWeightLayerLambda = [](ULandscapeInfo* LandscapeInfo, ULandscapeLayerInfoObject* LayerInfo)
			{
				// Copy from ULandscapeInfo::CreateTargetLayerSettingsFor, but NOT to call ALandscapeProxy::PostEditChangeProperty
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
				LandscapeInfo->ForEachLandscapeProxy([LayerInfo](ALandscapeProxy* Proxy)
					{
						if (Proxy->HasTargetLayer(LayerInfo->GetLayerName()))
						{
							Proxy->UpdateTargetLayer(LayerInfo->GetLayerName(), FLandscapeTargetLayerSettings(LayerInfo), false);
						}
						else
						{
							Proxy->AddTargetLayer(LayerInfo->GetLayerName(), FLandscapeTargetLayerSettings(LayerInfo), false);
						}

						return true;
					});
#elif ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
				LandscapeInfo->ForEachLandscapeProxy([LayerInfo](ALandscapeProxy* Proxy)
					{
						if (Proxy->HasTargetLayer(LayerInfo->LayerName))
						{
							Proxy->UpdateTargetLayer(LayerInfo->LayerName, FLandscapeTargetLayerSettings(LayerInfo), false);
						}
						else
						{
							Proxy->AddTargetLayer(LayerInfo->LayerName, FLandscapeTargetLayerSettings(LayerInfo), false);
						}

						return true;
					});
#else
				LandscapeInfo->CreateLayerEditorSettingsFor(LayerInfo);
#endif
			};

		// If LayerName not in landscape, then add this layer
		if (FoundLayerIdx == INDEX_NONE)
		{
			LandscapeInfo->Layers.Add(FLandscapeInfoLayerSettings(OutLayerInfo, TargetLandscape));
			CreateWeightLayerLambda(LandscapeInfo, OutLayerInfo);
			bOutHasTempLayerAdded = true;
		}
		else
		{
			ULandscapeLayerInfoObject* PrevLayerInfo = LandscapeInfo->Layers[FoundLayerIdx].LayerInfoObj;
			if (!PrevLayerInfo)
			{
				LandscapeInfo->Layers[FoundLayerIdx].LayerInfoObj = OutLayerInfo;
				CreateWeightLayerLambda(LandscapeInfo, OutLayerInfo);
			}
			else if (PrevLayerInfo != OutLayerInfo)
				LandscapeInfo->ReplaceLayer(PrevLayerInfo, OutLayerInfo);
		}
	}

	return true;
}

static bool HapiSetWeightLayerBlend(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
	ULandscapeEditLayerBase* EditLayer, ULandscapeLayerInfoObject* LayerInfo, bool& bOutModified)
#else
	FLandscapeLayer* EditLayer, ULandscapeLayerInfoObject* LayerInfo, bool& bOutModified)
#endif
{
	bOutModified = false;

	if (!EditLayer)
		return true;

	HAPI_AttributeOwner SubtractiveOwner = FHoudiniEngineUtils::QueryAttributeOwner(
		AttribNames, AttribCounts, HAPI_ATTRIB_UNREAL_LANDSCAPE_EDITLAYER_SUBTRACTIVE);
	TArray<int8> IsSubtractive;
	HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
		HAPI_ATTRIB_UNREAL_LANDSCAPE_EDITLAYER_SUBTRACTIVE, IsSubtractive, SubtractiveOwner));
	if (IsSubtractive.IsValidIndex(0))
	{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
PRAGMA_DISABLE_INTERNAL_WARNINGS
		if (IsSubtractive[0] >= 1)
		{
			if (const bool* bFoundSubtractivePtr = EditLayer->GetWeightmapLayerAllocationBlend().Find(LayerInfo))
			{
				if (!(*bFoundSubtractivePtr))
				{
					EditLayer->AddOrUpdateWeightmapAllocationLayerBlend(LayerInfo, true, true);
					bOutModified = true;
				}
			}
			else
			{
				EditLayer->AddOrUpdateWeightmapAllocationLayerBlend(LayerInfo, true, true);
				bOutModified = true;
			}
		}
		else if (const bool* bFoundSubtractivePtr = EditLayer->GetWeightmapLayerAllocationBlend().Find(LayerInfo))
		{
			if (*bFoundSubtractivePtr)
			{
				bool bPrevSubtractive = true;
				EditLayer->RemoveAndCopyWeightmapAllocationLayerBlend(LayerInfo, bPrevSubtractive, true);
				bOutModified = true;
			}
		}
PRAGMA_ENABLE_INTERNAL_WARNINGS
#else
		if (IsSubtractive[0] >= 1)
		{
			bool& bSubtractive = EditLayer->WeightmapLayerAllocationBlend.FindOrAdd(LayerInfo);
			if (!bSubtractive)
			{
				bSubtractive = true;
				bOutModified = true;
			}
		}
		else if(const bool* bFoundSubtractivePtr = EditLayer->WeightmapLayerAllocationBlend.Find(LayerInfo))
		{
			if (*bFoundSubtractivePtr)
			{
				EditLayer->WeightmapLayerAllocationBlend.Remove(LayerInfo);
				bOutModified = true;
			}
		}
#endif
	}

	return true;
}



template<typename T>
static void ResampleLandscapeData(TArray<T>& InOutData,
	const int32 OldWidth, const int32 OldHeight, const int32 NewWidth, const int32 NewHeight)
{
	TArray<T> Result;
	Result.SetNumUninitialized(NewWidth * NewHeight);

	const float XScale = (NewWidth <= 1) ? 0.0f : ((OldWidth - 1.0f) / (NewWidth - 1.0f));
	const float YScale = (NewHeight <= 1) ? 0.0f : ((OldHeight - 1.0f) / (NewHeight - 1.0f));
	ParallelFor(NewHeight, [&](int32 Y)
		{
			const float OldY = Y * YScale;
			const int32 Y0 = FMath::FloorToInt(OldY);
			const int32 Y1 = FMath::Min(Y0 + 1, OldHeight - 1);
			const int32 Y0MultiplyOldWidth = Y0 * OldWidth;
			const int32 Y1MultiplyOldWidth = Y1 * OldWidth;
			const int32 YMultiplyNewWidth = Y * NewWidth;
			const float BiasY = OldY - Y0;
			for (int32 X = 0; X < NewWidth; ++X)
			{
				const float OldX = X * XScale;
				const int32 X0 = FMath::FloorToInt(OldX);
				const int32 X1 = FMath::Min(X0 + 1, OldWidth - 1);
				const T& Original00 = InOutData[Y0MultiplyOldWidth + X0];
				const T& Original10 = InOutData[Y0MultiplyOldWidth + X1];
				const T& Original01 = InOutData[Y1MultiplyOldWidth + X0];
				const T& Original11 = InOutData[Y1MultiplyOldWidth + X1];
				Result[YMultiplyNewWidth + X] = FMath::BiLerp(Original00, Original10, Original01, Original11, OldX - X0, BiasY);
			}
		});

	InOutData = Result;
}

static void ConvertHeightmapData(const float* FloatData, const int32& HeightfieldXSize, const float& Scale,
	TArray<uint16>& LandscapeData, const FIntRect& HeightfieldExtent, const FIntRect& LandscapeExtent)  // height
{
	const int32 HoudiniXSize = HeightfieldExtent.Width() + 1;
	const int32 HoudiniYSize = HeightfieldExtent.Height() + 1;
	LandscapeData.SetNumUninitialized(HoudiniXSize * HoudiniYSize);

	ParallelFor(HoudiniXSize, [&](const int32& nX)
		{
			const int32 YOffset = HeightfieldExtent.Min.Y + (nX + HeightfieldExtent.Min.X) * HeightfieldXSize;
			for (int32 nY = 0; nY < HoudiniYSize; nY++)
			{
				const float& HeightfieldValue = FloatData[nY + YOffset];
				LandscapeData[nY * HoudiniXSize + nX] = FMath::Clamp(FMath::RoundToInt((HeightfieldValue * Scale + 256.0f) * float(65535.0 / 512.0)), 0, 65535);
			}
		});

	const int32 UnrealXSize = LandscapeExtent.Width() + 1;
	const int32 UnrealYSize = LandscapeExtent.Height() + 1;
	if ((HoudiniXSize != UnrealXSize) || (HoudiniYSize != UnrealYSize))
		ResampleLandscapeData(LandscapeData, HoudiniXSize, HoudiniYSize, UnrealXSize, UnrealYSize);
}

static void ConvertWeightmapData(const float* FloatData, const int32& HeightfieldXSize,
	TArray<uint8>& LandscapeData, const FIntRect& HeightfieldExtent, const FIntRect& LandscapeExtent)  // weight layers
{
	const int32 HoudiniXSize = HeightfieldExtent.Width() + 1;
	const int32 HoudiniYSize = HeightfieldExtent.Height() + 1;
	LandscapeData.SetNumUninitialized(HoudiniXSize * HoudiniYSize);

	ParallelFor(HoudiniXSize, [&](const int32& nX)
		{
			const int32 YOffset = HeightfieldExtent.Min.Y + (nX + HeightfieldExtent.Min.X) * HeightfieldXSize;
			for (int32 nY = 0; nY < HoudiniYSize; nY++)
			{
				const float& HeightfieldValue = FloatData[nY + YOffset];
				LandscapeData[nY * HoudiniXSize + nX] = FMath::Clamp(FMath::RoundToInt(HeightfieldValue * 255.f), 0, 255);
			}
		});

	const int32 UnrealXSize = LandscapeExtent.Width() + 1;
	const int32 UnrealYSize = LandscapeExtent.Height() + 1;
	if ((HoudiniXSize != UnrealXSize) || (HoudiniYSize != UnrealYSize))
		ResampleLandscapeData(LandscapeData, HoudiniXSize, HoudiniYSize, UnrealXSize, UnrealYSize);
}

static void ConvertAlphaData(const float* FloatData, const int32& HeightfieldXSize, const UMaterialInterface* HoleMaterial,
	TArray<uint8>& LandscapeData, const FIntRect& HeightfieldExtent, const FIntRect& LandscapeExtent)  // Alpha
{
	const int32 HoudiniXSize = HeightfieldExtent.Width() + 1;
	const int32 HoudiniYSize = HeightfieldExtent.Height() + 1;
	LandscapeData.SetNumUninitialized(HoudiniXSize * HoudiniYSize);

	const UMaterial* Material = IsValid(HoleMaterial) ? HoleMaterial->GetMaterial() : nullptr;
	const float Scale = Material ? (Material->OpacityMaskClipValue * 2.0f) : (1.0f / 1.5f);
	ParallelFor(HoudiniXSize, [&](const int32& nX)
		{
			const int32 YOffset = HeightfieldExtent.Min.Y + (nX + HeightfieldExtent.Min.X) * HeightfieldXSize;
			for (int32 nY = 0; nY < HoudiniYSize; nY++)
			{
				const float& HeightfieldValue = FloatData[nY + YOffset];
				LandscapeData[nY * HoudiniXSize + nX] = FMath::Clamp(FMath::RoundToInt((1.0 - HeightfieldValue * Scale) * 255.f), 0, 255);
			}
		});

	const int32 UnrealXSize = LandscapeExtent.Width() + 1;
	const int32 UnrealYSize = LandscapeExtent.Height() + 1;
	if ((HoudiniXSize != UnrealXSize) || (HoudiniYSize != UnrealYSize))
		ResampleLandscapeData(LandscapeData, HoudiniXSize, HoudiniYSize, UnrealXSize, UnrealYSize);
}

#define EXPAND_EXTENT(EXTENT) EXTENT.Min.X, EXTENT.Min.Y, EXTENT.Max.X, EXTENT.Max.Y

static void PrintLandscapeOutputLog(const ALandscape* Landscape, const FName& EditLayerName, const FName& LayerName, const FIntRect& TargetLandscapeExtent)
{
	if (EditLayerName != NAME_None)
	{
		UE_LOG(LogHoudiniEngine, Log, TEXT("%s: %s/%s: (%d, %d) -> (%d, %d)"),
			*Landscape->GetName(), *EditLayerName.ToString(), *LayerName.ToString(), EXPAND_EXTENT(TargetLandscapeExtent));
	}
	else
	{
		UE_LOG(LogHoudiniEngine, Log, TEXT("%s: %s: (%d, %d) -> (%d, %d)"),
			*Landscape->GetName(), *LayerName.ToString(), EXPAND_EXTENT(TargetLandscapeExtent));
	}
}

static void PrintLandscapeOutputLog(const ALandscape* Landscape, const FName& EditLayerName, const FName& LayerName)
{
	if (EditLayerName != NAME_None)
	{
		UE_LOG(LogHoudiniEngine, Log, TEXT("%s: %s/%s: Volume position does NOT match!"),
			*Landscape->GetName(), *EditLayerName.ToString(), *LayerName.ToString());
	}
	else
	{
		UE_LOG(LogHoudiniEngine, Log, TEXT("%s: %s: Volume position does NOT match!"),
			*Landscape->GetName(), *LayerName.ToString());
	}
}


bool FHoudiniLandscapeOutputBuilder::HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniModifyLandscape);

	struct FHoudiniPartialOutputMask  // if pixel value == 0 then we should NOT output the pixel, use for output a irregular region like circle
	{
		FHoudiniPartialOutputMask(
			const int32& InPartId, const HAPI_VolumeInfo& InVolumeInfo, const FName& InEditLayerName, ALandscape* InTargetLandscape) :
			PartId(InPartId), VolumeInfo(InVolumeInfo), EditLayerName(InEditLayerName), TargetLandscape(InTargetLandscape) {}

		int32 PartId;
		HAPI_VolumeInfo VolumeInfo;
		FName EditLayerName;
		ALandscape* TargetLandscape = nullptr;

		FIntRect Extent;
		TArray<uint8> Data;

		bool HapiRetrieve(const int32& NodeId, const uint8*& OutMaskData)
		{
			if (!Data.IsEmpty())  // We should NOT Retrieve data twice
			{
				OutMaskData = Data.GetData();
				return true;
			}

			FIntRect LandscapeExtent;
			TargetLandscape->GetLandscapeInfo()->GetLandscapeExtent(LandscapeExtent);

			FIntRect HeightfieldExtent;
			GetHeightfieldOutputExtent(TargetLandscape->GetTransform(), LandscapeExtent, VolumeInfo,
				Extent, HeightfieldExtent);

			const float* FloatData = nullptr;
			size_t SHMHandle = 0;
			HOUDINI_FAIL_RETURN(HapiGetHeightfieldData(FloatData, SHMHandle, NodeId, PartId, VolumeInfo));

			ConvertWeightmapData(FloatData, VolumeInfo.xLength,
				Data, HeightfieldExtent, Extent);

			OutMaskData = Data.GetData();

			FHoudiniOutputUtils::CloseData(FloatData, SHMHandle);

			return true;
		}
	};

	struct FHoudiniLandscapePart
	{
		FHoudiniLandscapePart(const HAPI_PartInfo& PartInfo, const HAPI_VolumeInfo& InVolumeInfo, const TArray<std::string>& InAttribNames,
			const FName& InEditLayerName, const FName& InLayerName, const bool bInClear,
			ALandscape* InTargetLandscape) :
			Info(PartInfo), VolumeInfo(InVolumeInfo), AttribNames(InAttribNames),
			EditLayerName(InEditLayerName), LayerName(InLayerName), bClear(bInClear),
			TargetLandscape(InTargetLandscape) {}

		HAPI_PartInfo Info;
		HAPI_VolumeInfo VolumeInfo;
		TArray<std::string> AttribNames;

		FName EditLayerName;
		FName LayerName;
		bool bClear = false;

		FORCEINLINE bool ShouldClearEditLayer() const { return bClear && LayerName.IsNone(); }
		FORCEINLINE bool ShouldClearLayer() const { return bClear && !LayerName.IsNone(); }

		ALandscape* TargetLandscape = nullptr;
	};

	struct FHoudiniRemoveLayers
	{
		TArray<FName> RemoveEditLayers;
		TArray<FName> RemoveWeightLayers;
	};

	// Gather all landscapes
	TMap<FString, TArray<ALandscape*>> InputLandscapesMap;
	TArray<ALandscape*> Landscapes;
	if (!FHoudiniOutputUtils::GatherAllLandscapes(Node, InputLandscapesMap, Landscapes))  // If false, means there are No Landscapes in the world
		return true;

	const int32& NodeId = GeoInfo.nodeId;

	// Retrieve heightfield output info, include TargetLandscape
	TArray<FHoudiniPartialOutputMask> PartialOutputMasks;
	TArray<FHoudiniLandscapePart> Parts;
	TMap<ALandscape*, FHoudiniRemoveLayers> RemoveLayersMap;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
	{
		const int32& PartId = PartInfo.id;

		HAPI_VolumeInfo VolumeInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVolumeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &VolumeInfo));
		if (VolumeInfo.type != HAPI_VOLUMETYPE_HOUDINI ||
			VolumeInfo.storage != HAPI_STORAGETYPE_FLOAT || VolumeInfo.tupleSize != 1)
			continue;
		
		FString NameStr;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(VolumeInfo.nameSH, NameStr));
		if (NameStr.IsEmpty())
			continue;
		const FName LayerName = *NameStr;

		TArray<std::string> AttribNames;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, AttribNames));

		// Find TargetLandscape
		FString TargetLandscapeName;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
			AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OUTPUT_NAME, TargetLandscapeName));

		if (LayerName == HoudiniMaskLayerName)
		{
			FTransform LandscapeTransform;
			FIntRect LandscapeExtent;
			if (UHoudiniInputMask* FoundMaskInput = FHoudiniOutputUtils::FindMaskInput(Node, TargetLandscapeName, LandscapeTransform, LandscapeExtent))
			{
				const float* FloatData = nullptr;
				size_t SHMHandle = 0;
				HOUDINI_FAIL_RETURN(HapiGetHeightfieldData(FloatData, SHMHandle, NodeId, PartId, VolumeInfo));

				// Retrieve heightfield data
				FIntRect HeightfieldExtent;
				FIntRect TargetLandscapeExtent;
				GetHeightfieldOutputExtent(LandscapeTransform, LandscapeExtent, VolumeInfo,
					TargetLandscapeExtent, HeightfieldExtent);

				TArray<uint8> WeightData;
				ConvertWeightmapData(FloatData, VolumeInfo.xLength,
					WeightData, HeightfieldExtent, TargetLandscapeExtent);

				FHoudiniOutputUtils::CloseData(FloatData, SHMHandle);

				FHoudiniOutputUtils::WriteToMaskInput(FoundMaskInput, WeightData, TargetLandscapeExtent);
				
				UE_LOG(LogHoudiniEngine, Log, TEXT("Mask Input: %s: (%d, %d) -> (%d, %d)"), *TargetLandscapeName, EXPAND_EXTENT(TargetLandscapeExtent));

				continue;
			}
		}

		ALandscape* TargetLandscape = FHoudiniOutputUtils::FindTargetLandscape(TargetLandscapeName, InputLandscapesMap, Landscapes);
		if (!TargetLandscape)
			continue;

		// Find or add EditLayer
		auto HapiGetLambdaLayerClearMode = [&](const char* AttribName, int8& OutClearMode) -> bool
			{
				HAPI_AttributeOwner ClearEditLayerOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, AttribName);
				TArray<int8> LayerClearMode;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
					AttribName, [](FUtf8StringView AttribValue) -> int8
					{
						if ((UE::String::FindFirst(AttribValue, "remove", ESearchCase::IgnoreCase) != INDEX_NONE) ||
							(UE::String::FindFirst(AttribValue, "del", ESearchCase::IgnoreCase) != INDEX_NONE))
							return HAPI_UNREAL_LANDSCAPE_REMOVE_LAYER;
						else if ((UE::String::FindFirst(AttribValue, "clea", ESearchCase::IgnoreCase) != INDEX_NONE) ||
							(UE::String::FindFirst(AttribValue, "empty", ESearchCase::IgnoreCase) != INDEX_NONE))
							return HAPI_UNREAL_LANDSCAPE_CLEAR_LAYER;
						return 0;
					}, LayerClearMode, ClearEditLayerOwner));

				OutClearMode = 0;
				if (LayerClearMode.IsValidIndex(0))
				{
					if (LayerClearMode[0] == 1)
						OutClearMode = 1;
					if (LayerClearMode[0] >= 2)
						OutClearMode = 2;
				}
				return true;
			};
		
		FName TargetEditLayerName;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
		if (TargetLandscape->bCanHaveLayersContent)
#endif
		{
			FString TargetEditLayerNameStr;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
				AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_LANDSCAPE_EDITLAYER_NAME, TargetEditLayerNameStr));
			TargetEditLayerName = *TargetEditLayerNameStr;

			int8 ClearMode = 0;
			HapiGetLambdaLayerClearMode(HAPI_ATTRIB_UNREAL_LANDSCAPE_EDITLAYER_CLEAR, ClearMode);

			if (ClearMode == HAPI_UNREAL_LANDSCAPE_CLEAR_LAYER)
			{
				Parts.Add(FHoudiniLandscapePart(PartInfo, VolumeInfo, AttribNames, TargetEditLayerName, NAME_None, true, TargetLandscape));
				continue;  // Will clear all height and weight layers, so we need not parse layer settings
			}
			else if (ClearMode >= HAPI_UNREAL_LANDSCAPE_REMOVE_LAYER)  // We should delete EditLayer later, as there must be at least one EditLayer on landscapes
				RemoveLayersMap.FindOrAdd(TargetLandscape).RemoveEditLayers.AddUnique(TargetEditLayerName);
		}

		if (LayerName == HoudiniPartialOutputMaskName)
			PartialOutputMasks.Add(FHoudiniPartialOutputMask(PartId, VolumeInfo, TargetEditLayerName, TargetLandscape));
		else
		{
			int8 ClearMode = 0;
			HapiGetLambdaLayerClearMode(HAPI_ATTRIB_UNREAL_LANDSCAPE_LAYER_CLEAR, ClearMode);
			if ((ClearMode <= HAPI_UNREAL_LANDSCAPE_CLEAR_LAYER) || (LayerName == HoudiniHeightLayerName || LayerName == HoudiniAlphaLayerName))  // height and Alpha layer cannot remove
				Parts.Add(FHoudiniLandscapePart(PartInfo, VolumeInfo, AttribNames, TargetEditLayerName, LayerName, (ClearMode >= HAPI_UNREAL_LANDSCAPE_CLEAR_LAYER), TargetLandscape));
			else  // Delete WeightLayer
			{
				RemoveLayersMap.FindOrAdd(TargetLandscape).RemoveWeightLayers.AddUnique(LayerName);
				
				if (ULandscapeInfo* TargetLandscapeInfo = TargetLandscape->GetLandscapeInfo())
				{
					if (const FLandscapeInfoLayerSettings* FoundLayerSettingsPtr =
						TargetLandscapeInfo->Layers.FindByPredicate([LayerName](const FLandscapeInfoLayerSettings& LayerSettings)
							{
								return LayerSettings.LayerName == LayerName;
							}))
					{
						if (IsValid(FoundLayerSettingsPtr->LayerInfoObj))
						{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
							TargetLandscapeInfo->DeleteLayer(FoundLayerSettingsPtr->LayerInfoObj, LayerName);
#else
							bool bIsInMaterial = IsValid(TargetLandscape->LandscapeMaterial);
							if (bIsInMaterial)
								bIsInMaterial = ALandscapeProxy::GetLayersFromMaterial(TargetLandscape->LandscapeMaterial).Contains(LayerName);

							if (bIsInMaterial)  // Should retain the layer settings in LandscapeInfo
							{
								FLandscapeEditDataInterface LandscapeEdit(TargetLandscapeInfo);
								LandscapeEdit.DeleteLayer(FoundLayerSettingsPtr->LayerInfoObj);
								
								TargetLandscapeInfo->ForEachLandscapeProxy([FoundLayerSettingsPtr](ALandscapeProxy* Proxy)
									{
										Proxy->Modify();
										int32 Index = Proxy->EditorLayerSettings.IndexOfByKey(FoundLayerSettingsPtr->LayerInfoObj);
										if (Index != INDEX_NONE)
										{
											Proxy->EditorLayerSettings[Index].LayerInfoObj = nullptr;
										}
										return true;
									});

								const_cast<FLandscapeInfoLayerSettings*>(FoundLayerSettingsPtr)->LayerInfoObj = nullptr;
							}
							else
								TargetLandscapeInfo->DeleteLayer(FoundLayerSettingsPtr->LayerInfoObj, LayerName);
#endif
							UE_LOG(LogHoudiniEngine, Log, TEXT("%s: deleted weight layer: %s"), *TargetLandscape->GetName(), *LayerName.ToString());
						}
					}
				}
			}
		}
	}

	// We need not output the layers that is pending delete
	Parts.RemoveAll([&](const FHoudiniLandscapePart& Part)
		{
			if (const FHoudiniRemoveLayers* FoundRemoveLayersPtr = RemoveLayersMap.Find(Part.TargetLandscape))
				return (FoundRemoveLayersPtr->RemoveEditLayers.Contains(Part.LayerName) || FoundRemoveLayersPtr->RemoveEditLayers.Contains(Part.EditLayerName));
			return false;
		});

	// Try to delete EditLayers
	for (const auto& RemoveLayers : RemoveLayersMap)
	{
		for (const FName& EditLayerName : RemoveLayers.Value.RemoveEditLayers)
		{
			const int32& LayerIdx = RemoveLayers.Key->GetLayerIndex(EditLayerName);
			if (LayerIdx >= 0)
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
				if (RemoveLayers.Key->GetLayersConst().Num() <= 1)
#else
				if (RemoveLayers.Key->GetLayerCount() <= 1)
#endif
				{
					// When edit layer enabled, there must be at least one EditLayer on landscapes, so we should try to create before remove
					for (const FHoudiniLandscapePart& Part : Parts)
					{
						if (Part.TargetLandscape == RemoveLayers.Key)
						{
							FindOrCreateEditLayer(Part.TargetLandscape, Part.EditLayerName);
							break;
						}
					}
				}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
				if (RemoveLayers.Key->DeleteLayer(LayerIdx))
					UE_LOG(LogHoudiniEngine, Log, TEXT("%s: deleted edit layer: %s"), *RemoveLayers.Key->GetName(), *EditLayerName.ToString())
				else
					UE_LOG(LogHoudiniEngine, Error, TEXT("%s: failed to delete edit layer: %s"), *RemoveLayers.Key->GetName(), *EditLayerName.ToString())
#else
				const int32 NumPrevEditLayers = RemoveLayers.Key->GetLayerCount();
				RemoveLayers.Key->DeleteLayer(LayerIdx);
				if (NumPrevEditLayers > RemoveLayers.Key->GetLayerCount())
					UE_LOG(LogHoudiniEngine, Log, TEXT("%s: deleted edit layer: %s"), *RemoveLayers.Key->GetName(), *EditLayerName.ToString())
				else
					UE_LOG(LogHoudiniEngine, Error, TEXT("%s: failed to delete edit layer: %s"), *RemoveLayers.Key->GetName(), *EditLayerName.ToString())
#endif
			}
		}
	}

	FHoudiniLandscapeOutputHelper LandscapeOutputHelper;
	TArray<ALandscape*> SetLandscapes;  // Ensure one ALandscape only be set UProperties once
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 6)) || (ENGINE_MAJOR_VERSION > 5)
	TArray<ALandscape*> ModifiedLandscapes;
	TArray<FGuid> SetEditLayerGuids;  // Ensure one EditLayer only be set UProperties once
#endif
	bool bHasTempLayerAdded = false;
	for (const FHoudiniLandscapePart& Part : Parts)
	{
		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;
		const HAPI_VolumeInfo& VolumeInfo = Part.VolumeInfo;
		const TArray<std::string>& AttribNames = Part.AttribNames;
		const FName& LayerName = Part.LayerName;

		ALandscape* Landscape = Part.TargetLandscape;
		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		if (!IsValid(LandscapeInfo))
			continue;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
		ULandscapeEditLayerBase* EditLayer = const_cast<ULandscapeEditLayerBase*>(FindOrCreateEditLayer(Landscape, Part.EditLayerName));
		const FName EditLayerName = EditLayer->GetName();
		const FGuid EditLayerGuid = EditLayer->GetGuid();
#elif ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
		ULandscapeEditLayerBase* EditLayer = Landscape->bCanHaveLayersContent ?
			const_cast<ULandscapeEditLayerBase*>(FindOrCreateEditLayer(Landscape, Part.EditLayerName)) : nullptr;
		const FName EditLayerName = EditLayer ? EditLayer->GetName() : NAME_None;
		const FGuid EditLayerGuid = EditLayer ? EditLayer->GetGuid() : FGuid();
#else
		FLandscapeLayer* EditLayer = Landscape->bCanHaveLayersContent ?
			const_cast<FLandscapeLayer*>(FindOrCreateEditLayer(Landscape, Part.EditLayerName)) : nullptr;
		const FName& EditLayerName = EditLayer ? EditLayer->Name : NAME_None;
		const FGuid& EditLayerGuid = EditLayer ? EditLayer->Guid : FGuid();
#endif
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
		if (!EditLayer && Landscape->HasLayersContent())  // May failed to create a new Editlayer if num >= MaxEditlayerNum
			continue;

		// Clear layer
		if (Landscape->HasLayersContent())
#endif
		{
			if (Part.ShouldClearEditLayer())
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
				Landscape->ClearEditLayer(EditLayerGuid, nullptr, ELandscapeToolTargetTypeFlags::All);
#else
				Landscape->ClearLayer(EditLayerGuid, nullptr, ELandscapeClearMode::Clear_All);
#endif
				UE_LOG(LogHoudiniEngine, Log, TEXT("%s: clear %s"), *Landscape->GetName(), *EditLayerName.ToString());
			}
			else if (Part.ShouldClearLayer())
			{
				if (LayerName == HoudiniHeightLayerName)
				{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
					Landscape->ClearEditLayer(EditLayerGuid, nullptr, ELandscapeToolTargetTypeFlags::Heightmap);
#else
					Landscape->ClearLayer(EditLayerGuid, nullptr, ELandscapeClearMode::Clear_Heightmap);
#endif
					UE_LOG(LogHoudiniEngine, Log, TEXT("%s: clear %s/height"), *Landscape->GetName(), *EditLayerName.ToString());
				}
				else if (LayerName == HoudiniAlphaLayerName)
				{
					Landscape->ClearPaintLayer(EditLayerGuid, ALandscapeProxy::VisibilityLayer);
					UE_LOG(LogHoudiniEngine, Log, TEXT("%s: clear %s/Alpha"), *Landscape->GetName(), *EditLayerName.ToString());
				}
				else if (const FLandscapeInfoLayerSettings* FoundLayerSettingsPtr =
					LandscapeInfo->Layers.FindByPredicate([LayerName](const FLandscapeInfoLayerSettings& LayerSettings)
						{
							return LayerSettings.LayerName == LayerName;
						}))
				{
					if (IsValid(FoundLayerSettingsPtr->LayerInfoObj))
					{
						Landscape->ClearPaintLayer(EditLayerGuid, FoundLayerSettingsPtr->LayerInfoObj);
						UE_LOG(LogHoudiniEngine, Log, TEXT("%s: clear %s/%s"), *Landscape->GetName(), *EditLayerName.ToString(), *LayerName.ToString());
					}
				}
			}
		}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
		else if (Part.ShouldClearLayer())
		{
			if (LayerName == HoudiniHeightLayerName)
			{
				UE_LOG(LogHoudiniEngine, Error, TEXT("Cannot clear height layer on Non-EditLayer landscape"))
			}
			else if (LayerName == HoudiniAlphaLayerName)
			{
				FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
				LandscapeEdit.DeleteLayer(ALandscapeProxy::VisibilityLayer);
				UE_LOG(LogHoudiniEngine, Log, TEXT("%s: clear height"), *Landscape->GetName());
			}
			else if (const FLandscapeInfoLayerSettings* FoundLayerSettingsPtr =
				Landscape->GetLandscapeInfo()->Layers.FindByPredicate([LayerName](const FLandscapeInfoLayerSettings& LayerSettings)
					{
						return LayerSettings.LayerName == LayerName;
					}))
			{
				if (IsValid(FoundLayerSettingsPtr->LayerInfoObj))
				{
					FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
					LandscapeEdit.DeleteLayer(FoundLayerSettingsPtr->LayerInfoObj);
					UE_LOG(LogHoudiniEngine, Log, TEXT("%s: clear %s"), *Landscape->GetName(), *LayerName.ToString());
				}
			}
		}
#endif
		if (Part.bClear)  // Has been cleared
			continue;

		// Retrieve UProperties
		TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

		// Set UProperties on current EditLayer
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
		SetPropertiesOnEditLayer(PropAttribs, EditLayer, Landscape);
#else
		bool bHasLandscapeChanged = ModifiedLandscapes.Contains(Landscape);
		SetPropertiesOnEditLayer(PropAttribs, EditLayer, Landscape,
			SetEditLayerGuids, bHasLandscapeChanged);
		if (bHasLandscapeChanged)
			ModifiedLandscapes.AddUnique(Landscape);
#endif
		// Set UProperties on current landscape
		if (!SetLandscapes.Contains(Landscape))
		{
			SetLandscapes.Add(Landscape);
			for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
			{
				const FString& PropertyName = PropAttrib->GetAttributeName();
				if (PropertyName == TEXT("LandscapeMaterial") || PropertyName == TEXT("LandscapeHoleMaterial"))
					PropAttrib->SetObjectPropertyValues(Landscape, 0);
			}
		}

		FIntRect LandscapeExtent;
		LandscapeInfo->GetLandscapeExtent(LandscapeExtent);

		// Retrieve heightfield data
		FIntRect HeightfieldExtent;
		FIntRect TargetLandscapeExtent;
		GetHeightfieldOutputExtent(Landscape->GetTransform(), LandscapeExtent, VolumeInfo,
			TargetLandscapeExtent, HeightfieldExtent);

		ULandscapeLayerInfoObject* LayerInfo = nullptr;
		bool bSubtractiveModified = false;
		if (LayerName != HoudiniHeightLayerName)
		{
			HOUDINI_FAIL_RETURN(HapiFindOrCreateLayerInfo(LayerInfo, Landscape, LandscapeInfo, LayerName, Node->GetCookFolderPath(),
				NodeId, PartId, AttribNames, PartInfo.attributeCounts, bHasTempLayerAdded));
			if (!IsValid(LayerInfo))
				continue;

			// Set UProperties on LayerInfo
			for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
			{
				if (PropAttrib->GetAttributeName() != TEXT("LayerName"))  // We should NOT allow layer name to be set by houdini
					PropAttrib->SetObjectPropertyValues(LayerInfo, 0);
			}

			if (EditLayer)
				HOUDINI_FAIL_RETURN(HapiSetWeightLayerBlend(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
					EditLayer, LayerInfo, bSubtractiveModified));
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 6)) || (ENGINE_MAJOR_VERSION < 5)
			if (bSubtractiveModified)
				bHasLandscapeChanged = true;
#endif
		}

		if (TargetLandscapeExtent == INVALID_HEIGHTFIELD_EXTENT)
		{
			PrintLandscapeOutputLog(Landscape, EditLayerName, LayerName);
			continue;
		}

		const float* FloatData = nullptr;
		size_t SHMHandle = 0;
		HOUDINI_FAIL_RETURN(HapiGetHeightfieldData(FloatData, SHMHandle, NodeId, PartId, VolumeInfo));

		// Retrieve or get MaskData
		TArray<uint8> AlignedMaskData;
		const uint8* MaskData = nullptr;
		{
			FHoudiniPartialOutputMask* FoundMaskPtr = PartialOutputMasks.FindByPredicate(
				[Landscape, EditLayerName](const FHoudiniPartialOutputMask& Mask)
				{
					return Mask.TargetLandscape == Landscape && Mask.EditLayerName == EditLayerName;
				});

			if (FoundMaskPtr)
			{
				HOUDINI_FAIL_RETURN(FoundMaskPtr->HapiRetrieve(NodeId, MaskData));
				AlignDataExtent(MaskData, FoundMaskPtr->Extent, AlignedMaskData, TargetLandscapeExtent);  // Maybe mask extent not equal to data extent, so we need align mask data
				if (!AlignedMaskData.IsEmpty())
					MaskData = AlignedMaskData.GetData();
			}
		}

		// Convert data and write to landscape
		if (LayerName == HoudiniHeightLayerName)
		{
			TArray<uint16> HeightData;
			ConvertHeightmapData(FloatData, VolumeInfo.xLength, POSITION_SCALE_TO_UNREAL / Landscape->GetTransform().GetScale3D().Z,
				HeightData, HeightfieldExtent, TargetLandscapeExtent);

			if (EditLayer)
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
				FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayer->GetGuid());
#else
				FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayer->Guid);
#endif
				LandscapeOutputHelper.SetHeightData(LandscapeInfo, EXPAND_EXTENT(TargetLandscapeExtent), HeightData.GetData(), MaskData);
			}
			else
			{
				FHeightmapAccessor<false> HeightMapAccessor(LandscapeInfo);
				HeightMapAccessor.SetData(EXPAND_EXTENT(TargetLandscapeExtent), HeightData.GetData());
			}
		}
		else
		{
			// Set WeightData
			TArray<uint8> WeightData;
			if (LayerInfo == ALandscapeProxy::VisibilityLayer)
				ConvertAlphaData(FloatData, VolumeInfo.xLength, Landscape->LandscapeHoleMaterial,
					WeightData, HeightfieldExtent, TargetLandscapeExtent);
			else
				ConvertWeightmapData(FloatData, VolumeInfo.xLength,
					WeightData, HeightfieldExtent, TargetLandscapeExtent);
			
			if (EditLayer)
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
				FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayer->GetGuid());
#else
				FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayer->Guid);
#endif
				LandscapeOutputHelper.SetWeightData(LandscapeInfo, LayerInfo, EXPAND_EXTENT(TargetLandscapeExtent), WeightData.GetData(), MaskData, bSubtractiveModified);
			}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
			else
			{
				FAlphamapAccessor<false, false> AlphaAccessor(LandscapeInfo, LayerInfo);
				AlphaAccessor.SetData(EXPAND_EXTENT(TargetLandscapeExtent), WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
			}
#endif
		}

		FHoudiniOutputUtils::CloseData(FloatData, SHMHandle);

		FHoudiniOutputUtils::NotifyLandscapeChanged(Landscape->GetFName(), EditLayerName, LayerName, TargetLandscapeExtent);

		PrintLandscapeOutputLog(Landscape, EditLayerName, LayerName, TargetLandscapeExtent);
	}
	LandscapeOutputHelper.Release();

	if (bHasTempLayerAdded)  // When layer added and not defined in material, then we should refresh landscape to avoid RHI crashes
		ULandscapeInfo::RecreateLandscapeInfo(Node->GetWorld(), true);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 6)) || (ENGINE_MAJOR_VERSION > 5)
	for (ALandscape* ModifiedLandscape : ModifiedLandscapes)
		ModifiedLandscape->Modify();
#endif
	return true;
}

UE_DISABLE_OPTIMIZATION
FHoudiniLandscapeOutput::FHoudiniLandscapeOutput(ALandscape* InLandscape)
{
	Landscape = InLandscape;
	if (IsValid(InLandscape))
	{
		LandscapeName = Landscape->GetFName();

		ULandscapeInfo* LandscapeInfo = InLandscape->GetLandscapeInfo();
		if (IsValid(LandscapeInfo))
		{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			for (const TWeakObjectPtr<ALandscapeStreamingProxy>& LandscapeProxy : LandscapeInfo->GetSortedStreamingProxies())
#else
			for (const TWeakObjectPtr<ALandscapeStreamingProxy>& LandscapeProxy : LandscapeInfo->StreamingProxies)
#endif
			{
				if (LandscapeProxy.IsValid())
					LandscapeProxyHolders.Add(LandscapeProxy.Get());
			}
		}
	}

	FHoudiniEngine::Get().RegisterActor(InLandscape);
}

ALandscape* FHoudiniLandscapeOutput::Load() const
{
	ALandscape* LoadLandscape = Landscape.IsValid() ? Landscape.Get() :
		Cast<ALandscape>(FHoudiniEngine::Get().GetActorByName(LandscapeName));
	
	if (!IsValid(LoadLandscape))
		return nullptr;

	for (const FHoudiniActorHolder& LandscapeProxyHolder : LandscapeProxyHolders)  // Force load all landscape proxies
		LandscapeProxyHolder.Load();
	
	return LoadLandscape;
}

void FHoudiniLandscapeOutput::Destroy() const
{
	for (const FHoudiniActorHolder& LandscapeProxyHolder : LandscapeProxyHolders)
		LandscapeProxyHolder.Destroy();

	FHoudiniEngine::Get().DestroyActorByName(LandscapeName);
}


static void CalculateLandscapeSize(const int32& SrcXSize, const int32& SrcYSize,
	int32& OutSubsectionSizeQuads, int32& OutComponentNumSubsections, int32& OutDstXSize, int32& OutDstYSize)
{
	const int32 SrcSize = FMath::Max(SrcXSize, SrcYSize);
	const double LogSize2 = FMath::Log2(double(SrcSize));
	if (LogSize2 < 12.5)
		OutSubsectionSizeQuads = 63;
	else if (LogSize2 < 15.5)
		OutSubsectionSizeQuads = 127;
	else
		OutSubsectionSizeQuads = 255;
	OutComponentNumSubsections = 2;

	const int32 ComponentSizeQuads = OutSubsectionSizeQuads * OutComponentNumSubsections;
	const int32 NumComponentsX = FMath::Max(FMath::RoundToInt32(double(SrcYSize) / ComponentSizeQuads), 1);  // Houdini X Size is Unreal Y Size
	const int32 NumComponentsY = FMath::Max(FMath::RoundToInt32(double(SrcXSize) / ComponentSizeQuads), 1);  // Houdini X Size is Unreal Y Size

	OutDstXSize = NumComponentsX * ComponentSizeQuads + 1;
	OutDstYSize = NumComponentsY * ComponentSizeQuads + 1;
}
UE_ENABLE_OPTIMIZATION

// See LandscapeConfigHelper.cpp (64) FLandscapeConfigHelper::FindOrAddLandscapeStreamingProxy
static ALandscapeProxy* CreateLandscapeStreamingProxy(UActorPartitionSubsystem* InActorPartitionSubsystem, ULandscapeInfo* InLandscapeInfo, const UActorPartitionSubsystem::FCellCoord& InCellCoord)
{
	ALandscape* Landscape = InLandscapeInfo->LandscapeActor.Get();
	check(Landscape);

	auto LandscapeProxyCreated = [InCellCoord, Landscape, InLandscapeInfo](APartitionActor* PartitionActor)
		{
			const FIntPoint CellLocation(static_cast<int32>(InCellCoord.X) * Landscape->GetGridSize(), static_cast<int32>(InCellCoord.Y) * Landscape->GetGridSize());

			ALandscapeProxy* LandscapeProxy = CastChecked<ALandscapeProxy>(PartitionActor);
			// copy shared properties to this new proxy
			LandscapeProxy->SynchronizeSharedProperties(Landscape);
			const FVector ProxyLocation = Landscape->GetActorLocation() + FVector(CellLocation.X * Landscape->GetActorRelativeScale3D().X, CellLocation.Y * Landscape->GetActorRelativeScale3D().Y, 0.0f);

			LandscapeProxy->CreateLandscapeInfo();
			LandscapeProxy->SetActorLocationAndRotation(ProxyLocation, Landscape->GetActorRotation());
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			LandscapeProxy->SetSectionBase(FIntPoint(CellLocation.X, CellLocation.Y));
#else
			LandscapeProxy->LandscapeSectionOffset = FIntPoint(CellLocation.X, CellLocation.Y);
#endif
			LandscapeProxy->SetIsSpatiallyLoaded(LandscapeProxy->GetLandscapeInfo()->AreNewLandscapeActorsSpatiallyLoaded());
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			// Fixup the transform based on the section base offset
			InLandscapeInfo->FixupProxiesTransform();
#endif
		};

	const bool bCreate = true;
	const bool bBoundsSearch = false;

	ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(InActorPartitionSubsystem->GetActor(ALandscapeStreamingProxy::StaticClass(), InCellCoord, bCreate, InLandscapeInfo->LandscapeGuid, Landscape->GetGridSize(), bBoundsSearch, LandscapeProxyCreated));
	check(!LandscapeProxy || LandscapeProxy->GetGridSize() == Landscape->GetGridSize());
	return LandscapeProxy;
}

// See LandscapeConfigHelper.cpp (92) FLandscapeConfigHelper::ChangeGridSize
static bool SplitLandscape(ULandscapeInfo* InLandscapeInfo, uint32 InNewGridSizeInComponents, TSet<AActor*>& OutActorsToDelete)
{
	check(InLandscapeInfo);

	const uint32 GridSize = InLandscapeInfo->GetGridSize(InNewGridSizeInComponents);

	InLandscapeInfo->LandscapeActor->Modify();
	InLandscapeInfo->LandscapeActor->SetGridSize(GridSize);

	// This needs to be done before moving components
	InLandscapeInfo->LandscapeActor->InitializeLandscapeLayersWeightmapUsage();

	// Make sure if actor didn't include grid size in name it now does. This will avoid recycling 
	// LandscapeStreamingProxy actors and create new ones with the proper name.
	InLandscapeInfo->LandscapeActor->bIncludeGridSizeInNameForLandscapeActors = true;

	FIntRect Extent;
	InLandscapeInfo->GetLandscapeExtent(Extent.Min.X, Extent.Min.Y, Extent.Max.X, Extent.Max.Y);
	const FBox Bounds(FVector(Extent.Min), FVector(Extent.Max));

	UWorld* World = InLandscapeInfo->LandscapeActor->GetWorld();
	UActorPartitionSubsystem* ActorPartitionSubsystem = World->GetSubsystem<UActorPartitionSubsystem>();

	TArray<ULandscapeComponent*> LandscapeComponents;
	LandscapeComponents.Reserve(InLandscapeInfo->XYtoComponentMap.Num());
	InLandscapeInfo->ForAllLandscapeComponents([&LandscapeComponents](ULandscapeComponent* LandscapeComponent)
		{
			LandscapeComponents.Add(LandscapeComponent);
		});

	TSet<ALandscapeProxy*> ProxiesToDelete;

	FActorPartitionGridHelper::ForEachIntersectingCell(ALandscapeStreamingProxy::StaticClass(), Extent, World->PersistentLevel, [ActorPartitionSubsystem, InLandscapeInfo, InNewGridSizeInComponents, &LandscapeComponents, &ProxiesToDelete](const UActorPartitionSubsystem::FCellCoord& CellCoord, const FIntRect& CellBounds)
		{
			TMap<ULandscapeComponent*, UMaterialInterface*> ComponentMaterials;
			TMap<ULandscapeComponent*, UMaterialInterface*> ComponentHoleMaterials;
			TMap <ULandscapeComponent*, TMap<int32, UMaterialInterface*>> ComponentLODMaterials;

			TArray<ULandscapeComponent*> ComponentsToMove;
			const int32 MaxComponents = (int32)(InNewGridSizeInComponents * InNewGridSizeInComponents);
			ComponentsToMove.Reserve(MaxComponents);
			for (int32 i = 0; i < LandscapeComponents.Num();)
			{
				ULandscapeComponent* LandscapeComponent = LandscapeComponents[i];
				if (CellBounds.Contains(LandscapeComponent->GetSectionBase()))
				{
					ComponentMaterials.FindOrAdd(LandscapeComponent, LandscapeComponent->GetLandscapeMaterial());
					ComponentHoleMaterials.FindOrAdd(LandscapeComponent, LandscapeComponent->GetLandscapeHoleMaterial());
					TMap<int32, UMaterialInterface*>& LODMaterials = ComponentLODMaterials.FindOrAdd(LandscapeComponent);
					for (int8 LODIndex = 0; LODIndex <= 8; ++LODIndex)
					{
						LODMaterials.Add(LODIndex, LandscapeComponent->GetLandscapeMaterial(LODIndex));
					}

					ComponentsToMove.Add(LandscapeComponent);
					LandscapeComponents.RemoveAtSwap(i);
					ProxiesToDelete.Add(LandscapeComponent->GetTypedOuter<ALandscapeProxy>());
				}
				else
				{
					i++;
				}
			}

			check(ComponentsToMove.Num() <= MaxComponents);
			if (ComponentsToMove.Num())
			{
				ALandscapeProxy* LandscapeProxy = CreateLandscapeStreamingProxy(ActorPartitionSubsystem, InLandscapeInfo, CellCoord);
				check(LandscapeProxy);
				InLandscapeInfo->MoveComponentsToProxy(ComponentsToMove, LandscapeProxy);

				// Make sure components retain their Materials if they don't match with their parent proxy
				for (ULandscapeComponent* MovedComponent : ComponentsToMove)
				{
					UMaterialInterface* PreviousLandscapeMaterial = ComponentMaterials.FindChecked(MovedComponent);
					UMaterialInterface* PreviousLandscapeHoleMaterial = ComponentHoleMaterials.FindChecked(MovedComponent);
					TMap<int32, UMaterialInterface*> PreviousLandscapeLODMaterials = ComponentLODMaterials.FindChecked(MovedComponent);

					MovedComponent->OverrideMaterial = nullptr;
					if (PreviousLandscapeMaterial != nullptr && PreviousLandscapeMaterial != MovedComponent->GetLandscapeMaterial())
					{
						// If Proxy doesn't differ from Landscape override material there first
						if (LandscapeProxy->GetLandscapeMaterial() == LandscapeProxy->GetLandscapeActor()->GetLandscapeMaterial())
						{
							LandscapeProxy->LandscapeMaterial = PreviousLandscapeMaterial;
						}
						else // If it already differs it means that the component differs from it, override on component
						{
							MovedComponent->OverrideMaterial = PreviousLandscapeMaterial;
						}
					}

					MovedComponent->OverrideHoleMaterial = nullptr;
					if (PreviousLandscapeHoleMaterial != nullptr && PreviousLandscapeHoleMaterial != MovedComponent->GetLandscapeHoleMaterial())
					{
						// If Proxy doesn't differ from Landscape override material there first
						if (LandscapeProxy->GetLandscapeHoleMaterial() == LandscapeProxy->GetLandscapeActor()->GetLandscapeHoleMaterial())
						{
							LandscapeProxy->LandscapeHoleMaterial = PreviousLandscapeHoleMaterial;
						}
						else // If it already differs it means that the component differs from it, override on component
						{
							MovedComponent->OverrideHoleMaterial = PreviousLandscapeHoleMaterial;
						}
					}

					TArray<FLandscapePerLODMaterialOverride> PerLODOverrideMaterialsForComponent;
					TArray<FLandscapePerLODMaterialOverride> PerLODOverrideMaterialsForProxy = LandscapeProxy->GetPerLODOverrideMaterials();
					for (int8 LODIndex = 0; LODIndex <= 8; ++LODIndex)
					{
						UMaterialInterface* PreviousLODMaterial = PreviousLandscapeLODMaterials.FindChecked(LODIndex);
						// If Proxy doesn't differ from Landscape override material there first
						if (PreviousLODMaterial != nullptr && PreviousLODMaterial != MovedComponent->GetLandscapeMaterial(LODIndex))
						{
							if (LandscapeProxy->GetLandscapeMaterial(LODIndex) == LandscapeProxy->GetLandscapeActor()->GetLandscapeMaterial(LODIndex))
							{
								PerLODOverrideMaterialsForProxy.Add({ LODIndex, TObjectPtr<UMaterialInterface>(PreviousLODMaterial) });
							}
							else // If it already differs it means that the component differs from it, override on component
							{
								PerLODOverrideMaterialsForComponent.Add({ LODIndex, TObjectPtr<UMaterialInterface>(PreviousLODMaterial) });
							}
						}
					}
					MovedComponent->SetPerLODOverrideMaterials(PerLODOverrideMaterialsForComponent);
					LandscapeProxy->SetPerLODOverrideMaterials(PerLODOverrideMaterialsForProxy);
				}
			}

			return true;
		}, GridSize);

	// Only delete Proxies that where not reused
	for (ALandscapeProxy* ProxyToDelete : ProxiesToDelete)
	{
		if (ProxyToDelete->LandscapeComponents.Num() > 0 || ProxyToDelete->IsA<ALandscape>())
		{
			check(ProxyToDelete->GetGridSize() == GridSize);
			continue;
		}

		OutActorsToDelete.Add(ProxyToDelete);
	}
	
	//if (InLandscapeInfo->CanHaveLayersContent())
	//{
	//	InLandscapeInfo->ForceLayersFullUpdate();
	//}
	
	return true;
}

UE_DISABLE_OPTIMIZATION
bool UHoudiniOutputLandscape::HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputLandscape);

	struct FHoudiniLandscapePart
	{
		FHoudiniLandscapePart(const HAPI_PartInfo& PartInfo, const HAPI_VolumeInfo& InVolumeInfo) :
			Info(PartInfo), VolumeInfo(InVolumeInfo) {}

		HAPI_PartInfo Info;
		HAPI_VolumeInfo VolumeInfo;
		TArray<std::string> AttribNames;
		FName LayerName;
		FName EditLayerName;
	};


	const int32& NodeId = GeoInfo.nodeId;

	TMap<TPair<UMaterialInterface*, uint32>, UMaterialInstance*> MatParmMap;  // Use to find created MaterialInstance quickly
	TMap<FString, TArray<FHoudiniLandscapePart>> LandscapeLabelPartsMap;  // <OutputName, Parts>
	for (const HAPI_PartInfo& PartInfo : PartInfos)
	{
		const int32& PartId = PartInfo.id;

		HAPI_VolumeInfo VolumeInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVolumeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &VolumeInfo));
		if (VolumeInfo.type != HAPI_VOLUMETYPE_HOUDINI && VolumeInfo.storage != HAPI_STORAGETYPE_FLOAT)
			continue;

		FHoudiniLandscapePart Part(PartInfo, VolumeInfo);

		std::string NameStr;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(VolumeInfo.nameSH, NameStr));
		if (NameStr.empty())
			continue;
		Part.LayerName = NameStr.c_str();
		
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, Part.AttribNames));
		const TArray<std::string>& AttribNames = Part.AttribNames;

		// Find target landscape name
		FString OutputName;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
			AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OUTPUT_NAME, OutputName));
		
		// Find target EditLayer name
		FString EditLayerName;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
			AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_LANDSCAPE_EDITLAYER_NAME, EditLayerName));
		Part.EditLayerName = *EditLayerName;

		LandscapeLabelPartsMap.FindOrAdd(OutputName).Add(Part);
	}

	TArray<FHoudiniLandscapeOutput> NewLandscapeOutputs;
	bool bHasTempLayerAdded = false;
	UWorld* World = GetNode()->GetWorld();
	for (const auto& LabelParts : LandscapeLabelPartsMap)
	{
		const FHoudiniLandscapePart* MainPartPtr = LabelParts.Value.FindByPredicate(
			[](const FHoudiniLandscapePart& Part) { return Part.LayerName == HoudiniHeightLayerName; });
		if (!MainPartPtr)
			MainPartPtr = &LabelParts.Value[0];
		
		const int32& XSize = MainPartPtr->VolumeInfo.xLength;
		const int32& YSize = MainPartPtr->VolumeInfo.yLength;

		FName FirstEditLayerName;
		if (MainPartPtr->EditLayerName.IsNone())
		{
			for (const FHoudiniLandscapePart& Part : LabelParts.Value)
			{
				if (Part.EditLayerName != NAME_None)
				{
					FirstEditLayerName = Part.EditLayerName;
					break;
				}
			}
		}
		else
			FirstEditLayerName = MainPartPtr->EditLayerName;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
		if (FirstEditLayerName.IsNone())
			FirstEditLayerName = FName("Layer");
#endif
		// See "Source/Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_NewLandscape.cpp" Line 1104

		// We should convert houdini size to unreal landscape size first, in order to judge whether landscape could be reused
		int32 SubsectionSizeQuads, ComponentNumSubsections, DstXSize, DstYSize;
		CalculateLandscapeSize(XSize, YSize, SubsectionSizeQuads, ComponentNumSubsections, DstXSize, DstYSize);
		
		// Process material and material instances
		bool bHasMatAttrib = false;
		UMaterialInterface* Material = nullptr;
		bool bHasHoleMatAttrib = false;
		UMaterialInterface* HoleMaterial = nullptr;
		{
			const HAPI_PartInfo& PartInfo = MainPartPtr->Info;
			const int32& PartId = PartInfo.id;
			const TArray<std::string>& AttribNames = MainPartPtr->AttribNames;

			TArray<TSharedPtr<FHoudiniAttribute>> MatAttribs;
			FString MaterialRef;

			// LandscapeMaterial
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
				AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, MaterialRef, &bHasMatAttrib));
			if (bHasMatAttrib && !IS_ASSET_PATH_INVALID(MaterialRef))
			{
				Material = LoadObject<UMaterialInterface>(nullptr, *MaterialRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
				if (IsValid(Material))
				{
					FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
						HAPI_ATTRIB_PREFIX_UNREAL_MATERIAL_PARAMETER, MatAttribs);

					Material = FHoudiniOutputUtils::GetMaterialInstance(Material, MatAttribs,
						[](const HAPI_AttributeOwner&) { return 0; }, GetNode()->GetCookFolderPath(), MatParmMap);
				}
			}
			
			if (!IsValid(Material))
			{
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
					AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_MATERIAL, MaterialRef, &bHasMatAttrib));
				Material = IS_ASSET_PATH_INVALID(MaterialRef) ? nullptr :
					LoadObject<UMaterialInterface>(nullptr, *MaterialRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
			}
			
			// LandscapeHoleMaterial
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
				AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_LANDSCAPE_HOLE_MATERIAL_INSTANCE, MaterialRef, &bHasHoleMatAttrib));
			if (bHasHoleMatAttrib && !IS_ASSET_PATH_INVALID(MaterialRef))
			{
				HoleMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
				if (IsValid(HoleMaterial))
				{
					FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
						HAPI_ATTRIB_PREFIX_UNREAL_MATERIAL_PARAMETER, MatAttribs);

					HoleMaterial = FHoudiniOutputUtils::GetMaterialInstance(HoleMaterial, MatAttribs,
						[](const HAPI_AttributeOwner&) { return 0; }, GetNode()->GetCookFolderPath(), MatParmMap);
				}
			}

			if (!IsValid(HoleMaterial))
			{
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
					AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_LANDSCAPE_HOLE_MATERIAL, MaterialRef, &bHasHoleMatAttrib));
				HoleMaterial = IS_ASSET_PATH_INVALID(MaterialRef) ? nullptr :
					LoadObject<UMaterialInterface>(nullptr, *MaterialRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
			}
		}

		// Check whether we should split landscape
		bool bSplitLandscape = GetNode()->GetWorld()->IsPartitionedWorld();
		{
			HAPI_AttributeOwner SplitActorsOwner = FHoudiniEngineUtils::QueryAttributeOwner(
				MainPartPtr->AttribNames, MainPartPtr->Info.attributeCounts, HAPI_ATTRIB_UNREAL_SPLIT_ACTORS);
			TArray<int8> bSplitActors;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, MainPartPtr->Info.id,
				HAPI_ATTRIB_UNREAL_SPLIT_ACTORS, bSplitActors, SplitActorsOwner));
			if (!bSplitActors.IsEmpty())
				bSplitLandscape = bool(bSplitActors[0]);
		}

		ALandscape* Landscape = nullptr;
		const int32 FoundOldLandscapeIdx = LandscapeOutputs.IndexOfByPredicate(
			[DstXSize, DstYSize, bSplitLandscape](const FHoudiniLandscapeOutput& OldLandscapeOutput)
			{
				if (ALandscape* Landscape = OldLandscapeOutput.Load())
				{
					ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
					if (IsValid(LandscapeInfo))
					{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
						if (LandscapeInfo->GetSortedStreamingProxies().IsEmpty() == bSplitLandscape)
#else
						if (LandscapeInfo->StreamingProxies.IsEmpty() == bSplitLandscape)
#endif
							return false;

						FIntRect LandscapeExtent;
						LandscapeInfo->GetLandscapeExtent(LandscapeExtent);
						return ((LandscapeExtent.Max.X + 1) == DstXSize) && ((LandscapeExtent.Max.Y + 1) == DstYSize);
					}
				}
				
				return false;
			});


		// Set Landscape Transform
		bool bHasLandscapeChanged = false;
		FTransform LandscapeTransform;
		{
			// If landscape size changed, we should correct transform
			const HAPI_Transform& HapiTransform = MainPartPtr->VolumeInfo.transform;
			LandscapeTransform.SetLocation(FVector(HapiTransform.position[0], HapiTransform.position[2], HapiTransform.position[1]) * POSITION_SCALE_TO_UNREAL);
			FRotator Rotator = FQuat(HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[1], -HapiTransform.rotationQuaternion[3]).Rotator();
			Rotator.Yaw -= 90.0;
			Rotator.Roll += 90.0;
			LandscapeTransform.SetRotation(Rotator.Quaternion());
			LandscapeTransform.SetScale3D(FVector(HapiTransform.scale[2], HapiTransform.scale[0], HapiTransform.scale[1]) * 2.0 * POSITION_SCALE_TO_UNREAL);

			FTransform SizeScaleTransform = FTransform::Identity;
			SizeScaleTransform.SetScale3D(FVector(YSize / double(DstXSize), XSize / double(DstYSize), 1.0));
			LandscapeTransform.Accumulate(SizeScaleTransform);
		}

		if (LandscapeOutputs.IsValidIndex(FoundOldLandscapeIdx))
		{
			Landscape = LandscapeOutputs[FoundOldLandscapeIdx].Load();
			LandscapeOutputs.RemoveAt(FoundOldLandscapeIdx);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
			if (FirstEditLayerName == NAME_None)
			{
				Landscape->DeleteLayers();
				Landscape->bCanHaveLayersContent = false;
			}
			else
				Landscape->bCanHaveLayersContent = true;
#endif
			if (!Landscape->GetActorTransform().Equals(LandscapeTransform))
			{
				bHasLandscapeChanged = true;
				Landscape->SetActorTransform(LandscapeTransform);
				Landscape->GetLandscapeInfo()->FixupProxiesTransform(true);  // Also correct proxies' transform
			}
		}
		else
		{
			bHasLandscapeChanged = true;

			FHoudiniEngine::Get().FinishHoudiniMainTaskMessage();  // Avoid D3D12 invalid scissor crash

			const bool bBackupIsSilent = GIsSilent;
			GIsSilent = true;  // Disable FSlowTask::MakeDialog, (process bar) in ALandscapeProxy::Import

			Landscape = World->SpawnActor<ALandscape>(LandscapeTransform.GetLocation(), LandscapeTransform.GetRotation().Rotator());
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
			Landscape->bCanHaveLayersContent = FirstEditLayerName != NAME_None;
#endif
			Landscape->LandscapeMaterial = nullptr;
			Landscape->SetActorRelativeScale3D(LandscapeTransform.GetScale3D());
			Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((DstXSize * DstYSize) / (2048 * 2048) + 1), (uint32)2);  // Copy from LandscapeEditorDetailCustomization_NewLandscape.cpp Line 1114
			
			TMap<FGuid, TArray<uint16>> ImportHeightData;
			ImportHeightData.Add(FGuid());
			TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportLayerInfo;
			ImportLayerInfo.Add(FGuid());
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
			Landscape->Import(FGuid::NewGuid(), 0, 0, DstXSize - 1, DstYSize - 1, ComponentNumSubsections, SubsectionSizeQuads,
				ImportHeightData, TEXT(""), ImportLayerInfo, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
			if (Landscape->bCanHaveLayersContent)
#endif
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
				Landscape->GetEditLayer(0)->SetName(FirstEditLayerName, false);
#else
				const_cast<FLandscapeLayer*>(Landscape->GetLayerConst(0))->Name = FirstEditLayerName;
#endif
#else
			Landscape->Import(FGuid::NewGuid(), 0, 0, DstXSize - 1, DstYSize - 1, ComponentNumSubsections, SubsectionSizeQuads,
				ImportHeightData, TEXT(""), ImportLayerInfo, ELandscapeImportAlphamapType::Additive);

			if (Landscape->bCanHaveLayersContent)
				Landscape->LandscapeLayers[0].Name = FirstEditLayerName;
#endif

			if (!LabelParts.Key.IsEmpty())
				Landscape->SetActorLabel(LabelParts.Key, false);
			else
				Landscape->SetActorLabel(TEXT("Landscape_") + GetNode()->GetActorLabel(false), false);

			if (bSplitLandscape)
			{
				TSet<AActor*> ActorsToDelete;
				SplitLandscape(Landscape->GetLandscapeInfo(), ComponentNumSubsections, ActorsToDelete);
			}

			GIsSilent = bBackupIsSilent;
		}

		const bool bHasMaterialChanged = bHasMatAttrib && (Landscape->LandscapeMaterial != Material);
		if (bHasMaterialChanged)
			Landscape->LandscapeMaterial = Material;
		const bool bHasHoleMaterialChanged = bHasHoleMatAttrib && (Landscape->LandscapeHoleMaterial != HoleMaterial);
		if (bHasHoleMaterialChanged)
			Landscape->LandscapeHoleMaterial = HoleMaterial;

		ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
		if (bHasMaterialChanged || bHasHoleMaterialChanged)
		{
			LandscapeInfo->UpdateLayerInfoMap();
			bHasLandscapeChanged = true;
		}

		FIntRect LandscapeExtent;
		LandscapeInfo->GetLandscapeExtent(LandscapeExtent);

		
		TMap<FName, FGuid> FoundEditLayers;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
		if (Landscape->HasLayersContent())  // Find EditLayers first, then could avoid rename editlayer
#endif
		{
			for (const FHoudiniLandscapePart& Part : LabelParts.Value)
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
				if (const ULandscapeEditLayerBase* FoundEditLayer = Landscape->GetEditLayerConst(Part.EditLayerName))
					FoundEditLayers.Add(Part.EditLayerName, FoundEditLayer->GetGuid());
#else
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
				if (const FLandscapeLayer* FoundEditLayerPtr = Landscape->GetLayerConst(Part.EditLayerName))
#else
				if (const FLandscapeLayer* FoundEditLayerPtr = Landscape->GetLayer(Part.EditLayerName))
#endif
					FoundEditLayers.Add(Part.EditLayerName, FoundEditLayerPtr->Guid);
#endif
			}
		}

		// Get Main Part UProperties
		TArray<TSharedPtr<FHoudiniAttribute>> MainPropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, MainPartPtr->Info.id, MainPartPtr->AttribNames, MainPartPtr->Info.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, MainPropAttribs));

		// Set Landscape UProperties
		MainPropAttribs.RemoveAll([Landscape](const TSharedPtr<FHoudiniAttribute>& PropAttrib)
			{
				const FString& PropertyName = PropAttrib->GetAttributeName();
				if (PropertyName == TEXT("LandscapeMaterial") || PropertyName == TEXT("LandscapeHoleMaterial"))
					return true;
				else if (PropAttrib->SetObjectPropertyValues(Landscape, 0))
					return true;
				return false;
			});

		FHoudiniLandscapeOutputHelper LandscapeOutputHelper;
		TArray<FGuid> SetEditLayerGuids;  // Ensure one editlayer only be set UProperties once
		TArray<FName> NewLayerNames;
		for (const FHoudiniLandscapePart& Part : LabelParts.Value)
		{
			const HAPI_PartInfo& PartInfo = Part.Info;
			const int32& PartId = PartInfo.id;
			const HAPI_VolumeInfo& VolumeInfo = Part.VolumeInfo;
			const FName& LayerName = Part.LayerName;
			const TArray<std::string>& AttribNames = Part.AttribNames;
			const FName& EditLayerName = (Part.EditLayerName == NAME_None) ? FirstEditLayerName : Part.EditLayerName;

			FGuid EditLayerGuid = FGuid();
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
			ULandscapeEditLayerBase* EditLayer = nullptr;
#else
			FLandscapeLayer* EditLayer = nullptr;
#endif
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
			if (Landscape->bCanHaveLayersContent)
#endif
			{
				if (const FGuid* FoundEditLayerGuidPtr = FoundEditLayers.Find(EditLayerName))
				{
					EditLayerGuid = *FoundEditLayerGuidPtr;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
					EditLayer = Landscape->GetEditLayer(EditLayerGuid);
#elif ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
					EditLayer = const_cast<FLandscapeLayer*>(Landscape->GetLayerConst(EditLayerGuid));
#else
					EditLayer = const_cast<FLandscapeLayer*>(Landscape->GetLayer(EditLayerGuid));
#endif
				}
				else
				{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
					const int32 NumOrigEditLayers = Landscape->GetLayersConst().Num();
#else
					const int32 NumOrigEditLayers = int32(Landscape->GetLayerCount());
#endif
					if (NumOrigEditLayers > FoundEditLayers.Num())
					{
						for (int32 OrigEditLayerIdx = 0; OrigEditLayerIdx < NumOrigEditLayers; ++OrigEditLayerIdx)
						{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
							ULandscapeEditLayerBase* OrigEditLayer = Landscape->GetEditLayer(OrigEditLayerIdx);
							if (!FoundEditLayers.Contains(OrigEditLayer->GetName()))  // If NOT found, then we could just rename this editlayer to reuse it
							{
								EditLayerGuid = OrigEditLayer->GetGuid();
								OrigEditLayer->SetName(EditLayerName, false);
#else
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
							FLandscapeLayer* OrigEditLayer = const_cast<FLandscapeLayer*>(Landscape->GetLayerConst(OrigEditLayerIdx));
#else
							FLandscapeLayer* OrigEditLayer = const_cast<FLandscapeLayer*>(Landscape->GetLayer(OrigEditLayerIdx));
#endif
							if (!FoundEditLayers.Contains(OrigEditLayer->Name))  // If NOT found, then we could just rename this editlayer to reuse it
							{
								EditLayerGuid = OrigEditLayer->Guid;
								OrigEditLayer->Name = EditLayerName;
#endif
								FoundEditLayers.Add(EditLayerName, EditLayerGuid);
								EditLayer = OrigEditLayer;
								Landscape->Modify();
								break;
							}
						}
					}

					if (!EditLayerGuid.IsValid())
					{
						const int32 TargetEditLayerIdx = Landscape->CreateLayer(EditLayerName);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
						EditLayer = Landscape->GetEditLayer(TargetEditLayerIdx);
						EditLayerGuid = EditLayer->GetGuid();
#else
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
						EditLayer = const_cast<FLandscapeLayer*>(Landscape->GetLayerConst(TargetEditLayerIdx));
#else
						EditLayer = &(Landscape->LandscapeLayers[TargetEditLayerIdx]);
#endif
						EditLayerGuid = EditLayer->Guid;
#endif
					}
				}
			}

			// Retrieve UProperties
			TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
			if (PartId == MainPartPtr->Info.id)  // We could just use the gotten MainPropAttribs
				PropAttribs = MainPropAttribs;
			else  // Need retrieve from current part
				HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
					HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

			// Set UProperty on current EditLayer
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
			SetPropertiesOnEditLayer(PropAttribs, EditLayer, Landscape);
#else
			SetPropertiesOnEditLayer(PropAttribs, EditLayer, Landscape, SetEditLayerGuids, bHasLandscapeChanged);
#endif
			// Retrieve heightfield data
			FIntRect HeightfieldExtent;
			FIntRect TargetLandscapeExtent;
			GetHeightfieldOutputExtent(LandscapeTransform, LandscapeExtent, VolumeInfo,
				TargetLandscapeExtent, HeightfieldExtent);

			ULandscapeLayerInfoObject* LayerInfo = nullptr;
			bool bSubtractiveModified = false;
			if (LayerName != HoudiniHeightLayerName)
			{
				HOUDINI_FAIL_RETURN(HapiFindOrCreateLayerInfo(LayerInfo, Landscape, LandscapeInfo, LayerName, GetNode()->GetCookFolderPath(),
					NodeId, PartId, AttribNames, PartInfo.attributeCounts, bHasTempLayerAdded));
				if (!IsValid(LayerInfo))
					continue;

				// Set UProperties on LayerInfo
				for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
				{
					if (PropAttrib->GetAttributeName() != TEXT("LayerName"))  // We should NOT allow layer name to be set by houdini
						PropAttrib->SetObjectPropertyValues(LayerInfo, 0);
				}

				NewLayerNames.AddUnique(LayerName);

				if (EditLayer)
					HOUDINI_FAIL_RETURN(HapiSetWeightLayerBlend(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
						EditLayer, LayerInfo, bSubtractiveModified));
				if (bSubtractiveModified)
					bHasLandscapeChanged = true;
			}

			if (TargetLandscapeExtent == INVALID_HEIGHTFIELD_EXTENT)
			{
				PrintLandscapeOutputLog(Landscape, EditLayerName, LayerName);
				continue;
			}

			const float* FloatData = nullptr;
			size_t SHMHandle = 0;
			HOUDINI_FAIL_RETURN(HapiGetHeightfieldData(FloatData, SHMHandle, NodeId, PartId, VolumeInfo));

			// Convert data and write to landscape
			if (LayerName == HoudiniHeightLayerName)
			{
				TArray<uint16> HeightData;
				ConvertHeightmapData(FloatData, VolumeInfo.xLength, POSITION_SCALE_TO_UNREAL / Landscape->GetTransform().GetScale3D().Z,
					HeightData, HeightfieldExtent, TargetLandscapeExtent);

				if (EditLayerGuid.IsValid())
				{
					FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayerGuid);
					LandscapeOutputHelper.SetHeightData(LandscapeInfo, EXPAND_EXTENT(TargetLandscapeExtent), HeightData.GetData(), nullptr);
				}
				else
				{
					FHeightmapAccessor<false> HeightMapAccessor(LandscapeInfo);
					HeightMapAccessor.SetData(EXPAND_EXTENT(TargetLandscapeExtent), HeightData.GetData());
				}
			}
			else
			{
				TArray<uint8> WeightData;
				if (LayerInfo == ALandscapeProxy::VisibilityLayer)
					ConvertAlphaData(FloatData, VolumeInfo.xLength, Landscape->LandscapeHoleMaterial,
						WeightData, HeightfieldExtent, TargetLandscapeExtent);
				else
					ConvertWeightmapData(FloatData, VolumeInfo.xLength,
						WeightData, HeightfieldExtent, TargetLandscapeExtent);

				if (EditLayerGuid.IsValid())
				{
					FScopedSetLandscapeEditingLayer Scope(Landscape, EditLayerGuid);
					LandscapeOutputHelper.SetWeightData(LandscapeInfo, LayerInfo, EXPAND_EXTENT(TargetLandscapeExtent), WeightData.GetData(), nullptr, bSubtractiveModified);
				}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
				else
				{
					FAlphamapAccessor<false, false> AlphaAccessor(LandscapeInfo, LayerInfo);
					AlphaAccessor.SetData(EXPAND_EXTENT(TargetLandscapeExtent), WeightData.GetData(), ELandscapeLayerPaintingRestriction::None);
				}
#endif
			}
			FHoudiniOutputUtils::CloseData(FloatData, SHMHandle);

			PrintLandscapeOutputLog(Landscape, EditLayerName, LayerName, TargetLandscapeExtent);
		}
		LandscapeOutputHelper.Release();

		// Delete useless weightmaps
		TArray<TPair<ULandscapeLayerInfoObject*, FName>> LayerInfosToDelete;
		for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
		{
			if (!NewLayerNames.Contains(LayerSettings.LayerName))
				LayerInfosToDelete.Add(TPair<ULandscapeLayerInfoObject*, FName>(LayerSettings.LayerInfoObj, LayerSettings.LayerName));
		}
		for (const TPair<ULandscapeLayerInfoObject*, FName>& LayerInfoToDelete : LayerInfosToDelete)
			LandscapeInfo->DeleteLayer(LayerInfoToDelete.Key, LayerInfoToDelete.Value);

		// Delete useless EditLayers
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION < 7)) || (ENGINE_MAJOR_VERSION < 5)
		if (Landscape->HasLayersContent())
#endif
		{
			TArray<int32> EditLayerIndicesToDelete;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
			for (int32 EditLayerIdx = Landscape->GetLayersConst().Num() - 1; EditLayerIdx >= 0; --EditLayerIdx)
			{
				if (!FoundEditLayers.Contains(Landscape->GetEditLayerConst(0)->GetName()))
#else
			for (int32 EditLayerIdx = int32(Landscape->GetLayerCount()) - 1; EditLayerIdx >= 0; --EditLayerIdx)
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
				if (!FoundEditLayers.Contains(Landscape->GetLayerConst(0)->Name))
#else
				if (!FoundEditLayers.Contains(Landscape->LandscapeLayers[EditLayerIdx].Name))
#endif
#endif
					EditLayerIndicesToDelete.Add(EditLayerIdx);
			}
			for (const int32& EditLayerIdxToDelete : EditLayerIndicesToDelete)
				Landscape->DeleteLayer(EditLayerIdxToDelete);
		}

		if (bHasLandscapeChanged && FoundOldLandscapeIdx >= 0)  // if reused the old landscape and it changed, then we need Modify()
			Landscape->Modify();

		NewLandscapeOutputs.Add(Landscape);
	}

	// Post-process
	Destroy();

	if (bHasTempLayerAdded)  // When layer added and not defined in material, then we should refresh landscape to avoid RHI crashes
		ULandscapeInfo::RecreateLandscapeInfo(GetNode()->GetWorld(), true);

	LandscapeOutputs = NewLandscapeOutputs;

	return true;
}
UE_ENABLE_OPTIMIZATION

void UHoudiniOutputLandscape::Destroy() const
{
	for (const FHoudiniLandscapeOutput& OldLandscapeOutput : LandscapeOutputs)
		OldLandscapeOutput.Destroy();
}

void UHoudiniOutputLandscape::OnNodeTransformed(const FMatrix& DeltaXform) const
{
	// TODO:
}
