// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "JsonObjectConverter.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Components/DynamicMeshComponent.h"
#include "MeshUtilitiesCommon.h"
#include "StaticMeshCompiler.h"

#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniAttribute.h"
#include "HoudiniMeshComponent.h"


//#include "MeshDescriptionBuilder.h"
//#include "Runtime/MeshConversion/Private/MeshDescriptionToDynamicMesh.cpp"

bool FHoudiniMeshOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = ((PartInfo.type == HAPI_PARTTYPE_MESH) && (PartInfo.faceCount >= 1) && (PartInfo.vertexCount >= 3));
	bOutShouldHoldByOutput = true;

	return true;
}

// StaticMeshOutput
UStaticMeshComponent* FHoudiniStaticMeshOutput::Find(const AHoudiniNode* Node) const
{
	return Find_Internal<false>(Component, Node);
}

void FHoudiniStaticMeshOutput::Destroy(const AHoudiniNode* Node) const
{
	DestroyComponent(Node, Component, false);
	Component.Reset();
}

UStaticMeshComponent* FHoudiniStaticMeshOutput::Commit(AHoudiniNode* Node, const int32& InPartId, const FString& InSplitValue, const bool& bInSplitActor)
{
	PartId = InPartId;

	if (PartId < 0)  // We should have an SMC here
	{
		USceneComponent* SMC = Component.IsValid() ? Component.Get() : nullptr;
		CreateOrUpdateComponent(Node, SMC, UStaticMeshComponent::StaticClass(), InSplitValue, bInSplitActor, false);
		Component = ((UStaticMeshComponent*)SMC);
		((UStaticMeshComponent*)SMC)->SetStaticMesh(StaticMesh.LoadSynchronous());

		return ((UStaticMeshComponent*)SMC);
	}
	else  // This is a packed mesh, we should NOT have SMC here, the SMC will create in UHoudiniOutputInstancer
	{
		AActor* ParentActor = nullptr;
		UStaticMeshComponent* ExistSMC = Component.IsValid() ?
			Component.Get() : Cast<UStaticMeshComponent>(FindComponent(Node, ParentActor, false));
		
		if (ExistSMC)  // If SMC exists, then we should destroy it
		{
			Component.Reset();
			ComponentName = NAME_None;

			FHoudiniEngineUtils::DestroyComponent(ExistSMC);
		}

		// We should update split info here, as component has been find upon
		SplitValue = InSplitValue;
		bSplitActor = bInSplitActor;
	}

	return nullptr;
}


IConsoleVariable* FHoudiniMeshOutputBuilder::MeshDistanceFieldCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GenerateMeshDistanceFields"));
bool FHoudiniMeshOutputBuilder::GShouldRecoverMeshDistanceField = false;

void FHoudiniMeshOutputBuilder::OnStaticMeshBuild()
{
	if (GShouldRecoverMeshDistanceField)
		return;

	if (!GetDefault<UHoudiniEngineSettings>()->bDeferMeshDistanceFieldGeneration)
		return;

	if (!MeshDistanceFieldCVar->GetBool())
		return;
	
	MeshDistanceFieldCVar->Set(false);
	GShouldRecoverMeshDistanceField = true;
}

void FHoudiniMeshOutputBuilder::PostProcess(const AHoudiniNode* Node, const bool& bForce)
{
	if (bForce || GShouldRecoverMeshDistanceField)
	{
		const double StartTime = FPlatformTime::Seconds();
		FStaticMeshCompilingManager::Get().FinishAllCompilation();
		const double DuringTime = FPlatformTime::Seconds() - StartTime;
		if (DuringTime > 0.001)
			UE_LOG(LogHoudiniEngine, Log, TEXT("Wait All Static Mesh Compilation: %f (s)"), DuringTime);
	}
	
	if (GShouldRecoverMeshDistanceField)
	{
		MeshDistanceFieldCVar->Set(true);
		GShouldRecoverMeshDistanceField = false;
	}
}


// DynamicMeshOutput
UDynamicMeshComponent* FHoudiniDynamicMeshOutput::Find(const AHoudiniNode* Node) const
{
	return Find_Internal<false>(Component, Node);
}

UDynamicMeshComponent* FHoudiniDynamicMeshOutput::CreateOrUpdate(AHoudiniNode* Node, const FString& InSplitValue, const bool& bInSplitActor)
{
	return CreateOrUpdate_Internal<false>(Component, Node, InSplitValue, bInSplitActor);
}

void FHoudiniDynamicMeshOutput::Destroy(const AHoudiniNode* Node) const
{
	DestroyComponent(Node, Component, false);
	Component.Reset();
}


// HoudiniMeshOutput
UHoudiniMeshComponent* FHoudiniMeshOutput::Find(const AHoudiniNode* Node) const
{
	return Find_Internal<true>(Component, Node);
}

UHoudiniMeshComponent* FHoudiniMeshOutput::CreateOrUpdate(AHoudiniNode* Node, const FString& InSplitValue, const bool& bInSplitActor)
{
	return CreateOrUpdate_Internal<true>(Component, Node, InSplitValue, bInSplitActor);
}

void FHoudiniMeshOutput::Destroy(const AHoudiniNode* Node) const
{
	DestroyComponent(Node, Component, true);
	Component.Reset();
}


static bool HapiGetLodGroupShips(const int32& NodeId, const int32& PartId, const TArray<std::string>& PrimGroupNames,
	const int32& NumPrims, const bool& bOnPackedPart, TArray<TArray<int32>>& OutLodGroupShips)  // Starts with second lod, may be "lod_1"
{
	TArray<TPair<int32, FUtf8StringView>> LodIndicators;
	for (const std::string& PrimGroupName : PrimGroupNames)  // Find all lod groups, as some lod idx may loss, we should consider this.
	{
		if (PrimGroupName.starts_with(HAPI_GROUP_PREFIX_LOD))
		{
			const int32 LodIdx = FCStringAnsi::Atoi(PrimGroupName.c_str() + 4);
			if (LodIdx >= 1)  // We should NOT parse "lod_0", main_geo will be the rest of lod and collision group
			{
				if (!LodIndicators.ContainsByPredicate([LodIdx](const TPair<int32, FUtf8StringView>& Lod) { return Lod.Key == LodIdx; }))
					LodIndicators.Add(TPair<int32, FUtf8StringView>(LodIdx, FUtf8StringView(PrimGroupName.c_str(), PrimGroupName.size())));
			}
		}
	}

	LodIndicators.Sort();

	for (const TPair<int32, FUtf8StringView>& LodIndicator : LodIndicators)
	{
		TArray<int32> LodGroupship;
		bool bLodGroupshipConst = false;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetGroupMembership(NodeId, PartId,
			NumPrims, bOnPackedPart, HAPI_GROUPTYPE_PRIM, (const char*)LodIndicator.Value.GetData(),
			LodGroupship, bLodGroupshipConst));
		if (LodGroupship[0] || !bLodGroupshipConst)  // Skip unused group
			OutLodGroupShips.Add(LodGroupship);
	}

	return true;
}

static bool HapiGetUVAttributeData(TArray<TArray<float>>& OutUVsData, TArray<HAPI_AttributeOwner>& OutUVOwners,
	const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX])
{
	TArray<TPair<int32, std::string>> UVIndicators;
	for (const std::string& AttribName : AttribNames)  // Find all uvs
	{
		if (AttribName.starts_with(HAPI_ATTRIB_UV))
		{
			const int32 UVIdx = (AttribName == HAPI_ATTRIB_UV) ? 0 : FCString::Atoi(UTF8_TO_TCHAR(AttribName.c_str() + 2));
			if (!UVIndicators.ContainsByPredicate([UVIdx](const TPair<int32, std::string>& UV) { return UV.Key == UVIdx; }))
				UVIndicators.Add(TPair<int32, std::string>(UVIdx, AttribName));
		}
	}
	UVIndicators.Sort();

	for (const TPair<int32, std::string>& UVIndicator : UVIndicators)
	{
		const int32& UVIdx = UVIndicator.Key;
		if (UVIdx < 0)
			continue;

		const std::string& UVName = UVIndicator.Value;
		HAPI_AttributeOwner UVOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, AttribCounts, UVName);
		if (UVOwner == HAPI_ATTROWNER_INVALID)
			continue;

		TArray<float>& UVData = OutUVsData.AddDefaulted_GetRef();
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId,
			UVName.c_str(), 2, UVData, UVOwner));  // We only need two channels of uv attribute
		if (UVData.IsEmpty())
		{
			OutUVsData.Pop();
			continue;
		}

		OutUVOwners.Add(UVOwner);
	}

	return true;
}

static int32 MeshAttributeEntryIdx(const HAPI_AttributeOwner& AttribOwner,
	const int32& DstElemIdx, const HAPI_AttributeOwner& DstOwner, const TArray<int32>& Vertices)
{
	switch (AttribOwner)
	{
	case HAPI_ATTROWNER_VERTEX:
	{
		switch (DstOwner)
		{
		case HAPI_ATTROWNER_VERTEX: return DstElemIdx;
		case HAPI_ATTROWNER_POINT: return Vertices.IndexOfByKey(DstElemIdx);
		case HAPI_ATTROWNER_PRIM: return DstElemIdx * 3;
		case HAPI_ATTROWNER_DETAIL: return 0;
		}
		return -1;
	}
	case HAPI_ATTROWNER_POINT:
	{
		switch (DstOwner)
		{
		case HAPI_ATTROWNER_VERTEX: return Vertices[DstElemIdx];
		case HAPI_ATTROWNER_POINT: return DstElemIdx;
		case HAPI_ATTROWNER_PRIM: return Vertices[DstElemIdx * 3];
		case HAPI_ATTROWNER_DETAIL: return 0;
		}
		return -1;
	}
	case HAPI_ATTROWNER_PRIM:
	{
		switch (DstOwner)
		{
		case HAPI_ATTROWNER_VERTEX: return DstElemIdx / 3;
		case HAPI_ATTROWNER_POINT: return Vertices.IndexOfByKey(DstElemIdx) / 3;
		case HAPI_ATTROWNER_PRIM: return DstElemIdx;
		case HAPI_ATTROWNER_DETAIL: return 0;
		}
		return -1;
	}
	case HAPI_ATTROWNER_DETAIL:
		return DstOwner == HAPI_ATTROWNER_INVALID ? -1 : 0;
	}
	return -1;
}

FORCEINLINE static FString GetStaticMeshAssetPath(const FString& CookFolder, const FString& GeoName, const FString& SplitValue, const int32& PartId)
{
	return CookFolder + (SplitValue.IsEmpty() ?
		FString::Printf(TEXT("SM_%s_%d"), *FHoudiniEngineUtils::GetValidatedString(GeoName), PartId) :
		FString::Printf(TEXT("SM_%s_%s_%d"), *FHoudiniEngineUtils::GetValidatedString(GeoName), *SplitValue, PartId));
}


static bool HapiRetrieveMeshMaterialData(TArray<UMaterialInterface*>& OutMatInsts, HAPI_AttributeOwner& OutMatInstOwner, TArray<TSharedPtr<FHoudiniAttribute>>& OutMatAttribs,
	TArray<UMaterialInterface*>& OutMats, HAPI_AttributeOwner& OutMatOwner,
	const TArray<int32>& Vertices, const int32& NodeId, const HAPI_PartInfo& PartInfo, const TArray<std::string>& AttribNames)
{
	const int32& PartId = PartInfo.id;

	auto HapiGetSHMaterialMap = [](const TArray<HAPI_StringHandle>& MatRefSHs, TMap<HAPI_StringHandle, UMaterialInterface*>& OutSHMaterialMap)
		{
			const TArray<HAPI_StringHandle> UniqueMaterialRefSHs = TSet<HAPI_StringHandle>(MatRefSHs).Array();
			TArray<FString> UniqueMaterialRefs;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueMaterialRefSHs, UniqueMaterialRefs));
			for (int32 UniqueIdx = 0; UniqueIdx < UniqueMaterialRefSHs.Num(); ++UniqueIdx)
			{
				UMaterialInterface* Material = UniqueMaterialRefs[UniqueIdx].IsEmpty() ? nullptr :
					LoadObject<UMaterialInterface>(nullptr, *UniqueMaterialRefs[UniqueIdx], nullptr, LOAD_Quiet | LOAD_NoWarn);
				OutSHMaterialMap.Add(UniqueMaterialRefSHs[UniqueIdx], (IsValid(Material) ? Material : nullptr));
			}

			return true;
		};

	auto HapiGetMaterialDataMap = [&](HAPI_AttributeInfo& AttribInfo, const char* AttribName,
		TArray<HAPI_StringHandle>& InOutSHs, HAPI_AttributeOwner& InOutOwner, TMap<HAPI_StringHandle, UMaterialInterface*>& OutSHMaterialMap)
		{
			InOutSHs.SetNumUninitialized(AttribInfo.count);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				AttribName, &AttribInfo, InOutSHs.GetData(), 0, AttribInfo.count));

			if (InOutOwner < HAPI_ATTROWNER_PRIM)
			{
				TArray<HAPI_StringHandle> AllSHs;
				AllSHs.SetNumUninitialized(PartInfo.faceCount);
				for (int32 PrimIdx = 0; PrimIdx < PartInfo.faceCount; ++PrimIdx)
					AllSHs[PrimIdx] = InOutSHs[MeshAttributeEntryIdx(InOutOwner, PrimIdx, HAPI_ATTROWNER_PRIM, Vertices)];
				InOutSHs = AllSHs;

				InOutOwner = HAPI_ATTROWNER_PRIM;
			}

			return HapiGetSHMaterialMap(InOutSHs, OutSHMaterialMap);
		};

	auto HapiRetrieveMaterialAttribute = [&]()
		{
			if (OutMatOwner != HAPI_ATTROWNER_INVALID)
			{
				HAPI_AttributeInfo AttribInfo;
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_ATTRIB_UNREAL_MATERIAL, OutMatOwner, &AttribInfo));

				if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
				{
					TArray<HAPI_StringHandle> MaterialRefSHs;
					TMap<HAPI_StringHandle, UMaterialInterface*> SHMaterialMap;
					HOUDINI_FAIL_RETURN(HapiGetMaterialDataMap(AttribInfo,
						HAPI_ATTRIB_UNREAL_MATERIAL, MaterialRefSHs, OutMatOwner, SHMaterialMap));

					OutMats.SetNumUninitialized(MaterialRefSHs.Num());
					for (int32 ElemIdx = 0; ElemIdx < MaterialRefSHs.Num(); ++ElemIdx)
						OutMats[ElemIdx] = SHMaterialMap[MaterialRefSHs[ElemIdx]];
				}
			}

			return true;
		};

	OutMatInstOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, HAPI_ATTROWNER_PRIM);
	OutMatOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_MATERIAL, HAPI_ATTROWNER_PRIM);
	if (OutMatInstOwner != HAPI_ATTROWNER_INVALID)  // Material instance superiority
	{
		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, OutMatInstOwner, &AttribInfo));

		if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
		{
			TArray<HAPI_StringHandle> MaterialRefSHs;
			TMap<HAPI_StringHandle, UMaterialInterface*> SHMaterialMap;
			HOUDINI_FAIL_RETURN(HapiGetMaterialDataMap(AttribInfo,
				HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, MaterialRefSHs, OutMatInstOwner, SHMaterialMap));

			TArray<int32> NullIndices;
			OutMatInsts.SetNumUninitialized(MaterialRefSHs.Num());
			for (int32 ElemIdx = 0; ElemIdx < MaterialRefSHs.Num(); ++ElemIdx)
			{
				UMaterialInterface* Mat = SHMaterialMap[MaterialRefSHs[ElemIdx]];
				OutMatInsts[ElemIdx] = Mat;
				if (!Mat)
					NullIndices.Add(ElemIdx);
			}

			if (NullIndices.Num() == MaterialRefSHs.Num())
			{
				OutMatInsts.Empty();
				OutMatInstOwner = HAPI_ATTROWNER_INVALID;
				return HapiRetrieveMaterialAttribute();  // Fallback to use unreal_material
			}
			else if (!NullIndices.IsEmpty() && OutMatOwner != HAPI_ATTROWNER_INVALID)  // In this case, OutMatInstOwner must be HAPI_ATTROWNER_PRIM
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_ATTRIB_UNREAL_MATERIAL, OutMatOwner, &AttribInfo));

				if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
				{
					MaterialRefSHs.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_UNREAL_MATERIAL, &AttribInfo, MaterialRefSHs.GetData(), 0, AttribInfo.count));

					if (OutMatOwner <= HAPI_ATTROWNER_PRIM)
					{
						TArray<HAPI_StringHandle> AllSHs;
						AllSHs.SetNumUninitialized(NullIndices.Num());
						for (int32 NullIdx = 0; NullIdx < NullIndices.Num(); ++NullIdx)
							AllSHs[NullIdx] = MaterialRefSHs[MeshAttributeEntryIdx(OutMatOwner, NullIndices[NullIdx], HAPI_ATTROWNER_PRIM, Vertices)];
						MaterialRefSHs = AllSHs;

						OutMatOwner = HAPI_ATTROWNER_PRIM;
					}

					SHMaterialMap.Empty();
					HOUDINI_FAIL_RETURN(HapiGetSHMaterialMap(MaterialRefSHs, SHMaterialMap));

					OutMats.SetNumZeroed(OutMatInstOwner == HAPI_ATTROWNER_PRIM ? PartInfo.faceCount : 1);
					for (int32 NullIdx = 0; NullIdx < NullIndices.Num(); ++NullIdx)
						OutMats[NullIndices[NullIdx]] = SHMaterialMap[MaterialRefSHs[NullIdx]];
				}
			}

			return FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
				HAPI_ATTRIB_PREFIX_UNREAL_MATERIAL_PARAMETER, OutMatAttribs);
		}
	}

	return HapiRetrieveMaterialAttribute();
}

FORCEINLINE static UMaterialInterface* GetMeshMaterial(const TArray<UMaterialInterface*>& Mats, const HAPI_AttributeOwner& MatOwner,
	const TArray<UMaterialInterface*>& MatInsts, const HAPI_AttributeOwner& MatInstOwner, const int32& TriIdx, const TArray<int32>& Vertices, const bool& bHasAlphaData, bool& bOutIsMatInst)
{
	bOutIsMatInst = !MatInsts.IsEmpty();
	UMaterialInterface* Material = nullptr;
	if (bOutIsMatInst)
		Material = MatInsts[MeshAttributeEntryIdx(MatInstOwner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices)];

	if (!Material)  // Fallback to use material
	{
		bOutIsMatInst = false;
		const int32 MatDataIdx = MeshAttributeEntryIdx(MatOwner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices);
		Material = Mats.IsValidIndex(MatDataIdx) ? Mats[MatDataIdx] : nullptr;
	}

	if (!Material)  // If material not found, then use default VertexColorMaterial
		Material = FHoudiniEngineUtils::GetVertexColorMaterial(bHasAlphaData);

	return Material;
}


bool UHoudiniOutputMesh::HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputMesh);

	static const auto GetMeshOutputModeLambda = [](const FUtf8StringView& AttribValue) -> int8
		{
			if (AttribValue.StartsWith("dynamic") || AttribValue.StartsWith("dm") || AttribValue.StartsWith("proced"))
				return HAPI_UNREAL_OUTPUT_MESH_TYPE_DYNAMICMESH;
			else if (AttribValue.StartsWith("houdini") || AttribValue.StartsWith("hm") || AttribValue.StartsWith("proxy"))
				return HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH;
			return HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH;
		};

	struct FHoudiniMeshTrianglesHolder
	{
		FHoudiniMeshTrianglesHolder(const int8& UpdateMode, const FString& InSplitValue) :
			PartialOutputMode(UpdateMode), SplitValue(InSplitValue) {}

		int8 PartialOutputMode = 0;
		FString SplitValue;

		// < < MeshOutputMode, GroupIdx > , Triangles >
		TMap<TPair<int8, int32>, TArray<int32>> GroupTrianglesMap;
	};
	
	struct FHoudiniMeshPart
	{
		FHoudiniMeshPart(const HAPI_PartInfo& PartInfo) : Info(PartInfo) {}

		HAPI_PartInfo Info;
		TArray<std::string> AttribNames;
		TArray<int32> Vertices;
		TArray<FString> EditableGroupNames;
		TMap<int32, FHoudiniMeshTrianglesHolder> SplitMeshMap;

		FString GetGroupName(const int32& GroupIdx) const
		{
			return (GroupIdx < 0) ? TEXT(HAPI_GROUP_MAIN_GEO) : EditableGroupNames[GroupIdx];
		}
	};


	// -------- Retrieve all part data --------
	TArray<FHoudiniMeshPart> Parts;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
		Parts.Add(FHoudiniMeshPart(PartInfo));
	
	AHoudiniNode* Node = GetNode();
	NodeId = GeoInfo.nodeId;

	// Output UStaticMesh by default, if some groups has mark as editable, then the meshes in these groups will be UHoudiniMeshComponent
	const bool& bCanHaveEditableOutput = CanHaveEditableOutput();
	bool bPartialUpdate = false;

	bool bIsMainGeoEditable = false;  // If true, means the GroupIdx == -1 is also editable
	HAPI_AttributeInfo AttribInfo;
	for (FHoudiniMeshPart& Part : Parts)
	{
		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;


		// -------- Retrieve attrib and group names --------
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, Part.AttribNames));
		const TArray<std::string>& AttribNames = Part.AttribNames;

		TArray<std::string> PrimGroupNames;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetPrimitiveGroupNames(GeoInfo, PartInfo, PrimGroupNames));


		// -------- Get EditableGroupships for UHoudiniMeshComponent generating --------
		TArray<TArray<int32>> EditableGroupships;
		if (bCanHaveEditableOutput)  // Editable mesh must be UHoudiniMeshComponent
		{
			HapiGetEditableGroups(Part.EditableGroupNames, EditableGroupships, bIsMainGeoEditable,
				NodeId, PartInfo, PrimGroupNames);
		}


		// -------- Retrieve collision and LOD groups --------
		bool bCollisionGroupShipConst = false;
		TArray<int32> CollisionGroupship;
		if (PrimGroupNames.Contains(HAPI_GROUP_COLLISION_GEO))
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetGroupMembership(NodeId, PartId,
				PartInfo.faceCount, PartInfo.isInstanced, HAPI_GROUPTYPE_PRIM, HAPI_GROUP_COLLISION_GEO, CollisionGroupship, bCollisionGroupShipConst));

		TArray<TArray<int32>> LodGroupships;  // Starts with second lod, may be "lod_1"
		HOUDINI_FAIL_RETURN(HapiGetLodGroupShips(NodeId, PartId,
			PrimGroupNames, PartInfo.faceCount, PartInfo.isInstanced, LodGroupships));

		// -------- Retrieve mesh_output_mode ---------
		HAPI_AttributeOwner MeshOutputModeOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_UNREAL_OUTPUT_MESH_TYPE);
		TArray<int8> MeshOutputModes;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
			HAPI_ATTRIB_UNREAL_OUTPUT_MESH_TYPE, GetMeshOutputModeLambda, MeshOutputModes, MeshOutputModeOwner));


		// -------- Retrieve vertex list --------
		Part.Vertices.SetNumUninitialized(PartInfo.vertexCount);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVertexList(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			Part.Vertices.GetData(), 0, PartInfo.vertexCount));
		const TArray<int32>& Vertices = Part.Vertices;


		// -------- Retrieve split values and partial output modes if exists --------
		TArray<int32> SplitKeys;  // Maybe int or HAPI_StringHandle
		HAPI_AttributeOwner SplitAttribOwner = HAPI_ATTROWNER_PRIM;
		TMap<HAPI_StringHandle, FString> SplitValueMap;
		HOUDINI_FAIL_RETURN(FHoudiniOutputUtils::HapiGetSplitValues(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			SplitKeys, SplitValueMap, SplitAttribOwner));
		const bool bHasSplitValues = !SplitKeys.IsEmpty();

		HAPI_AttributeOwner PartialOutputModeOwner = bHasSplitValues ? FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
			PartInfo.attributeCounts, HAPI_ATTRIB_PARTIAL_OUTPUT_MODE) : HAPI_ATTROWNER_INVALID;
		TArray<int8> PartialOutputModes;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
			HAPI_ATTRIB_PARTIAL_OUTPUT_MODE, PartialOutputModes, PartialOutputModeOwner));
		if (!PartialOutputModes.IsEmpty())  // If has i@partial_output_mode attrib, then we should treat packed mesh as normal mesh, because PartId record in unchanged is out-of-data, we can NOT instantiate them
			Part.Info.isInstanced = false;  // As we have already retrieved groups, the HAPI_PartInfo::isInstanced can be set here


		// -------- Split mesh --------
		TMap<int32, FHoudiniMeshTrianglesHolder>& SplitMap = Part.SplitMeshMap;  // SplitValue, <MeshOutputMesh, GroupIdx>, Triangles
		FHoudiniMeshTrianglesHolder AllMesh(HAPI_PARTIAL_OUTPUT_MODE_REPLACE, FString());
		TMap<TPair<int8, int32>, TArray<int32>>& AllGroupTrianglesMap = AllMesh.GroupTrianglesMap;  // Only for "no split values" condition
		for (int32 TriIdx = 0; TriIdx < PartInfo.faceCount; ++TriIdx)
		{
			// Judge PartialOutputMode, if remove && previous NOT set, then we will NOT parse the GroupIdx
			const int32 SplitKey = bHasSplitValues ?
				SplitKeys[MeshAttributeEntryIdx(SplitAttribOwner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices)] : 0;
			FHoudiniMeshTrianglesHolder* FoundHolderPtr = bHasSplitValues ? SplitMap.Find(SplitKey) : nullptr;

			const int8 PartialOutputMode = FMath::Clamp(PartialOutputModes.IsEmpty() ? HAPI_PARTIAL_OUTPUT_MODE_REPLACE :
				PartialOutputModes[MeshAttributeEntryIdx(PartialOutputModeOwner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices)],
				HAPI_PARTIAL_OUTPUT_MODE_REPLACE, HAPI_PARTIAL_OUTPUT_MODE_REMOVE);

			if (PartialOutputMode == HAPI_PARTIAL_OUTPUT_MODE_MODIFY)
				bPartialUpdate = true;
			else if (PartialOutputMode == HAPI_PARTIAL_OUTPUT_MODE_REMOVE)  // If has PartialOutputModes, then must also HasSplitValues
			{
				bPartialUpdate = true;
				if (FoundHolderPtr)  // If previous triangles has defined PartialOutputMode, We should NOT change it
				{
					if (FoundHolderPtr->PartialOutputMode == HAPI_PARTIAL_OUTPUT_MODE_REMOVE)
						continue;
				}
				else
				{
					SplitMap.Add(SplitKey, FHoudiniMeshTrianglesHolder(HAPI_PARTIAL_OUTPUT_MODE_REMOVE, GET_SPLIT_VALUE_STR));
					continue;
				}
			}

			// Judge is HoudiniMesh and query EditableGroupIdx
			int8 MeshOutputMode = FMath::Clamp(MeshOutputModes.IsEmpty() ? (bIsMainGeoEditable ? HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH : HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH) :
				MeshOutputModes[MeshAttributeEntryIdx(MeshOutputModeOwner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices)],
				HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH, HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH);  // We should clamp it, to ensure the mode int is limit to 0 - 2

			int32 EditIdx = -1;  // -1 represents "main_geo"
			if (bCanHaveEditableOutput)
			{
				for (int32 EditableGroupIdx = 0; EditableGroupIdx < EditableGroupships.Num(); ++EditableGroupIdx)
				{
					if (EditableGroupships[EditableGroupIdx][TriIdx])
					{
						EditIdx = EditableGroupIdx;
						MeshOutputMode = HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH;  // If this is a solver node, and in editable group, we should force it to generate houdini mesh
						break;
					}
				}
			}

			// Query LodIdx or Collision group
			int32 LodIdx = 0;  // -1 represents collision
			for (int32 LodGroupIdx = 0; LodGroupIdx < LodGroupships.Num(); ++LodGroupIdx)
			{
				if (LodGroupships[LodGroupIdx][TriIdx])
				{
					LodIdx = LodGroupIdx + 1;  // LodGroupships starts from "lod_1"
					break;
				}
			}

			if (LodIdx == 0)  // Means this face is not in any LOD Group
			{
				if (CollisionGroupship.IsValidIndex(TriIdx) && CollisionGroupship[TriIdx])
					LodIdx = -1;  // -1 represents collision
			}


			if (MeshOutputMode == HAPI_UNREAL_OUTPUT_MESH_TYPE_DYNAMICMESH)
			{
				if (LodIdx >= 1)  // DynamicMesh does NOT support LODs
					continue;
			}
			else if (MeshOutputMode == HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH)  // HAPI_MESH_OUTPUT_MODE_HOUDINIMESH
			{
				if (LodIdx != 0)  // HoudiniMesh does NOT support LODs and collision
					continue;
			}

			// Finally, add triangle index to the specify group
			const TPair<int8, int32> GroupIdentifier(MeshOutputMode, MeshOutputMode == HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH ? EditIdx : LodIdx);
			if (bHasSplitValues)  // Split mesh by SplitValueSHs
			{
				if (!FoundHolderPtr)
					FoundHolderPtr = &SplitMap.Add(SplitKey, FHoudiniMeshTrianglesHolder(PartialOutputMode, GET_SPLIT_VALUE_STR));
				FoundHolderPtr->GroupTrianglesMap.FindOrAdd(GroupIdentifier).Add(TriIdx);
			}
			else
				AllGroupTrianglesMap.FindOrAdd(GroupIdentifier).Add(TriIdx);
		}

		if (!bHasSplitValues)
			SplitMap.Add(0, AllMesh);  // We just add AllLodTrianglesMap to SplitMeshMap
	}


	// -------- Update output holders --------
	TDoubleLinkedList<FHoudiniStaticMeshOutput*> OldStaticMeshOutputs;
	TArray<FHoudiniStaticMeshOutput> NewStaticMeshOutputs;
	TDoubleLinkedList<FHoudiniDynamicMeshOutput*> OldDynamicMeshOutputs;
	TArray<FHoudiniDynamicMeshOutput> NewDynamicMeshOutputs;
	TDoubleLinkedList<FHoudiniMeshOutput*> OldHoudiniMeshOutputs;
	TArray<FHoudiniMeshOutput> NewHoudiniMeshOutputs;

	if (bPartialUpdate)
	{
		TSet<FString> ModifySplitValues;
		TSet<FString> RemoveSplitValues;
		for (FHoudiniMeshPart& Part : Parts)
		{
			for (TMap<int32, FHoudiniMeshTrianglesHolder>::TIterator SplitIter(Part.SplitMeshMap); SplitIter; ++SplitIter)
			{
				if (SplitIter->Value.PartialOutputMode >= HAPI_PARTIAL_OUTPUT_MODE_REMOVE)
				{
					RemoveSplitValues.FindOrAdd(SplitIter->Value.SplitValue);
					SplitIter.RemoveCurrent();
				}
				else
					ModifySplitValues.FindOrAdd(SplitIter->Value.SplitValue);
			}
		}

		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, StaticMeshOutputs,
			[Node](const FHoudiniStaticMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldStaticMeshOutputs, NewStaticMeshOutputs);
		for (FHoudiniStaticMeshOutput& NewSMOutput : NewStaticMeshOutputs)
		{
			// If bPartialUpdate, then we must treat packed mesh as normal mesh, because PartId record in unchanged holder is out-of-data, we can NOT instantiate them
			if (NewSMOutput.IsInstanced())
				NewSMOutput.Commit(GetNode(), -1, NewSMOutput.GetSplitValue(), NewSMOutput.IsSplitActor());
		}

		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, DynamicMeshOutputs,
			[Node](const FHoudiniDynamicMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldDynamicMeshOutputs, NewDynamicMeshOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, HoudiniMeshOutputs,
			[Node](const FHoudiniMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldHoudiniMeshOutputs, NewHoudiniMeshOutputs);
	}
	else
	{
		// Collect valid old output holders for reuse
		FHoudiniOutputUtils::UpdateOutputHolders(StaticMeshOutputs,
			[Node](const FHoudiniStaticMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldStaticMeshOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(DynamicMeshOutputs,
			[Node](const FHoudiniDynamicMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldDynamicMeshOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(HoudiniMeshOutputs,
			[Node](const FHoudiniMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldHoudiniMeshOutputs);
	}
	
	// -------- Update mesh data --------
	TMap<TPair<UMaterialInterface*, uint32>, UMaterialInstance*> MatParmMap;  // Use to find created MaterialInstance quickly
	TMap<AActor*, TArray<FString>> ActorPropertyNamesMap;  // Use to avoid Set the same property in same SplitActor twice
	for (const FHoudiniMeshPart& Part : Parts)
	{
		if (Part.SplitMeshMap.IsEmpty())
			continue;

		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;
		const TArray<std::string>& AttribNames = Part.AttribNames;
		const TArray<int32>& Vertices = Part.Vertices;

		// -------- Retrieve mesh data --------
		TArray<float> PositionData;
		PositionData.SetNumUninitialized(PartInfo.pointCount * 3);

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_POSITION, HAPI_ATTROWNER_POINT, &AttribInfo));

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_POSITION, &AttribInfo, -1, PositionData.GetData(), 0, PartInfo.pointCount));

		HAPI_AttributeOwner NormalOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_NORMAL);
		TArray<float> NormalData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_NORMAL, 3, NormalData, NormalOwner));

		HAPI_AttributeOwner TangentUOwner = NormalData.IsEmpty() ? HAPI_ATTROWNER_INVALID :  // Must have "N", then we could have "tangentu"
			FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_TANGENT);
		TArray<float> TangentUData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_TANGENT, 3, TangentUData, TangentUOwner));

		HAPI_AttributeOwner TangentVOwner = TangentUData.IsEmpty() ? HAPI_ATTROWNER_INVALID :  // Must have "N" and "tangentu", then we could have "tangentv"
			FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_TANGENT2);
		TArray<float> TangentVData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_TANGENT2, 3, TangentVData, TangentVOwner));

		HAPI_AttributeOwner ColorOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_COLOR);
		TArray<float> ColorData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_COLOR, 3, ColorData, ColorOwner));

		HAPI_AttributeOwner AlphaOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ALPHA);
		TArray<float> AlphaData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ALPHA, 1, AlphaData, AlphaOwner));
		
		const bool bHasColorAttrib = !ColorData.IsEmpty() || !AlphaData.IsEmpty();

		TArray<HAPI_AttributeOwner> UVOwners;
		TArray<TArray<float>> UVsData;
		HOUDINI_FAIL_RETURN(HapiGetUVAttributeData(UVsData, UVOwners, NodeId, PartId, AttribNames, PartInfo.attributeCounts));

		// Material
		TArray<UMaterialInterface*> MatInsts;  // Material instance superiority
		HAPI_AttributeOwner MatInstOwner = HAPI_ATTROWNER_INVALID;
		TArray<TSharedPtr<FHoudiniAttribute>> MatAttribs;
		TArray<UMaterialInterface*> Mats;  // If invalid, then fallback to use material attribute
		HAPI_AttributeOwner MatOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_FAIL_RETURN(HapiRetrieveMeshMaterialData(MatInsts, MatInstOwner, MatAttribs,
			Mats, MatOwner, Vertices, NodeId, PartInfo, AttribNames));

		// StaticMesh asset path
		HAPI_AttributeOwner ObjectPathOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OBJECT_PATH);
		TArray<HAPI_StringHandle> ObjectPathSHs;
		TMap<HAPI_StringHandle, FString> SHObjectPathMap;
		if (ObjectPathOwner != HAPI_ATTROWNER_INVALID)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, ObjectPathOwner, &AttribInfo));

			if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
			{
				ObjectPathSHs.SetNumUninitialized(AttribInfo.count);
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_ATTRIB_UNREAL_OBJECT_PATH, &AttribInfo, ObjectPathSHs.GetData(), 0, AttribInfo.count));
				const TArray<HAPI_StringHandle> UniqueObjectPathSHs = TSet<HAPI_StringHandle>(ObjectPathSHs).Array();
				TArray<FString> UniqueObjectPaths;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueObjectPathSHs, UniqueObjectPaths));
				for (int32 UniqueIdx = 0; UniqueIdx < UniqueObjectPathSHs.Num(); ++UniqueIdx)
					SHObjectPathMap.Add(UniqueObjectPathSHs[UniqueIdx], UniqueObjectPaths[UniqueIdx]);
			}
			else
				ObjectPathOwner = HAPI_ATTROWNER_INVALID;
		}


		// -------- Static Mesh Attributes ---------
		HAPI_AttributeOwner NaniteEnableOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_NANITE_ENABLED);
		TArray<int8> bNaniteEnables;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId, HAPI_ATTRIB_UNREAL_NANITE_ENABLED,
			bNaniteEnables, NaniteEnableOwner));


		// -------- Common Attributes --------
		HAPI_AttributeOwner SplitActorsOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_SPLIT_ACTORS);
		TArray<int8> bSplitActors;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId, HAPI_ATTRIB_UNREAL_SPLIT_ACTORS,
			bSplitActors, SplitActorsOwner));

		// Retrieve UProperties
		TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

		// Retrieve attribs for HoudiniMesh
		TArray<int32> PrimIds;  // Only For HoudiniMesh to Retrieve polygons and edges, so Retrieve data Only when HoudiniMesh found
		TArray<UHoudiniParameterAttribute*> EditPointParmAttribs;
		TArray<UHoudiniParameterAttribute*> EditPrimParmAttribs;


		// -------- Create or update StaticMesh, DynamicMesh, or HoudiniMesh --------
		for (const auto& SplitMesh : Part.SplitMeshMap)
		{
			const FString& SplitValue = SplitMesh.Value.SplitValue;

			// -------- Parse Mesh Output Settings --------
			// For StaticMesh
			TArray<int32> StaticMeshLodIndices;
			bool bHasCollisionStaticMesh = false;  // If only has collision mesh, we just create One mesh

			// For DynamicMesh
			bool bHasDisplayDynamicMesh = false;
			bool bHasCollisionDynamicMesh = false;

			// For HoudiniMesh
			TArray<int32> HoudiniMeshGroupIndices;

			for (const auto& GroupTriangles : SplitMesh.Value.GroupTrianglesMap)
			{
				const int8& MeshOutputMode = GroupTriangles.Key.Key;
				if (MeshOutputMode == HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH)  // Is StaticMesh
				{
					if (GroupTriangles.Key.Value >= 0)
						StaticMeshLodIndices.Add(GroupTriangles.Key.Value);
					else
						bHasCollisionStaticMesh = true;
				}
				else if (MeshOutputMode == HAPI_UNREAL_OUTPUT_MESH_TYPE_DYNAMICMESH)  // Is DynamicMesh
				{
					if (GroupTriangles.Key.Value >= 0)  // Lods should have been Blast when Retrieve SplitMeshMap
						bHasDisplayDynamicMesh = true;
					else
						bHasCollisionDynamicMesh = true;
				}
				else  // Is HoudiniMesh
					HoudiniMeshGroupIndices.Add(GroupTriangles.Key.Value);  // Lods and collision should have been Blast when Retrieve SplitMeshMap
			}
			const bool bHasDisplayStaticMesh = !StaticMeshLodIndices.IsEmpty();
			if (bHasDisplayStaticMesh)
				StaticMeshLodIndices.Sort();


			// StaticMesh
			if (bHasDisplayStaticMesh || bHasCollisionStaticMesh)
			{
				// -------- Retrieve main StaticMesh asset path --------
				FString MainStaticMeshPath;
				const int32 MainTriangleIdx =  // Will Retrieve either in DisplayStaticMesh or CollisionStaticMesh, so it will must be Retrieved (>= 0)
					SplitMesh.Value.GroupTrianglesMap[TPair<int8, int32>(HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH,
						bHasDisplayStaticMesh ? StaticMeshLodIndices[0] : -1)][0];
				
				if (!ObjectPathSHs.IsEmpty())
				{
					// Use the first triangle's s@unreal_object_path
					const int32 ObjectPathDataIdx = MeshAttributeEntryIdx(ObjectPathOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices);
					MainStaticMeshPath = SHObjectPathMap[ObjectPathSHs[ObjectPathDataIdx]];
				}

				if (!IS_ASSET_PATH_INVALID(MainStaticMeshPath))
					MainStaticMeshPath = FHoudiniEngineUtils::GetPackagePath(MainStaticMeshPath);

				if (IS_ASSET_PATH_INVALID(MainStaticMeshPath))  // If Path is Invalid, we should fallback to use node's CookFolder
					MainStaticMeshPath = GetStaticMeshAssetPath(GetNode()->GetCookFolderPath(), Name, SplitValue, PartId);

				// -------- Find or create a UStaticMesh display --------
				UStaticMesh* SM = FHoudiniEngineUtils::FindOrCreateAsset<UStaticMesh>(MainStaticMeshPath);
				if (bHasDisplayStaticMesh)  // Parse this mesh as display mesh
				{
					FString MeshAssetFolderPath;
					{
						FString MeshAssetName;
						MainStaticMeshPath.Split(TEXT("/"), &MeshAssetFolderPath, &MeshAssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
						if (MeshAssetFolderPath.IsEmpty())
							MeshAssetFolderPath = Node->GetCookFolderPath();
						else
							MeshAssetFolderPath += TEXT("/");
					}

					TArray<FStaticMaterial>& MaterialSlots = SM->GetStaticMaterials();
					MaterialSlots.Empty();  // Reset materials
					FMeshSectionInfoMap& SectionInfoMap = SM->GetSectionInfoMap();
					SectionInfoMap.Clear();

					int32 SourceModelIdx = 0;
					for (const int32& LodIdx : StaticMeshLodIndices)  // Some LOD may disappear in this piece, as we could split mesh by attribute
					{
						FStaticMeshSourceModel* SourceModelPtr = SM->IsSourceModelValid(SourceModelIdx) ? &SM->GetSourceModel(SourceModelIdx) : &SM->AddSourceModel();
						FMeshDescription* MeshDesc = SourceModelPtr->CreateMeshDescription();

						const TArray<int32>& Triangles = SplitMesh.Value.GroupTrianglesMap[TPair<int8, int32>(HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH, LodIdx)];  // These TriangleIndices is in global

						MeshDesc->ReserveNewTriangles(Triangles.Num());
						MeshDesc->ReserveNewVertexInstances(Triangles.Num() * 3);
						MeshDesc->SetNumUVChannels(UVOwners.Num());
						TVertexAttributesRef<FVector3f> SMPositionAttrib = MeshDesc->GetVertexPositions();

						FStaticMeshAttributes Attributes(*MeshDesc);
						Attributes.Register();
						TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
						TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
						TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
						TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
						TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
						VertexInstanceUVs.SetNumChannels(FMath::Max(UVOwners.Num(), 1));  // Must have one uv channel
						TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
							MeshDesc->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

						TMap<int32, FVertexID> PointIdxMap;  // Global PointIdx map tp local VertexID
						TMap<UMaterialInterface*, FPolygonGroupID> MaterialPolygonGroupMap;
						for (const int32& TriIdx : Triangles)
						{
							const FIntVector3 HoudiniPoints(Vertices[TriIdx * 3], Vertices[TriIdx * 3 + 1], Vertices[TriIdx * 3 + 2]);
							if ((HoudiniPoints.X == HoudiniPoints.Y) || (HoudiniPoints.Y == HoudiniPoints.Z) || (HoudiniPoints.Z == HoudiniPoints.X))  // Skip degenerated triangle
								continue;

							TArray<FVertexInstanceID> TriVtxIndices;
							TriVtxIndices.SetNumUninitialized(3);
							for (int32 TriVtxIdx = 2; TriVtxIdx >= 0; --TriVtxIdx)  // Need reverse vertex order
							{
								const int32& PointIdx = HoudiniPoints[TriVtxIdx];  // This PointIdx is in global
								FVertexID VertexID;
								{
									FVertexID* FoundVertexIDPtr = PointIdxMap.Find(PointIdx);
									if (!FoundVertexIDPtr)
									{
										VertexID = MeshDesc->CreateVertex();
										SMPositionAttrib[VertexID] = POSITION_SCALE_TO_UNREAL_F *
											FVector3f(PositionData[PointIdx * 3], PositionData[PointIdx * 3 + 2], PositionData[PointIdx * 3 + 1]);
										PointIdxMap.Add(PointIdx, VertexID);
									}
									else
										VertexID = *FoundVertexIDPtr;
								}

								const FVertexInstanceID VtxInstID = MeshDesc->CreateVertexInstance(VertexID);
								TriVtxIndices[2 - TriVtxIdx] = VtxInstID;

								// Mesh VertexInstance Attributes
								const int32 GlobalVtxIdx = TriIdx * 3 + TriVtxIdx;
								if (!NormalData.IsEmpty())
								{
									const int32 NormalDataIdx = MeshAttributeEntryIdx(NormalOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
									const FVector3f Normal(NormalData[NormalDataIdx], NormalData[NormalDataIdx + 2], NormalData[NormalDataIdx + 1]);
									VertexInstanceNormals[VtxInstID] = Normal;

									if (!TangentUData.IsEmpty())  // Must have "N", then we could have "tangentu"
									{
										const int32 TangentUDataIdx = MeshAttributeEntryIdx(TangentUOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
										const FVector3f TangentU(TangentUData[TangentUDataIdx], TangentUData[TangentUDataIdx + 2], TangentUData[TangentUDataIdx + 1]);
										VertexInstanceTangents[VtxInstID] = TangentU;

										if (!TangentVData.IsEmpty())  // Must have "N" and "tangentu", then we could have "tangentv"
										{
											const int32 TangentVDataIdx = MeshAttributeEntryIdx(TangentVOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
											const FVector3f TangentV(TangentVData[TangentVDataIdx], TangentVData[TangentVDataIdx + 2], TangentVData[TangentVDataIdx + 1]);
											VertexInstanceBinormalSigns[VtxInstID] = GetBasisDeterminantSign((FVector)TangentU, (FVector)TangentV, (FVector)Normal);
										}
										else
											VertexInstanceBinormalSigns[VtxInstID] = 1.0f;
									}
								}

								if (bHasColorAttrib)
								{
									FVector4f Color(1.0f, 1.0f, 1.0f, 1.0f);
									if (!ColorData.IsEmpty())
									{
										const int32 ColorDataIdx = MeshAttributeEntryIdx(ColorOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
										Color.X = ColorData[ColorDataIdx];
										Color.Y = ColorData[ColorDataIdx + 1];
										Color.Z = ColorData[ColorDataIdx + 2];
									}

									if (!AlphaData.IsEmpty())
										Color.W = AlphaData[MeshAttributeEntryIdx(AlphaOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices)];

									VertexInstanceColors[VtxInstID] = Color;
								}

								for (int32 UVIdx = 0; UVIdx < UVOwners.Num(); ++UVIdx)
								{
									const int32 UVDataIdx = MeshAttributeEntryIdx(UVOwners[UVIdx], GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 2;
									const TArray<float>& UVData = UVsData[UVIdx];
									const FVector2f UV(UVData[UVDataIdx], 1.0f - UVData[UVDataIdx + 1]);
									VertexInstanceUVs.Set(VtxInstID, UVIdx, UV);
								}
							}

							// Parse material/material instance
							bool bIsMaterialInstance = false;
							UMaterialInterface* Material = GetMeshMaterial(Mats, MatOwner, MatInsts, MatInstOwner, TriIdx, Vertices, !AlphaData.IsEmpty(), bIsMaterialInstance);

							FPolygonGroupID PolygonGroupID;
							if (FPolygonGroupID* FoundPolygonGroupIDPtr = MaterialPolygonGroupMap.Find(Material))  // Just using raw Material to find PolygonGroupID, do NOT use FinalMaterial
								PolygonGroupID = *FoundPolygonGroupIDPtr;
							else
							{
								UMaterialInterface* FinalMaterial = bIsMaterialInstance ? FHoudiniOutputUtils::GetMaterialInstance(Material, MatAttribs,
									[&](const HAPI_AttributeOwner& Owner) { return MeshAttributeEntryIdx(Owner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices); },
									MeshAssetFolderPath, MatParmMap) : Material;

								PolygonGroupID = MeshDesc->CreatePolygonGroup();
								const int32 FoundMaterialSlotIdx = MaterialSlots.IndexOfByPredicate(
									[FinalMaterial](const FStaticMaterial& StaticMaterial) { return StaticMaterial.MaterialInterface == FinalMaterial; });

								PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = MaterialSlots.IsValidIndex(FoundMaterialSlotIdx) ?
									MaterialSlots[FoundMaterialSlotIdx].ImportedMaterialSlotName : SM->AddMaterial(FinalMaterial);  // If not found, create a new slot

								// This code block works around a bug in UE when we build meshes from FMeshDescription structures. The FMeshDescription
								// structures are built correctly (one per LOD). UE then creates one mesh section for each material in each LOD and
								// fills in the correct Material Id. So far so good.
								//
								// However, UE then calls the function FStaticMeshRenderData::ResolveSectionInfo() which then overwrites this 
								// material Id with one store inthe Section Info Map:
								//
								//			FMeshSectionInfo Info = Owner->GetSectionInfoMap().Get(LODIndex, SectionIndex);
								//			...
								//			Section.MaterialIndex = Info.MaterialIndex;
								//
								// The problem is that there is no Section Info Map, so FMeshSectionInfoMap::Get() creates one, but the one created is not
								// // correct - if FMeshSectionInfoMap::Get(LodIndex,...) is called, and no section map exists for LodIndex, it uses LODIndex
								// of 0. This causes us to end up with the wrong materials on the LODs.
								//
								// So the work around is to crib the code that creates the sections (BuildVertexBuffer inside StaticMeshBuilder.cpp) and
								// create a correct Section info Map before building the Static Mesh; then everything works.
								SectionInfoMap.Set(SourceModelIdx, PolygonGroupID.GetValue(),
									FMeshSectionInfo(MaterialSlots.IsValidIndex(FoundMaterialSlotIdx) ? FoundMaterialSlotIdx : (MaterialSlots.Num() - 1)));

								MaterialPolygonGroupMap.Add(Material, PolygonGroupID);  // Just using raw Material to mark PolygonGroupID, do NOT use FinalMaterial
							}

							MeshDesc->CreateTriangle(PolygonGroupID, TriVtxIndices);
						}



						SourceModelPtr->BuildSettings.bRecomputeNormals = NormalData.IsEmpty();
						SourceModelPtr->BuildSettings.bRecomputeTangents = TangentUData.IsEmpty();
						SourceModelPtr->BuildSettings.bGenerateLightmapUVs = UVOwners.IsEmpty();

						for (const auto& PropAttrib : PropAttribs) // Set uproperty on SourceModel
						{
							const HAPI_AttributeOwner PropAttribOwner = (HAPI_AttributeOwner)PropAttrib->GetOwner();
							PropAttrib->SetStructPropertyValues(SourceModelPtr, SourceModelPtr->StaticStruct(),
								MeshAttributeEntryIdx(PropAttribOwner, Triangles[0], HAPI_ATTROWNER_PRIM, Vertices));
						}

						SM->CommitMeshDescription(SourceModelIdx);

						++SourceModelIdx;
					}

					// Remove useless SourceModels
					for (int32 LodIdx = SM->GetNumSourceModels() - 1; LodIdx >= SourceModelIdx; --LodIdx)
						SM->RemoveSourceModel(LodIdx);

					SM->SetLightMapCoordinateIndex(UVOwners.Num() >= 2 ? 1 : 0);

					SET_OBJECT_UPROPERTIES(SM, MeshAttributeEntryIdx(PropAttribOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices));

					if (!bNaniteEnables.IsEmpty())
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
						SM->GetNaniteSettings().bEnabled = bNaniteEnables[MeshAttributeEntryIdx(NaniteEnableOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices)];
#else
						SM->NaniteSettings.bEnabled = bNaniteEnables[MeshAttributeEntryIdx(NaniteEnableOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices)];
#endif
					// Build this SM After collision set
				}


				// -------- Find or create a UStaticMesh for collision --------
				if (bHasCollisionStaticMesh)
				{
					// Try to find the first triangle index of this mesh
					const int32 CollisionTriangleIdx = bHasDisplayStaticMesh ?
						SplitMesh.Value.GroupTrianglesMap[TPair<int8, int32>(HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH, -1)][0] : MainTriangleIdx;

					UStaticMesh* CollisionSM = nullptr;  // If there are NO DisplayStaticMesh, then collision will be MainStaticMeshPath
					if (bHasDisplayStaticMesh)
					{
						FString CollisionStaticMeshPath;
						if (!ObjectPathSHs.IsEmpty())
						{
							// use this triangle's s@unreal_object_path
							const int32 ObjectPathDataIdx = MeshAttributeEntryIdx(ObjectPathOwner, CollisionTriangleIdx, HAPI_ATTROWNER_VERTEX, Vertices);
							CollisionStaticMeshPath = SHObjectPathMap[ObjectPathSHs[ObjectPathDataIdx]];
						}
						else
							CollisionStaticMeshPath = MainStaticMeshPath;

						if (!IS_ASSET_PATH_INVALID(CollisionStaticMeshPath))
							CollisionStaticMeshPath = FHoudiniEngineUtils::GetPackagePath(CollisionStaticMeshPath);

						if (IS_ASSET_PATH_INVALID(CollisionStaticMeshPath))  // If Path is Invalid, we should fallback to use node's CookFolder
							CollisionStaticMeshPath = GetStaticMeshAssetPath(GetNode()->GetCookFolderPath(), Name, SplitValue, PartId);

						if (CollisionStaticMeshPath == MainStaticMeshPath)  // Avoid asset name collision
							CollisionStaticMeshPath += TEXT("_ComplexCollision");  // to add collision suffix

						CollisionSM = FHoudiniEngineUtils::FindOrCreateAsset<UStaticMesh>(CollisionStaticMeshPath);
					}
					else
					{
						if (!IsValid(SM))
							SM = FHoudiniEngineUtils::FindOrCreateAsset<UStaticMesh>(MainStaticMeshPath);

						CollisionSM = SM;
					}

					// Remove useless SourceModels
					for (int32 SourceModelIdx = CollisionSM->GetNumSourceModels() - 1; SourceModelIdx >= 1; --SourceModelIdx)
						CollisionSM->RemoveSourceModel(SourceModelIdx);

					FMeshSectionInfoMap& SectionInfoMap = SM->GetSectionInfoMap();
					SectionInfoMap.Clear();

					FStaticMeshSourceModel* SourceModelPtr = CollisionSM->IsSourceModelValid(0) ? &CollisionSM->GetSourceModel(0) : &CollisionSM->AddSourceModel();
					FMeshDescription* MeshDesc = SourceModelPtr->CreateMeshDescription();

					const TArray<int32>& Triangles = SplitMesh.Value.GroupTrianglesMap[TPair<int8, int32>(HAPI_UNREAL_OUTPUT_MESH_TYPE_STATICMESH, -1)];  // These TriangleIndices is in global
					
					MeshDesc->ReserveNewTriangles(Triangles.Num());
					MeshDesc->ReserveNewVertexInstances(Triangles.Num() * 3);
					MeshDesc->SetNumUVChannels(0);
					TVertexAttributesRef<FVector3f> SMPositionAttrib = MeshDesc->GetVertexPositions();

					const FPolygonGroupID PolygonGroupID = MeshDesc->CreatePolygonGroup();

					TMap<int32, FVertexID> PointIdxMap;  // Global PointIdx map tp local VertexID
					for (const int32& TriIdx : Triangles)
					{
						const FIntVector3 HoudiniPoints(Vertices[TriIdx * 3], Vertices[TriIdx * 3 + 1], Vertices[TriIdx * 3 + 2]);
						if ((HoudiniPoints.X == HoudiniPoints.Y) || (HoudiniPoints.Y == HoudiniPoints.Z) || (HoudiniPoints.Z == HoudiniPoints.X))  // Skip degenerated triangle
							continue;

						TArray<FVertexInstanceID> TriVtxIndices;
						TriVtxIndices.SetNumUninitialized(3);
						for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
						{
							const int32& PointIdx = HoudiniPoints[TriVtxIdx];  // This PointIdx is in global
							FVertexID VertexID;
							{
								FVertexID* FoundVertexIDPtr = PointIdxMap.Find(PointIdx);
								if (!FoundVertexIDPtr)
								{
									VertexID = MeshDesc->CreateVertex();
									SMPositionAttrib[VertexID] = POSITION_SCALE_TO_UNREAL_F *
										FVector3f(PositionData[PointIdx * 3], PositionData[PointIdx * 3 + 2], PositionData[PointIdx * 3 + 1]);
									PointIdxMap.Add(PointIdx, VertexID);
								}
								else
									VertexID = *FoundVertexIDPtr;
							}

							TriVtxIndices[2 - TriVtxIdx] = MeshDesc->CreateVertexInstance(VertexID);
						}

						MeshDesc->CreateTriangle(PolygonGroupID, TriVtxIndices);
					}

					SectionInfoMap.Set(0, PolygonGroupID.GetValue(), FMeshSectionInfo(0));  // Force to set SectionInfo to avoid ensure failed for GetSectionInfoMap when UBodySetup check contains collision

					SourceModelPtr->BuildSettings.bRecomputeNormals = true;
					SourceModelPtr->BuildSettings.bRecomputeTangents = true;
					SourceModelPtr->BuildSettings.bGenerateLightmapUVs = false;

					CollisionSM->CommitMeshDescription(0);

					SET_OBJECT_UPROPERTIES(CollisionSM, MeshAttributeEntryIdx(PropAttribOwner, CollisionTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices));

					CollisionSM->Build(true);
					
					CollisionSM->Modify();
					
					FStaticMeshCompilingManager::Get().FinishCompilation(TArray<UStaticMesh*>{ CollisionSM });  // Avoid ensure failed for GetSectionInfoMap when UBodySetup check contains collision

					// Setup collision
					SM->ComplexCollisionMesh = CollisionSM;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
					SM->SetCustomizedCollision(true);
#else
					SM->bCustomizedCollision = true;
#endif
					UBodySetup* BodySetup = SM->GetBodySetup();
					if (!BodySetup)
					{
						SM->CreateBodySetup();
						BodySetup = SM->GetBodySetup();
					}

					BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
				}
				else if (bHasDisplayStaticMesh)  // If not has collision mesh, the we should recover the collision setting to default
				{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
					const bool bPrevCustomCollision = SM->GetCustomizedCollision();
#else
					const bool bPrevCustomCollision = SM->bCustomizedCollision;
#endif

					SM->ComplexCollisionMesh = nullptr;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
					SM->SetCustomizedCollision(false);
#else
					SM->bCustomizedCollision = false;
#endif
					if (bPrevCustomCollision)
					{
						UBodySetup* BodySetup = SM->GetBodySetup();
						if (IsValid(BodySetup) && (BodySetup->CollisionTraceFlag != CTF_UseDefault))
						{
							BodySetup->CollisionTraceFlag = CTF_UseDefault;

							BodySetup->InvalidatePhysicsData();
							BodySetup->CreatePhysicsMeshes();
						}
						UE_LOG(LogHoudiniEngine, Log, TEXT("Recover collision settings on %s"), *(SM->GetName()))
					}
				}
				
				if (SM)  // Build after collision mesh set, to avoid build error
				{
					bool bHasBodySetupProperties = false;  // Check whether UBodySetup should be set
					for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
					{
						TSharedPtr<FEditPropertyChain> PropertyChain;
						size_t Offset = 0;
						if (FHoudiniAttribute::FindProperty(UBodySetup::StaticClass(), PropAttrib->GetAttributeName(), PropertyChain, Offset, false))
						{
							bHasBodySetupProperties = true;
							break;
						}
					}

					if (bHasBodySetupProperties)  // Need find or create a BodySetup
					{
						UBodySetup* BodySetup = SM->GetBodySetup();
						if (!BodySetup)
						{
							SM->CreateBodySetup();
							BodySetup = SM->GetBodySetup();
						}

						SET_OBJECT_UPROPERTIES(BodySetup, MeshAttributeEntryIdx(PropAttribOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices));
					}

					FHoudiniMeshOutputBuilder::OnStaticMeshBuild();

					SM->Build(true);

					SM->Modify();
				}

				bool bSplitActor = false;
				if (!bSplitActors.IsEmpty())
					bSplitActor = bSplitActors[MeshAttributeEntryIdx(SplitActorsOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices)] >= 1;

				// -------- Find or create a FHoudiniStaticMeshOutput --------
				FHoudiniStaticMeshOutput NewSMOutput;
				if (FHoudiniStaticMeshOutput* FoundSMOutput = FHoudiniOutputUtils::FindOutputHolder(OldStaticMeshOutputs,
					[&](const FHoudiniStaticMeshOutput* OldSMOutput) { return OldSMOutput->CanReuse(SplitValue, bSplitActor); }))
					NewSMOutput = *FoundSMOutput;

				NewSMOutput.StaticMesh = SM;

				UStaticMeshComponent* SMC = NewSMOutput.Commit(GetNode(), PartInfo.isInstanced ? PartId : -1, SplitValue, bSplitActor);

				// -------- Set UProperties --------
				if (SMC)
				{
					if (SMC->IsPhysicsStateCreated())  // Avoid ensure failed in NavigationSystemV1 check whether StaticMesh has finished compilation
						SMC->RecreatePhysicsState();

					SET_OBJECT_UPROPERTIES(SMC, MeshAttributeEntryIdx(PropAttribOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices));
				}
				SET_SPLIT_ACTOR_UPROPERTIES(NewSMOutput, MeshAttributeEntryIdx(PropAttribOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices), false);

				NewStaticMeshOutputs.Add(NewSMOutput);
			}


			if (bHasDisplayDynamicMesh || bHasCollisionDynamicMesh)
			{
				const TArray<int32>& TriangleIndices = SplitMesh.Value.GroupTrianglesMap[TPair<int8, int32>(HAPI_UNREAL_OUTPUT_MESH_TYPE_DYNAMICMESH, 0)];

				bool bSplitActor = false;
				if (!bSplitActors.IsEmpty())
					bSplitActor = bSplitActors[MeshAttributeEntryIdx(SplitActorsOwner, TriangleIndices[0], HAPI_ATTROWNER_PRIM, Vertices)] >= 1;

				FHoudiniDynamicMeshOutput NewDMOutput;
				if (FHoudiniDynamicMeshOutput* FoundDMOutput = FHoudiniOutputUtils::FindOutputHolder(OldDynamicMeshOutputs,
					[&](const FHoudiniDynamicMeshOutput* OldDMOutput) { return OldDMOutput->CanReuse(SplitValue, bSplitActor); }))
					NewDMOutput = *FoundDMOutput;

				UDynamicMeshComponent* DMC = NewDMOutput.CreateOrUpdate(GetNode(), SplitValue, bSplitActor);
				DMC->BaseMaterials.Empty();
				FDynamicMesh3* DM = DMC->GetMesh();
				DM->Clear();


				DM->EnableAttributes();
				UE::Geometry::FDynamicMeshAttributeSet* AttributeSet = DM->Attributes();
				
				AttributeSet->SetNumNormalLayers((!NormalData.IsEmpty() && !TangentUData.IsEmpty() && !TangentVData.IsEmpty()) ? 3 : 1);
				UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = AttributeSet->PrimaryNormals();
				UE::Geometry::FDynamicMeshNormalOverlay* TangentOverlay = nullptr;
				UE::Geometry::FDynamicMeshNormalOverlay* BiTangentOverlay = nullptr;
				if (!TangentUData.IsEmpty() && !TangentVData.IsEmpty())
				{
					TangentOverlay = AttributeSet->PrimaryTangents();
					BiTangentOverlay = AttributeSet->PrimaryBiTangents();
				}
				
				if (bHasColorAttrib)
					AttributeSet->EnablePrimaryColors();
				UE::Geometry::FDynamicMeshColorOverlay* ColorOverlay = bHasColorAttrib ? AttributeSet->PrimaryColors() : nullptr;
				AttributeSet->EnableMaterialID();
				UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDAttrib = AttributeSet->GetMaterialID();
				
				AttributeSet->SetNumUVLayers(UVsData.Num());
				TArray<UE::Geometry::FDynamicMeshUVOverlay*> UVOverlays;
				for (int32 UVIdx = 0; UVIdx < UVsData.Num(); ++UVIdx)
					UVOverlays.Add(AttributeSet->GetUVLayer(UVIdx));

				TMap<int32, int> PointIdxMap;  // Global PointIdx map tp local
				TMap<UMaterialInterface*, int> MaterialPolygonGroupMap;
				for (const int32& TriIdx : TriangleIndices)
				{
					const FIntVector3 HoudiniPoints(Vertices[TriIdx * 3], Vertices[TriIdx * 3 + 1], Vertices[TriIdx * 3 + 2]);
					if ((HoudiniPoints.X == HoudiniPoints.Y) || (HoudiniPoints.Y == HoudiniPoints.Z) || (HoudiniPoints.Z == HoudiniPoints.X))  // Skip degenerated triangle
						continue;

					FIntVector3 IsNewPoints = FIntVector3::ZeroValue;
					UE::Geometry::FIndex3i Triangle;
					for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
					{
						const int32& GlobalPointIdx = HoudiniPoints[TriVtxIdx];  // This PointIdx is in global
						int LocalPointIdx;
						{
							const int* FoundLocalPointIdxPtr = PointIdxMap.Find(GlobalPointIdx);
							if (!FoundLocalPointIdxPtr)
							{
								LocalPointIdx = DM->AppendVertex(POSITION_SCALE_TO_UNREAL *
									FVector3d(PositionData[GlobalPointIdx * 3], PositionData[GlobalPointIdx * 3 + 2], PositionData[GlobalPointIdx * 3 + 1]));
								PointIdxMap.Add(GlobalPointIdx, LocalPointIdx);
								IsNewPoints[TriVtxIdx] = 1;
							}
							else
								LocalPointIdx = *FoundLocalPointIdxPtr;
						}

						Triangle[2 - TriVtxIdx] = LocalPointIdx;


						//const FVertexInstanceID VtxInstID = MeshDesc->CreateVertexInstance(VertexID);
						//TriVtxIndices[2 - TriVtxIdx] = VtxInstID;

						// Mesh VertexInstance Attributes
						const int32 GlobalVtxIdx = TriIdx * 3 + TriVtxIdx;
						if (!NormalData.IsEmpty())
						{
							const int32 NormalDataIdx = MeshAttributeEntryIdx(NormalOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
							const FVector3f Normal(NormalData[NormalDataIdx], NormalData[NormalDataIdx + 2], NormalData[NormalDataIdx + 1]);
							NormalOverlay->AppendElement(&(Normal.X));

							if (!TangentUData.IsEmpty() && !TangentVData.IsEmpty())  // Must have "N", then we could have "tangentu"
							{
								const int32 TangentUDataIdx = MeshAttributeEntryIdx(TangentUOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
								const FVector3f TangentU(TangentUData[TangentUDataIdx], TangentUData[TangentUDataIdx + 2], TangentUData[TangentUDataIdx + 1]);
								TangentOverlay->AppendElement(&(TangentU.X));
								
								const int32 TangentVDataIdx = MeshAttributeEntryIdx(TangentVOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
								const FVector3f TangentV(TangentVData[TangentVDataIdx], TangentVData[TangentVDataIdx + 2], TangentVData[TangentVDataIdx + 1]);
								BiTangentOverlay->AppendElement(&(TangentV.X));
							}
						}

						if (bHasColorAttrib)
						{
							FVector4f Color(1.0f, 1.0f, 1.0f, 1.0f);
							if (!ColorData.IsEmpty())
							{
								const int32 ColorDataIdx = MeshAttributeEntryIdx(ColorOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
								Color.X = ColorData[ColorDataIdx];
								Color.Y = ColorData[ColorDataIdx + 1];
								Color.Z = ColorData[ColorDataIdx + 2];
							}

							if (!AlphaData.IsEmpty())
								Color.W = AlphaData[MeshAttributeEntryIdx(AlphaOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices)];

							ColorOverlay->AppendElement(&Color.X);
						}

						for (int32 UVIdx = 0; UVIdx < UVOverlays.Num(); ++UVIdx)
						{
							const int32 UVDataIdx = MeshAttributeEntryIdx(UVOwners[UVIdx], GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 2;
							const TArray<float>& UVData = UVsData[UVIdx];
							const FVector2f UV(UVData[UVDataIdx], 1.0f - UVData[UVDataIdx + 1]);
							UVOverlays[UVIdx]->AppendElement(&UV.X);
						}
					}

					// Parse material/material instance
					bool bIsMaterialInstance = false;
					UMaterialInterface* Material = GetMeshMaterial(Mats, MatOwner, MatInsts, MatInstOwner, TriIdx, Vertices, !AlphaData.IsEmpty(), bIsMaterialInstance);
					
					int GroupID = -1;
					if (const int* GroupIDPtr = MaterialPolygonGroupMap.Find(Material))
						GroupID = *GroupIDPtr;
					else
					{
						UMaterialInterface* FinalMaterial = bIsMaterialInstance ? FHoudiniOutputUtils::GetMaterialInstance(Material, MatAttribs,
							[&](const HAPI_AttributeOwner& Owner) { return MeshAttributeEntryIdx(Owner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices); },
							Node->GetCookFolderPath(), MatParmMap) : Material;

						GroupID = DM->AllocateTriangleGroup();
						MaterialPolygonGroupMap.Add(Material, GroupID);
						DMC->BaseMaterials.Add(FinalMaterial);
					}

					int TriangleID = DM->AppendTriangle(Triangle.A, Triangle.B, Triangle.C, GroupID);
					if (TriangleID < 0)  // NonManifold or Duplicate
					{
						// Append unique points
						for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
						{
							if (!IsNewPoints[TriVtxIdx])
							{
								const int32& GlobalPointIdx = HoudiniPoints[TriVtxIdx];  // This PointIdx is in global
								Triangle[2 - TriVtxIdx] = DM->AppendVertex(POSITION_SCALE_TO_UNREAL *
									FVector3d(PositionData[GlobalPointIdx * 3], PositionData[GlobalPointIdx * 3 + 2], PositionData[GlobalPointIdx * 3 + 1]));
							}
						}
						TriangleID = DM->AppendTriangle(Triangle.A, Triangle.B, Triangle.C, GroupID);
					}
					MaterialIDAttrib->SetValue(TriangleID, GroupID);
					
					// Must SetTriangle otherwise attributes will NOT be rendered.
					Triangle[0] = TriangleID * 3 + 2; Triangle[1] = TriangleID * 3 + 1; Triangle[2] = TriangleID * 3;
					if (ColorOverlay)
						ColorOverlay->SetTriangle(TriangleID, Triangle);
					if (!NormalData.IsEmpty())
					{
						NormalOverlay->SetTriangle(TriangleID, Triangle);
						if (TangentOverlay)
							TangentOverlay->SetTriangle(TriangleID, Triangle);
						if (BiTangentOverlay)
							BiTangentOverlay->SetTriangle(TriangleID, Triangle);
					}
					for (UE::Geometry::FDynamicMeshUVOverlay* UVOverlay : UVOverlays)
						UVOverlay->SetTriangle(TriangleID, Triangle);
				}
				
				if (NormalData.IsEmpty())  // If No normal data, then calculate it
				{
					TArray<FVector3f> Normals;
					Normals.SetNumZeroed(DM->VertexCount());
					TArray<float> Weights;
					Weights.SetNumZeroed(Normals.Num());
					for (int TriIdx = 0; TriIdx < DM->TriangleCount(); ++TriIdx)
					{
						const UE::Geometry::FIndex3i Triangle = DM->GetTriangle(TriIdx);
						const FVector3f V0 = FVector3f(DM->GetVertexRef(Triangle.A));
						const FVector3f V1 = FVector3f(DM->GetVertexRef(Triangle.B));
						const FVector3f V2 = FVector3f(DM->GetVertexRef(Triangle.C));

						const FVector3f Normal = FVector3f::CrossProduct((V2 - V0) * POSITION_SCALE_TO_HOUDINI_F, (V1 - V0) * POSITION_SCALE_TO_HOUDINI_F);
						const float Area = Normal.Size();

						{
							const float Weight = Area * TriangleUtilities::ComputeTriangleCornerAngle(V0, V1, V2);
							Normals[Triangle.A] += Normal * Weight;
							Weights[Triangle.A] += Weight;
						}
						{
							const float Weight = Area * TriangleUtilities::ComputeTriangleCornerAngle(V1, V2, V0);
							Normals[Triangle.B] += Normal * Weight;
							Weights[Triangle.B] += Weight;
						}
						{
							const float Weight = Area * TriangleUtilities::ComputeTriangleCornerAngle(V2, V0, V1);
							Normals[Triangle.C] += Normal * Weight;
							Weights[Triangle.C] += Weight;
						}
					}

					for (int32 PointIdx = 0; PointIdx < Normals.Num(); ++PointIdx)
					{
						FVector3f& Normal = Normals[PointIdx];
						Normal /= Weights[PointIdx];
						Normal.Normalize();
						NormalOverlay->AppendElement(&Normal.X);
					}

					for (int TriIdx = 0; TriIdx < DM->TriangleCount(); ++TriIdx)
						NormalOverlay->SetTriangle(TriIdx, DM->GetTriangle(TriIdx));
				}

				DMC->SetTangentsType(TangentOverlay ? EDynamicMeshComponentTangentsMode::ExternallyProvided : EDynamicMeshComponentTangentsMode::AutoCalculated);
				DMC->GetDynamicMesh()->Modify();
				DMC->NotifyMeshUpdated();
				DMC->Modify();

				const int32& MainTriangleIdx = TriangleIndices[0];
				SET_OBJECT_UPROPERTIES(DMC, MeshAttributeEntryIdx(PropAttribOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices));
				SET_SPLIT_ACTOR_UPROPERTIES(NewDMOutput, MeshAttributeEntryIdx(PropAttribOwner, MainTriangleIdx, HAPI_ATTROWNER_PRIM, Vertices), false);

				NewDynamicMeshOutputs.Add(NewDMOutput);
			}


			// HoudiniMesh
			for (const int32& GroupIdx : HoudiniMeshGroupIndices)
			{
				if (PrimIds.IsEmpty() && (GroupIdx >= 0 || bIsMainGeoEditable))  // Has editable mesh output, we should build poly
				{
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_PRIMITIVE_ID, HAPI_ATTROWNER_PRIM, &AttribInfo));

					PrimIds.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_PRIMITIVE_ID, &AttribInfo, 1, PrimIds.GetData(), 0, AttribInfo.count));
				}

				const TArray<int32>& TriangleIndices = SplitMesh.Value.GroupTrianglesMap[TPair<int8, int32>(HAPI_UNREAL_OUTPUT_MESH_TYPE_HOUDINIMESH, GroupIdx)];  // These TriangleIndices is in global

				bool bSplitActor = false;
				if (!bSplitActors.IsEmpty())
					bSplitActor = bSplitActors[MeshAttributeEntryIdx(SplitActorsOwner, TriangleIndices[0], HAPI_ATTROWNER_PRIM, Vertices)] >= 1;
				
				// -------- Find or create a FHoudiniMeshOutput --------
				const FString GroupName = Part.EditableGroupNames.IsValidIndex(GroupIdx) ? Part.EditableGroupNames[GroupIdx] : TEXT(HAPI_GROUP_MAIN_GEO);
				FHoudiniMeshOutput NewHMOutput;
				if (FHoudiniMeshOutput* FoundHMOutput = FHoudiniOutputUtils::FindOutputHolder(OldHoudiniMeshOutputs,
					[&](const FHoudiniMeshOutput* OldHMOutput)
					{
						if (OldHMOutput->CanReuse(SplitValue, bSplitActor))
						{
							if (const UHoudiniMeshComponent* HMC = OldHMOutput->Find(Node))
								return GroupName == HMC->GetGroupName();
						}

						return false;
					}))
					NewHMOutput = *FoundHMOutput;
				
				UHoudiniMeshComponent* HMC = NewHMOutput.CreateOrUpdate(Node, SplitValue, bSplitActor);
				HMC->ResetMeshData();

				TMap<UMaterialInterface*, int32> MaterialSectionIdxMap;
				TArray<int32> PointIndices;  // Record the global point index
				TMap<int32, int32> PointIdxMap;  // Global PointIdx map tp local
				for (const int32& TriIdx : TriangleIndices)
				{
					FIntVector3 Triangle;
					for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
					{
						const int32& GlobalPointIdx = Vertices[TriIdx * 3 + TriVtxIdx];  // This PointIdx is in global
						int32 LocalPointIdx;
						{
							const int32* FoundLocalPointIdxPtr = PointIdxMap.Find(GlobalPointIdx);
							if (!FoundLocalPointIdxPtr)
							{
								LocalPointIdx = HMC->AddPoint(POSITION_SCALE_TO_UNREAL_F *
									FVector3f(PositionData[GlobalPointIdx * 3], PositionData[GlobalPointIdx * 3 + 2], PositionData[GlobalPointIdx * 3 + 1]));
								PointIdxMap.Add(GlobalPointIdx, LocalPointIdx);
								PointIndices.Add(GlobalPointIdx);
							}
							else
								LocalPointIdx = *FoundLocalPointIdxPtr;
						}

						Triangle[2 - TriVtxIdx] = LocalPointIdx;
					}

					// Parse material/material instance
					bool bIsMaterialInstance = false;
					UMaterialInterface* Material = GetMeshMaterial(Mats, MatOwner, MatInsts, MatInstOwner, TriIdx, Vertices, !AlphaData.IsEmpty(), bIsMaterialInstance);

					int32 SectionIdx = -1;
					if (const int* SectionIdxPtr = MaterialSectionIdxMap.Find(Material))
						SectionIdx = *SectionIdxPtr;
					else
					{
						UMaterialInterface* FinalMaterial = bIsMaterialInstance ? FHoudiniOutputUtils::GetMaterialInstance(Material, MatAttribs,
							[&](const HAPI_AttributeOwner& Owner) { return MeshAttributeEntryIdx(Owner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices); },
							Node->GetCookFolderPath(), MatParmMap) : Material;

						SectionIdx = HMC->AddSection(FinalMaterial);
						MaterialSectionIdxMap.Add(Material, SectionIdx);
					}

					HMC->AddTriangle(Triangle, SectionIdx);
				}

				
				if (!NormalData.IsEmpty())
				{
					HMC->SetNormalData(EHoudiniAttributeOwner(NormalOwner), NormalData, PointIndices, TriangleIndices);
					if (!TangentUData.IsEmpty() && !TangentVData.IsEmpty())
					{
						HMC->SetTangentUData(EHoudiniAttributeOwner(TangentUOwner), TangentUData, PointIndices, TriangleIndices);
						HMC->SetTangentVData(EHoudiniAttributeOwner(TangentVOwner), TangentVData, PointIndices, TriangleIndices);
					}
				}
				else
					HMC->ComputeNormal();

				HMC->SetColorData(EHoudiniAttributeOwner(ColorOwner), ColorData, EHoudiniAttributeOwner(AlphaOwner), AlphaData,
					PointIndices, TriangleIndices);
				
				TArray<EHoudiniAttributeOwner> UVOwners_;
				for (const HAPI_AttributeOwner& UVOwner : UVOwners)
					UVOwners_.Add(EHoudiniAttributeOwner(UVOwner));
				HMC->SetUVData(UVOwners_, UVsData, PointIndices, TriangleIndices);
				

				if ((GroupIdx >= 0) || bIsMainGeoEditable)  // This HoudiniMesh can be edited
				{
					TArray<int32> PrimIndices;
					HMC->Build(PrimIds, TriangleIndices, PrimIndices);
					HOUDINI_FAIL_RETURN(HMC->HapiUpdate(GetNode(), Part.GetGroupName(GroupIdx), NodeId, PartId, PointIndices, PrimIndices,
						AttribNames, PartInfo.attributeCounts, EditPointParmAttribs, EditPrimParmAttribs, TArray<FString>{}, TArray<FString>{}));
				}
				else
					HMC->ClearEditData();
				
				HMC->Modify();
				HMC->MarkRenderStateDirty();

				// Set UProperties
				SET_OBJECT_UPROPERTIES(HMC, MeshAttributeEntryIdx(PropAttribOwner, TriangleIndices[0], HAPI_ATTROWNER_PRIM, Vertices));
				SET_SPLIT_ACTOR_UPROPERTIES(NewHMOutput, MeshAttributeEntryIdx(PropAttribOwner, TriangleIndices[0], HAPI_ATTROWNER_PRIM, Vertices), true);

				NewHoudiniMeshOutputs.Add(NewHMOutput);
			}
		}
	}


	// -------- Post-processing --------
	// Destroy old outputs, like this->Destroy()
	for (const FHoudiniStaticMeshOutput* OldSMOutput : OldStaticMeshOutputs)
		OldSMOutput->Destroy(GetNode());
	OldStaticMeshOutputs.Empty();

	for (const FHoudiniDynamicMeshOutput* OldDMOutput : OldDynamicMeshOutputs)
		OldDMOutput->Destroy(GetNode());
	OldDynamicMeshOutputs.Empty();

	for (const FHoudiniMeshOutput* OldHMOutput : OldHoudiniMeshOutputs)
		OldHMOutput->Destroy(GetNode());
	OldHoudiniMeshOutputs.Empty();

	// Update output holders
	StaticMeshOutputs = NewStaticMeshOutputs;
	DynamicMeshOutputs = NewDynamicMeshOutputs;
	HoudiniMeshOutputs = NewHoudiniMeshOutputs;

	return true;
}

void UHoudiniOutputMesh::Destroy() const
{
	for (const FHoudiniStaticMeshOutput& OldSMOutput : StaticMeshOutputs)
		OldSMOutput.Destroy(GetNode());

	for (const FHoudiniDynamicMeshOutput& OldDMOutput : DynamicMeshOutputs)
		OldDMOutput.Destroy(GetNode());

	for (const FHoudiniMeshOutput& OldHMOutput : HoudiniMeshOutputs)
		OldHMOutput.Destroy(GetNode());
}

void UHoudiniOutputMesh::CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const
{
	for (const FHoudiniStaticMeshOutput& SMOutput : StaticMeshOutputs)
	{
		if (SMOutput.IsSplitActor())
			InOutSplitValues.FindOrAdd(SMOutput.GetSplitValue());
	}

	for (const FHoudiniDynamicMeshOutput& DMOutput : DynamicMeshOutputs)
	{
		if (DMOutput.IsSplitActor())
			InOutSplitValues.FindOrAdd(DMOutput.GetSplitValue());
	}

	for (const FHoudiniMeshOutput& HMOutput : HoudiniMeshOutputs)
	{
		if (HMOutput.IsSplitActor())
			InOutEditableSplitValues.FindOrAdd(HMOutput.GetSplitValue());
	}
}

void UHoudiniOutputMesh::CollectEditMeshes(TMap<FString, TArray<UHoudiniMeshComponent*>>& InOutGroupMeshesMap) const
{
	for (const FHoudiniMeshOutput& HMOutput : HoudiniMeshOutputs)
	{
		UHoudiniMeshComponent* HMC = HMOutput.Find(GetNode());
		if (HMC && HMC->GetUpdateCycles())
			InOutGroupMeshesMap.FindOrAdd(HMC->GetGroupName()).Add(HMC);
	}
}

void UHoudiniOutputMesh::GetInstancedStaticMeshes(const TArray<int32>& InstancedPartIds, TArray<UStaticMesh*>& OutSMs) const
{
	for (const FHoudiniStaticMeshOutput& SMOutput : StaticMeshOutputs)
	{
		if (SMOutput.PartId >= 0 && InstancedPartIds.Contains(SMOutput.PartId))
		{
			UStaticMesh* SM = SMOutput.StaticMesh.LoadSynchronous();
			if (IsValid(SM))
				OutSMs.Add(SM);
		}
	}
}


// Bake Houdini Mesh
UStaticMesh* UHoudiniMeshComponent::SaveStaticMesh(const FString& StaticMeshName) const
{
	UStaticMesh* SM = nullptr;
	if (IS_ASSET_PATH_INVALID(StaticMeshName))
	{
		FString AssetPath = GetParentNode()->GetBakeFolderPath() + (StaticMeshName.IsEmpty() ?
			FString::Printf(TEXT("SM_%08X"), GetTypeHash(GetPathName())) : FHoudiniEngineUtils::GetValidatedString(StaticMeshName));
		SM = FHoudiniEngineUtils::FindOrCreateAsset<UStaticMesh>(AssetPath);
	}
	else
		SM = FHoudiniEngineUtils::FindOrCreateAsset<UStaticMesh>(StaticMeshName);

	SaveToStaticMesh(SM);
	return SM;
}

void UHoudiniMeshComponent::SaveToStaticMesh(UStaticMesh* SM) const
{
	if (!IsValid(SM))
		return;

	// Only retain one lod
	for (int32 LodIdx = SM->GetNumSourceModels() - 1; LodIdx >= 1; --LodIdx)
		SM->RemoveSourceModel(LodIdx);

	TArray<FStaticMaterial>& MaterialSlots = SM->GetStaticMaterials();
	MaterialSlots.Empty();  // Reset materials
	FMeshSectionInfoMap& SectionInfoMap = SM->GetSectionInfoMap();
	SectionInfoMap.Clear();

	FStaticMeshSourceModel* SourceModelPtr = SM->IsSourceModelValid(0) ? &SM->GetSourceModel(0) : &SM->AddSourceModel();
	FMeshDescription* MeshDesc = SourceModelPtr->CreateMeshDescription();

	MeshDesc->ReserveNewTriangles(Triangles.Num());
	MeshDesc->ReserveNewVertexInstances(Triangles.Num() * 3);
	MeshDesc->SetNumUVChannels(UVs.Num());
	TVertexAttributesRef<FVector3f> SMPositionAttrib = MeshDesc->GetVertexPositions();

	FStaticMeshAttributes Attributes(*MeshDesc);
	Attributes.Register();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	VertexInstanceUVs.SetNumChannels(FMath::Max(UVs.Num(), 1));  // Must have one uv channel
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
		MeshDesc->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

	auto GetMeshVtxAttribEntryIdxLambda = [](const EHoudiniAttributeOwner& AttribOwner,
		const int32& TriIdx, const int32& LocalVtxIdx, const TArray<FIntVector3>& MeshTriangles) -> int32
		{
			switch (AttribOwner)
			{
			case EHoudiniAttributeOwner::Vertex: return TriIdx * 3 + LocalVtxIdx;
			case EHoudiniAttributeOwner::Point: return MeshTriangles[TriIdx][LocalVtxIdx];
			case EHoudiniAttributeOwner::Prim: return TriIdx;
			case EHoudiniAttributeOwner::Detail: return 0;
			}

			return -1;
		};

	TMap<int32, FVertexID> PointIdxMap;  // Global PointIdx map tp local VertexID
	for (const FHoudiniMeshSection& Section : Sections)
	{
		UMaterialInterface* FinalMaterial = Section.Material;

		FPolygonGroupID PolygonGroupID = MeshDesc->CreatePolygonGroup();
		const int32 FoundMaterialSlotIdx = MaterialSlots.IndexOfByPredicate(
			[FinalMaterial](const FStaticMaterial& StaticMaterial) { return StaticMaterial.MaterialInterface == FinalMaterial; });

		PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = MaterialSlots.IsValidIndex(FoundMaterialSlotIdx) ?
			MaterialSlots[FoundMaterialSlotIdx].ImportedMaterialSlotName : SM->AddMaterial(FinalMaterial);  // If not found, create a new slot

		// This code block works around a bug in UE when we build meshes from FMeshDescription structures. The FMeshDescription
		// structures are built correctly (one per LOD). UE then creates one mesh section for each material in each LOD and
		// fills in the correct Material Id. So far so good.
		//
		// However, UE then calls the function FStaticMeshRenderData::ResolveSectionInfo() which then overwrites this 
		// material Id with one store inthe Section Info Map:
		//
		//			FMeshSectionInfo Info = Owner->GetSectionInfoMap().Get(LODIndex, SectionIndex);
		//			...
		//			Section.MaterialIndex = Info.MaterialIndex;
		//
		// The problem is that there is no Section Info Map, so FMeshSectionInfoMap::Get() creates one, but the one created is not
		// // correct - if FMeshSectionInfoMap::Get(LodIndex,...) is called, and no section map exists for LodIndex, it uses LODIndex
		// of 0. This causes us to end up with the wrong materials on the LODs.
		//
		// So the work around is to crib the code that creates the sections (BuildVertexBuffer inside StaticMeshBuilder.cpp) and
		// create a correct Section info Map before building the Static Mesh; then everything works.
		SectionInfoMap.Set(0, PolygonGroupID.GetValue(),
			FMeshSectionInfo(MaterialSlots.IsValidIndex(FoundMaterialSlotIdx) ? FoundMaterialSlotIdx : (MaterialSlots.Num() - 1)));

		for (const int32& TriIdx : Section.TriangleIndices)
		{
			const FIntVector3& HoudiniPoints = Triangles[TriIdx];
			if ((HoudiniPoints.X == HoudiniPoints.Y) || (HoudiniPoints.Y == HoudiniPoints.Z) || (HoudiniPoints.Z == HoudiniPoints.X))  // Skip degenerated triangle
				continue;

			TArray<FVertexInstanceID> TriVtxIndices;
			TriVtxIndices.SetNumUninitialized(3);
			for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)  // Need reverse vertex order
			{
				const int32& PointIdx = HoudiniPoints[TriVtxIdx];  // This PointIdx is in global
				FVertexID VertexID;
				{
					FVertexID* FoundVertexIDPtr = PointIdxMap.Find(PointIdx);
					if (!FoundVertexIDPtr)
					{
						VertexID = MeshDesc->CreateVertex();
						SMPositionAttrib[VertexID] = Positions[PointIdx];
						PointIdxMap.Add(PointIdx, VertexID);
					}
					else
						VertexID = *FoundVertexIDPtr;
				}
				TriVtxIndices[TriVtxIdx] = MeshDesc->CreateVertexInstance(VertexID);
			}

			for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
			{
				const FVertexInstanceID& VtxInstID = TriVtxIndices[2 - TriVtxIdx];

				// Mesh VertexInstance Attributes
				if (!NormalData.IsEmpty())
				{
					const int32 NormalDataIdx = GetMeshVtxAttribEntryIdxLambda(NormalOwner, TriIdx, TriVtxIdx, Triangles);
					VertexInstanceNormals[VtxInstID] = NormalData[NormalDataIdx];

					if (!TangentUData.IsEmpty())  // Must have "N", then we could have "tangentu"
					{
						const int32 TangentUDataIdx = GetMeshVtxAttribEntryIdxLambda(TangentUOwner, TriIdx, TriVtxIdx, Triangles);
						VertexInstanceTangents[VtxInstID] = TangentUData[TangentUDataIdx];

						if (!TangentVData.IsEmpty())  // Must have "N" and "tangentu", then we could have "tangentv"
						{
							const int32 TangentVDataIdx = GetMeshVtxAttribEntryIdxLambda(TangentVOwner, TriIdx, TriVtxIdx, Triangles);
							VertexInstanceBinormalSigns[VtxInstID] = GetBasisDeterminantSign((FVector)TangentUData[TangentUDataIdx], (FVector)TangentVData[TangentVDataIdx], (FVector)NormalData[NormalDataIdx]);
						}
						else
							VertexInstanceBinormalSigns[VtxInstID] = 1.0f;
					}
				}

				if (!ColorData.IsEmpty())
				{
					const int32 ColorDataIdx = GetMeshVtxAttribEntryIdxLambda(ColorOwner, TriIdx, TriVtxIdx, Triangles);
					VertexInstanceColors[VtxInstID] = FLinearColor(ColorData[ColorDataIdx]);
				}

				for (int32 UVIdx = 0; UVIdx < UVs.Num(); ++UVIdx)
				{
					const int32 UVDataIdx = GetMeshVtxAttribEntryIdxLambda(UVs[UVIdx].Owner, TriIdx, TriVtxIdx, Triangles);
					VertexInstanceUVs.Set(VtxInstID, UVIdx, UVs[UVIdx].Data[UVDataIdx]);
				}
			}

			MeshDesc->CreateTriangle(PolygonGroupID, TriVtxIndices);
		}
	}

	SourceModelPtr->BuildSettings.bRecomputeNormals = NormalData.IsEmpty();
	SourceModelPtr->BuildSettings.bRecomputeTangents = TangentUData.IsEmpty();
	SourceModelPtr->BuildSettings.bGenerateLightmapUVs = UVs.IsEmpty();

	SM->CommitMeshDescription(0);
	SM->SetLightMapCoordinateIndex(UVs.Num() >= 2 ? 1 : 0);

	SM->Build();
}


// Skeletal Mesh Output
bool FHoudiniSkeletalMeshOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutShouldHoldByOutput = false;
	bOutIsValid = false;
	if ((PartInfo.type == HAPI_PARTTYPE_MESH) && (PartInfo.faceCount >= 1) && (PartInfo.vertexCount >= 3))
	{
		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(),
			NodeId, PartInfo.id, HAPI_ATTRIB_BONE_CAPTURE_INDEX, HAPI_ATTROWNER_POINT, &AttribInfo));
		if (AttribInfo.exists && FHoudiniEngineUtils::IsArray(AttribInfo.storage) && (AttribInfo.tupleSize == 1) &&
			(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int))
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(),
				NodeId, PartInfo.id, HAPI_ATTRIB_BONE_CAPTURE_DATA, HAPI_ATTROWNER_POINT, &AttribInfo));
			if (AttribInfo.exists && FHoudiniEngineUtils::IsArray(AttribInfo.storage) && (AttribInfo.tupleSize == 1) &&
				(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Float))
			{
				bOutIsValid = true;
				return true;
			}
		}
	}
	else if ((PartInfo.type == HAPI_PARTTYPE_CURVE) && (PartInfo.pointCount == PartInfo.faceCount * 2))  // All curves must be segment-liked.
	{
		// Must have s@name && 3[]@transform on points
		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(),
			NodeId, PartInfo.id, HAPI_ATTRIB_NAME, HAPI_ATTROWNER_POINT, &AttribInfo));
		if (AttribInfo.exists && !FHoudiniEngineUtils::IsArray(AttribInfo.storage) &&
			(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::String))
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(),
				NodeId, PartInfo.id, HAPI_ATTRIB_TRANSFORM, HAPI_ATTROWNER_POINT, &AttribInfo));
			if (AttribInfo.exists && !FHoudiniEngineUtils::IsArray(AttribInfo.storage) && (AttribInfo.tupleSize == 9) &&
				(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Float))
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(),
					NodeId, PartInfo.id, HAPI_CURVE_TYPE, HAPI_ATTROWNER_PRIM, &AttribInfo));
				if (!AttribInfo.exists)
				{
					bOutIsValid = true;
					return true;
				}
			}
		}
	}

	return true;
}

static FName AddMaterial(TArray<FSkeletalMaterial>& InOutMaterialSlots, UMaterialInterface* Material)
{
	if (Material == nullptr)
	{
		return NAME_None;
	}

	// Create a unique slot name for the material
	FName MaterialName = Material->GetFName();
	for (const FSkeletalMaterial& SkeletalMaterial : InOutMaterialSlots)
	{
		const FName ExistingName = SkeletalMaterial.MaterialSlotName;
		if (ExistingName.GetComparisonIndex() == MaterialName.GetComparisonIndex())
		{
			MaterialName = FName(MaterialName, FMath::Max(MaterialName.GetNumber(), ExistingName.GetNumber() + 1));
		}
	}

#if WITH_EDITORONLY_DATA
	FSkeletalMaterial& SkeletalMaterial = InOutMaterialSlots.Emplace_GetRef(Material, MaterialName, MaterialName);
#else
	FSkeletalMaterial& SkeletalMaterial = InOutMaterialSlots.Emplace_GetRef(Material, MaterialName);
#endif

	SkeletalMaterial.UVChannelData = FMeshUVChannelInfo(1.0f);

	return MaterialName;
}

bool FHoudiniSkeletalMeshOutputBuilder::HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputSkeletalMesh);

	TArray<int32> MeshPartIndices;
	TArray<int32> SkeletonPartIndices;
	for (int32 PartIdx = 0; PartIdx < PartInfos.Num(); ++PartIdx)
	{
		const HAPI_PartType& PartType = PartInfos[PartIdx].type;
		if (PartType == HAPI_PARTTYPE_MESH)
			MeshPartIndices.Add(PartIdx);
		else if (PartType == HAPI_PARTTYPE_CURVE)
			SkeletonPartIndices.Add(PartIdx);
	}

	if (MeshPartIndices.Num() > SkeletonPartIndices.Num())
	{
		UE_LOG(LogHoudiniEngine, Error, TEXT("Output: \"%s\" Has invalid skeletal mesh part, please check your HDA!"), *OutputName);
	}
	else if (MeshPartIndices.Num() < SkeletonPartIndices.Num())
	{
		UE_LOG(LogHoudiniEngine, Error, TEXT("Output: \"%s\" Has invalid skeleton curve part, please check your HDA!"), *OutputName)
	}

	const int32 NumSMs = FMath::Min(MeshPartIndices.Num(), SkeletonPartIndices.Num());

	const int32& NodeId = GeoInfo.nodeId;

	HAPI_AttributeInfo AttribInfo;
	for (int32 SMIdx = 0; SMIdx < NumSMs; ++SMIdx)
	{
		struct FKineFXOutputPart
		{
			FKineFXOutputPart(const HAPI_PartInfo& InPartInfo) : PartInfo(InPartInfo) {}

			const HAPI_PartInfo& PartInfo;
			TArray<std::string> AttribNames;
			FString AssetPath;

			bool HapiRetrieveBasicInfo(const AHoudiniNode* Node, const int32& NodeId, const FString& OutputName, const int32& SMIdx)
			{
				const int32& PartId = PartInfo.id;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, AttribNames));
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
					AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OBJECT_PATH, AssetPath));
				if (!IS_ASSET_PATH_INVALID(AssetPath))
					AssetPath = FHoudiniEngineUtils::GetPackagePath(AssetPath);
				else
					AssetPath = Node->GetCookFolderPath() + FString::Printf(TEXT("SKM_%s_%d"), *FHoudiniEngineUtils::GetValidatedString(OutputName), SMIdx);
				return true;
			}
		};

		FKineFXOutputPart SKMPart(PartInfos[MeshPartIndices[SMIdx]]);
		HOUDINI_FAIL_RETURN(SKMPart.HapiRetrieveBasicInfo(Node, NodeId, OutputName, SMIdx));

		FKineFXOutputPart SKPart(PartInfos[SkeletonPartIndices[SMIdx]]);
		HOUDINI_FAIL_RETURN(SKPart.HapiRetrieveBasicInfo(Node, NodeId, OutputName, SMIdx));

		if (SKMPart.AssetPath == SKPart.AssetPath)  // Ensure asset path not conflict
		{
			FString FolderPath;
			FString AssetName;
			SKMPart.AssetPath.Split(TEXT("/"), &FolderPath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			AssetName.RemoveFromStart(TEXT("SKM_"));
			SKPart.AssetPath = FolderPath + TEXT("/SK_") + AssetName;
		}

		USkeleton* SK = nullptr;
		UPhysicsAsset* PA = nullptr;
		{  // Skeleton
			const HAPI_PartInfo& PartInfo = SKPart.PartInfo;
			const TArray<std::string>& AttribNames = SKPart.AttribNames;
			const int32& PartId = PartInfo.id;

			TArray<float> PositionData;
			PositionData.SetNumUninitialized(PartInfo.pointCount * 3);

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_POSITION, HAPI_ATTROWNER_POINT, &AttribInfo));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_POSITION, &AttribInfo, -1, PositionData.GetData(), 0, PartInfo.pointCount));

			TArray<float> TransformData;
			TransformData.SetNumUninitialized(PartInfo.pointCount * 9);

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_TRANSFORM, HAPI_ATTROWNER_POINT, &AttribInfo));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_TRANSFORM, &AttribInfo, -1, TransformData.GetData(), 0, PartInfo.pointCount));  // AttributeInfo has been checked in FHoudiniSkeletalMeshOutputBuilder::HapiIsPartValid

			TArray<HAPI_StringHandle> BoneNameSHs;
			BoneNameSHs.SetNumUninitialized(PartInfo.pointCount);

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_NAME, HAPI_ATTROWNER_POINT, &AttribInfo));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_NAME, &AttribInfo, BoneNameSHs.GetData(), 0, PartInfo.pointCount));  // AttributeInfo has been checked in FHoudiniSkeletalMeshOutputBuilder::HapiIsPartValid

			TMap<HAPI_StringHandle, FName> SHBoneNameMap;
			{
				const TArray<HAPI_StringHandle> UniqueNameSHs = TSet<HAPI_StringHandle>(BoneNameSHs).Array();
				TArray<FString> UniqueNames;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueNameSHs, UniqueNames));
				for (int32 UniqueIdx = 0; UniqueIdx < UniqueNameSHs.Num(); ++UniqueIdx)
					SHBoneNameMap.Add(UniqueNameSHs[UniqueIdx], *UniqueNames[UniqueIdx]);
			}

			TArray<int32> PointIndices;  // Num() == NumBones
			TArray<int32> BoneIndices;  // Num() == PartInfo.pointCount
			{
				TMap<uint32, int32> BoneHashIdxMap;
				for (int32 PointIdx = 0; PointIdx < PartInfo.pointCount; ++PointIdx)
				{
					TArray<uint8> RawData;
					RawData.SetNumUninitialized(((3 + 9) * sizeof(float)) + sizeof(HAPI_StringHandle));
					FMemory::Memcpy(RawData.GetData(), PositionData.GetData() + PointIdx * 3, 3 * sizeof(float));
					FMemory::Memcpy(RawData.GetData() + 3 * sizeof(float), TransformData.GetData() + PointIdx * 9, 9 * sizeof(float));
					FMemory::Memcpy(RawData.GetData() + (3 + 9) * sizeof(float), &BoneNameSHs[PointIdx], sizeof(HAPI_StringHandle));
					const uint32 PointHash = GetTypeHash(RawData);
					if (const int32* FoundBoneIdxPtr = BoneHashIdxMap.Find(PointHash))
						BoneIndices.Add(*FoundBoneIdxPtr);
					else
					{
						const int32 BoneIdx = BoneHashIdxMap.Num();
						BoneHashIdxMap.Add(PointHash, BoneIdx);
						BoneIndices.Add(BoneIdx);
						PointIndices.Add(PointIdx);
					}
				}
			}

			// Retrieve UProperties
			TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
			HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
				HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

			// -------- Retrieve vertex list --------
			TArray<int32> CurveCounts;
			CurveCounts.SetNumUninitialized(PartInfo.faceCount);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetCurveCounts(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				CurveCounts.GetData(), 0, PartInfo.faceCount));

			TArray<int32> ParentIndices;
			ParentIndices.SetNumUninitialized(PointIndices.Num());
			for (int32& ParentIdx : ParentIndices)
				ParentIdx = -1;

			int32 CurrPointIdx = 0;
			for (const int32 VertexCount : CurveCounts)
			{
				ParentIndices[BoneIndices[CurrPointIdx + VertexCount - 1]] = BoneIndices[CurrPointIdx];
				CurrPointIdx += VertexCount;
			}

			SK = FHoudiniEngineUtils::CreateAsset<USkeleton>(SKPart.AssetPath);

			TArray<FMatrix> PoseMatrics;
			for (int32 BoneIdx = 0; BoneIdx < PointIndices.Num(); ++BoneIdx)
			{
				const int32& PointIdx = PointIndices[BoneIdx];

				const float* TransformDataPtr = TransformData.GetData() + PointIdx * 9;
				FMatrix PoseMatrix;
				PoseMatrix.M[0][0] = TransformDataPtr[0];
				PoseMatrix.M[0][1] = TransformDataPtr[2];
				PoseMatrix.M[0][2] = TransformDataPtr[1];
				PoseMatrix.M[0][3] = 0.0;
				PoseMatrix.M[1][0] = TransformDataPtr[6];
				PoseMatrix.M[1][1] = TransformDataPtr[8];
				PoseMatrix.M[1][2] = TransformDataPtr[7];
				PoseMatrix.M[1][3] = 0.0;
				PoseMatrix.M[2][0] = TransformDataPtr[3];
				PoseMatrix.M[2][1] = TransformDataPtr[5];
				PoseMatrix.M[2][2] = TransformDataPtr[4];
				PoseMatrix.M[2][3] = 0.0;
				const float* PositionDataPtr = PositionData.GetData() + PointIdx * 3;
				PoseMatrix.M[3][0] = PositionDataPtr[0] * 100.0;
				PoseMatrix.M[3][1] = PositionDataPtr[2] * 100.0;
				PoseMatrix.M[3][2] = PositionDataPtr[1] * 100.0;
				PoseMatrix.M[3][3] = 1.0;

				PoseMatrics.Add(PoseMatrix);
			}

			FReferenceSkeletonModifier SkeletonModifier(SK);
			for (int32 BoneIdx = 0; BoneIdx < PointIndices.Num(); ++BoneIdx)
			{
				const int32& PointIdx = PointIndices[BoneIdx];
				const int32& ParentIdx = ParentIndices[BoneIdx];

				FMatrix PoseMatrix = PoseMatrics[BoneIdx];
				if (ParentIdx >= 0)
					PoseMatrix *= PoseMatrics[ParentIdx].Inverse();
				const FName& BoneName = SHBoneNameMap[BoneNameSHs[PointIdx]];
				SkeletonModifier.Add(FMeshBoneInfo(BoneName, BoneName.ToString(), ParentIdx), FTransform(PoseMatrix));
			}

			SET_OBJECT_UPROPERTIES(SK, 0);

			SK->Modify();

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_SIMPLE_COLLISIONS, HAPI_ATTROWNER_POINT, &AttribInfo));
			if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_DICTIONARY))
			{
				TArray<FString> SimpleCollisionJsonStrs;
				{
					TArray<HAPI_StringHandle> AllSimpleCollisionSHs;
					AllSimpleCollisionSHs.SetNumUninitialized(PartInfo.pointCount);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeDictionaryData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_UNREAL_SIMPLE_COLLISIONS, &AttribInfo, AllSimpleCollisionSHs.GetData(), 0, PartInfo.pointCount));
					TArray<HAPI_StringHandle> SimpleCollisionSHs;
					for (const int32& PointIdx : PointIndices)
						SimpleCollisionSHs.Add(AllSimpleCollisionSHs[PointIdx]);
					HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SimpleCollisionSHs, SimpleCollisionJsonStrs));
				}
				
				FString PhysicsAssetPath;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
					AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_PHYSICS_ASSET, PhysicsAssetPath));
				if (IS_ASSET_PATH_INVALID(PhysicsAssetPath))
					PhysicsAssetPath = Node->GetCookFolderPath() + FString::Printf(TEXT("PA_%s_%d"), *FHoudiniEngineUtils::GetValidatedString(OutputName), SMIdx);

				PA = FHoudiniEngineUtils::FindOrCreateAsset<UPhysicsAsset>(PhysicsAssetPath);
				
				TArray<TObjectPtr<USkeletalBodySetup>> SkeletalBodySetups;
				for (int32 BoneIdx = 0; BoneIdx < PointIndices.Num(); ++BoneIdx)
				{
					const FString& SimpleCollisionJsonStr = SimpleCollisionJsonStrs[BoneIdx];
					if (!SimpleCollisionJsonStr.IsEmpty())
					{
						FKAggregateGeom AggGeom;

						TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(SimpleCollisionJsonStr);
						TSharedPtr<FJsonObject> JsonStruct;
						if (FJsonSerializer::Deserialize(JsonReader, JsonStruct))
							FJsonObjectConverter::JsonObjectToUStruct(JsonStruct.ToSharedRef(), FKAggregateGeom::StaticStruct(), &AggGeom);

						if (AggGeom.GetElementCount() >= 1)
						{
							TObjectPtr<USkeletalBodySetup> SkeletalBodySetup = nullptr;
							if (PA->SkeletalBodySetups.IsValidIndex(SkeletalBodySetups.Num()))
								SkeletalBodySetup = PA->SkeletalBodySetups[SkeletalBodySetups.Num()];
							if (!IsValid(SkeletalBodySetup))
								SkeletalBodySetup = NewObject<USkeletalBodySetup>(PA);
							SkeletalBodySetups.Add(SkeletalBodySetup);

							const int32& PointIdx = PointIndices[BoneIdx];
							SkeletalBodySetup->BoneName = SHBoneNameMap[BoneNameSHs[PointIdx]];
							SkeletalBodySetup->AggGeom = AggGeom;

							SET_OBJECT_UPROPERTIES(SkeletalBodySetup, ((PropAttribOwner == HAPI_ATTROWNER_DETAIL) ? 0 : PointIdx));
						}
					}
				}

				PA->SkeletalBodySetups = SkeletalBodySetups;

				SET_OBJECT_UPROPERTIES(SK, 0);

				PA->Modify();
			}
		}

		{  // Mesh
			const HAPI_PartInfo& PartInfo = SKMPart.PartInfo;
			const TArray<std::string>& AttribNames = SKMPart.AttribNames;
			const int32& PartId = PartInfo.id;

			TArray<std::string> PrimGroupNames;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetPrimitiveGroupNames(GeoInfo, PartInfo, PrimGroupNames));

			TArray<TArray<int32>> LodGroupships;  // Starts with second lod, may be "lod_1"
			HOUDINI_FAIL_RETURN(HapiGetLodGroupShips(NodeId, PartId,
				PrimGroupNames, PartInfo.faceCount, PartInfo.isInstanced, LodGroupships));

			// -------- Retrieve vertex list --------
			TArray<int32> Vertices;
			Vertices.SetNumUninitialized(PartInfo.vertexCount);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetVertexList(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				Vertices.GetData(), 0, PartInfo.vertexCount));
			
			// -------- Retrieve mesh data --------
			TArray<float> PositionData;
			PositionData.SetNumUninitialized(PartInfo.pointCount * 3);

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_POSITION, HAPI_ATTROWNER_POINT, &AttribInfo));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_POSITION, &AttribInfo, -1, PositionData.GetData(), 0, PartInfo.pointCount));

			HAPI_AttributeOwner NormalOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_NORMAL);
			TArray<float> NormalData;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_NORMAL, 3, NormalData, NormalOwner));

			HAPI_AttributeOwner TangentUOwner = NormalData.IsEmpty() ? HAPI_ATTROWNER_INVALID :  // Must have "N", then we could have "tangentu"
				FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_TANGENT);
			TArray<float> TangentUData;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_TANGENT, 3, TangentUData, TangentUOwner));

			HAPI_AttributeOwner TangentVOwner = TangentUData.IsEmpty() ? HAPI_ATTROWNER_INVALID :  // Must have "N" and "tangentu", then we could have "tangentv"
				FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_TANGENT2);
			TArray<float> TangentVData;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_TANGENT2, 3, TangentVData, TangentVOwner));

			HAPI_AttributeOwner ColorOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_COLOR);
			TArray<float> ColorData;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_COLOR, 3, ColorData, ColorOwner));

			HAPI_AttributeOwner AlphaOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ALPHA);
			TArray<float> AlphaData;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ALPHA, 1, AlphaData, AlphaOwner));

			const bool bHasColorAttrib = !ColorData.IsEmpty() || !AlphaData.IsEmpty();

			TArray<HAPI_AttributeOwner> UVOwners;
			TArray<TArray<float>> UVsData;
			HOUDINI_FAIL_RETURN(HapiGetUVAttributeData(UVsData, UVOwners, NodeId, PartId, AttribNames, PartInfo.attributeCounts));


			// Material
			TArray<UMaterialInterface*> MatInsts;  // Material instance superiority
			HAPI_AttributeOwner MatInstOwner = HAPI_ATTROWNER_INVALID;
			TArray<TSharedPtr<FHoudiniAttribute>> MatAttribs;
			TArray<UMaterialInterface*> Mats;  // If invalid, then fallback to use material attribute
			HAPI_AttributeOwner MatOwner = HAPI_ATTROWNER_INVALID;
			HOUDINI_FAIL_RETURN(HapiRetrieveMeshMaterialData(MatInsts, MatInstOwner, MatAttribs,
				Mats, MatOwner, Vertices, NodeId, PartInfo, AttribNames));

			// Retrieve UProperties
			TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
			HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
				HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

			// Bone Data
			TArray<int32> BoneDataArrayLens;
			BoneDataArrayLens.SetNumUninitialized(PartInfo.pointCount);
			TArray<int32> BoneIndices;
			TArray<float> BoneWeights;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_BONE_CAPTURE_INDEX, HAPI_ATTROWNER_POINT, &AttribInfo));

			BoneIndices.SetNumUninitialized(AttribInfo.totalArrayElements);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_BONE_CAPTURE_INDEX, &AttribInfo, BoneIndices.GetData(), AttribInfo.totalArrayElements,
				BoneDataArrayLens.GetData(), 0, PartInfo.pointCount));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_BONE_CAPTURE_DATA, HAPI_ATTROWNER_POINT, &AttribInfo));

			BoneWeights.SetNumUninitialized(AttribInfo.totalArrayElements);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_BONE_CAPTURE_DATA, &AttribInfo, BoneWeights.GetData(), AttribInfo.totalArrayElements,
				BoneDataArrayLens.GetData(), 0, PartInfo.pointCount));
			
			// Accumulated count for quick search
			for (int32 PointIdx = 1; PointIdx < PartInfo.pointCount; ++PointIdx)
				BoneDataArrayLens[PointIdx] += BoneDataArrayLens[PointIdx - 1];

			BoneIndices.SetNum(AttribInfo.totalArrayElements);  // Ensure count is same

			USkeletalMesh* SM = FHoudiniEngineUtils::FindOrCreateAsset<USkeletalMesh>(SKMPart.AssetPath);
			FString MeshAssetFolderPath;
			{
				FString MeshAssetName;
				SKMPart.AssetPath.Split(TEXT("/"), &MeshAssetFolderPath, &MeshAssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				if (MeshAssetFolderPath.IsEmpty())
					MeshAssetFolderPath = Node->GetCookFolderPath();
				else
					MeshAssetFolderPath += TEXT("/");
			}

			TArray<TArray<int32>> LodTriangleIndices;
			LodTriangleIndices.SetNum(LodGroupships.Num() + 1);
			for (int32 TriIdx = 0; TriIdx < PartInfo.faceCount; ++TriIdx)
			{
				bool bFound = false;
				for (int32 LodIdx = 1; LodIdx <= LodGroupships.Num(); ++LodIdx)
				{
					if (LodGroupships[LodIdx - 1][TriIdx])
					{
						LodTriangleIndices[LodIdx].Add(TriIdx);
						bFound = true;
						break;
					}
				}

				if (!bFound)
					LodTriangleIndices[0].Add(TriIdx);
			}

			FSkeletalMeshModel* ImportedModel = SM->GetImportedModel();

			TArray<FSkeletalMaterial>& MaterialSlots = SM->GetMaterials();
			MaterialSlots.Empty();  // Reset materials

			TMap<TPair<UMaterialInterface*, uint32>, UMaterialInstance*> MatParmMap;  // Use to find created MaterialInstance quickly
			int32 SourceModelIdx = 0;
			for (const TArray<int32>& Triangles : LodTriangleIndices)
			{
				if (Triangles.IsEmpty())
					continue;

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
				FSkeletalMeshLODInfo* LodInfo = (SourceModelIdx >= SM->GetNumSourceModels()) ? &SM->AddLODInfo() : SM->GetLODInfo(SourceModelIdx);
#else
				FSkeletalMeshLODInfo* LodInfo = (SourceModelIdx >= SM->GetLODNum()) ? &SM->AddLODInfo() : SM->GetLODInfo(SourceModelIdx);
#endif
				if (SourceModelIdx >= ImportedModel->LODModels.Num())
					ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
				FMeshDescription* MeshDesc = SM->GetSourceModel(SourceModelIdx).CreateMeshDescription();
#else
				FMeshDescription* MeshDesc = new FMeshDescription;
#endif
				MeshDesc->ReserveNewTriangles(Triangles.Num());
				MeshDesc->ReserveNewVertexInstances(Triangles.Num() * 3);
				MeshDesc->SetNumUVChannels(UVOwners.Num());
				TVertexAttributesRef<FVector3f> SMPositionAttrib = MeshDesc->GetVertexPositions();

				FSkeletalMeshAttributes Attributes(*MeshDesc);
				Attributes.Register();
				TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
				TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
				TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
				TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
				TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
				VertexInstanceUVs.SetNumChannels(FMath::Max(UVOwners.Num(), 1));  // Must have one uv channel
				TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
					MeshDesc->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

				FSkinWeightsVertexAttributesRef VertexSkinWeights = Attributes.GetVertexSkinWeights();

				TMap<int32, FVertexID> PointIdxMap;  // Global PointIdx map tp local VertexID
				TMap<UMaterialInterface*, FPolygonGroupID> MaterialPolygonGroupMap;
				for (const int32& TriIdx : Triangles)
				{
					const FIntVector3 HoudiniPoints(Vertices[TriIdx * 3], Vertices[TriIdx * 3 + 1], Vertices[TriIdx * 3 + 2]);
					if ((HoudiniPoints.X == HoudiniPoints.Y) || (HoudiniPoints.Y == HoudiniPoints.Z) || (HoudiniPoints.Z == HoudiniPoints.X))  // Skip degenerated triangle
						continue;

					TArray<FVertexInstanceID> TriVtxIndices;
					TriVtxIndices.SetNumUninitialized(3);
					for (int32 TriVtxIdx = 2; TriVtxIdx >= 0; --TriVtxIdx)
					{
						const int32& PointIdx = HoudiniPoints[TriVtxIdx];  // This PointIdx is in global
						FVertexID VertexID;
						{
							FVertexID* FoundVertexIDPtr = PointIdxMap.Find(PointIdx);
							if (!FoundVertexIDPtr)
							{
								VertexID = MeshDesc->CreateVertex();
								SMPositionAttrib[VertexID] = POSITION_SCALE_TO_UNREAL_F *
									FVector3f(PositionData[PointIdx * 3], PositionData[PointIdx * 3 + 2], PositionData[PointIdx * 3 + 1]);
								PointIdxMap.Add(PointIdx, VertexID);

								TArray<UE::AnimationCore::FBoneWeight> VertexBoneWeights;
								for (int32 BoneArrayIdx = ((PointIdx == 0) ? 0 : BoneDataArrayLens[PointIdx - 1]); BoneArrayIdx < BoneDataArrayLens[PointIdx]; ++BoneArrayIdx)
									VertexBoneWeights.Add(UE::AnimationCore::FBoneWeight(uint16(BoneIndices[BoneArrayIdx]), BoneWeights[BoneArrayIdx]));
								VertexSkinWeights.Set(VertexID, UE::AnimationCore::FBoneWeights::Create(VertexBoneWeights));
							}
							else
								VertexID = *FoundVertexIDPtr;
						}

						const FVertexInstanceID VtxInstID = MeshDesc->CreateVertexInstance(VertexID);
						TriVtxIndices[2 - TriVtxIdx] = VtxInstID;

						// Mesh VertexInstance Attributes
						const int32 GlobalVtxIdx = TriIdx * 3 + TriVtxIdx;
						if (!NormalData.IsEmpty())
						{
							const int32 NormalDataIdx = MeshAttributeEntryIdx(NormalOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
							const FVector3f Normal(NormalData[NormalDataIdx], NormalData[NormalDataIdx + 2], NormalData[NormalDataIdx + 1]);
							VertexInstanceNormals[VtxInstID] = Normal;

							if (!TangentUData.IsEmpty())  // Must have "N", then we could have "tangentu"
							{
								const int32 TangentUDataIdx = MeshAttributeEntryIdx(TangentUOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
								const FVector3f TangentU(TangentUData[TangentUDataIdx], TangentUData[TangentUDataIdx + 2], TangentUData[TangentUDataIdx + 1]);
								VertexInstanceTangents[VtxInstID] = TangentU;

								if (!TangentVData.IsEmpty())  // Must have "N" and "tangentu", then we could have "tangentv"
								{
									const int32 TangentVDataIdx = MeshAttributeEntryIdx(TangentVOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
									const FVector3f TangentV(TangentVData[TangentVDataIdx], TangentVData[TangentVDataIdx + 2], TangentVData[TangentVDataIdx + 1]);
									VertexInstanceBinormalSigns[VtxInstID] = GetBasisDeterminantSign((FVector)TangentU, (FVector)TangentV, (FVector)Normal);
								}
								else
									VertexInstanceBinormalSigns[VtxInstID] = 1.0f;
							}
						}

						if (bHasColorAttrib)
						{
							FVector4f Color(1.0f, 1.0f, 1.0f, 1.0f);
							if (!ColorData.IsEmpty())
							{
								const int32 ColorDataIdx = MeshAttributeEntryIdx(ColorOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 3;
								Color.X = ColorData[ColorDataIdx];
								Color.Y = ColorData[ColorDataIdx + 1];
								Color.Z = ColorData[ColorDataIdx + 2];
							}

							if (!AlphaData.IsEmpty())
								Color.W = AlphaData[MeshAttributeEntryIdx(AlphaOwner, GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices)];

							VertexInstanceColors[VtxInstID] = Color;
						}

						for (int32 UVIdx = 0; UVIdx < UVOwners.Num(); ++UVIdx)
						{
							const int32 UVDataIdx = MeshAttributeEntryIdx(UVOwners[UVIdx], GlobalVtxIdx, HAPI_ATTROWNER_VERTEX, Vertices) * 2;
							const TArray<float>& UVData = UVsData[UVIdx];
							const FVector2f UV(UVData[UVDataIdx], 1.0f - UVData[UVDataIdx + 1]);
							VertexInstanceUVs.Set(VtxInstID, UVIdx, UV);
						}
					}

					// Parse material/material instance
					bool bIsMaterialInstance = false;
					UMaterialInterface* Material = GetMeshMaterial(Mats, MatOwner, MatInsts, MatInstOwner, TriIdx, Vertices, !AlphaData.IsEmpty(), bIsMaterialInstance);

					FPolygonGroupID PolygonGroupID;
					if (FPolygonGroupID* FoundPolygonGroupIDPtr = MaterialPolygonGroupMap.Find(Material))  // Just using raw Material to find PolygonGroupID, do NOT use FinalMaterial
						PolygonGroupID = *FoundPolygonGroupIDPtr;
					else
					{
						UMaterialInterface* FinalMaterial = bIsMaterialInstance ? FHoudiniOutputUtils::GetMaterialInstance(Material, MatAttribs,
							[&](const HAPI_AttributeOwner& Owner) { return MeshAttributeEntryIdx(Owner, TriIdx, HAPI_ATTROWNER_PRIM, Vertices); },
							MeshAssetFolderPath, MatParmMap) : Material;

						PolygonGroupID = MeshDesc->CreatePolygonGroup();
						const int32 FoundMaterialSlotIdx = MaterialSlots.IndexOfByPredicate(
							[FinalMaterial](const FSkeletalMaterial& SkeletalMaterial) { return SkeletalMaterial.MaterialInterface == FinalMaterial; });

						PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = MaterialSlots.IsValidIndex(FoundMaterialSlotIdx) ?
							MaterialSlots[FoundMaterialSlotIdx].ImportedMaterialSlotName : AddMaterial(MaterialSlots, FinalMaterial);  // If not found, create a new slot

						MaterialPolygonGroupMap.Add(Material, PolygonGroupID);  // Just using raw Material to mark PolygonGroupID, do NOT use FinalMaterial
					}

					MeshDesc->CreateTriangle(PolygonGroupID, TriVtxIndices);
				}

				LodInfo->BuildSettings.bRecomputeNormals = NormalData.IsEmpty();
				LodInfo->BuildSettings.bRecomputeTangents = TangentUData.IsEmpty();
				//LodInfo->BuildSettings.bGenerateLightmapUVs = UVOwners.IsEmpty();
				LodInfo->bHasBeenSimplified = false;  // Mark as imported, rather than generated
				LodInfo->ReductionSettings.NumOfTrianglesPercentage = 1.0f;  // Do NOT reduce
				LodInfo->ReductionSettings.NumOfVertPercentage = 1.0f;  // Do NOT reduce

				for (const auto& PropAttrib : PropAttribs)  // Set uproperty on LodInfo
				{
					const HAPI_AttributeOwner PropAttribOwner = (HAPI_AttributeOwner)PropAttrib->GetOwner();
					PropAttrib->SetStructPropertyValues(LodInfo, LodInfo->StaticStruct(),
						MeshAttributeEntryIdx(PropAttribOwner, Triangles[0], HAPI_ATTROWNER_PRIM, Vertices));
				}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
				SM->CommitMeshDescription(SourceModelIdx);
#else
				SM->CommitMeshDescription(SourceModelIdx, *MeshDesc);
				delete MeshDesc;
#endif
				++SourceModelIdx;
			}

			// Remove useless Lods
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
			for (int32 LodIdx = SM->GetNumSourceModels() - 1; LodIdx >= SourceModelIdx; --LodIdx)
#else
			for (int32 LodIdx = SM->GetImportedModel()->LODModels.Num() - 1; LodIdx >= SourceModelIdx; --LodIdx)
#endif
			{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
				SM->RemoveSourceModel(LodIdx);
#else
				SM->RemoveLODInfo(LodIdx);
#endif
				ImportedModel->LODModels.RemoveAt(LodIdx);
			}

			SM->SetSkeleton(SK);
			SM->SetRefSkeleton(SK->GetReferenceSkeleton());
			SM->SetPhysicsAsset(PA);
			if (IsValid(PA))
				PA->SetPreviewMesh(SM);

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
			{  // Nanite enabled
				const HAPI_AttributeOwner NaniteEnableOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_NANITE_ENABLED);
				if (NaniteEnableOwner != HAPI_ATTROWNER_INVALID)
				{
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_UNREAL_NANITE_ENABLED, NaniteEnableOwner, &AttribInfo));
					if (AttribInfo.exists && !FHoudiniEngineUtils::IsArray(AttribInfo.storage) &&
						(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int))
					{
						int8 bNaniteEnabled = 0;
						AttribInfo.tupleSize = 1;  // The tupleSize should be 1 when used for enum
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInt8Data(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
							HAPI_ATTRIB_UNREAL_NANITE_ENABLED, &AttribInfo, -1, &bNaniteEnabled, 0, 1));
						SM->NaniteSettings.bEnabled = bool(bNaniteEnabled);
					}
				}
			}
#endif
			SET_OBJECT_UPROPERTIES(SM, 0);

			SM->CalculateInvRefMatrices();
			SM->Modify();

			SM->Build();
		}
	}
	
	return true;
}
