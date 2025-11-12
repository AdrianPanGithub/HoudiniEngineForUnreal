// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "JsonObjectConverter.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAttribute.h"


bool FHoudiniAssetOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = false;
	bOutShouldHoldByOutput = false;
	if ((PartInfo.type == HAPI_PARTTYPE_MESH) && (PartInfo.faceCount == 0) && (PartInfo.pointCount >= 1))
	{
		// Check whether this is an asset output
		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id,
			HAPI_ATTRIB_UNREAL_INSTANCE, HAPI_ATTROWNER_POINT, &AttribInfo));

		if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_STRING))
		{
			bOutIsValid = false;
			return true;
		}

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id,
			HAPI_ATTRIB_PARTIAL_OUTPUT_MODE, HAPI_ATTROWNER_POINT, &AttribInfo));

		if (AttribInfo.exists)
		{
			bOutIsValid = false;
			return true;
		}

		// Check this is material instance asset output
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id,
			HAPI_ATTRIB_UNREAL_OBJECT_PATH, HAPI_ATTROWNER_POINT, &AttribInfo));

		if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_STRING))
		{
			bOutIsValid = true;
			return true;
		}
	}

	return true;
}

bool FHoudiniAssetOutputBuilder::HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputAsset);

	const int32& NodeId = GeoInfo.nodeId;

	HAPI_AttributeInfo AttribInfo;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
	{
		const int32& PartId = PartInfo.id;
		
		TArray<std::string> AttribNames;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, AttribNames));

		TArray<FString> ObjectPaths;
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, HAPI_ATTROWNER_POINT, &AttribInfo));

			TArray<HAPI_StringHandle> SHs;
			SHs.SetNumUninitialized(AttribInfo.count);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, &AttribInfo, SHs.GetData(), 0, AttribInfo.count));

			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, ObjectPaths));
		}

		// Retrieve UProperties
		TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

		TArray<FString> ObjectJsonStrs;
		HAPI_AttributeOwner ObjectJsonOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OBJECT_METADATA);
		if (ObjectJsonOwner != HAPI_ATTROWNER_INVALID)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_OBJECT_METADATA, HAPI_ATTROWNER_POINT, &AttribInfo));

			if (AttribInfo.storage == HAPI_STORAGETYPE_DICTIONARY)
			{
				TArray<HAPI_StringHandle> SHs;
				SHs.SetNumUninitialized(AttribInfo.count);
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeDictionaryData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_ATTRIB_UNREAL_OBJECT_METADATA, &AttribInfo, SHs.GetData(), 0, AttribInfo.count));

				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, ObjectJsonStrs));
			}
		}

		for (int32 PointIdx = 0; PointIdx < ObjectPaths.Num(); ++PointIdx)
		{
			FString& ObjectPath = ObjectPaths[PointIdx];
			if (IS_ASSET_PATH_INVALID(ObjectPath))
				continue;

			int32 SplitIdx;
			if (ObjectPath.FindChar(TCHAR(';'), SplitIdx))  // UHoudiniParameterAsset support import asset ref with info, and append by ';', so we need to support it here
				ObjectPath.LeftInline(SplitIdx);

			UClass* ObjectClass = nullptr;
			int32 ClassEndIdx = -1;
			if (ObjectPath.FindChar(TCHAR('\''), ClassEndIdx))
			{
				const FString ClassPath = ObjectPath.Left(ClassEndIdx);
				ObjectClass = LoadObject<UClass>(nullptr, *ClassPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
				if (!ObjectClass)
					ObjectClass = FindFirstObject<UClass>(*ClassPath, EFindFirstObjectOptions::NativeFirst);
			}

			UObject* Asset = IsValid(ObjectClass) ? FHoudiniEngineUtils::FindOrCreateAsset(ObjectClass, ObjectPath) :
				LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);

			if (!IsValid(Asset))
				continue;

			if (!ObjectJsonStrs.IsEmpty())
			{
				TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ObjectJsonStrs[POINT_ATTRIB_ENTRY_IDX(ObjectJsonOwner, PointIdx)]);
				TSharedPtr<FJsonObject> JsonStruct;
				if (FJsonSerializer::Deserialize(JsonReader, JsonStruct))
					FJsonObjectConverter::JsonObjectToUStruct(JsonStruct.ToSharedRef(), Asset->GetClass(), Asset);
			}

			SET_OBJECT_UPROPERTIES(Asset, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), PointIdx));
		}
	}

	return true;
}
