// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOperatorUtils.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"


#define HAPI_PRESET_PREFIX                                   "#PSI_PRESET\nversion 2.0a\nopvalues\n{\nversion 0.8\n"
#define HAPI_PRESET_ROW_MIDFIX                               "\t[ 0\tlocks=0 ]\t(\t"
#define HAPI_PRESET_ROW_DELIM                                "\t)\n"
#define HAPI_PRESET_SUFFIX                                   "}\n"

void FHoudiniParameterPresetHelper::Append(const std::string& ParmName, const std::string& ParmValueStr)
{
	if (PresetStr.empty())
		PresetStr = HAPI_PRESET_PREFIX;

	PresetStr += ParmName + HAPI_PRESET_ROW_MIDFIX + ParmValueStr + HAPI_PRESET_ROW_DELIM;
}

bool FHoudiniParameterPresetHelper::HapiSet(const int32& NodeId)
{
	if (!PresetStr.empty())
	{
		PresetStr += HAPI_PRESET_SUFFIX;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetPreset(FHoudiniEngine::Get().GetSession(), NodeId,
			HAPI_PRESETTYPE_BINARY, nullptr, PresetStr.c_str(), PresetStr.length() - 1));
	}
	return true;
}


#define HAPI_CREATE_SOP_NODE(TYPE_NAME) if (ParentNodeId >= 0)\
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),\
		ParentNodeId, #TYPE_NAME, NodeLabel.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*NodeLabel), false, &OutNodeId))\
else\
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),\
		-1, "SOP/" #TYPE_NAME, NodeLabel.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*NodeLabel), false, &OutNodeId))\
return true;


// -------- sharedmemory_geometryinput --------
bool FHoudiniSharedMemoryGeometryInput::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(sharedmemory_geometryinput);
}

FHoudiniSharedMemoryGeometryInput::FHoudiniSharedMemoryGeometryInput(const int32& InNumPoints, const int32& InNumPrims, const int32& InNumVertices)
{
	NumPoints = InNumPoints;
	NumPrims = InNumPrims;
	
	Size32 = NumPoints * 3 + InNumVertices + InNumPrims;  // Each PrimVertexArray will end with a minus number, which abs represent primtype(open/closed)
}

void FHoudiniSharedMemoryGeometryInput::AppendGroup(const char* GroupName,
	const EHoudiniAttributeOwner& Class, const size_t& InSize32, const bool& bUniqueValue)
{
	Size32 += InSize32;

	FHoudiniSHMAttribInfo InputAttribInfo;
	InputAttribInfo.Name = GroupName;
	InputAttribInfo.Owner = Class;
	InputAttribInfo.Type = EHoudiniInputAttributeStorage::Group;
	InputAttribInfo.Compression = bUniqueValue ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None;
	InputAttribInfos.Add(InputAttribInfo);
}

void FHoudiniSharedMemoryGeometryInput::AppendAttribute(const char* AttribName,
	const EHoudiniAttributeOwner& Class, const EHoudiniInputAttributeStorage& Storage, const int32& TupleSize, const size_t& InSize32,
	const EHoudiniInputAttributeCompression& Compression)
{
	Size32 += InSize32;

	FHoudiniSHMAttribInfo InputAttribInfo;
	InputAttribInfo.Name = AttribName;
	InputAttribInfo.Owner = Class;
	InputAttribInfo.Type = Storage;
	InputAttribInfo.TupleSize = TupleSize;
	InputAttribInfo.Compression = Compression;
	InputAttribInfos.Add(InputAttribInfo);
}

float* FHoudiniSharedMemoryGeometryInput::GetSharedMemory(const FString& SHMIdentifier, size_t& InOutHandle)
{
	// Force 4kb align
	if (Size32 % 1024)  // 4kb = 1024 * size(float) bytes
		Size32 = (Size32 / 1024 + 1) * 1024;
#if PLATFORM_WINDOWS
	SHMPath = FHoudiniEngine::GetProcessIdentifier() + SHMIdentifier + TEXT("_") + FString::FromInt(Size32);
#else
	SHMPath = FHoudiniEngine::GetProcessIdentifier() + ((SHMIdentifier.Len() <= 16) ? (SHMIdentifier + TEXT("_") + FString::FromInt(Size32)) :
		FString::Printf(TEXT("%08X_%d"), FCrc::StrCrc32(*SHMIdentifier), Size32));  // macOS does NOT support long file name
#endif
	bool bFound = false;
	float* SHM = FHoudiniEngineUtils::FindOrCreateSharedMemory(*SHMPath, Size32, InOutHandle, bFound);
	
	FMemory::Memzero(SHM, Size32 * sizeof(float));
	return SHM;
}

bool FHoudiniSharedMemoryGeometryInput::HapiUpload(const int32& SHMGeoInputNodeId, const float* SHMToUnmap) const
{
	FHoudiniEngineUtils::UnmapSharedMemory(SHMToUnmap);
	
	FString ParmStr = FString::Printf(TEXT("{\"shmpath\":\"%s\",\"datasize32\":%d,\"numpts\":%d,\"numprims\":%d,"),
		*SHMPath, Size32, NumPoints, NumPrims);
	if (!InputAttribInfos.IsEmpty())
	{
		FString NameStr = TEXT("\"attribname\":[\"");
		FString OwnerStr = TEXT("\"owner\":[");
		FString StorageStr = TEXT("\"storage\":[");
		FString TupleSizeStr = TEXT("\"tuplesize\":[");
		FString CompressionStr = TEXT("\"compression\":[");
		for (const FHoudiniSHMAttribInfo& AttribInfo : InputAttribInfos)
		{
			NameStr += FString(AttribInfo.Name.c_str()) + TEXT("\",\"");
			OwnerStr += FString::FromInt(int32(AttribInfo.Owner)) + TEXT(",");
			StorageStr += FString::FromInt(int32(AttribInfo.Type)) + TEXT(",");
			TupleSizeStr += FString::FromInt(AttribInfo.TupleSize) + TEXT(",");
			CompressionStr += FString::FromInt(int32(AttribInfo.Compression)) + TEXT(",");
		}
		NameStr.RemoveFromEnd(TEXT(",\"")); ParmStr += NameStr + TEXT("],");
		OwnerStr.RemoveFromEnd(TEXT(",")); ParmStr += OwnerStr + TEXT("],");
		StorageStr.RemoveFromEnd(TEXT(",")); ParmStr += StorageStr + TEXT("],");
		TupleSizeStr.RemoveFromEnd(TEXT(",")); ParmStr += TupleSizeStr + TEXT("],");
		CompressionStr.RemoveFromEnd(TEXT(",")); ParmStr += CompressionStr + TEXT("]");
	}
	ParmStr += TEXT("}");

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
		SHMGeoInputNodeId, TCHAR_TO_UTF8(*ParmStr), 0, 0));

	return true;
}


// -------- sharedmemory_volumeinput --------
bool FHoudiniSharedMemoryVolumeInput::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(sharedmemory_volumeinput);
}

float* FHoudiniSharedMemoryVolumeInput::GetSharedMemory(const FString& SHMIdentifier, size_t& InOutHandle, bool& bOutFound)
{
	const size_t TupleSize = (Storage == EHoudiniVolumeStorageType::Int) ? 1 : ((int32)Storage);
	const size_t ElemSize = FHoudiniEngineUtils::GetElemSize(InputDataType);
	size_t Size32 = Resolution.X * Resolution.Y * Resolution.Z * TupleSize * ElemSize;
	Size32 = (Size32 % 4) ? (Size32 / 4 + 1) : (Size32 / 4);  // Align 32bit
#if PLATFORM_WINDOWS
	SHMPath = FHoudiniEngine::GetProcessIdentifier() + SHMIdentifier + TEXT("_") + FString::FromInt(Size32);
#else
	SHMPath = FHoudiniEngine::GetProcessIdentifier() + ((SHMIdentifier.Len() <= 16) ? (SHMIdentifier + TEXT("_") + FString::FromInt(Size32)) :
		FString::Printf(TEXT("%08X_%d"), FCrc::StrCrc32(*SHMIdentifier), Size32));  // macOS does NOT support long file name
#endif
	return FHoudiniEngineUtils::FindOrCreateSharedMemory(*SHMPath, Size32, InOutHandle, bOutFound);
}

void FHoudiniSharedMemoryVolumeInput::AppendAttribute(const char* AttribName, const EHoudiniAttributeOwner& Owner, const bool& bIsInt, const int32& TupleSize, const FVector4f& Value)
{
	std::string AttribStr = AttribName;
	const std::string AttribIdxStr = TCHAR_TO_UTF8(*FString::FromInt(InputAttribStrs.Num() + 1));
	AttribStr = "attribname" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX "\"" + AttribStr +
		"\"" HAPI_PRESET_ROW_DELIM "owner" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(int32(Owner)))) +
		HAPI_PRESET_ROW_DELIM "storage" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(bIsInt ? "0" : "1") +
		HAPI_PRESET_ROW_DELIM "tuplesize" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(TupleSize))) +
		HAPI_PRESET_ROW_DELIM "numericvalue" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX +
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.X))) + HAPI_PRESET_VALUE_DELIM +
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.Y))) + HAPI_PRESET_VALUE_DELIM +
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.Z))) + HAPI_PRESET_VALUE_DELIM +
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.W))) + HAPI_PRESET_ROW_DELIM;

	InputAttribStrs.Add(AttribStr);
}

void FHoudiniSharedMemoryVolumeInput::AppendAttribute(const char* AttribName, const EHoudiniAttributeOwner& Owner, const bool& bIsDict, const FString& Value)
{
	std::string AttribStr = AttribName;
	const std::string AttribIdxStr = TCHAR_TO_UTF8(*FString::FromInt(InputAttribStrs.Num() + 1));
	AttribStr = "attribname" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX "\"" + AttribStr +
		"\"" HAPI_PRESET_ROW_DELIM "owner" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(int32(Owner)))) +
		HAPI_PRESET_ROW_DELIM "storage" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(bIsDict ? "3" : "2") +
		HAPI_PRESET_ROW_DELIM "stringvalue" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX "\"" + std::string(TCHAR_TO_UTF8(*Value)) + "\"" HAPI_PRESET_ROW_DELIM;

	InputAttribStrs.Add(AttribStr);
}

bool FHoudiniSharedMemoryVolumeInput::HapiUpload(const int32& SHMVolumeInputNodeId, const float* SHMToUnmap, const FVector2f& VolumeInputDataRange,
	const FString& VolumeName, const bool& bPartialUpdate, const FIntVector3& BBoxMin, const FIntVector3& BBoxMax, const bool& bIsTexture) const
{
	FHoudiniEngineUtils::UnmapSharedMemory(SHMToUnmap);

	int ParmIntValues[14] = { int(InputDataType), int(Storage), Resolution.X, Resolution.Y, Resolution.Z,
		int(bPartialUpdate), BBoxMin.X, BBoxMin.Y, BBoxMin.Z, BBoxMax.X, BBoxMax.Y, BBoxMax.Z, (bIsTexture ? 5 : 0), InputAttribStrs.Num()};

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), SHMVolumeInputNodeId,
		ParmIntValues, 0, 14));

	float ParmFloatValues[2] = {VolumeInputDataRange.X, VolumeInputDataRange.Y};
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), SHMVolumeInputNodeId,
		ParmFloatValues, 0, 2));

	std::string PresetStr = HAPI_PRESET_PREFIX "shmpath" HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*SHMPath)) +
		HAPI_PRESET_ROW_DELIM "name" HAPI_PRESET_ROW_MIDFIX "\"" + std::string(TCHAR_TO_UTF8(*VolumeName)) + "\"" HAPI_PRESET_ROW_DELIM;
	for (const std::string& AttribStr : InputAttribStrs)
		PresetStr += AttribStr;
	PresetStr += HAPI_PRESET_SUFFIX;
	
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetPreset(FHoudiniEngine::Get().GetSession(), SHMVolumeInputNodeId,
		HAPI_PRESETTYPE_BINARY, nullptr, PresetStr.c_str(), PresetStr.length() - 1));

	return true;
}

bool FHoudiniSharedMemoryVolumeInput::HapiPartialUpload(const int32& SHMVolInputNodeId, const float* SHMToUnmap,
	const bool& bPartialUpdate, const FIntVector3& BBoxMin, const FIntVector3& BBoxMax) const
{
	FHoudiniEngineUtils::UnmapSharedMemory(SHMToUnmap);

	int ParmIntValues[12] = { int(InputDataType), int(Storage), Resolution.X, Resolution.Y, Resolution.Z,
		int(bPartialUpdate), BBoxMin.X, BBoxMin.Y, BBoxMin.Z, BBoxMax.X, BBoxMax.Y, BBoxMax.Z };

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), SHMVolInputNodeId,
		ParmIntValues, 0, 12));

	return true;
}


// -------- he_setup_mesh_input --------
bool FHoudiniEngineSetupMeshInput::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(he_setup_mesh_input);
}

bool FHoudiniEngineSetupMeshInput::HapiConnectInput(const int32& SettingsNodeId, const int32& MeshNodeId)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, 0, MeshNodeId, 0));

	return true;
}

void FHoudiniEngineSetupMeshInput::AppendAttribute(const char* AttribName,
	const EHoudiniAttributeOwner& Owner, const EHoudiniStorageType& AttribStorage, const int32& TupleSize, const FVector4f& Value)
{
	auto GetHoudiniAttribType = [](const EHoudiniStorageType& AttribStorage, const int32& TupleSize) -> int32
		{
			switch (AttribStorage)
			{
			case EHoudiniStorageType::Float: return ((TupleSize >= 2) ? 2 : 0);
			case EHoudiniStorageType::Int: return 1;
			case EHoudiniStorageType::String: return 3;
			}
			return 0;
		};

	std::string AttribStr = AttribName;
	const std::string AttribIdxStr = TCHAR_TO_UTF8(*FString::FromInt(InputAttribStrs.Num() + 1));
	AttribStr = "name" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX "\"" + AttribStr +
		"\"" HAPI_PRESET_ROW_DELIM "class" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(3 - int32(Owner)))) +
		HAPI_PRESET_ROW_DELIM "type" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(GetHoudiniAttribType(AttribStorage, TupleSize)))) +
		HAPI_PRESET_ROW_DELIM "size" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(TupleSize))) +
		HAPI_PRESET_ROW_DELIM "value" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX +
			std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.X))) + HAPI_PRESET_VALUE_DELIM +
			std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.Y))) + HAPI_PRESET_VALUE_DELIM + 
			std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.Z))) + HAPI_PRESET_VALUE_DELIM + 
			std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.W))) + HAPI_PRESET_ROW_DELIM;

	InputAttribStrs.Add(AttribStr);
}

void FHoudiniEngineSetupMeshInput::AppendAttribute(const char* AttribName, const EHoudiniAttributeOwner& Owner, const FString& Value)
{
	std::string AttribStr = AttribName;
	const std::string AttribIdxStr = TCHAR_TO_UTF8(*FString::FromInt(InputAttribStrs.Num() + 1));
	AttribStr = "name" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX "\"" + AttribStr +
		"\"" HAPI_PRESET_ROW_DELIM "class" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX + std::string(TCHAR_TO_UTF8(*FString::FromInt(3 - int32(Owner)))) +
		HAPI_PRESET_ROW_DELIM "type" + AttribIdxStr + HAPI_PRESET_ROW_MIDFIX "3" HAPI_PRESET_ROW_DELIM "string" + AttribIdxStr +
		HAPI_PRESET_ROW_MIDFIX "\"" + std::string(TCHAR_TO_UTF8(*Value)) + "\"" HAPI_PRESET_ROW_DELIM;

	InputAttribStrs.Add(AttribStr);
}

bool FHoudiniEngineSetupMeshInput::HapiUpload(const int32& SettingsNodeId) const
{
	if (!InputAttribStrs.IsEmpty())
	{
		//std::string PresetStr = HAPI_PRESET_PREFIX "numattr" HAPI_PRESET_ROW_MIDFIX +
		//	std::string(TCHAR_TO_UTF8(*FString::FromInt(InputAttribStrs.Num()))) + HAPI_PRESET_ROW_DELIM;  // It seems that do not set multiparm could be faster?
		std::string PresetStr = HAPI_PRESET_PREFIX;
		for (const std::string& AttribStr : InputAttribStrs)
			PresetStr += AttribStr;
		PresetStr += HAPI_PRESET_SUFFIX;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetPreset(FHoudiniEngine::Get().GetSession(), SettingsNodeId,
			HAPI_PRESETTYPE_BINARY, nullptr, PresetStr.c_str(), PresetStr.length() - 1));
	}
	return true;
}

bool FHoudiniEngineSetupMeshInput::HapiUpload(const int32& SettingsNodeId, const FTransform& Transform) const
{
	float FloatValues[9];

	const FVector3f Position = FVector3f(Transform.GetLocation() * POSITION_SCALE_TO_HOUDINI);
	FloatValues[0] = Position.X;
	FloatValues[1] = Position.Z;
	FloatValues[2] = Position.Y;

	const FQuat4f Rotation = FQuat4f(Transform.GetRotation());
	const FRotator3f Rotator = FQuat4f(Rotation.X, Rotation.Z, Rotation.Y, -Rotation.W).Rotator();
	FloatValues[3] = -Rotator.Roll;
	FloatValues[4] = -Rotator.Pitch;
	FloatValues[5] = Rotator.Yaw;

	const FVector3f Scale = FVector3f(Transform.GetScale3D());
	FloatValues[6] = Scale.X;
	FloatValues[7] = Scale.Z;
	FloatValues[8] = Scale.Y;

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), SettingsNodeId,
		FloatValues, 0, 9));

	return HapiUpload(SettingsNodeId);
}

// -------- he_setup_kinefx_input --------
bool FHoudiniEngineSetupKineFXInput::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(he_setup_kinefx_input);
}

bool FHoudiniEngineSetupKineFXInput::HapiConnectInput(const int32& SettingsNodeId, const int32& MeshNodeId, const int32& SkeletonNodeId)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, 0, MeshNodeId, 0));

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, 1, SkeletonNodeId, 0));

	return true;
}

// -------- he_setup_heightfield_input --------
bool FHoudiniEngineSetupHeightfieldInput::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(he_setup_heightfield_input);
}

bool FHoudiniEngineSetupHeightfieldInput::HapiConnectInput(const int32& SettingsNodeId, const int32& NodeId)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, 0, NodeId, 0));

	return true;
}

bool FHoudiniEngineSetupHeightfieldInput::HapiSetResolution(const int32& SettingsNodeId, const bool& bMask, const FIntVector2& Resolution)
{
	const int ParmValues[3] = { int(bMask), Resolution.X, Resolution.Y };
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), SettingsNodeId,
		ParmValues, 0, 3));

	return true;
}

bool FHoudiniEngineSetupHeightfieldInput::HapiSetLandscapeTransform(const int32& SettingsNodeId,
	const FTransform& LandscapeTransform, const FIntRect& Extent)
{
	float HapiTransform[9];
	FHoudiniEngineUtils::ConvertLandscapeTransform(HapiTransform, LandscapeTransform, Extent);

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), SettingsNodeId,
		HapiTransform, 0, 9));

	return true;
}

bool FHoudiniEngineSetupHeightfieldInput::HapiSetVexScript(const int32& SettingsNodeId, const FString& VexScript)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), SettingsNodeId,
		TCHAR_TO_UTF8(*VexScript), 5, 0));

	return true;
}


// -------- "null" sop --------
bool FHoudiniSopNull::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(null);
}

bool FHoudiniSopNull::HapiSetupBaseInfos(const int32& SettingsNodeId, const FVector3f UnrealPosition)
{
	HAPI_PartInfo PartInfo;
	PartInfo.id = 0;
	PartInfo.nameSH = 0;
	PartInfo.type = HAPI_PARTTYPE_MESH;
	PartInfo.faceCount = 0;
	PartInfo.vertexCount = 0;
	PartInfo.pointCount = 1;
	PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	PartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	PartInfo.isInstanced = false;
	PartInfo.instancedPartCount = 0;
	PartInfo.instanceCount = 0;
	PartInfo.hasChanged = false;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetPartInfo(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0, &PartInfo));

	return HapiAddFloatAttribute(SettingsNodeId, HAPI_ATTRIB_POSITION, 3,
		FVector4f(UnrealPosition.X, UnrealPosition.Z, UnrealPosition.Y, 0.0f) * POSITION_SCALE_TO_HOUDINI);  // We must set v@P immediately, otherwise, session will fail.
}

bool FHoudiniSopNull::HapiAddFloatAttribute(const int32& SettingsNodeId,
	const char* AttribName, const int32& TupleSize, const FVector4f& AttribValue)
{
	HAPI_AttributeInfo AttribInfo;
	AttribInfo.count = 1;
	AttribInfo.exists = true;
	AttribInfo.owner = HAPI_ATTROWNER_POINT;
	AttribInfo.storage = HAPI_STORAGETYPE_FLOAT;
	AttribInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	AttribInfo.count = 1;
	AttribInfo.tupleSize = TupleSize;
	AttribInfo.totalArrayElements = TupleSize;
	AttribInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_INVALID;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0,
		AttribName, &AttribInfo));

	float Values[4] = { AttribValue.X, AttribValue.Y, AttribValue.Z, AttribValue.W };
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetAttributeFloatData(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0,
		AttribName, &AttribInfo, Values, 0, 1));

	return true;
}

bool FHoudiniSopNull::HapiAddStringAttribute(const int32& SettingsNodeId, const char* AttribName, const FString& AttribValue)
{
	HAPI_AttributeInfo AttribInfo;
	AttribInfo.count = 1;
	AttribInfo.exists = true;
	AttribInfo.owner = HAPI_ATTROWNER_POINT;
	AttribInfo.storage = HAPI_STORAGETYPE_STRING;
	AttribInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	AttribInfo.count = 1;
	AttribInfo.tupleSize = 1;
	AttribInfo.totalArrayElements = 1;
	AttribInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_INVALID;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0,
		AttribName, &AttribInfo));

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetAttributeStringUniqueData(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0,
		AttribName, &AttribInfo, TCHAR_TO_UTF8(*AttribValue), 1, 0, 1));

	return true;
}

bool FHoudiniSopNull::HapiAddDictAttribute(const int32& SettingsNodeId, const char* AttribName, const FString& AttribValue)
{
	HAPI_AttributeInfo AttribInfo;
	AttribInfo.count = 1;
	AttribInfo.exists = true;
	AttribInfo.owner = HAPI_ATTROWNER_POINT;
	AttribInfo.storage = HAPI_STORAGETYPE_DICTIONARY;
	AttribInfo.originalOwner = HAPI_ATTROWNER_INVALID;
	AttribInfo.count = 1;
	AttribInfo.tupleSize = 1;
	AttribInfo.totalArrayElements = 1;
	AttribInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_INVALID;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::AddAttribute(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0,
		AttribName, &AttribInfo));

	std::string AttribValueStr = TCHAR_TO_UTF8(*AttribValue);
	const char* AttribValuePtr = AttribValueStr.c_str();
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetAttributeDictionaryData(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0,
		AttribName, &AttribInfo, &AttribValuePtr, 0, 1));

	return true;
}


// -------- "copytopoints" sop --------
bool FHoudiniSopCopyToPoints::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(copytopoints);
}

bool FHoudiniSopCopyToPoints::HapiInitialize(const int32& SettingsNodeId)
{
	// Set: pack, origin, viewportlod, transform, useimplicitn, resettargetattribs, targetattribs
	static const int ParmIntValues[7] = { 1, 0, 0, 1, 1, 1, 1 };
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), SettingsNodeId,
		ParmIntValues, 2, 6));  // TODO: From Houdini 20.5.584, multi-parm cannot set with the buttom.

	return true;
}

bool FHoudiniSopCopyToPoints::HapiConnectInputs(const int32& SettingsNodeId, const int32& SrcNodeId, const int32 DstNodeId)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, 0, SrcNodeId, 0));

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, 1, DstNodeId, 0));

	return true;
}

bool FHoudiniSopCopyToPoints::HapiSetSourcePointGroup(const int32& SettingsNodeId, const FString& GroupStr)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
		SettingsNodeId, TCHAR_TO_UTF8(*GroupStr), 2, 0));

	return true;
}


// -------- "blast" sop --------
bool FHoudiniSopBlast::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(blast);
}

bool FHoudiniSopBlast::HapiConnectInput(const int32& BlastNodeId, const int32& NodeId)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
		BlastNodeId, 0, NodeId, 0));

	return true;
}

bool FHoudiniSopBlast::HapiSetGroup(const int32& BlastNodeId, const FString& Group, const EHoudiniBlastGroupType& GroupType, const bool& bDeleteNonSelected)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
		BlastNodeId, TCHAR_TO_UTF8(*Group), 0, 0));

	int IntValues[3] = { int(GroupType), 0, int(bDeleteNonSelected) };  // grouptype, computenorms, negate
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(),
		BlastNodeId, IntValues, 0, 3));

	return true;
}


// -------- "attribcreate" sop --------
bool FHoudiniSopAttribCreate::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(attribcreate);
}

bool FHoudiniSopAttribCreate::HapiSetupDeltaInfoAttribute(const int32& AttribCreateNodeId)
{
	// Set attribute name
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
		AttribCreateNodeId, HAPI_ATTRIB_DELTA_INFO, 5, 0));
	// Set attribute class
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(),
		AttribCreateNodeId, "class1", 0, 0));
	// Set attribute type
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(),
		AttribCreateNodeId, "type1", 0, 3));

	return true;
}

bool FHoudiniSopAttribCreate::HapiSetDeltaInfo(const int32& AttribCreateNodeId, const FString& DeltaInfo)
{
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
		AttribCreateNodeId, TCHAR_TO_UTF8(*DeltaInfo), 19, 0));

	return true;
}


// -------- "file" sop --------
bool FHoudiniSopFile::HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId)
{
	HAPI_CREATE_SOP_NODE(file);
}

bool FHoudiniSopFile::HapiLoadFile(const int32& FileNodeId, const char* GeoFilePath)
{
	// Set file path
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(),
		FileNodeId, GeoFilePath, 1, 0));

	return true;
}
