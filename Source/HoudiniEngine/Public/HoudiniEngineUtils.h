// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include <string>
#include "CoreMinimal.h"

#include "HAPI/HAPI_Common.h"


#define IS_ASSET_PATH_INVALID(ASSET_PATH) (!ASSET_PATH.Contains(TEXT("'/")) && !ASSET_PATH.StartsWith(TEXT("/")))
#define PRINT_HOUDINI_FLOAT(VALUE) *FString::SanitizeFloat(FMath::RoundToInt64((VALUE) * 100.0) * 0.0001, 0)


enum class EHoudiniVolumeConvertDataType : int;
enum class EHoudiniStorageType : int32;

struct HOUDINIENGINE_API FHoudiniEngineUtils
{
	// -------- Convert HAPI_StringHandle --------
	static bool HapiConvertStringHandle(const HAPI_StringHandle& InSH, FString& OutString);

	static bool HapiConvertStringHandle(const HAPI_StringHandle& InSH, std::string& OutString);

	static bool HapiConvertStringHandles(const TArray<HAPI_StringHandle>& InSHs, TArray<FString>& OutStrings);

	static bool HapiConvertStringHandles(const TArray<HAPI_StringHandle>& InSHs, TArray<std::string>& OutStrings);

	static bool HapiConvertUniqueStringHandles(const TArray<HAPI_StringHandle>& InUniqueSHs, TArray<FString>& OutStrings);  // InUniqueSHs Must be uniqued

	static bool HapiConvertUniqueStringHandles(const TArray<HAPI_StringHandle>& InUniqueSHs, TArray<std::string>& OutStrings);  // InUniqueSHs Must be uniqued

protected:
	static bool HapiGetStringHandles(const TArray<HAPI_StringHandle>& InSHs, TArray<char>& OutBuffer);

public:
	template<typename TDataType, typename TConvertFunc>
	static bool HapiConvertStringHandles(const TArray<HAPI_StringHandle>& InSHs, TConvertFunc Func, TMap<HAPI_StringHandle, TDataType>& OutSHMap)
	{
		if (InSHs.IsEmpty())
			return true;

		const TArray<HAPI_StringHandle> UniqueSHs = TSet<HAPI_StringHandle>(InSHs).Array();

		TArray<char> Buffer;
		if (!HapiGetStringHandles(UniqueSHs, Buffer))
			return false;

		if (Buffer.IsEmpty())
			return true;

		// Parse the buffer to a SH-Data Map
		int32 StringIdx = 0;
		int32 CharOffset = 0;
		while (CharOffset < Buffer.Num())
		{
			FUtf8StringView StrView = &Buffer[CharOffset];
			CharOffset += StrView.Len() + 1;

			OutSHMap.Add(UniqueSHs[StringIdx], Func(StrView));  // Convert the current string to our dictionary.
			++StringIdx;
		}

		return true;
	}

	// -------- Session Status --------
	static bool HapiGetStatusString(const HAPI_StatusType& StatusType, const HAPI_StatusVerbosity& StatusVerbosity, FString& OutStatusString);

	static void PrintFailedResult(const TCHAR* SourceLocationStr, const HAPI_Result& Result);


	// -------- String Relevant --------
	static FString GetAssetReference(const UObject* Asset);

	static FString GetAssetReference(const FSoftObjectPath& AssetPath);

	static FString ConvertXformToString(const FTransform& InTransform, const FVector& InPivot);

	static FString GetValidatedString(FString String);

	static void ParseFilterPattern(const FString& FilterPattern, TArray<FString>& OutFilters, TArray<FString>& OutInvertedFilters);


	// -------- Attribute And Group --------
	static bool HapiGetAttributeNames(const int32& NodeId, const int32& PartId, const int AttribCounts[HAPI_ATTROWNER_MAX],
		TArray<std::string>& OutAttribNames);

	static bool IsAttributeExists(const TArray<std::string>& AttribNames,  // AttribNames should get by FHoudiniApi::GetAttributeNames
		const int AttribCounts[HAPI_ATTROWNER_MAX], const std::string& AttribName, const HAPI_AttributeOwner& Owner = HAPI_ATTROWNER_INVALID);

	static HAPI_AttributeOwner QueryAttributeOwner(const TArray<std::string>& AttribNames,  // AttribNames should get by FHoudiniApi::GetAttributeNames
		const int AttribCounts[HAPI_ATTROWNER_MAX], const std::string& AttribName);

	FORCEINLINE static HAPI_AttributeOwner QueryAttributeOwner(const TArray<std::string>& AttribNames,  // AttribNames should get by FHoudiniApi::GetAttributeNames
		const int AttribCounts[HAPI_ATTROWNER_MAX], const std::string& AttribName, const HAPI_AttributeOwner& PreferOwner)
	{
		if ((PreferOwner != HAPI_ATTROWNER_INVALID) && IsAttributeExists(AttribNames, AttribCounts, AttribName, PreferOwner))
			return PreferOwner;
		return QueryAttributeOwner(AttribNames, AttribCounts, AttribName);
	}

	static EHoudiniStorageType ConvertStorageType(HAPI_StorageType Storage);

	FORCEINLINE static bool IsArray(const HAPI_StorageType& Storage) { return Storage >= HAPI_STORAGETYPE_INT_ARRAY; }

	// For int attribute that represent bool or enum
	static bool HapiGetEnumAttributeData(const int32& NodeId, const int32& PartId,
		const char* AttribName, TArray<int8>& OutData, HAPI_AttributeOwner& InOutOwner);  // InOutOwner should get by QueryAttributeOwner

	static bool HapiGetEnumAttributeData(const int32& NodeId, const int32& PartId,
		const char* AttribName, TFunctionRef<int8(FUtf8StringView)> EnumGetter, TArray<int8>& OutData, HAPI_AttributeOwner& InOutOwner);  // InOutOwner should get by QueryAttributeOwner

	static bool HapiGetFloatAttributeData(const int32& NodeId, const int32& PartId,
		const char* AttribName, const int32& DesiredTupleSize, TArray<float>& OutData, HAPI_AttributeOwner& InOutOwner);  // InOutOwner should get by QueryAttributeOwner

	static bool HapiGetStringAttributeValue(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
		const char* AttribName, FString& OutValue, bool* bOutHasAttrib = nullptr);

	static bool HapiGetPrimitiveGroupNames(const HAPI_GeoInfo& GeoInfo, const HAPI_PartInfo& PartInfo, TArray<std::string>& OutPrimGroupNames);

	static bool HapiGetGroupMembership(const int32& NodeId, const int32& PartId,
		const int32 NumElems, const bool bOnPackedPart, const HAPI_GroupType& GroupType, const char* GroupName,
		TArray<int32>& GroupMembership, bool& bMembershipConst);


	// -------- Shared Memory --------
	static const float* GetSharedMemory(const TCHAR* SHMPath, const size_t& Size32, size_t& OutHandle);

	static float* FindOrCreateSharedMemory(const TCHAR* SHMPath, const size_t& Size32, size_t& InOutHandle, bool& bOutFound);

	static void UnmapSharedMemory(const void* SHM);

	static void CloseSharedMemoryHandle(const size_t& Handle);

	
	// -------- Asset ---------
	static UMaterial* GetVertexColorMaterial(const bool& bGetTranslucent = false);

	static FString GetPackagePath(const FString& AssetPath);  // Return the package path like "/Game/HoudiniEngine/SM_Test"

	static UObject* FindOrCreateAsset(const UClass* AssetClass, const FString& AssetPath, bool* bOutFound = nullptr);

	static UObject* CreateAsset(const UClass* AssetClass, const FString& AssetPath);

	template<typename TAssetClass>
	FORCEINLINE static TAssetClass* FindOrCreateAsset(const FString& AssetPath, bool* bOutFound = nullptr)  // Will try to reuse exist asset if found
	{
		return Cast<TAssetClass>(FindOrCreateAsset(TAssetClass::StaticClass(), AssetPath, bOutFound));
	}

	template<typename TAssetClass>
	FORCEINLINE static TAssetClass* CreateAsset(const FString& AssetPath)  // Force new an asset, Ignore exist asset
	{
		return Cast<TAssetClass>(CreateAsset(TAssetClass::StaticClass(), AssetPath));
	}

	static USceneComponent* CreateComponent(AActor* Owner, const TSubclassOf<USceneComponent>& ComponentClass);

	static void DestroyComponent(USceneComponent* Component);

	static void NotifyAssetChanged(const UObject* Asset);

	static bool FilterClass(const TArray<const UClass*>& AllowClasses, const TArray<const UClass*>& DisallowClasses, const UClass* ObjectClass);


	// -------- Misc ---------
	static int32 BinarySearch(const TArray<int32>& SortedArray, const int32& Elem);  // Return the negative index pos if NOT found, and index if found

	static void ConvertLandscapeTransform(float OutHapiTransform[9],
		const FTransform& LandscapeTransform, const FIntRect& LandscapeExtent);

	static HAPI_TransformEuler ConvertTransform(const FTransform& Transform);

	static EHoudiniVolumeConvertDataType ConvertTextureSourceFormat(const ETextureSourceFormat& TextureFormat);

	static size_t GetElemSize(const EHoudiniVolumeConvertDataType& DataType);

	static ULevel* GetCurrentLevel();
};
