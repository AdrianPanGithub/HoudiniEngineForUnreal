// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "HoudiniEngineCommon.h"


struct HOUDINIENGINE_API FHoudiniParameterPresetHelper
{
protected:
	std::string PresetStr;

public:
	void Append(const std::string& ParmName, const std::string& ParmValueStr);

	bool HapiSet(const int32& NodeId);
};


enum class EHoudiniInputAttributeStorage
{
	Group = 0,
	Float = 1,
	Int = 2,
	String = 3,
	Dict = 4,
	FloatArray = 5,
	IntArray = 6,
	StringArray = 7,
	DictArray = 8
};

enum class EHoudiniInputAttributeCompression
{
	None = 0,
	UniqueValue = 1,
	Indexing = 2  // Only for string and dict
};

#define HOUDINI_SHM_GEO_INPUT_POLY     -1
#define HOUDINI_SHM_GEO_INPUT_POLYLINE -2

class HOUDINIENGINE_API FHoudiniSharedMemoryGeometryInput  // sharedmemory_geometryinput
{
protected:
	int32 NumPoints = 0;

	int32 NumPrims = 0;

	FString SHMPath;

	size_t Size32 = 0;

	struct FHoudiniSHMAttribInfo
	{
		std::string Name;
		EHoudiniAttributeOwner Owner = EHoudiniAttributeOwner::Invalid;
		EHoudiniInputAttributeStorage Type = EHoudiniInputAttributeStorage::Group;
		int32 TupleSize = 0;
		EHoudiniInputAttributeCompression Compression = EHoudiniInputAttributeCompression::None;
	};

	TArray<FHoudiniSHMAttribInfo> InputAttribInfos;

public:
	FHoudiniSharedMemoryGeometryInput(const int32& InNumPoints, const int32& InNumPrims, const int32& InNumVertices);

	// follow the methods below to upload geometry to houdini
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	void AppendGroup(const char* GroupName, const EHoudiniAttributeOwner& Class, const size_t& InSize32, const bool& bUniqueValue = false);

	void AppendAttribute(const char* AttribName,
		const EHoudiniAttributeOwner& Class, const EHoudiniInputAttributeStorage& Storage, const int32& TupleSize, const size_t& InSize32,
		const EHoudiniInputAttributeCompression& Compression = EHoudiniInputAttributeCompression::None);

	float* GetSharedMemory(const FString& SHMIdentifier, size_t& InOutHandle);

	bool HapiUpload(const int32& SHMGeoInputNodeId, const float* SHMToUnmap) const;
};


class HOUDINIENGINE_API FHoudiniSharedMemoryVolumeInput  // sharedmemory_volumeinput
{
protected:
	EHoudiniVolumeConvertDataType InputDataType = EHoudiniVolumeConvertDataType::Float;

	EHoudiniVolumeStorageType Storage = EHoudiniVolumeStorageType::Float;

	FIntVector3 Resolution = FIntVector3(1, 1, 1);

	FString SHMPath;

	TArray<std::string> InputAttribStrs;

public:
	FHoudiniSharedMemoryVolumeInput(const EHoudiniVolumeConvertDataType& VolumeInputDataType, const EHoudiniVolumeStorageType& VolumeStorage,
		const FIntVector3& VolumeResolution) : InputDataType(VolumeInputDataType), Storage(VolumeStorage), Resolution(VolumeResolution) {}

	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	float* GetSharedMemory(const FString& SHMIdentifier, size_t& InOutHandle, bool& bOutFound);

	void AppendAttribute(const char* AttribName, const EHoudiniAttributeOwner& Owner, const bool& bIsInt, const int32& TupleSize, const FVector4f& Value);

	void AppendAttribute(const char* AttribName, const EHoudiniAttributeOwner& Owner, const bool& bIsDict, const FString& Value);

	bool HapiUpload(const int32& SHMVolInputNodeId, const float* SHMToUnmap, const FVector2f& VolumeInputDataRange, const FString& VolumeName,
		const bool& bPartialUpdate = false, const FIntVector3& PartialBBoxMin = FIntVector3(-1, -1, -1), const FIntVector3& PartialBBoxMax = FIntVector3(-1, -1, -1), const bool& bIsTexture = false) const;

	bool HapiPartialUpload(const int32& SHMVolInputNodeId, const float* SHMToUnmap,
		const bool& bPartialUpdate, const FIntVector3& BBoxMin, const FIntVector3& BBoxMax) const;
};


class HOUDINIENGINE_API FHoudiniEngineSetupMeshInput  // he_setup_mesh_input
{
protected:
	TArray<std::string> InputAttribStrs;

public:
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiConnectInput(const int32& SettingsNodeId, const int32& MeshNodeId);

	void AppendAttribute(const char* AttribName,
		const EHoudiniAttributeOwner& Owner, const EHoudiniStorageType& AttribStorage, const int32& TupleSize, const FVector4f& Value);

	void AppendAttribute(const char* AttribName, const EHoudiniAttributeOwner& Owner, const FString& Value);

	bool HapiUpload(const int32& SettingsNodeId) const;

	bool HapiUpload(const int32& SettingsNodeId, const FTransform& Transform) const;
};

class HOUDINIENGINE_API FHoudiniEngineSetupKineFXInput : public FHoudiniEngineSetupMeshInput // he_setup_kinefx_input
{
public:
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiConnectInput(const int32& SettingsNodeId, const int32& MeshNodeId, const int32& SkeletonNodeId);
};


struct HOUDINIENGINE_API FHoudiniEngineSetupHeightfieldInput  // he_setup_heightfield_input
{
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiConnectInput(const int32& SettingsNodeId, const int32& NodeId);

	static bool HapiSetResolution(const int32& SettingsNodeId, const bool& bMask, const FIntVector2& Resolution);

	static bool HapiSetLandscapeTransform(const int32& SettingsNodeId, const FTransform& LandscapeTransform, const FIntRect& Extent);

	static bool HapiSetVexScript(const int32& SettingsNodeId, const FString& VexScript);
};


struct HOUDINIENGINE_API FHoudiniSopNull
{
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiSetupBaseInfos(const int32& SettingsNodeId, const FVector3f UnrealPosition);  // Must call this before add other attribs, Will switch Y and Z of Position and * 0.01

	static bool HapiAddFloatAttribute(const int32& SettingsNodeId, const char* AttribName, const int32& TupleSize, const FVector4f& AttribValue);  // Will switch Y and Z of AttribValue

	static bool HapiAddStringAttribute(const int32& SettingsNodeId, const char* AttribName, const FString& AttribValue);

	static bool HapiAddDictAttribute(const int32& SettingsNodeId, const char* AttribName, const FString& AttribValue);
};

struct HOUDINIENGINE_API FHoudiniSopCopyToPoints
{
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiInitialize(const int32& SettingsNodeId);  // Will set pack to origin and No target attribs to copy

	static bool HapiConnectInputs(const int32& SettingsNodeId, const int32& SrcNodeId, const int32 DstNodeId);

	static bool HapiSetSourcePointGroup(const int32& SettingsNodeId, const FString& GroupStr);
};

enum class EHoudiniBlastGroupType
{
	GuessFromGroup = 0,
	Breakpoints,
	Edges,
	Points,
	Primitives
};

struct HOUDINIENGINE_API FHoudiniSopBlast
{
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiConnectInput(const int32& BlastNodeId, const int32& NodeId);

	static bool HapiSetGroup(const int32& BlastNodeId, const FString& Group, const EHoudiniBlastGroupType& GroupType, const bool& bDeleteNonSelected);
};

struct HOUDINIENGINE_API FHoudiniSopAttribCreate
{
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiSetupDeltaInfoAttribute(const int32& AttribCreateNodeId);

	static bool HapiSetDeltaInfo(const int32& AttribCreateNodeId, const FString& DeltaInfo);
};

struct HOUDINIENGINE_API FHoudiniSopFile
{
	static bool HapiCreateNode(const int32& ParentNodeId, const FString& NodeLabel, int32& OutNodeId);

	static bool HapiLoadFile(const int32& FileNodeId, const char* GeoFilePath);
};
