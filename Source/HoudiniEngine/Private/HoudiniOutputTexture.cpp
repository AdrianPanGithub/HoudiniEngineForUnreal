// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "SparseVolumeTexture/SparseVolumeTextureData.h"

#include "HAPI/HAPI_Version.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniAttribute.h"


//#include "F:\UnrealEngine\Engine\Plugins\Experimental\Mutable\Source\CustomizableObject\Private\MuCO\UnrealBakeHelpers.cpp"
//#include "F:\UnrealEngine\Engine\Plugins\Runtime\MeshModelingToolset\Source\ModelingComponents\Private\AssetUtils\Texture2DBuilder.cpp"
//#include "F:\UnrealEngine\Engine\Plugins\Experimental\GeometryScripting\Source\GeometryScriptingEditor\Private\EditorTextureMapFunctions.cpp"


bool FHoudiniTextureOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = false;
	bOutShouldHoldByOutput = false;
	if (PartInfo.type == HAPI_PARTTYPE_VOLUME)
	{
		HAPI_VolumeInfo VolumeInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVolumeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id, &VolumeInfo));
		if (VolumeInfo.type == HAPI_VOLUMETYPE_VDB)
		{
			bOutIsValid = true;
			return true;
		}
		else if (VolumeInfo.type == HAPI_VOLUMETYPE_HOUDINI)
		{
			HAPI_VolumeVisualInfo VolumeVisInfo;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVolumeVisualInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id, &VolumeVisInfo));
			if (VolumeVisInfo.type == HAPI_VOLUMEVISTYPE_INVALID)  // TODO: change to HAPI_VOLUMEVISTYPE_IMAGE when HAPI support it
			{
				bOutIsValid = true;
				return true;
			}
		}
	}

	return true;
}


static void VolumeConvertInfoToTextureFormats(const EHoudiniVolumeConvertDataType& TextureStorage, const int32& VolumeTupleSize,
	EPixelFormat& PixelFormat, ETextureSourceFormat& TextureFormat)
{
	PixelFormat = PF_R8G8B8A8;
	TextureFormat = TSF_BGRA8;

	switch (VolumeTupleSize)
	{
	case 1:
	{
		switch (TextureStorage)
		{
		case EHoudiniVolumeConvertDataType::Uint8: { PixelFormat = PF_G8; TextureFormat = TSF_G8; } break;
		case EHoudiniVolumeConvertDataType::Uint16: { PixelFormat = PF_G16; TextureFormat = TSF_G16; } break;
		case EHoudiniVolumeConvertDataType::Float16: { PixelFormat = PF_R16F; TextureFormat = TSF_R16F; } break;
		case EHoudiniVolumeConvertDataType::Float: { PixelFormat = PF_R32_FLOAT; TextureFormat = TSF_R32F; } break;
		}
	}
	break;
	case 2:
	{
		switch (TextureStorage)
		{
		case EHoudiniVolumeConvertDataType::Uint8: { PixelFormat = PF_R8G8; TextureFormat = TSF_BGRA8; } break;
		case EHoudiniVolumeConvertDataType::Uint16: { PixelFormat = PF_G16R16; TextureFormat = TSF_RGBA16; } break;
		case EHoudiniVolumeConvertDataType::Float16: { PixelFormat = PF_G16R16F; TextureFormat = TSF_RGBA16F; } break;
		case EHoudiniVolumeConvertDataType::Float: { PixelFormat = PF_G32R32F; TextureFormat = TSF_RGBA32F; } break;
		}
	}
	break;
	case 3:
	{
		switch (TextureStorage)
		{
		case EHoudiniVolumeConvertDataType::Uint8: { PixelFormat = PF_B8G8R8A8; TextureFormat = TSF_BGRA8; } break;
		case EHoudiniVolumeConvertDataType::Uint16: { PixelFormat = PF_R16G16B16A16_UINT; TextureFormat = TSF_RGBA16; } break;
		case EHoudiniVolumeConvertDataType::Float16: { PixelFormat = PF_FloatRGB; TextureFormat = TSF_RGBA16F; } break;
		case EHoudiniVolumeConvertDataType::Float: { PixelFormat = PF_R32G32B32F; TextureFormat = TSF_RGBA32F; } break;
		}
	}
	break;
	case 4:
	{
		switch (TextureStorage)
		{
		case EHoudiniVolumeConvertDataType::Uint8: { PixelFormat = PF_B8G8R8A8; TextureFormat = TSF_BGRA8; } break;
		case EHoudiniVolumeConvertDataType::Uint16: { PixelFormat = PF_R16G16B16A16_UINT; TextureFormat = TSF_RGBA16; } break;
		case EHoudiniVolumeConvertDataType::Float16: { PixelFormat = PF_FloatRGBA; TextureFormat = TSF_RGBA16F; } break;
		case EHoudiniVolumeConvertDataType::Float: { PixelFormat = PF_A32B32G32R32F; TextureFormat = TSF_RGBA32F; } break;
		}
	}
	break;
	}
}

static bool HapiGetVolumeData(const float*& OutData, size_t& OutHandle,  // 0 means not a SHM
	const int32& NodeId, const int32& PartId, const HAPI_VolumeInfo& VolumeInfo)
{
	const size_t NumPixels = size_t(VolumeInfo.xLength) * size_t(VolumeInfo.yLength);
	const size_t DataLength32 = NumPixels * size_t(VolumeInfo.tupleSize) * (VolumeInfo.storage == HAPI_STORAGETYPE_INT ? 2 : 1);  // int data is int64 in houdini volume

	OutHandle = 0;
	HAPI_AttributeInfo AttribInfo;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		HAPI_ATTRIB_SHARED_MEMORY_PATH, HAPI_ATTROWNER_PRIM, &AttribInfo));
	if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_STRING))
	{
		HAPI_StringHandle SHMPathSH;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_SHARED_MEMORY_PATH, &AttribInfo, &SHMPathSH, 0, 1));
		FString SHMPath;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(SHMPathSH, SHMPath));
		if (!SHMPath.IsEmpty())
			OutData = FHoudiniEngineUtils::GetSharedMemory(*SHMPath, DataLength32, OutHandle);
	}

	if (!OutHandle)
	{
		OutData = (const float*)FMemory::Malloc(DataLength32 * sizeof(float));
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetHeightFieldData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			(float*)OutData, 0, NumPixels));
	}

	return true;
}

static bool HapiGetTextureStorage(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
	EHoudiniVolumeConvertDataType& OutTextureStorage)
{
	HAPI_AttributeOwner TextureStorageOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
		AttribCounts, HAPI_ATTRIB_UNREAL_TEXTURE_STORAGE);
	TArray<int8> TextureStorages;
	HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
		HAPI_ATTRIB_UNREAL_TEXTURE_STORAGE, [](FUtf8StringView AttribValue)
		{
			if (AttribValue.StartsWith("float16"))
				return HAPI_UNREAL_TEXTURE_STORAGE_FLOAT16;
			else if (AttribValue.StartsWith("uint16"))
				return HAPI_UNREAL_TEXTURE_STORAGE_UINT16;
			else if (AttribValue.StartsWith("float"))
				return HAPI_UNREAL_TEXTURE_STORAGE_FLOAT;

			return HAPI_UNREAL_TEXTURE_STORAGE_UINT8;
		}, TextureStorages, TextureStorageOwner));
	if (TextureStorages.IsValidIndex(0))
		OutTextureStorage = EHoudiniVolumeConvertDataType(
			FMath::Clamp(TextureStorages[0], HAPI_UNREAL_TEXTURE_STORAGE_UINT8, HAPI_UNREAL_TEXTURE_STORAGE_FLOAT));

	return true;
}

bool HapiGetVolumeStorageType(const int32& NodeId, const int32& PartId, EHoudiniVolumeStorageType& OutVolumeStorage)
{
	HAPI_AttributeInfo AttribInfo;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(),
		NodeId, PartId, "__volume_storage_type__", HAPI_ATTROWNER_PRIM, &AttribInfo));

	if (!AttribInfo.exists)
	{
		UE_LOG(LogHoudiniEngine, Error, TEXT("Please add \"sharedmemory_volumeoutput\" Sop to your HDA to output textures, as a bug in houdini HAPI"))
		return true;
	}

	int32 IntValue = 1;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(),
		NodeId, PartId, "__volume_storage_type__", &AttribInfo, -1, &IntValue, 0, 1));

	OutVolumeStorage = EHoudiniVolumeStorageType(IntValue);

	return true;
}


class FHoudiniSparseVolumeDataProvider : public UE::SVT::ITextureDataProvider
{
public:
	bool HapiInitialize(const int32& NodeId, const int32& PartId, const HAPI_VolumeInfo& VolumeInfo, const EHoudiniVolumeConvertDataType& TextureStorage)
	{
		TupleSize = VolumeInfo.tupleSize;
		TileSize = VolumeInfo.tileSize;

		// Min should always >= 0, otherwise data won't import, see UStreamableSparseVolumeTexture::AppendFrame
		CreateInfo.VirtualVolumeAABBMin.X = 0;
		CreateInfo.VirtualVolumeAABBMin.Y = 0;
		CreateInfo.VirtualVolumeAABBMin.Z = 0;
		CreateInfo.VirtualVolumeAABBMax.X = VolumeInfo.xLength;
		CreateInfo.VirtualVolumeAABBMax.Y = VolumeInfo.yLength;
		CreateInfo.VirtualVolumeAABBMax.Z = VolumeInfo.zLength;

		CreateInfo.AttributesFormats[0] = PF_R8G8B8A8;
		ETextureSourceFormat TextureFormat = TSF_BGRA8;
		VolumeConvertInfoToTextureFormats(TextureStorage, VolumeInfo.tupleSize, CreateInfo.AttributesFormats[0], TextureFormat);

		TArray<float> TileData;
		TileData.SetNumUninitialized(VolumeInfo.tileSize * VolumeInfo.tileSize * VolumeInfo.tileSize * VolumeInfo.tupleSize);

		HAPI_VolumeTileInfo TileInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetFirstVolumeTile(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &TileInfo));
		for (;;)
		{
			if (!TileInfo.isValid)
				break;

			FMemory::Memzero(TileData.GetData(), TileData.Num() * sizeof(float));

			// Get the color data.
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVolumeTileFloatData(FHoudiniEngine::Get().GetSession(),
				NodeId, PartId, 0.0f, &TileInfo, TileData.GetData(), TileData.Num()));

			Tiles.Emplace(FIntVector(TileInfo.minX - VolumeInfo.minX, TileInfo.minY - VolumeInfo.minY, TileInfo.minZ - VolumeInfo.minZ), TileData);  // Min should always >= 0, otherwise data won't import, see UStreamableSparseVolumeTexture::AppendFrame

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetNextVolumeTile(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &TileInfo));
		}

		return true;
	}

	UE::SVT::FTextureDataCreateInfo CreateInfo;

	int32 TupleSize = 0;
	int32 TileSize = 0;
	TArray<TPair<FIntVector, TArray<float>>> Tiles;

	virtual UE::SVT::FTextureDataCreateInfo GetCreateInfo() const override
	{
		return CreateInfo;
	}

	virtual void IteratePhysicalSource(TFunctionRef<void(const FIntVector3& Coord, int32 AttributesIdx, int32 ComponentIdx, float VoxelValue)> OnVisit) const override
	{
		for (const TPair<FIntVector, TArray<float>>& Tile : Tiles)
		{
			const FIntVector TileMax(FMath::Min(Tile.Key.X + TileSize, CreateInfo.VirtualVolumeAABBMax.X),
				FMath::Min(Tile.Key.Y + TileSize, CreateInfo.VirtualVolumeAABBMax.Y),
				FMath::Min(Tile.Key.Z + TileSize, CreateInfo.VirtualVolumeAABBMax.Z));
			for (int32 X = Tile.Key.X; X < TileMax.X; ++X)
			{
				for (int32 Y = Tile.Key.Y; Y < TileMax.Y; ++Y)
				{
					for (int32 Z = Tile.Key.Z; Z < TileMax.Z; ++Z)
					{
						const int32 TileVoxelIdx = ((X - Tile.Key.X) + (Y - Tile.Key.Y) * TileSize + (Z - Tile.Key.Z) * TileSize * TileSize) * TupleSize;
						bool bHasValue = false;
						for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
						{
							if (Tile.Value[TileVoxelIdx + TupleIdx] != 0.0f)
							{
								bHasValue = true;
								break;
							}
						}

						if (bHasValue)
						{
							for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
								OnVisit(FIntVector3(X, Y, Z), 0, TupleIdx, Tile.Value[TileVoxelIdx + TupleIdx]);
						}
					}
				}
			}
		}
	}
};

bool FHoudiniTextureOutputBuilder::HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputTexture);

	const int32& NodeId = GeoInfo.nodeId;

	TMap<FString, TArray<TPair<HAPI_PartInfo, HAPI_VolumeInfo>>> NameVDBsMap;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
	{
		const int32& PartId = PartInfo.id;

		HAPI_VolumeInfo VolumeInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVolumeInfo(FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, &VolumeInfo));

		FString Name;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(VolumeInfo.nameSH, Name));

		if (VolumeInfo.type == HAPI_VOLUMETYPE_HOUDINI)
		{
			TArray<std::string> AttribNames;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(
				NodeId, PartId, PartInfo.attributeCounts, AttribNames));

			FString ObjectPath;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
				AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OBJECT_PATH, ObjectPath));

			const int32& HoudiniXSize = VolumeInfo.xLength;
			const int32& HoudiniYSize = VolumeInfo.yLength;

#if true
			// Till Houdini 21.0, we need another attribute "__volume_storage_type__" to identify
			EHoudiniVolumeStorageType VolumeStorage = EHoudiniVolumeStorageType::Float;
			HOUDINI_FAIL_RETURN(HapiGetVolumeStorageType(NodeId, PartId, VolumeStorage));
			VolumeInfo.tupleSize = FMath::Max(int32(VolumeStorage), 1);
#else
			const EHoudiniVolumeStorageType VolumeStorage = (VolumeInfo.storage == HAPI_STORAGETYPE_INT) ?
				EHoudiniVolumeStorageType::Int : EHoudiniVolumeStorageType(VolumeInfo.tupleSize);
#endif

			UTexture2D* T = FHoudiniEngineUtils::FindOrCreateAsset<UTexture2D>(
				IS_ASSET_PATH_INVALID(ObjectPath) ? Node->GetCookFolderPath() + Name : ObjectPath);
			
			EHoudiniVolumeConvertDataType TextureStorage = T->Source.GetSizeX() <= 0 ? EHoudiniVolumeConvertDataType::Uint8 :  // If the texture is new, then we just use the default uint8 color
				FHoudiniEngineUtils::ConvertTextureSourceFormat(T->Source.GetFormat());  // Use the previous format
			
			HOUDINI_FAIL_RETURN(HapiGetTextureStorage(NodeId, PartId, AttribNames, PartInfo.attributeCounts, TextureStorage));
			

			EPixelFormat PixelFormat = PF_R8G8B8A8;
			ETextureSourceFormat TextureFormat = TSF_BGRA8;
			VolumeConvertInfoToTextureFormats(TextureStorage, VolumeInfo.tupleSize, PixelFormat, TextureFormat);


			const float* VolumeData = nullptr;
			size_t SHMHandle = 0;
			HOUDINI_FAIL_RETURN(HapiGetVolumeData(VolumeData, SHMHandle, NodeId, PartId, VolumeInfo));

			T->Source.Init(VolumeInfo.xLength, VolumeInfo.yLength, 1, 1, TextureFormat);
			uint8* MipData = T->Source.LockMip(0);

			// Convert houdini volume data to texture mipdata
			switch (VolumeStorage)
			{
			case EHoudiniVolumeStorageType::Int:
			{
				uint8* DstData = MipData;
				const int64* SrcData = (const int64*)VolumeData;
				ParallelFor(HoudiniYSize, [&](int32 nY)
					{
						const int32 SrcYOffset = (HoudiniYSize - 1 - nY) * HoudiniXSize;
						uint8* DstPtr = DstData + nY * HoudiniXSize;
						for (int32 nX = 0; nX < HoudiniXSize; ++nX)
						{
							const int64* SrcDataPtr = SrcData + (nX + SrcYOffset);
							*DstPtr = uint8(*SrcDataPtr);
							++DstPtr;
						}
					});
			}
			break;
			case EHoudiniVolumeStorageType::Float:
			{
				uint8* DstData = MipData;
				const float* SrcData = VolumeData;
				ParallelFor(HoudiniYSize, [&](int32 nY)
					{
						const int32 SrcYOffset = (HoudiniYSize - 1 - nY) * HoudiniXSize;
						uint8* DstPtr = DstData + nY * HoudiniXSize;
						for (int32 nX = 0; nX < HoudiniXSize; ++nX)
						{
							const float* SrcDataPtr = SrcData + (nX + SrcYOffset);
							*DstPtr = uint8(FMath::Clamp(FMath::RoundToInt(float(*SrcDataPtr) * 255.0), 0, 255));
							++DstPtr;
						}
					});
			}
			break;
			case EHoudiniVolumeStorageType::Vector2:
			{
				FColor* DstData = (FColor*)MipData;
				const float* SrcData = VolumeData;
				ParallelFor(HoudiniYSize, [&](int32 nY)
					{
						const int32 SrcYOffset = (HoudiniYSize - 1 - nY) * HoudiniXSize;
						FColor* DstPtr = DstData + nY * HoudiniXSize;
						for (int32 nX = 0; nX < HoudiniXSize; ++nX)
						{
							const float* SrcDataPtr = SrcData + (nX + SrcYOffset) * 2;
							DstPtr->R = uint8(FMath::Clamp(FMath::RoundToInt(float(*SrcDataPtr) * 255.0), 0, 255));
							DstPtr->G = uint8(FMath::Clamp(FMath::RoundToInt(float(*(SrcDataPtr + 1)) * 255.0), 0, 255));
							DstPtr->B = 0;
							DstPtr->A = 255;
							++DstPtr;
						}
					});
			}
			break;
			case EHoudiniVolumeStorageType::Vector3:
			{
				FColor* DstData = (FColor*)MipData;
				const float* SrcData = VolumeData;
				ParallelFor(HoudiniYSize, [&](int32 nY)
					{
						const int32 SrcYOffset = (HoudiniYSize - 1 - nY) * HoudiniXSize;
						FColor* DstPtr = DstData + nY * HoudiniXSize;
						for (int32 nX = 0; nX < HoudiniXSize; ++nX)
						{
							const float* SrcDataPtr = SrcData + (nX + SrcYOffset) * 3;
							DstPtr->R = uint8(FMath::Clamp(FMath::RoundToInt(float(*SrcDataPtr) * 255.0), 0, 255));
							DstPtr->G = uint8(FMath::Clamp(FMath::RoundToInt(float(*(SrcDataPtr + 1)) * 255.0), 0, 255));
							DstPtr->B = uint8(FMath::Clamp(FMath::RoundToInt(float(*(SrcDataPtr + 2)) * 255.0), 0, 255));
							DstPtr->A = 255;
							++DstPtr;
						}
					});
			}
			break;
			case EHoudiniVolumeStorageType::Vector4:
			{
				FColor* DstData = (FColor*)MipData;
				const float* SrcData = VolumeData;
				ParallelFor(HoudiniYSize, [&](int32 nY)
					{
						const int32 SrcYOffset = (HoudiniYSize - 1 - nY) * HoudiniXSize;
						FColor* DstPtr = DstData + nY * HoudiniXSize;
						for (int32 nX = 0; nX < HoudiniXSize; ++nX)
						{
							const float* SrcDataPtr = SrcData + (nX + SrcYOffset) * 4;
							DstPtr->R = uint8(FMath::Clamp(FMath::RoundToInt(float(*SrcDataPtr) * 255.0), 0, 255));
							DstPtr->G = uint8(FMath::Clamp(FMath::RoundToInt(float(*(SrcDataPtr + 1)) * 255.0), 0, 255));
							DstPtr->B = uint8(FMath::Clamp(FMath::RoundToInt(float(*(SrcDataPtr + 2)) * 255.0), 0, 255));
							DstPtr->A = uint8(FMath::Clamp(FMath::RoundToInt(float(*(SrcDataPtr + 3)) * 255.0), 0, 255));
							++DstPtr;
						}
					});
			}
			break;
			}

			FHoudiniOutputUtils::CloseData(VolumeData, SHMHandle);

			T->Source.UnlockMip(0);


			// Texture creation parameters.
			/*
			if (!T->GetPlatformData())
				T->SetPlatformData(new FTexturePlatformData());
			T->GetPlatformData()->SizeX = HoudiniYSize;
			T->GetPlatformData()->SizeY = HoudiniXSize;
			T->GetPlatformData()->PixelFormat = PixelFormat;
			*/
			T->CompressionSettings = TC_Default;
			T->CompressionNoAlpha = VolumeInfo.tupleSize < 4;
			T->MipGenSettings = TMGS_NoMipmaps;

			T->PostEditChange();

			// Retrieve UProperties
			TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
			HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
				HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

			for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
				PropAttrib->SetObjectPropertyValues(T, 0);

			T->Modify();
		}
		else if (VolumeInfo.type == HAPI_VOLUMETYPE_VDB)
		{
			if (VolumeInfo.storage != HAPI_STORAGETYPE_FLOAT || VolumeInfo.tupleSize > 4)
				continue;

			NameVDBsMap.FindOrAdd(Name).Emplace(PartInfo, VolumeInfo);
		}
	}

	if (NameVDBsMap.IsEmpty())
		return true;

	//FHoudiniEngine::Get().FinishHoudiniMainTaskMessage();  // TODO: Check whether this is needed - Avoid D3D12 invalid scissor crash

	const bool bBackupIsSilent = GIsSilent;
	GIsSilent = true;  // Disable FSlowTask::MakeDialog, (process bar) when create SVT

	for (const auto& NameVDBs : NameVDBsMap)
	{
		const HAPI_PartInfo& MainPartInfo = NameVDBs.Value[0].Key;
		const int32& MainPartId = MainPartInfo.id;

		TArray<std::string> AttribNames;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, MainPartId,
			MainPartInfo.attributeCounts, AttribNames));

		FString ObjectPath;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, MainPartId,
			AttribNames, MainPartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OBJECT_PATH, ObjectPath));

		EHoudiniVolumeConvertDataType TextureStorage = EHoudiniVolumeConvertDataType::Uint8;
		HOUDINI_FAIL_RETURN(HapiGetTextureStorage(NodeId, MainPartId,
			AttribNames, MainPartInfo.attributeCounts, TextureStorage));

		// Retrieve UProperties
		TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, MainPartId,
			AttribNames, MainPartInfo.attributeCounts, HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

		if (NameVDBs.Value.Num() == 1)  // Static
		{
			const HAPI_VolumeInfo& VolumeInfo = NameVDBs.Value[0].Value;

			UE::SVT::FTextureData TextureData{};

			FHoudiniSparseVolumeDataProvider HSVDP;
			HOUDINI_FAIL_RETURN(HSVDP.HapiInitialize(NodeId, MainPartId, VolumeInfo, TextureStorage));
			TextureData.Create(HSVDP);

			UStaticSparseVolumeTexture* SVT = FHoudiniEngineUtils::CreateAsset<UStaticSparseVolumeTexture>(
				IS_ASSET_PATH_INVALID(ObjectPath) ? Node->GetCookFolderPath() + NameVDBs.Key : ObjectPath);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
			SVT->Initialize(MakeArrayView(&TextureData, 1), TArray<FTransform>{FTransform::Identity});
#else
			SVT->Initialize(MakeArrayView(&TextureData, 1));
#endif
			for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
				PropAttrib->SetObjectPropertyValues(SVT, 0);

			SVT->PostEditChange();
			SVT->Modify();
		}
		else  // Animated
		{
			TArray<UE::SVT::FTextureData> TextureDatas;
			TArray<FTransform> Transforms;
			for (const TPair<HAPI_PartInfo, HAPI_VolumeInfo>& VDB : NameVDBs.Value)
			{
				const HAPI_VolumeInfo& VolumeInfo = VDB.Value;

				UE::SVT::FTextureData TextureData{};

				FHoudiniSparseVolumeDataProvider HSVDP;
				HOUDINI_FAIL_RETURN(HSVDP.HapiInitialize(NodeId, VDB.Key.id, VolumeInfo, TextureStorage));
				TextureData.Create(HSVDP);
				TextureDatas.Add(TextureData);

				FTransform VDBOffset = FTransform::Identity;
				VDBOffset.SetLocation(FVector(VolumeInfo.minX, VolumeInfo.minY, VolumeInfo.minZ));
				Transforms.Add(VDBOffset);
			}

			UAnimatedSparseVolumeTexture* SVT = FHoudiniEngineUtils::CreateAsset<UAnimatedSparseVolumeTexture>(
				IS_ASSET_PATH_INVALID(ObjectPath) ? Node->GetCookFolderPath() + NameVDBs.Key : ObjectPath);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
			SVT->Initialize(TextureDatas, Transforms);
#else
			SVT->Initialize(TextureDatas);
#endif
			for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
				PropAttrib->SetObjectPropertyValues(SVT, 0);

			SVT->PostEditChange();
			SVT->Modify();
		}
	}

	GIsSilent = bBackupIsSilent;

	return true;
}
