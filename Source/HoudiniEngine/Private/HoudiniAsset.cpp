// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAsset.h"

#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
#include "UObject/AssetRegistryTagsContext.h"
#endif

#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineCommon.h"


void UHoudiniAsset::SetFilePath(const FString& InFilePath)
{
	if (FilePath.FilePath != InFilePath)
	{
		FilePath.FilePath = InFilePath;
		MarkAsUnloaded();
	}
}

bool UHoudiniAsset::LoadBuffer()
{
	LibraryBuffer.Empty();

	if (!FFileHelper::LoadFileToArray(LibraryBuffer, *FilePath.FilePath))
		return false;

	LibraryBuffer.Add(0);

	Modify();

	return true;
}

bool UHoudiniAsset::HapiLoad(TArray<FString>& OutAvailableAssetNames)
{
	if (!NeedLoad())  // Check if this asset has already been loaded, and need not load again
	{
		OutAvailableAssetNames = AvailableAssetNames;
		return true;
	}


	HAPI_AssetLibraryId LibraryId = -1;

	// First, load .hda file.
	if (FPaths::FileExists(FilePath.FilePath))
	{
		HAPI_Result Result = FHoudiniApi::LoadAssetLibraryFromFile(FHoudiniEngine::Get().GetSession(),
			TCHAR_TO_UTF8(*FilePath.FilePath), true, &LibraryId);
		if (HAPI_SESSION_INVALID_RESULT(Result))
			return false;

		if (HAPI_RESULT_SUCCESS == Result)
			LastEditTime = IFileManager::Get().GetTimeStamp(*FilePath.FilePath);  // Update file edit time
		else
			LibraryId = -1;

		if (LibraryId >= 0)
			UE_LOG(LogHoudiniEngine, Log, TEXT("HoudiniAsset: %s, %s loaded"), *GetName(), *FilePath.FilePath);
	}
	
	// If file load failed, then try load from buffer
	if (LibraryId < 0 && !LibraryBuffer.IsEmpty())
	{
		HAPI_Result Result = FHoudiniApi::LoadAssetLibraryFromMemory(FHoudiniEngine::Get().GetSession(),
			(char*)LibraryBuffer.GetData(), LibraryBuffer.Num() - 1, true, &LibraryId);
		
		if (HAPI_RESULT_SUCCESS != Result)
		{
			FHoudiniEngineUtils::PrintFailedResult(UE_SOURCE_LOCATION, Result);
			LibraryId = -1;
		}

		if (HAPI_SESSION_INVALID_RESULT(Result))
			return false;

		if (LibraryId >= 0)
			UE_LOG(LogHoudiniEngine, Log, TEXT("HoudiniAsset: %s, buffer loaded"), *GetName());
	}
	
	if (LibraryId >= 0)
	{
		int32 AssetCount;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAvailableAssetCount(FHoudiniEngine::Get().GetSession(), LibraryId, &AssetCount));
		if (AssetCount >= 1)
		{
			TArray<HAPI_StringHandle> AssetNameSHs;
			AssetNameSHs.SetNumUninitialized(AssetCount);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAvailableAssets(FHoudiniEngine::Get().GetSession(), LibraryId, AssetNameSHs.GetData(), AssetCount));

			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(AssetNameSHs, AvailableAssetNames));
		}
		else
		{
			AvailableAssetNames.Empty();
			UE_LOG(LogHoudiniEngine, Error, TEXT("HoudiniAsset: %s, No operator found"), *GetName());
		}

		OutAvailableAssetNames = AvailableAssetNames;
		InstantiatedNodes.Empty();

		FHoudiniEngine::Get().RegisterLoadedAsset(this);

		return true;
	}

	return false;
}

bool UHoudiniAsset::NeedLoad()
{
	if (!FHoudiniEngine::Get().IsAssetLoaded(this))
		return true;

	if (!FPaths::FileExists(FilePath.FilePath))
		return false;

	if (!FHoudiniEngine::Get().IsSessionSync())  // If Session Sync, then we need NOT check whether the HDA has been modified in houdini
	{
		if (IFileManager::Get().GetTimeStamp(*FilePath.FilePath) != LastEditTime)  // If hda has been changed, then we will automatically rebuild the node
			return true;
	}

	return false;
}

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
void UHoudiniAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Context.AddTag(FAssetRegistryTag("HDA Source", FilePath.FilePath, FAssetRegistryTag::TT_Alphabetical));
}
#else
void UHoudiniAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add(FAssetRegistryTag("HDA Source", FilePath.FilePath, FAssetRegistryTag::TT_Alphabetical));
}
#endif

#if WITH_EDITOR
void UHoudiniAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHoudiniAsset, FilePath))
	{
		LoadBuffer();
		MarkAsUnloaded();
	}
}
#endif
