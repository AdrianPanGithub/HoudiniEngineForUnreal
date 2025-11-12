// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputs.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"


//#include "F:\UnrealEngine\Engine\Plugins\Experimental\Mutable\Source\CustomizableObject\Private\MuCO\UnrealBakeHelpers.cpp"
//#include "F:\UnrealEngine\Engine\Plugins\Runtime\MeshModelingToolset\Source\ModelingComponents\Private\AssetUtils\Texture2DBuilder.cpp"
//#include "F:\UnrealEngine\Engine\Plugins\Experimental\GeometryScripting\Source\GeometryScriptingEditor\Private\EditorTextureMapFunctions.cpp"

UHoudiniInputHolder* FHoudiniTextureInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<UTexture2D, UHoudiniInputTexture>(Input, Asset, OldHolder);
}

void UHoudiniInputTexture::SetAsset(UTexture2D* NewTexture)
{
	if (Texture != NewTexture)
	{
		Texture = NewTexture;
		RequestReimport();
	}
}

static EHoudiniVolumeStorageType GetStorageTypeByTextureChannelFlags(const ETextureSourceFormat& TextureFormat, const EPixelFormatChannelFlags& ChannelFlags)
{
	if ((TextureFormat == ETextureSourceFormat::TSF_G8) || (TextureFormat == ETextureSourceFormat::TSF_G16) ||
		(TextureFormat == ETextureSourceFormat::TSF_R16F) || (TextureFormat == ETextureSourceFormat::TSF_R32F))
	{
		return EHoudiniVolumeStorageType::Float;  // These texture formats means only has one channel
	}

	switch (ChannelFlags)
	{
	case EPixelFormatChannelFlags::R:
	case EPixelFormatChannelFlags::G:
	case EPixelFormatChannelFlags::B: return EHoudiniVolumeStorageType::Float;
	case EPixelFormatChannelFlags::RG: return EHoudiniVolumeStorageType::Vector2;
	case EPixelFormatChannelFlags::RGB: return EHoudiniVolumeStorageType::Vector3;
	case EPixelFormatChannelFlags::RGBA: return EHoudiniVolumeStorageType::Vector4;
	}

	return EHoudiniVolumeStorageType::Float;
}

bool UHoudiniInputTexture::IsObjectExists() const
{
	return IsValid(Texture.LoadSynchronous());
}

#define COPY_TEXTURE_DATA_TO_VOLUME(DST_DATA_TYPE, SRC_DATA_TYPE, NUM_CHANNALS, DATA_COPY_FUNC)  {\
		const SRC_DATA_TYPE* TextureData = (const SRC_DATA_TYPE*)MipData;\
		DST_DATA_TYPE* VolumeData = (DST_DATA_TYPE*)SHM;\
		ParallelFor(TextureSizeY, [&](int32 nY)\
		{\
			const int32 DstYOffset = (TextureSizeY - 1 - nY) * TextureSizeX;\
			const SRC_DATA_TYPE* SrcPtr = TextureData + nY * TextureSizeX;\
			for (int32 nX = 0; nX < TextureSizeX; ++nX)\
			{\
				DST_DATA_TYPE* DstPtr = VolumeData + (nX + DstYOffset) * NUM_CHANNALS;\
				DATA_COPY_FUNC\
				++SrcPtr;\
			}\
		});\
	}

bool UHoudiniInputTexture::HapiUpload()
{
	UTexture2D* T = Texture.LoadSynchronous();
	if (!IsValid(T))
		return HapiDestroy();

	if (!T->Source.IsValid())
	{
		if (Handle)  // Previous Node is shm input, we should create a null reference node, so destroy the previous node 
			HOUDINI_FAIL_RETURN(HapiDestroy());

		const bool bCreateNewNode = (NodeId < 0);
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("reference_%s_%08X"), *T->GetName(), FPlatformTime::Cycles()), NodeId))

		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(NodeId, FVector3f::ZeroVector));
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(NodeId, HAPI_ATTRIB_UNREAL_OBJECT_PATH, FHoudiniEngineUtils::GetAssetReference(T)));
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), NodeId));

		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(NodeId));

		bHasChanged = false;

		return true;
	}

	if (NodeId >= 0 && !Handle)  // Means previous is a "null" reference node
		HOUDINI_FAIL_RETURN(HapiDestroy());


	// Get data type and storage type
	const ETextureSourceFormat TextureFormat = T->Source.GetFormat();
	const EHoudiniVolumeConvertDataType VolumeDataType = FHoudiniEngineUtils::ConvertTextureSourceFormat(TextureFormat);
	const EHoudiniVolumeStorageType VolumeStorage = GetStorageTypeByTextureChannelFlags(TextureFormat, GetPixelFormatValidChannels(T->GetPixelFormat()));
	
	const int32 TextureSizeX = T->Source.GetSizeX();
	const int32 TextureSizeY = T->Source.GetSizeY();


	FHoudiniSharedMemoryVolumeInput SHMVolumeInput(VolumeDataType, VolumeStorage, FIntVector3(TextureSizeX, TextureSizeY, 1));
	bool bSHMExists = false;
	float* const SHM = SHMVolumeInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)T), Handle, bSHMExists);


	const uint8* MipData = Texture->Source.LockMipReadOnly(0);
	switch (TextureFormat)
	{
	case TSF_G8:
		COPY_TEXTURE_DATA_TO_VOLUME(uint8, uint8, 1, *DstPtr = *SrcPtr;);
		break;
	case TSF_BGRA8:
	case TSF_BGRE8:
	{
		switch (VolumeStorage)
		{
		case EHoudiniVolumeStorageType::Float:
			COPY_TEXTURE_DATA_TO_VOLUME(uint8, FColor, 1, *DstPtr = SrcPtr->R;);
			break;
		case EHoudiniVolumeStorageType::Vector2:
			COPY_TEXTURE_DATA_TO_VOLUME(uint8, FColor, 2, *DstPtr = SrcPtr->R; *(DstPtr + 1) = SrcPtr->G;);
			break;
		case EHoudiniVolumeStorageType::Vector3:
			COPY_TEXTURE_DATA_TO_VOLUME(uint8, FColor, 3, *DstPtr = SrcPtr->R; *(DstPtr + 1) = SrcPtr->G; *(DstPtr + 2) = SrcPtr->B;);
			break;
		case EHoudiniVolumeStorageType::Vector4:
			COPY_TEXTURE_DATA_TO_VOLUME(uint8, FColor, 4, *DstPtr = SrcPtr->R; *(DstPtr + 1) = SrcPtr->G; *(DstPtr + 2) = SrcPtr->B; *(DstPtr + 3) = SrcPtr->A;);
			break;
		}
	}
	break;
	case TSF_RGBA16:
	{
		switch (VolumeStorage)
		{
		case EHoudiniVolumeStorageType::Float:
			COPY_TEXTURE_DATA_TO_VOLUME(uint16, FInt32Vector2, 1, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(uint16)););
			break;
		case EHoudiniVolumeStorageType::Vector2:
			COPY_TEXTURE_DATA_TO_VOLUME(uint16, FInt32Vector2, 2, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(uint16) * 2););
			break;
		case EHoudiniVolumeStorageType::Vector3:
			COPY_TEXTURE_DATA_TO_VOLUME(uint16, FInt32Vector2, 3, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(uint16) * 3););
			break;
		case EHoudiniVolumeStorageType::Vector4:
			COPY_TEXTURE_DATA_TO_VOLUME(uint16, FInt32Vector2, 4, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(uint16) * 4););
			break;
		}
	}
	break;
	case TSF_RGBA16F:
	{
		switch (VolumeStorage)
		{
		case EHoudiniVolumeStorageType::Float:
			COPY_TEXTURE_DATA_TO_VOLUME(FFloat16, FFloat16Color, 1, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(FFloat16)););
			break;
		case EHoudiniVolumeStorageType::Vector2:
			COPY_TEXTURE_DATA_TO_VOLUME(FFloat16, FFloat16Color, 2, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(FFloat16) * 2););
			break;
		case EHoudiniVolumeStorageType::Vector3:
			COPY_TEXTURE_DATA_TO_VOLUME(FFloat16, FFloat16Color, 3, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(FFloat16) * 3););
			break;
		case EHoudiniVolumeStorageType::Vector4:
			COPY_TEXTURE_DATA_TO_VOLUME(FFloat16, FFloat16Color, 4, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(FFloat16Color)););
			break;
		}
	}
	break;
	case TSF_G16:
		COPY_TEXTURE_DATA_TO_VOLUME(uint16, uint16, 1, *DstPtr = *SrcPtr;);
		break;
	case TSF_RGBA32F:
	{
		switch (VolumeStorage)
		{
		case EHoudiniVolumeStorageType::Float:
			COPY_TEXTURE_DATA_TO_VOLUME(float, FLinearColor, 1, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(float)););
			break;
		case EHoudiniVolumeStorageType::Vector2:
			COPY_TEXTURE_DATA_TO_VOLUME(float, FLinearColor, 2, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(float) * 2););
			break;
		case EHoudiniVolumeStorageType::Vector3:
			COPY_TEXTURE_DATA_TO_VOLUME(float, FLinearColor, 3, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(float) * 3););
			break;
		case EHoudiniVolumeStorageType::Vector4:
			COPY_TEXTURE_DATA_TO_VOLUME(float, FLinearColor, 4, FMemory::Memcpy(DstPtr, SrcPtr, sizeof(FLinearColor)););
			break;
		}
	}
	break;
	case TSF_R16F:
		COPY_TEXTURE_DATA_TO_VOLUME(FFloat16, FFloat16, 1, *DstPtr = *SrcPtr;);
		break;
	case TSF_R32F:
		COPY_TEXTURE_DATA_TO_VOLUME(float, float, 1, *DstPtr = *SrcPtr;);
		break;
	}
	T->Source.UnlockMip(0);


	SHMVolumeInput.AppendAttribute(HAPI_ATTRIB_UNREAL_OBJECT_PATH,
		EHoudiniAttributeOwner::Prim, false, FHoudiniEngineUtils::GetAssetReference(T));
	
	const bool bCreateNewNode = (NodeId < 0);
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryVolumeInput::HapiCreateNode(GetGeoNodeId(),
			FString::Printf(TEXT("%s_%08X"), *T->GetName(), FPlatformTime::Cycles()), NodeId));

	HOUDINI_FAIL_RETURN(SHMVolumeInput.HapiUpload(NodeId, SHM, FVector2f(0.0f, 1.0f), T->GetName(),
		false, FIntVector3::ZeroValue, FIntVector3::ZeroValue, true));
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(NodeId));

	bHasChanged = false;
	return true;
}

bool UHoudiniInputTexture::HapiDestroy()
{
	if (NodeId >= 0)
	{
		GetInput()->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeId));
	}

	Invalidate();

	return true;
}

void UHoudiniInputTexture::Invalidate()
{
	NodeId = -1;
	bHasChanged = false;
	if (Handle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(Handle);
		Handle = 0;
	}
}
