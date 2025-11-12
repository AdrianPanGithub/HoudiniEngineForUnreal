// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputs.h"

#include "JsonObjectConverter.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshComponentLODInfo.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshAttributes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"


UHoudiniInputHolder* FHoudiniStaticMeshInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<UStaticMesh, UHoudiniInputStaticMesh>(Input, Asset, OldHolder);
}

bool FHoudiniStaticMeshInputBuilder::GetInfo(const UObject* Asset, FString& OutInfoStr)
{
	return UHoudiniInputStaticMesh::AppendObjectInfo(Asset, OutInfoStr);
}


void AppendSimpleCollisionsToJsonStr(const FKAggregateGeom& SimpleCollisions, TArray<char>& InOutJsonStrs)
{
	if (SimpleCollisions.GetElementCount() >= 1)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
		if (FJsonObjectConverter::UStructToJsonObject(FKAggregateGeom::StaticStruct(), &SimpleCollisions, JsonObject, 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase))
#else
		if (FJsonObjectConverter::UStructToJsonObject(FKAggregateGeom::StaticStruct(), &SimpleCollisions, JsonObject))
#endif
		{
			for (TFieldIterator<FProperty> PropIter(FKAggregateGeom::StaticStruct(), EFieldIteratorFlags::ExcludeSuper); PropIter; ++PropIter)
			{
				const FProperty* Property = *PropIter;
				if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, &SimpleCollisions);
					if (ArrayHelper.Num() <= 0)
						JsonObject->RemoveField(Property->GetName());
				}
			}

			FString JsonStr;
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr, 2);
			bool bSuccess = FJsonSerializer::Serialize(JsonObject, JsonWriter);
			JsonWriter->Close();
			if (bSuccess)
			{
				std::string CollisionStr = TCHAR_TO_UTF8(*JsonStr);
				InOutJsonStrs.Append(CollisionStr.c_str(), CollisionStr.length());
			}
		}
	}
	InOutJsonStrs.Add('\0');
}

#define COPY_VERTEX_VECTOR3f_DATA(DATA_PTR, VECTOR) { \
		float* CurrVector3fDataPtr = DATA_PTR + (NumPrevVertices + VtxIdx) * 3; \
		*CurrVector3fDataPtr = VECTOR.X; \
		*(CurrVector3fDataPtr + 1) = VECTOR.Z; \
		*(CurrVector3fDataPtr + 2) = VECTOR.Y; \
	}

bool UHoudiniInputStaticMesh::HapiImportMeshDescription(const UStaticMesh* SM, const UStaticMeshComponent* SMC,
	const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle)
{
	TArray<TPair<int32, const FMeshDescription*>> IdxMeshDescs;
	if (Settings.CollisionImportMethod != EHoudiniMeshCollisionImportMethod::NoImportCollision && IsValid(SM->ComplexCollisionMesh))
	{
		for (int32 LodIdx = 0; LodIdx < SM->ComplexCollisionMesh->GetNumSourceModels(); ++LodIdx)
		{
			if (const FMeshDescription* MeshDesc = SM->ComplexCollisionMesh->GetMeshDescription(LodIdx))
			{
				IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(-1, MeshDesc));
				break;
			}
		}
	}

	if (IdxMeshDescs.IsEmpty() || Settings.CollisionImportMethod != EHoudiniMeshCollisionImportMethod::ImportWithoutMesh)
	{
		switch (Settings.LODImportMethod)
		{
		case EHoudiniStaticMeshLODImportMethod::FirstLOD:  // Try find the first MeshDesc
		{
			for (int32 LodIdx = 0; LodIdx < SM->GetNumSourceModels(); ++LodIdx)
			{
				if (const FMeshDescription* MeshDesc = SM->GetMeshDescription(LodIdx))
				{
					IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(LodIdx, MeshDesc));
					break;
				}
			}
		}
		break;
		case EHoudiniStaticMeshLODImportMethod::AllLODs:  // Try gather all MeshDesc
		{
			for (int32 LodIdx = 0; LodIdx < SM->GetNumSourceModels(); ++LodIdx)
			{
				if (const FMeshDescription* MeshDesc = SM->GetMeshDescription(LodIdx))
					IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(LodIdx, MeshDesc));
			}
		}
		break;
		case EHoudiniStaticMeshLODImportMethod::LastLOD:  // Try find the last MeshDesc
		{
			for (int32 LodIdx = SM->GetNumSourceModels() - 1; LodIdx >= 0; --LodIdx)
			{
				if (const FMeshDescription* MeshDesc = SM->GetMeshDescription(LodIdx))
				{
					IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(LodIdx, MeshDesc));
					break;
				}
			}
		}
		break;
		}
	}
	
	const int32 NumGroupsToImport = IdxMeshDescs.Num();

	// We need to statistic @numpt, @numprim, and then @numvtx = @numprim * 3 (all triangles) for All LODs need to import
	size_t NumPoints = 0;
	size_t NumTriangles = 0;
	int32 NumUVChannels = 0;

	TArray<int32> ImportMaterialIndices;
	for (const TPair<int32, const FMeshDescription*>& IdxMeshDesc : IdxMeshDescs)
	{
		const int32& LodIdx = IdxMeshDesc.Key;
		const FMeshDescription* MeshDesc = IdxMeshDesc.Value;

		NumPoints += MeshDesc->Vertices().Num();  // accumulate @numpt
		NumTriangles += MeshDesc->Triangles().Num();  // accumulate @numprim
		NumUVChannels = FMath::Max(MeshDesc->GetNumUVElementChannels(), NumUVChannels);

		// Count material data length here
		if (LodIdx < 0)
			ImportMaterialIndices.AddUnique(-1);
		else
		{
			for (const FPolygonGroupID& PolygonGroupID : MeshDesc->PolygonGroups().GetElementIDs())
				ImportMaterialIndices.AddUnique(SM->GetSectionInfoMap().Get(LodIdx, PolygonGroupID.GetValue()).MaterialIndex);
		}
	}

	const size_t NumVertices = NumTriangles * 3;  // Finally, @numvtx = @numprim * 3, as all prims are triangles.
	const bool bShouldImportLodGroups = (NumGroupsToImport >= 2);

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumTriangles, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_NORMAL, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@N
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenu
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT2, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenv
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_COLOR, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@Cd
	SHMGeoInput.AppendAttribute(HAPI_ALPHA, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 1, NumVertices);  // f@Alpha
	for (int32 UVChannelIdx = 1; UVChannelIdx <= NumUVChannels; ++UVChannelIdx)
		SHMGeoInput.AppendAttribute(UVChannelIdx == 1 ? HAPI_ATTRIB_UV : TCHAR_TO_UTF8(*(TEXT(HAPI_ATTRIB_UV) + FString::FromInt(UVChannelIdx))),
			EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@uv, v@uv2, v@uv3, etc.

	if (bShouldImportLodGroups)
	{
		for (const TPair<int32, const FMeshDescription*>& IdxMeshDesc : IdxMeshDescs)
			SHMGeoInput.AppendGroup(IdxMeshDesc.Key < 0 ?  // @group_collision_geo, @group_lod_0, @group_lod_1, @group_lod_2, etc.
				HAPI_GROUP_COLLISION_GEO : (TCHAR_TO_UTF8(*(TEXT(HAPI_GROUP_PREFIX_LOD) + FString::FromInt(IdxMeshDesc.Key)))),
				EHoudiniAttributeOwner::Prim, NumTriangles);
	}

	TMap<int32, int32> ImportMatIdxMap;
	TArray<std::string> ImportMaterialRefs;
	bool bHasEmptyRef = false;
	{
		const TArray<FStaticMaterial>& MaterialSlots = SM->GetStaticMaterials();
		for (const int32& MatIdx : ImportMaterialIndices)
		{
			if (MaterialSlots.IsValidIndex(MatIdx))
			{
				UMaterialInterface* Material = IsValid(SMC) ?
					SMC->GetMaterial(SMC->GetMaterialIndex(MaterialSlots[MatIdx].ImportedMaterialSlotName)) : MaterialSlots[MatIdx].MaterialInterface.Get();
				if (IsValid(Material))
				{
					ImportMatIdxMap.Add(MatIdx, ImportMaterialRefs.AddUnique(TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(Material))));
					continue;
				}
			}
			bHasEmptyRef = true;
		}
	}
	
	const int32 NumUniqueMaterialRefs = (ImportMaterialRefs.Num() + int32(bHasEmptyRef));
	const EHoudiniInputAttributeCompression MaterialAttribCompression = (NumUniqueMaterialRefs >= 2) ?
		EHoudiniInputAttributeCompression::Indexing : EHoudiniInputAttributeCompression::UniqueValue;
	size_t NumMaterialChars = 0;
	{
		for (const std::string& ImportMaterialRef : ImportMaterialRefs)
			NumMaterialChars += ImportMaterialRef.length() + 1;  // Add '\0'
		if (bHasEmptyRef)
			++NumMaterialChars;
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_MATERIAL, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::String, 1,  // s@unreal_material
			(MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? (NumMaterialChars / 4 + 1) : (NumMaterialChars / 4 + 2 + NumTriangles), MaterialAttribCompression);
	}

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)SM), InOutHandle);

	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	float* NormalDataPtr = (float*)(VertexDataPtr + NumVertices + NumTriangles);
	float* TangentUDataPtr = NormalDataPtr + NumVertices * 3;
	float* TangentVDataPtr = TangentUDataPtr + NumVertices * 3;
	float* ColorDataPtr = TangentVDataPtr + NumVertices * 3;
	float* AlphaDataPtr = ColorDataPtr + NumVertices * 3;
	float* UVsDataPtr = AlphaDataPtr + NumVertices;
	int32* GroupDataPtr = (int32*)(UVsDataPtr + NumVertices * NumUVChannels * 3);
	char* MaterialDataPtr = (char*)(bShouldImportLodGroups ?
		(GroupDataPtr + NumGroupsToImport * NumTriangles) : (int32*)(UVsDataPtr + NumVertices * NumUVChannels * 3));
	int32* MaterialIndicesDataPtr = (MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? nullptr :
		(((int32*)MaterialDataPtr) + NumMaterialChars / 4 + 2);  // First is num unique strings
	if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
	{
		*((int32*)MaterialDataPtr) = NumUniqueMaterialRefs;
		MaterialDataPtr += sizeof(int32);
	}
	for (const std::string& ImportMaterialRef : ImportMaterialRefs)
	{
		FMemory::Memcpy(MaterialDataPtr, ImportMaterialRef.c_str(), ImportMaterialRef.length());
		MaterialDataPtr += ImportMaterialRef.length() + 1;  // Add '\0'
	}

	size_t NumPrevPoints = 0;
	size_t NumPrevVertices = 0;
	size_t NumPrevTriangles = 0;
	for (int32 GroupIdx = 0; GroupIdx < NumGroupsToImport; ++GroupIdx)
	{
		const int32& LodIdx = IdxMeshDescs[GroupIdx].Key;
		const FMeshDescription* MeshDesc = IdxMeshDescs[GroupIdx].Value;

		for (const FVertexID& VtxID : MeshDesc->Vertices().GetElementIDs())
		{
			const FVector3f Position = MeshDesc->GetVertexPosition(VtxID) * POSITION_SCALE_TO_HOUDINI_F;
			*PositionDataPtr = Position.X;
			*(PositionDataPtr + 1) = Position.Z;
			*(PositionDataPtr + 2) = Position.Y;
			PositionDataPtr += 3;
		}

		FStaticMeshConstAttributes Attributes(*MeshDesc);
		//TVertexAttributesConstRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesConstRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesConstRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
		//TEdgeAttributesConstRef<bool> EdgeHardnesses = Attributes.GetEdgeHardnesses();

		int32* CurrLodDataPtr = bShouldImportLodGroups ? (GroupDataPtr + GroupIdx * NumTriangles + NumPrevTriangles) : nullptr;
		TArray<FVertexInstanceID> VtxInstIDs;  // This is houdini vertex order
		VtxInstIDs.Reserve(MeshDesc->Triangles().Num());
		for (const FTriangleID& TriID : MeshDesc->Triangles().GetElementIDs())
		{
			const TArrayView<const FVertexID> TriVtcs = MeshDesc->GetTriangleVertices(TriID);
			*VertexDataPtr = NumPrevPoints + TriVtcs[2].GetValue();
			*(VertexDataPtr + 1) = NumPrevPoints + TriVtcs[1].GetValue();
			*(VertexDataPtr + 2) = NumPrevPoints + TriVtcs[0].GetValue();
			*(VertexDataPtr + 3) = HOUDINI_SHM_GEO_INPUT_POLY;
			VertexDataPtr += 4;

			const TArrayView<const FVertexInstanceID> TriVtxInsts = MeshDesc->GetTriangleVertexInstances(TriID);
			VtxInstIDs.Add(TriVtxInsts[2]);
			VtxInstIDs.Add(TriVtxInsts[1]);
			VtxInstIDs.Add(TriVtxInsts[0]);

			if (CurrLodDataPtr)
			{
				*CurrLodDataPtr = 1;
				++CurrLodDataPtr;
			}
		}

		const int32 NumCurrUVChannels = MeshDesc->GetNumUVElementChannels();

		for (int32 VtxIdx = 0; VtxIdx < VtxInstIDs.Num(); ++VtxIdx)
		{
			const FVertexInstanceID& VtxInstID = VtxInstIDs[VtxIdx];

			const FVector3f Normal = VertexInstanceNormals[VtxInstID];
			COPY_VERTEX_VECTOR3f_DATA(NormalDataPtr, Normal);

			const FVector3f Tangent = VertexInstanceTangents[VtxInstID];
			COPY_VERTEX_VECTOR3f_DATA(TangentUDataPtr, Tangent);

			const FVector3f Binormal = (FVector3f::CrossProduct(Normal, Tangent).GetSafeNormal() * VertexInstanceBinormalSigns[VtxInstID]);
			COPY_VERTEX_VECTOR3f_DATA(TangentVDataPtr, Binormal);

			const FVector4f VertexColor = VertexInstanceColors[VtxInstID];
			float* CurrColorDataPtr = ColorDataPtr + (NumPrevVertices + VtxIdx) * 3;
			*CurrColorDataPtr = VertexColor.X;
			*(CurrColorDataPtr + 1) = VertexColor.Y;
			*(CurrColorDataPtr + 2) = VertexColor.Z;
			*(AlphaDataPtr + NumPrevVertices + VtxIdx) = VertexColor.W;

			for (int32 UVChannelIdx = 0; UVChannelIdx < NumUVChannels; ++UVChannelIdx)
			{
				if (UVChannelIdx < NumCurrUVChannels)
				{
					const FVector2f UV = VertexInstanceUVs.Get(VtxInstID, UVChannelIdx);
					float* CurrUVsDataPtr = UVsDataPtr + (UVChannelIdx * NumVertices + NumPrevVertices + VtxIdx) * 3;
					*CurrUVsDataPtr = UV.X;
					*(CurrUVsDataPtr + 1) = 1.0f - UV.Y;
				}
			}
		}

		if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
		{
			if (LodIdx >= 0)
			{
				for (const FPolygonGroupID& PolygonGroupID : MeshDesc->PolygonGroups().GetElementIDs())
				{
					int32 ImportIdx = -1;
					if (const int32* FoundImportIdxPtr = ImportMatIdxMap.Find(SM->GetSectionInfoMap().Get(LodIdx, PolygonGroupID.GetValue()).MaterialIndex))
						ImportIdx = *FoundImportIdxPtr;
					for (const FTriangleID& TriID : MeshDesc->GetPolygonGroupTriangles(PolygonGroupID))
						MaterialIndicesDataPtr[NumPrevTriangles + TriID] = ImportIdx;
				}
			}
			else  // Need not import material for collision geo, so all material refs is empty (-1)
			{
				for (int32 MatTriIdx = MeshDesc->Triangles().Num() - 1; MatTriIdx >= 0; --MatTriIdx)
					MaterialIndicesDataPtr[NumPrevTriangles + MatTriIdx] = -1;
			}
		}

		NumPrevPoints += MeshDesc->Vertices().Num();
		NumPrevVertices += MeshDesc->VertexInstances().Num();
		NumPrevTriangles += MeshDesc->Triangles().Num();
	}

	if (InOutSHMInputNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GeoNodeId,
			FString::Printf(TEXT("mesh_%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), InOutSHMInputNodeId));

	return SHMGeoInput.HapiUpload(InOutSHMInputNodeId, SHM);
}

bool UHoudiniInputStaticMesh::HapiImportRenderData(const UStaticMesh* SM, const UStaticMeshComponent* SMC,
	const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle)
{
	TArray<TPair<int32, const FStaticMeshLODResources*>> IdxMeshes;
	if (Settings.CollisionImportMethod != EHoudiniMeshCollisionImportMethod::NoImportCollision && IsValid(SM->ComplexCollisionMesh))
	{
		if (SM->ComplexCollisionMesh->GetNumLODs() >= 1)
			IdxMeshes.Add(TPair<int32, const FStaticMeshLODResources*>(-1, &SM->ComplexCollisionMesh->GetLODForExport(0)));
	}

	if (IdxMeshes.IsEmpty() || Settings.CollisionImportMethod != EHoudiniMeshCollisionImportMethod::ImportWithoutMesh)
	{
		switch (Settings.LODImportMethod)
		{
		case EHoudiniStaticMeshLODImportMethod::FirstLOD:  // Try find the first MeshDesc
		if (SM->GetNumLODs() >= 1)
			IdxMeshes.Add(TPair<int32, const FStaticMeshLODResources*>(0, &SM->GetLODForExport(0)));
		break;
		case EHoudiniStaticMeshLODImportMethod::AllLODs:  // Try gather all MeshDesc
		{
			for (int32 LodIdx = 0; LodIdx < SM->GetNumLODs(); ++LodIdx)
				IdxMeshes.Add(TPair<int32, const FStaticMeshLODResources*>(LodIdx, &SM->GetLODForExport(LodIdx)));
		}
		break;
		case EHoudiniStaticMeshLODImportMethod::LastLOD:  // Try find the last MeshDesc
		{
			const int32 NumLODs = SM->GetNumLODs();
			if (NumLODs >= 1)
				IdxMeshes.Add(TPair<int32, const FStaticMeshLODResources*>(NumLODs - 1, &SM->GetLODForExport(NumLODs - 1)));
		}
		break;
		}
	}

	const int32 NumGroupsToImport = IdxMeshes.Num();

	// We need to statistic @numpt, @numprim, and then @numvtx = @numprim * 3 (all triangles) for All LODs need to import
	size_t NumPoints = 0;
	size_t NumTriangles = 0;
	uint32 NumUVChannels = 0;
	bool bImportVertexColor = false;

	TArray<int32> ImportMaterialIndices;

	TArray<float> PositionData;
	TArray<int32> VertexIndices;
	for (const auto& IdxMesh : IdxMeshes)
	{
		const int32& LodIdx = IdxMesh.Key;
		const FStaticMeshLODResources& RenderMesh = *IdxMesh.Value;

		if (!bImportVertexColor)  // Judge whether we should import vertex color data
		{
			const FColorVertexBuffer* ColorVertexBuffer = &RenderMesh.VertexBuffers.ColorVertexBuffer;
			if (ColorVertexBuffer->GetNumVertices() == RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices())
				bImportVertexColor = true;
			else if (SMC && SMC->LODData.IsValidIndex(LodIdx))
			{
				ColorVertexBuffer = SMC->LODData[LodIdx].OverrideVertexColors;
				if (ColorVertexBuffer && (ColorVertexBuffer->GetNumVertices() == RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices()))
					bImportVertexColor = true;
			}
		}
		

		const uint32 NumOrigPoints = RenderMesh.GetNumVertices();
		TArray<uint32> WeldPointIndices;
		TMap<FVector3f, uint32> PosIdxMap;
		for (uint32 PointIdx = 0; PointIdx < NumOrigPoints; ++PointIdx)
		{
			const FVector3f& Position = RenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition(PointIdx);
			if (const uint32* FoundPointIdxPtr = PosIdxMap.Find(Position))
				WeldPointIndices.Add(*FoundPointIdxPtr);
			else
			{
				const uint32 WeldPointIdx = PosIdxMap.Num();
				PosIdxMap.Add(Position, WeldPointIdx);
				WeldPointIndices.Add(WeldPointIdx);
			}
		}

		for (const auto& PosIdx : PosIdxMap)
		{
			const FVector3f Position = PosIdx.Key * POSITION_SCALE_TO_HOUDINI_F;
			PositionData.Add(Position.X);
			PositionData.Add(Position.Z);
			PositionData.Add(Position.Y);
		}

		const size_t NumPrevPoints = NumPoints;
		NumPoints += PosIdxMap.Num();
		NumUVChannels = FMath::Max(RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(), NumUVChannels);
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();

		for (const FStaticMeshSection& Section : RenderMesh.Sections)
		{
			const uint32 StartTriIdx = Section.FirstIndex / 3;
			const uint32 EndTriIdx = StartTriIdx + Section.NumTriangles;
			for (uint32 TriIdx = StartTriIdx; TriIdx < EndTriIdx; ++TriIdx)
			{
				VertexIndices.Add(NumPrevPoints + WeldPointIndices[RawIndices[TriIdx * 3 + 2]]);
				VertexIndices.Add(NumPrevPoints + WeldPointIndices[RawIndices[TriIdx * 3 + 1]]);
				VertexIndices.Add(NumPrevPoints + WeldPointIndices[RawIndices[TriIdx * 3]]);
				VertexIndices.Add(HOUDINI_SHM_GEO_INPUT_POLY);  // Mark as closed
			}
			NumTriangles += Section.NumTriangles;
			ImportMaterialIndices.AddUnique((LodIdx < 0) ? -1 : Section.MaterialIndex);
		}
	}

	// copy from MeshDesc
	const size_t NumVertices = NumTriangles * 3;  // Finally, @numvtx = @numprim * 3, as all prims are triangles.
	const bool bShouldImportLodGroups = (NumGroupsToImport >= 2);

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumTriangles, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_NORMAL, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@N
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenu
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT2, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenv
	if (bImportVertexColor)
	{
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_COLOR, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@Cd
		SHMGeoInput.AppendAttribute(HAPI_ALPHA, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 1, NumVertices);  // f@Alpha
	}
	for (uint32 UVChannelIdx = 1; UVChannelIdx <= NumUVChannels; ++UVChannelIdx)
		SHMGeoInput.AppendAttribute(UVChannelIdx == 1 ? HAPI_ATTRIB_UV : TCHAR_TO_UTF8(*(TEXT(HAPI_ATTRIB_UV) + FString::FromInt(UVChannelIdx))),
			EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@uv, v@uv2, v@uv3, etc.

	if (bShouldImportLodGroups)
	{
		for (const TPair<int32, const FStaticMeshLODResources*>& IdxMesh : IdxMeshes)
			SHMGeoInput.AppendGroup(IdxMesh.Key < 0 ?  // @group_collision_geo, @group_lod_0, @group_lod_1, @group_lod_2, etc.
				HAPI_GROUP_COLLISION_GEO : (TCHAR_TO_UTF8(*(TEXT(HAPI_GROUP_PREFIX_LOD) + FString::FromInt(IdxMesh.Key)))),
				EHoudiniAttributeOwner::Prim, NumTriangles);
	}
	
	TMap<int32, int32> ImportMatIdxMap;
	TArray<std::string> ImportMaterialRefs;
	bool bHasEmptyRef = false;
	{
		const TArray<FStaticMaterial>& MaterialSlots = SM->GetStaticMaterials();
		for (const int32& MatIdx : ImportMaterialIndices)
		{
			if (MaterialSlots.IsValidIndex(MatIdx))
			{
				UMaterialInterface* Material = IsValid(SMC) ?
					SMC->GetMaterial(SMC->GetMaterialIndex(MaterialSlots[MatIdx].ImportedMaterialSlotName)) : MaterialSlots[MatIdx].MaterialInterface.Get();
				if (IsValid(Material))
				{
					ImportMatIdxMap.Add(MatIdx, ImportMaterialRefs.AddUnique(TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(Material))));
					continue;
				}
			}
			bHasEmptyRef = true;
		}
	}

	const int32 NumUniqueMaterialRefs = (ImportMaterialRefs.Num() + int32(bHasEmptyRef));
	const EHoudiniInputAttributeCompression MaterialAttribCompression = (NumUniqueMaterialRefs >= 2) ?
		EHoudiniInputAttributeCompression::Indexing : EHoudiniInputAttributeCompression::UniqueValue;
	size_t NumMaterialChars = 0;
	{
		for (const std::string& ImportMaterialRef : ImportMaterialRefs)
			NumMaterialChars += ImportMaterialRef.length() + 1;  // Add '\0'
		if (bHasEmptyRef)
			++NumMaterialChars;
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_MATERIAL, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::String, 1,  // s@unreal_material
			(MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? (NumMaterialChars / 4 + 1) : (NumMaterialChars / 4 + 2 + NumTriangles), MaterialAttribCompression);
	}

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)SM), InOutHandle);

	float* PositionDataPtr = SHM;
	FMemory::Memcpy(PositionDataPtr, PositionData.GetData(), size_t(PositionData.Num()) * sizeof(float));
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	FMemory::Memcpy(VertexDataPtr, VertexIndices.GetData(), size_t(VertexIndices.Num()) * sizeof(int32));
	float* NormalDataPtr = (float*)(VertexDataPtr + NumVertices + NumTriangles);
	float* TangentUDataPtr = NormalDataPtr + NumVertices * 3;
	float* TangentVDataPtr = TangentUDataPtr + NumVertices * 3;
	float* ColorDataPtr = nullptr;
	float* AlphaDataPtr = nullptr;
	if (bImportVertexColor)
	{
		ColorDataPtr = TangentVDataPtr + NumVertices * 3;
		AlphaDataPtr = ColorDataPtr + NumVertices * 3;
	}
	float* UVsDataPtr = (bImportVertexColor ? (AlphaDataPtr + NumVertices) : (TangentVDataPtr + NumVertices * 3));
	int32* LodDataPtr = (int32*)(UVsDataPtr + NumVertices * NumUVChannels * 3);
	char* MaterialDataPtr = (char*)(bShouldImportLodGroups ?
		(float*)(LodDataPtr + NumGroupsToImport * NumTriangles) : (UVsDataPtr + NumVertices * NumUVChannels * 3));
	int32* MaterialIndicesDataPtr = (MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? nullptr :
		(((int32*)MaterialDataPtr) + NumMaterialChars / 4 + 2);  // First is num unique strings
	if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
	{
		*((int32*)MaterialDataPtr) = NumUniqueMaterialRefs;
		MaterialDataPtr += sizeof(int32);
	}
	for (const std::string& ImportMaterialRef : ImportMaterialRefs)
	{
		FMemory::Memcpy(MaterialDataPtr, ImportMaterialRef.c_str(), ImportMaterialRef.length());
		MaterialDataPtr += ImportMaterialRef.length() + 1;  // Add '\0'
	}

	size_t NumPrevVertices = 0;  // Houdini vertex
	size_t NumPrevTriangles = 0;
	for (int32 GroupIdx = 0; GroupIdx < NumGroupsToImport; ++GroupIdx)
	{
		const int32& LodIdx = IdxMeshes[GroupIdx].Key;
		const FStaticMeshLODResources& RenderMesh = *(IdxMeshes[GroupIdx].Value);

		const FStaticMeshVertexBuffer& VtxBuffer = RenderMesh.VertexBuffers.StaticMeshVertexBuffer;
		const uint32 NumCurrUVChannels = VtxBuffer.GetNumTexCoords();

		const FColorVertexBuffer* ColorVertexBuffer = nullptr;
		if (bImportVertexColor)
		{
			if (SMC && SMC->LODData.IsValidIndex(LodIdx))
			{
				ColorVertexBuffer = SMC->LODData[LodIdx].OverrideVertexColors;
				if (ColorVertexBuffer && (ColorVertexBuffer->GetNumVertices() != VtxBuffer.GetNumVertices()))
					ColorVertexBuffer = nullptr;
			}
			
			if (!ColorVertexBuffer)
				ColorVertexBuffer = &RenderMesh.VertexBuffers.ColorVertexBuffer;

			if (ColorVertexBuffer->GetNumVertices() != VtxBuffer.GetNumVertices())
				ColorVertexBuffer = nullptr;
		}

		size_t NumCurrTriangles = 0;
		for (const FStaticMeshSection& Section : RenderMesh.Sections)
			NumCurrTriangles += Section.NumTriangles;

		if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
		{
			if (LodIdx >= 0)
			{
				for (const FStaticMeshSection& Section : RenderMesh.Sections)
				{
					int32 ImportIdx = -1;
					if (const int32* FoundImportIdxPtr = ImportMatIdxMap.Find(Section.MaterialIndex))
						ImportIdx = *FoundImportIdxPtr;
					for (int32 MatTriIdx = Section.NumTriangles - 1; MatTriIdx >= 0; --MatTriIdx)
					{
						*MaterialIndicesDataPtr = ImportIdx;
						++MaterialIndicesDataPtr;
					}
				}
			}
			else  // Need not import material for collision geo, so all material refs is empty (-1)
			{
				for (int32 MatTriIdx = NumCurrTriangles - 1; MatTriIdx >= 0; --MatTriIdx)
					MaterialIndicesDataPtr[MatTriIdx] = -1;
				MaterialIndicesDataPtr += NumCurrTriangles;
			}
		}

		// Lods
		if (bShouldImportLodGroups)
		{
			int32* CurrLodDataPtr = LodDataPtr + GroupIdx * NumTriangles + NumPrevTriangles;
			FMemory::Memset(CurrLodDataPtr, 1, NumCurrTriangles * sizeof(int32));
		}
		
		// Attributes
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		for (uint32 TriIdx = 0; TriIdx < NumCurrTriangles; ++TriIdx)
		{
			for (uint32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
			{
				const uint32 RawIdx = RawIndices[TriIdx * 3 + TriVtxIdx];  // Original RenderMesh Vertex Index
				const uint32 VtxIdx = TriIdx * 3 + 2 - TriVtxIdx;  // Houdini polygon vertex index

				const FVector3f Normal = VtxBuffer.VertexTangentZ(RawIdx);
				COPY_VERTEX_VECTOR3f_DATA(NormalDataPtr, Normal);

				const FVector4f Tangent = VtxBuffer.VertexTangentX(RawIdx);
				COPY_VERTEX_VECTOR3f_DATA(TangentUDataPtr, Tangent);

				const FVector3f Binormal = VtxBuffer.VertexTangentY(RawIdx);
				COPY_VERTEX_VECTOR3f_DATA(TangentVDataPtr, Binormal);

				if (ColorDataPtr && AlphaDataPtr)
				{
					const FColor VertexColor = ColorVertexBuffer ? ColorVertexBuffer->VertexColor(RawIdx) : FColor::White;
					float* CurrColorDataPtr = ColorDataPtr + (NumPrevVertices + VtxIdx) * 3;
					*CurrColorDataPtr = float(VertexColor.R) / 255.0f;
					*(CurrColorDataPtr + 1) = float(VertexColor.G) / 255.0f;
					*(CurrColorDataPtr + 2) = float(VertexColor.B) / 255.0f;
					*(AlphaDataPtr + NumPrevVertices + VtxIdx) = float(VertexColor.A) / 255.0f;
				}

				for (uint32 UVChannelIdx = 0; UVChannelIdx < NumUVChannels; ++UVChannelIdx)
				{
					if (UVChannelIdx < NumCurrUVChannels)
					{
						const FVector2f UV = VtxBuffer.GetVertexUV(RawIdx, UVChannelIdx);
						float* CurrUVsDataPtr = UVsDataPtr + (UVChannelIdx * NumVertices + NumPrevVertices + VtxIdx) * 3;
						*CurrUVsDataPtr = UV.X;
						*(CurrUVsDataPtr + 1) = 1.0f - UV.Y;
					}
				}
			}
		}

		NumPrevVertices += NumCurrTriangles * 3;
		NumPrevTriangles += NumCurrTriangles;
	}

	if (InOutSHMInputNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GeoNodeId,
			FString::Printf(TEXT("mesh_%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), InOutSHMInputNodeId));
	
	return SHMGeoInput.HapiUpload(InOutSHMInputNodeId, SHM);
}

bool UHoudiniInputStaticMesh::AppendObjectInfo(const UObject* InSM, FString& InOutInfoStr)
{
	const UStaticMesh* SM = Cast<UStaticMesh>(InSM);
	if (IsValid(SM))
	{
		const FBox BoundingBox = SM->GetBounds().GetBox();
		InOutInfoStr += FString::Printf(TEXT("bounds:%s,%s,%s,%s,%s,%s"), PRINT_HOUDINI_FLOAT(BoundingBox.Min.X), PRINT_HOUDINI_FLOAT(BoundingBox.Max.X),
			PRINT_HOUDINI_FLOAT(BoundingBox.Min.Z), PRINT_HOUDINI_FLOAT(BoundingBox.Max.Z), PRINT_HOUDINI_FLOAT(BoundingBox.Min.Y), PRINT_HOUDINI_FLOAT(BoundingBox.Max.Y));

		return true;
	}
	return false;
}

void UHoudiniInputStaticMesh::SetAsset(UStaticMesh* NewStaticMesh)
{
	if (StaticMesh != NewStaticMesh)
	{
		StaticMesh = NewStaticMesh;
		RequestReimport();
	}
}

bool UHoudiniInputStaticMesh::IsObjectExists() const
{
	return IsValid(StaticMesh.LoadSynchronous());
}

bool UHoudiniInputStaticMesh::HapiUpload()
{
	UStaticMesh* SM = StaticMesh.LoadSynchronous();
	if (!IsValid(SM))
		return HapiDestroy();

	if (GetSettings().bImportAsReference)
	{
		if (MeshNodeId >= 0)  // Means previous import the whole geo, so we should destroy the prev nodes and create new node
			HOUDINI_FAIL_RETURN(HapiDestroy());

		const bool bCreateNewNode = (SettingsNodeId < 0);
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("reference_%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
		
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(SettingsNodeId, FVector3f::ZeroVector));
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId, HAPI_ATTRIB_UNREAL_INSTANCE, FHoudiniEngineUtils::GetAssetReference(SM)));
		
		const FBox Box = SM->GetBounds().GetBox();
		const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
		const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddDictAttribute(SettingsNodeId,
			HAPI_ATTRIB_UNREAL_OBJECT_METADATA, FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y)));
		
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}
	else
	{
		if ((SettingsNodeId >= 0) && (MeshNodeId < 0))  // Means previous import as ref, so we should destroy the prev node and create new nodes
			HapiDestroy();

		HOUDINI_FAIL_RETURN(HapiImport(SM, nullptr, GetSettings(), GetGeoNodeId(), MeshNodeId, MeshHandle));

		const bool bCreateNewSettingsNode = (SettingsNodeId < 0);
		if (bCreateNewSettingsNode)
		{
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupMeshInput::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupMeshInput::HapiConnectInput(SettingsNodeId, MeshNodeId));
		}
		
		FHoudiniEngineSetupMeshInput MeshSettings;
		MeshSettings.AppendAttribute(HAPI_ATTRIB_UNREAL_INSTANCE, EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SM));
		HOUDINI_FAIL_RETURN(MeshSettings.HapiUpload(SettingsNodeId));
		if (bCreateNewSettingsNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}

	bHasChanged = false;

	return true;
}

bool UHoudiniInputStaticMesh::HapiDestroy()
{
	if (MeshNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MeshNodeId));

	if (SettingsNodeId >= 0)
	{
		GetInput()->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
	}

	Invalidate();

	return true;
}

void UHoudiniInputStaticMesh::Invalidate()
{
	SettingsNodeId = -1;
	MeshNodeId = -1;
	bHasChanged = false;
	if (MeshHandle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(MeshHandle);
		MeshHandle = 0;
	}
}


// -------- FoliageType --------
void FHoudiniFoliageTypeInputBuilder::AppendAllowClasses(TArray<const UClass*>& InOutAllowClasses)
{
	InOutAllowClasses.Add(UFoliageType_InstancedStaticMesh::StaticClass());
}

bool FHoudiniFoliageTypeInputBuilder::GetInfo(const UObject* Asset, FString& OutInfoStr)
{
	if (const UFoliageType_InstancedStaticMesh* FT = Cast<UFoliageType_InstancedStaticMesh>(Asset))
	{
		UStaticMesh* SM = FT->GetStaticMesh();
		if (IsValid(SM))
		{
			OutInfoStr = FHoudiniEngineUtils::GetAssetReference(SM) + TEXT(";");
			return UHoudiniInputStaticMesh::AppendObjectInfo(SM, OutInfoStr);
		}

		return true;
	}

	return false;
}

UHoudiniInputHolder* FHoudiniFoliageTypeInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<UFoliageType_InstancedStaticMesh, UHoudiniInputFoliageType>(Input, Asset, OldHolder);
}

void UHoudiniInputFoliageType::SetAsset(UFoliageType_InstancedStaticMesh* NewStaticMesh)
{
	if (FoliageType != NewStaticMesh)
	{
		FoliageType = NewStaticMesh;
		RequestReimport();
	}
}

TSoftObjectPtr<UObject> UHoudiniInputFoliageType::GetObject() const
{
	return FoliageType;
}

bool UHoudiniInputFoliageType::IsObjectExists() const
{
	return IsValid(FoliageType.LoadSynchronous());
}

bool UHoudiniInputFoliageType::HapiUpload()
{
	UFoliageType_InstancedStaticMesh* FT = FoliageType.LoadSynchronous();
	if (!IsValid(FT))
		return HapiDestroy();

	UStaticMesh* SM = FT->GetStaticMesh();
	
	// TODO: check SM is valid?

	if (!IsValid(SM) || GetSettings().bImportAsReference)
	{
		if (MeshNodeId >= 0)  // Means previous import the whole geo, so we should destroy the prev nodes and create new node
			HapiDestroy();

		const bool bCreateNewNode = (SettingsNodeId < 0);
		if (bCreateNewNode)
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), GetGeoNodeId(), "null",
				TCHAR_TO_UTF8(*FString::Printf(TEXT("reference_%s_%08X"), *FT->GetName(), FPlatformTime::Cycles())), false, &SettingsNodeId));
		
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(SettingsNodeId, FVector3f::ZeroVector));
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId, HAPI_ATTRIB_UNREAL_INSTANCE, FHoudiniEngineUtils::GetAssetReference(FT)));
		if (IsValid(SM))
		{
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, FHoudiniEngineUtils::GetAssetReference(SM)));

			const FBox Box = SM->GetBounds().GetBox();
			const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
			const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
			FHoudiniSopNull::HapiAddDictAttribute(SettingsNodeId, HAPI_ATTRIB_UNREAL_OBJECT_METADATA, FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y));
		}

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}
	else
	{
		if (SettingsNodeId >= 0 && MeshNodeId < 0)  // Means previous import as ref, so we should destroy the prev node and create new nodes
			HapiDestroy();

		HOUDINI_FAIL_RETURN(UHoudiniInputStaticMesh::HapiImport(SM, nullptr, GetSettings(), GetGeoNodeId(), MeshNodeId, MeshHandle));

		const bool bCreateNewSettingsNode = (SettingsNodeId < 0);
		if (bCreateNewSettingsNode)
		{
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupMeshInput::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupMeshInput::HapiConnectInput(SettingsNodeId, MeshNodeId));
		}
		
		FHoudiniEngineSetupMeshInput MeshSettings;
		MeshSettings.AppendAttribute(HAPI_ATTRIB_UNREAL_INSTANCE, EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(FT));
		MeshSettings.AppendAttribute(HAPI_ATTRIB_UNREAL_OBJECT_PATH, EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SM));
		HOUDINI_FAIL_RETURN(MeshSettings.HapiUpload(SettingsNodeId));
		if (bCreateNewSettingsNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}

	bHasChanged = false;

	return true;
}

bool UHoudiniInputFoliageType::HapiDestroy()
{
	if (MeshNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MeshNodeId));

	if (SettingsNodeId >= 0)
	{
		GetInput()->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
	}

	Invalidate();

	return true;
}

void UHoudiniInputFoliageType::Invalidate()
{
	SettingsNodeId = -1;
	MeshNodeId = -1;
	bHasChanged = false;
	if (MeshHandle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(MeshHandle);
		MeshHandle = 0;
	}
}


// -------- SkeletalMesh --------
UHoudiniInputHolder* FHoudiniSkeletalMeshInputBuilder::CreateOrUpdate(UHoudiniInput* Input, UObject* Asset, UHoudiniInputHolder* OldHolder)
{
	return CreateOrUpdateHolder<USkeletalMesh, UHoudiniInputSkeletalMesh>(Input, Asset, OldHolder);
}

bool UHoudiniInputSkeletalMesh::HapiImportMeshDescription(const USkeletalMesh* SM, const USkeletalMeshComponent* SMC,
	const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle)
{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
	TArray<TPair<int32, const FMeshDescription*>> IdxMeshDescs;
	switch (Settings.LODImportMethod)
	{
	case EHoudiniStaticMeshLODImportMethod::FirstLOD:  // Try find the first MeshDesc
	{
		for (int32 LodIdx = 0; LodIdx < SM->GetNumSourceModels(); ++LodIdx)
		{
			if (const FMeshDescription* MeshDesc = SM->GetMeshDescription(LodIdx))
			{
				IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(LodIdx, MeshDesc));
				break;
			}
		}
	}
	break;
	case EHoudiniStaticMeshLODImportMethod::AllLODs:  // Try gather all MeshDesc
	{
		for (int32 LodIdx = 0; LodIdx < SM->GetNumSourceModels(); ++LodIdx)
		{
			if (const FMeshDescription* MeshDesc = SM->GetMeshDescription(LodIdx))
				IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(LodIdx, MeshDesc));
		}
	}
	break;
	case EHoudiniStaticMeshLODImportMethod::LastLOD:  // Try find the last MeshDesc
	{
		for (int32 LodIdx = SM->GetNumSourceModels() - 1; LodIdx >= 0; --LodIdx)
		{
			if (const FMeshDescription* MeshDesc = SM->GetMeshDescription(LodIdx))
			{
				IdxMeshDescs.Add(TPair<int32, const FMeshDescription*>(LodIdx, MeshDesc));
				break;
			}
		}
	}
	break;
	}

	const int32 NumGroupsToImport = IdxMeshDescs.Num();

	// First of all, we should Get SlotName-MaterialPath map
	TArray<std::string> SlotIdxMaterialRefs;
	for (const FSkeletalMaterial& SkeletalMaterial : SM->GetMaterials())
	{
		std::string MaterialRef;
		if (IsValid(SMC))
		{
			UMaterialInterface* OverrideMaterialInterface = SMC->GetMaterial(SMC->GetMaterialIndex(SkeletalMaterial.ImportedMaterialSlotName));
			if (IsValid(OverrideMaterialInterface))
				MaterialRef = TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(OverrideMaterialInterface));
		}
		else if (IsValid(SkeletalMaterial.MaterialInterface))
			MaterialRef = TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(SkeletalMaterial.MaterialInterface.Get()));

		SlotIdxMaterialRefs.Add(MaterialRef);
	}

	// We need to statistic @numpt, @numprim, and then @numvtx = @numprim * 3 (all triangles) for All LODs need to import
	size_t NumPoints = 0;
	size_t NumTriangles = 0;
	int32 NumUVChannels = 0;

	TArray<int32> BoneDataArrayLens;
	TArray<int32> BoneIndices;
	TArray<float> BoneWeights;

	TArray<int32> ImportMaterialIndices;
	for (const TPair<int32, const FMeshDescription*>& IdxMeshDesc : IdxMeshDescs)
	{
		const int32& LodIdx = IdxMeshDesc.Key;
		const FMeshDescription* MeshDesc = IdxMeshDesc.Value;

		NumPoints += MeshDesc->Vertices().Num();  // accumulate @numpt
		NumTriangles += MeshDesc->Triangles().Num();  // accumulate @numprim
		NumUVChannels = FMath::Max(MeshDesc->GetNumUVElementChannels(), NumUVChannels);

		// Count material data length here
		if (LodIdx < 0)
			ImportMaterialIndices.AddUnique(-1);
		else
		{
			for (const FPolygonGroupID& PolygonGroupID : MeshDesc->PolygonGroups().GetElementIDs())
			{
				const TArray<int32>& LODMatIdcs = SM->GetLODInfo(LodIdx)->LODMaterialMap;
				ImportMaterialIndices.AddUnique(LODMatIdcs.IsEmpty() ? PolygonGroupID.GetValue() : LODMatIdcs[PolygonGroupID.GetValue()]);
			}
		}

		FSkeletalMeshConstAttributes Attributes(*MeshDesc);
		FSkinWeightsVertexAttributesConstRef VertexSkinWeights = Attributes.GetVertexSkinWeights();
		for (const FVertexID& VtxID : MeshDesc->Vertices().GetElementIDs())
		{
			FVertexBoneWeightsConst VertexBoneWeights = VertexSkinWeights.Get(VtxID);
			int32 WeightCount = 0;
			for (const UE::AnimationCore::FBoneWeight& BoneWeight : VertexBoneWeights)
			{
				// Get normalized weight
				const float Weight = BoneWeight.GetWeight();
				if (Weight > 0.0f)
				{
					BoneWeights.Add(Weight);
					const FBoneIndexType BoneIndex = BoneWeight.GetBoneIndex();
					BoneIndices.Add(BoneIndex);
					++WeightCount;
				}
			}
			BoneDataArrayLens.Add(WeightCount);
		}
	}

	const size_t NumVertices = NumTriangles * 3;  // Finally, @numvtx = @numprim * 3, as all prims are triangles.
	const bool bShouldImportLodGroups = (NumGroupsToImport >= 2);

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumTriangles, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_BONE_CAPTURE_INDEX, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::IntArray, 1, BoneDataArrayLens.Num() + BoneIndices.Num());  // i[]@boneCapture_index
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_BONE_CAPTURE_DATA, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::FloatArray, 1, BoneDataArrayLens.Num() + BoneWeights.Num());  // f[]@boneCapture_data
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_NORMAL, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@N
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenu
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT2, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenv
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_COLOR, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@Cd
	SHMGeoInput.AppendAttribute(HAPI_ALPHA, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 1, NumVertices);  // f@Alpha
	for (int32 UVChannelIdx = 1; UVChannelIdx <= NumUVChannels; ++UVChannelIdx)
		SHMGeoInput.AppendAttribute(UVChannelIdx == 1 ? HAPI_ATTRIB_UV : TCHAR_TO_UTF8(*(TEXT(HAPI_ATTRIB_UV) + FString::FromInt(UVChannelIdx))),
			EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@uv, v@uv2, v@uv3, etc.

	if (bShouldImportLodGroups)
	{
		for (const TPair<int32, const FMeshDescription*>& IdxMeshDesc : IdxMeshDescs)
			SHMGeoInput.AppendGroup(IdxMeshDesc.Key < 0 ?  // @group_collision_geo, @group_lod_0, @group_lod_1, @group_lod_2, etc.
				HAPI_GROUP_COLLISION_GEO : (TCHAR_TO_UTF8(*(TEXT(HAPI_GROUP_PREFIX_LOD) + FString::FromInt(IdxMeshDesc.Key)))),
				EHoudiniAttributeOwner::Prim, NumTriangles);
	}
	
	TMap<int32, int32> ImportMatIdxMap;
	TArray<std::string> ImportMaterialRefs;
	bool bHasEmptyRef = false;
	{
		const TArray<FSkeletalMaterial>& MaterialSlots = SM->GetMaterials();
		for (const int32& MatIdx : ImportMaterialIndices)
		{
			if (MaterialSlots.IsValidIndex(MatIdx))
			{
				UMaterialInterface* Material = IsValid(SMC) ?
					SMC->GetMaterial(SMC->GetMaterialIndex(MaterialSlots[MatIdx].ImportedMaterialSlotName)) : MaterialSlots[MatIdx].MaterialInterface.Get();
				if (IsValid(Material))
				{
					ImportMatIdxMap.Add(MatIdx, ImportMaterialRefs.AddUnique(TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(Material))));
					continue;
				}
			}
			bHasEmptyRef = true;
		}
	}

	const int32 NumUniqueMaterialRefs = (ImportMaterialRefs.Num() + int32(bHasEmptyRef));
	const EHoudiniInputAttributeCompression MaterialAttribCompression = (NumUniqueMaterialRefs >= 2) ?
		EHoudiniInputAttributeCompression::Indexing : EHoudiniInputAttributeCompression::UniqueValue;
	size_t NumMaterialChars = 0;
	{
		for (const std::string& ImportMaterialRef : ImportMaterialRefs)
			NumMaterialChars += ImportMaterialRef.length() + 1;  // Add '\0'
		if (bHasEmptyRef)
			++NumMaterialChars;
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_MATERIAL, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::String, 1,  // s@unreal_material
			(MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? (NumMaterialChars / 4 + 1) : (NumMaterialChars / 4 + 2 + NumTriangles), MaterialAttribCompression);
	}

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)SM), InOutHandle);

	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	
	int32* BoneDataPtr = VertexDataPtr + NumVertices + NumTriangles;
	FMemory::Memcpy(BoneDataPtr, BoneDataArrayLens.GetData(), BoneDataArrayLens.Num() * sizeof(int32));
	BoneDataPtr += BoneDataArrayLens.Num();
	FMemory::Memcpy(BoneDataPtr, BoneIndices.GetData(), BoneIndices.Num() * sizeof(int32));
	BoneDataPtr += BoneIndices.Num();
	FMemory::Memcpy(BoneDataPtr, BoneDataArrayLens.GetData(), BoneDataArrayLens.Num() * sizeof(int32));
	BoneDataPtr += BoneDataArrayLens.Num();
	FMemory::Memcpy(BoneDataPtr, BoneWeights.GetData(), BoneWeights.Num() * sizeof(float));

	float* NormalDataPtr = (float*)(BoneDataPtr + BoneWeights.Num());
	float* TangentUDataPtr = NormalDataPtr + NumVertices * 3;
	float* TangentVDataPtr = TangentUDataPtr + NumVertices * 3;
	float* ColorDataPtr = TangentVDataPtr + NumVertices * 3;
	float* AlphaDataPtr = ColorDataPtr + NumVertices * 3;
	float* UVsDataPtr = AlphaDataPtr + NumVertices;
	int32* GroupDataPtr = (int32*)(UVsDataPtr + NumVertices * NumUVChannels * 3);
	char* MaterialDataPtr = (char*)(bShouldImportLodGroups ?
		(GroupDataPtr + NumGroupsToImport * NumTriangles) : (int32*)(UVsDataPtr + NumVertices * NumUVChannels * 3));
	int32* MaterialIndicesDataPtr = (MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? nullptr :
		(((int32*)MaterialDataPtr) + NumMaterialChars / 4 + 2);  // First is num unique strings
	if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
	{
		*((int32*)MaterialDataPtr) = NumUniqueMaterialRefs;
		MaterialDataPtr += sizeof(int32);
	}
	for (const std::string& ImportMaterialRef : ImportMaterialRefs)
	{
		FMemory::Memcpy(MaterialDataPtr, ImportMaterialRef.c_str(), ImportMaterialRef.length());
		MaterialDataPtr += ImportMaterialRef.length() + 1;  // Add '\0'
	}

	size_t NumPrevPoints = 0;
	size_t NumPrevVertices = 0;
	size_t NumPrevTriangles = 0;
	for (int32 GroupIdx = 0; GroupIdx < NumGroupsToImport; ++GroupIdx)
	{
		const int32& LodIdx = IdxMeshDescs[GroupIdx].Key;
		const FMeshDescription* MeshDesc = IdxMeshDescs[GroupIdx].Value;

		for (const FVertexID& VtxID : MeshDesc->Vertices().GetElementIDs())
		{
			const FVector3f Position = MeshDesc->GetVertexPosition(VtxID) * POSITION_SCALE_TO_HOUDINI_F;
			*PositionDataPtr = Position.X;
			*(PositionDataPtr + 1) = Position.Z;
			*(PositionDataPtr + 2) = Position.Y;
			PositionDataPtr += 3;
		}

		FSkeletalMeshConstAttributes Attributes(*MeshDesc);
		//TVertexAttributesConstRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesConstRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesConstRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
		//TEdgeAttributesConstRef<bool> EdgeHardnesses = Attributes.GetEdgeHardnesses();
		
		int32* CurrLodDataPtr = bShouldImportLodGroups ? (GroupDataPtr + GroupIdx * NumTriangles + NumPrevTriangles) : nullptr;
		TArray<FVertexInstanceID> VtxInstIDs;  // This is houdini vertex order
		VtxInstIDs.Reserve(MeshDesc->Triangles().Num());
		for (const FTriangleID& TriID : MeshDesc->Triangles().GetElementIDs())
		{
			const TArrayView<const FVertexID>& TriVtcs = MeshDesc->GetTriangleVertices(TriID);
			*VertexDataPtr = NumPrevPoints + TriVtcs[2].GetValue();
			*(VertexDataPtr + 1) = NumPrevPoints + TriVtcs[1].GetValue();
			*(VertexDataPtr + 2) = NumPrevPoints + TriVtcs[0].GetValue();
			*(VertexDataPtr + 3) = HOUDINI_SHM_GEO_INPUT_POLY;
			VertexDataPtr += 4;

			const TArrayView<const FVertexInstanceID> TriVtxInsts = MeshDesc->GetTriangleVertexInstances(TriID);
			VtxInstIDs.Add(TriVtxInsts[2]);
			VtxInstIDs.Add(TriVtxInsts[1]);
			VtxInstIDs.Add(TriVtxInsts[0]);

			if (CurrLodDataPtr)
			{
				*CurrLodDataPtr = 1;
				++CurrLodDataPtr;
			}
		}

		const int32 NumCurrUVChannels = MeshDesc->GetNumUVElementChannels();

		for (int32 VtxIdx = 0; VtxIdx < VtxInstIDs.Num(); ++VtxIdx)
		{
			const FVertexInstanceID& VtxInstID = VtxInstIDs[VtxIdx];

			const FVector3f Normal = VertexInstanceNormals[VtxInstID];
			COPY_VERTEX_VECTOR3f_DATA(NormalDataPtr, Normal);

			const FVector3f Tangent = VertexInstanceTangents[VtxInstID];
			COPY_VERTEX_VECTOR3f_DATA(TangentUDataPtr, Tangent);

			const FVector3f Binormal = (FVector3f::CrossProduct(Normal, Tangent).GetSafeNormal() * VertexInstanceBinormalSigns[VtxInstID]);
			COPY_VERTEX_VECTOR3f_DATA(TangentVDataPtr, Binormal);

			const FVector4f VertexColor = VertexInstanceColors[VtxInstID];
			float* CurrColorDataPtr = ColorDataPtr + (NumPrevVertices + VtxIdx) * 3;
			*CurrColorDataPtr = VertexColor.X;
			*(CurrColorDataPtr + 1) = VertexColor.Y;
			*(CurrColorDataPtr + 2) = VertexColor.Z;
			*(AlphaDataPtr + NumPrevVertices + VtxIdx) = VertexColor.W;

			for (int32 UVChannelIdx = 0; UVChannelIdx < NumUVChannels; ++UVChannelIdx)
			{
				if (UVChannelIdx < NumCurrUVChannels)
				{
					const FVector2f UV = VertexInstanceUVs.Get(VtxInstID, UVChannelIdx);
					float* CurrUVsDataPtr = UVsDataPtr + (UVChannelIdx * NumVertices + NumPrevVertices + VtxIdx) * 3;
					*CurrUVsDataPtr = UV.X;
					*(CurrUVsDataPtr + 1) = 1.0f - UV.Y;
				}
			}
		}

		if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
		{
			if (LodIdx >= 0)
			{
				for (const FPolygonGroupID& PolygonGroupID : MeshDesc->PolygonGroups().GetElementIDs())
				{
					int32 ImportIdx = -1;
					const TArray<int32>& LODMatIdcs = SM->GetLODInfo(LodIdx)->LODMaterialMap;
					if (const int32* FoundImportIdxPtr = ImportMatIdxMap.Find(LODMatIdcs.IsEmpty() ? PolygonGroupID.GetValue() : LODMatIdcs[PolygonGroupID.GetValue()]))
						ImportIdx = *FoundImportIdxPtr;
					for (const FTriangleID& TriID : MeshDesc->GetPolygonGroupTriangles(PolygonGroupID))
						MaterialIndicesDataPtr[NumPrevTriangles + TriID] = ImportIdx;
				}
			}
			else  // Need not import material for collision geo, so all material refs is empty (-1)
			{
				for (int32 MatTriIdx = MeshDesc->Triangles().Num() - 1; MatTriIdx >= 0; --MatTriIdx)
					MaterialIndicesDataPtr[NumPrevTriangles + MatTriIdx] = -1;
			}
		}

		NumPrevPoints += MeshDesc->Vertices().Num();
		NumPrevVertices += MeshDesc->VertexInstances().Num();
		NumPrevTriangles += MeshDesc->Triangles().Num();
	}

	if (InOutSHMInputNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GeoNodeId,
			FString::Printf(TEXT("mesh_%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), InOutSHMInputNodeId));

	return SHMGeoInput.HapiUpload(InOutSHMInputNodeId, SHM);
#else
	return HapiImportRenderData(SM, SMC, Settings, GeoNodeId, InOutSHMInputNodeId, InOutHandle);
#endif
}

bool UHoudiniInputSkeletalMesh::HapiImportRenderData(const USkeletalMesh* SM, const USkeletalMeshComponent* SMC,
	const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle)
{
	TArray<TPair<int32, const FSkeletalMeshLODRenderData*>> IdxMeshes;
	const FSkeletalMeshRenderData* RenderData = SM->GetResourceForRendering();
	switch (Settings.LODImportMethod)
	{
	case EHoudiniStaticMeshLODImportMethod::FirstLOD:  // Try find the first MeshDesc
		if (RenderData->LODRenderData.Num() >= 1)
			IdxMeshes.Add(TPair<int32, const FSkeletalMeshLODRenderData*>(0, &RenderData->LODRenderData[0]));
		break;
	case EHoudiniStaticMeshLODImportMethod::AllLODs:  // Try gather all MeshDesc
	{
		for (int32 LodIdx = 0; LodIdx < RenderData->LODRenderData.Num(); ++LodIdx)
			IdxMeshes.Add(TPair<int32, const FSkeletalMeshLODRenderData*>(LodIdx, &RenderData->LODRenderData[LodIdx]));
	}
	break;
	case EHoudiniStaticMeshLODImportMethod::LastLOD:  // Try find the last MeshDesc
	{
		const int32 NumLODs = RenderData->LODRenderData.Num();
		if (NumLODs >= 1)
			IdxMeshes.Add(TPair<int32, const FSkeletalMeshLODRenderData*>(NumLODs - 1, &RenderData->LODRenderData[NumLODs - 1]));
	}
	break;
	}

	const int32 NumGroupsToImport = IdxMeshes.Num();

	// We need to statistic @numpt, @numprim, and then @numvtx = @numprim * 3 (all triangles) for All LODs need to import
	size_t NumPoints = 0;
	size_t NumTriangles = 0;
	uint32 NumUVChannels = 0;
	bool bImportVertexColor = false;

	TArray<int32> ImportMaterialIndices;

	TArray<int32> BoneDataArrayLens;
	TArray<int32> BoneIndices;
	TArray<float> BoneWeights;

	TArray<float> PositionData;
	TArray<int32> VertexIndices;
	for (const auto& IdxMesh : IdxMeshes)
	{
		const int32& LodIdx = IdxMesh.Key;
		const FSkeletalMeshLODRenderData& RenderMesh = *IdxMesh.Value;

		if (!bImportVertexColor)  // Judge whether we should import vertex color data
		{
			const FColorVertexBuffer* ColorVertexBuffer = &RenderMesh.StaticVertexBuffers.ColorVertexBuffer;
			if (ColorVertexBuffer->GetNumVertices() == RenderMesh.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumVertices())
				bImportVertexColor = true;
			else if (SMC && SMC->LODInfo.IsValidIndex(LodIdx))
			{
				ColorVertexBuffer = SMC->LODInfo[LodIdx].OverrideVertexColors;
				if (ColorVertexBuffer && (ColorVertexBuffer->GetNumVertices() == RenderMesh.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumVertices()))
					bImportVertexColor = true;
			}
		}

		TArray<uint32> WeldPointIndices;
		TMap<FVector3f, uint32> PosIdxMap;
		for (const FSkelMeshRenderSection& Section : RenderMesh.RenderSections)  // The bone index is local of its section, we need FSkelMeshRenderSection::BoneMap to get true bone index
		{
			for (uint32 PointIdx = Section.BaseVertexIndex; PointIdx < Section.BaseVertexIndex + Section.NumVertices; ++PointIdx)
			{
				const FVector3f& Position = RenderMesh.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(PointIdx);
				if (const uint32* FoundPointIdxPtr = PosIdxMap.Find(Position))
					WeldPointIndices.Add(*FoundPointIdxPtr);
				else
				{
					const uint32 WeldPointIdx = PosIdxMap.Num();
					PosIdxMap.Add(Position, WeldPointIdx);
					WeldPointIndices.Add(WeldPointIdx);

					int32 WeightCount = 0;
					for (uint32 InfluenceIdx = 0; InfluenceIdx < RenderMesh.SkinWeightVertexBuffer.GetMaxBoneInfluences(); ++InfluenceIdx)
					{
						uint16 BoneWeight = RenderMesh.SkinWeightVertexBuffer.GetBoneWeight(PointIdx, InfluenceIdx);
						if (BoneWeight)
						{
							BoneIndices.Add(Section.BoneMap[RenderMesh.SkinWeightVertexBuffer.GetBoneIndex(PointIdx, InfluenceIdx)]);  // The bone index is local of its section, we need BoneMap to get true bone index
							BoneWeights.Add(float(BoneWeight) / float(MAX_uint16));
							++WeightCount;
						}
					}
					BoneDataArrayLens.Add(WeightCount);
				}
			}
		}

		for (const auto& PosIdx : PosIdxMap)
		{
			const FVector3f Position = PosIdx.Key * POSITION_SCALE_TO_HOUDINI_F;
			PositionData.Add(Position.X);
			PositionData.Add(Position.Z);
			PositionData.Add(Position.Y);
		}

		const size_t NumPrevPoints = NumPoints;
		NumPoints += PosIdxMap.Num();
		NumUVChannels = FMath::Max(RenderMesh.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(), NumUVChannels);
		const FRawStaticIndexBuffer16or32Interface* RawIndices = RenderMesh.MultiSizeIndexContainer.GetIndexBuffer();

		for (const FSkelMeshRenderSection& Section : RenderMesh.RenderSections)
		{
			const uint32 StartTriIdx = Section.BaseIndex / 3;
			const uint32 EndTriIdx = StartTriIdx + Section.NumTriangles;
			for (uint32 TriIdx = StartTriIdx; TriIdx < EndTriIdx; ++TriIdx)
			{
				VertexIndices.Add(NumPrevPoints + WeldPointIndices[RawIndices->Get(TriIdx * 3 + 2)]);
				VertexIndices.Add(NumPrevPoints + WeldPointIndices[RawIndices->Get(TriIdx * 3 + 1)]);
				VertexIndices.Add(NumPrevPoints + WeldPointIndices[RawIndices->Get(TriIdx * 3)]);
				VertexIndices.Add(HOUDINI_SHM_GEO_INPUT_POLY);  // Mark as closed
			}
			NumTriangles += Section.NumTriangles;
			ImportMaterialIndices.AddUnique((LodIdx < 0) ? -1 : Section.MaterialIndex);
		}
	}

	// copy from MeshDesc
	const size_t NumVertices = NumTriangles * 3;  // Finally, @numvtx = @numprim * 3, as all prims are triangles.
	const bool bShouldImportLodGroups = (NumGroupsToImport >= 2);

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumTriangles, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_BONE_CAPTURE_INDEX, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::IntArray, 1, BoneDataArrayLens.Num() + BoneIndices.Num());  // i[]@boneCapture_index
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_BONE_CAPTURE_DATA, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::FloatArray, 1, BoneDataArrayLens.Num() + BoneWeights.Num());  // f[]@boneCapture_data
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_NORMAL, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@N
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenu
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TANGENT2, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@tangenv
	if (bImportVertexColor)
	{
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_COLOR, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@Cd
		SHMGeoInput.AppendAttribute(HAPI_ALPHA, EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 1, NumVertices);  // f@Alpha
	}
	for (uint32 UVChannelIdx = 1; UVChannelIdx <= NumUVChannels; ++UVChannelIdx)
		SHMGeoInput.AppendAttribute(UVChannelIdx == 1 ? HAPI_ATTRIB_UV : TCHAR_TO_UTF8(*(TEXT(HAPI_ATTRIB_UV) + FString::FromInt(UVChannelIdx))),
			EHoudiniAttributeOwner::Vertex, EHoudiniInputAttributeStorage::Float, 3, NumVertices * 3);  // v@uv, v@uv2, v@uv3, etc.

	if (bShouldImportLodGroups)
	{
		for (const TPair<int32, const FSkeletalMeshLODRenderData*>& IdxMesh : IdxMeshes)
			SHMGeoInput.AppendGroup(IdxMesh.Key < 0 ?  // @group_collision_geo, @group_lod_0, @group_lod_1, @group_lod_2, etc.
				HAPI_GROUP_COLLISION_GEO : (TCHAR_TO_UTF8(*(TEXT(HAPI_GROUP_PREFIX_LOD) + FString::FromInt(IdxMesh.Key)))),
				EHoudiniAttributeOwner::Prim, NumTriangles);
	}
	
	TMap<int32, int32> ImportMatIdxMap;
	TArray<std::string> ImportMaterialRefs;
	bool bHasEmptyRef = false;
	{
		const TArray<FSkeletalMaterial>& MaterialSlots = SM->GetMaterials();
		for (const int32& MatIdx : ImportMaterialIndices)
		{
			if (MaterialSlots.IsValidIndex(MatIdx))
			{
				UMaterialInterface* Material = IsValid(SMC) ?
					SMC->GetMaterial(SMC->GetMaterialIndex(MaterialSlots[MatIdx].ImportedMaterialSlotName)) : MaterialSlots[MatIdx].MaterialInterface.Get();
				if (IsValid(Material))
				{
					ImportMatIdxMap.Add(MatIdx, ImportMaterialRefs.AddUnique(TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(Material))));
					continue;
				}
			}
			bHasEmptyRef = true;
		}
	}

	const int32 NumUniqueMaterialRefs = (ImportMaterialRefs.Num() + int32(bHasEmptyRef));
	const EHoudiniInputAttributeCompression MaterialAttribCompression = (NumUniqueMaterialRefs >= 2) ?
		EHoudiniInputAttributeCompression::Indexing : EHoudiniInputAttributeCompression::UniqueValue;
	size_t NumMaterialChars = 0;
	{
		for (const std::string& ImportMaterialRef : ImportMaterialRefs)
			NumMaterialChars += ImportMaterialRef.length() + 1;  // Add '\0'
		if (bHasEmptyRef)
			++NumMaterialChars;
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_MATERIAL, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::String, 1,  // s@unreal_material
			(MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? (NumMaterialChars / 4 + 1) : (NumMaterialChars / 4 + 2 + NumTriangles), MaterialAttribCompression);
	}

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)SM), InOutHandle);

	float* PositionDataPtr = SHM;
	FMemory::Memcpy(PositionDataPtr, PositionData.GetData(), size_t(PositionData.Num()) * sizeof(float));
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	FMemory::Memcpy(VertexDataPtr, VertexIndices.GetData(), size_t(VertexIndices.Num()) * sizeof(int32));
	
	int32* BoneDataPtr = VertexDataPtr + NumVertices + NumTriangles;
	FMemory::Memcpy(BoneDataPtr, BoneDataArrayLens.GetData(), BoneDataArrayLens.Num() * sizeof(int32));
	BoneDataPtr += BoneDataArrayLens.Num();
	FMemory::Memcpy(BoneDataPtr, BoneIndices.GetData(), BoneIndices.Num() * sizeof(int32));
	BoneDataPtr += BoneIndices.Num();
	FMemory::Memcpy(BoneDataPtr, BoneDataArrayLens.GetData(), BoneDataArrayLens.Num() * sizeof(int32));
	BoneDataPtr += BoneDataArrayLens.Num();
	FMemory::Memcpy(BoneDataPtr, BoneWeights.GetData(), BoneWeights.Num() * sizeof(float));

	float* NormalDataPtr = (float*)(BoneDataPtr + BoneWeights.Num());
	float* TangentUDataPtr = NormalDataPtr + NumVertices * 3;
	float* TangentVDataPtr = TangentUDataPtr + NumVertices * 3;
	float* ColorDataPtr = nullptr;
	float* AlphaDataPtr = nullptr;
	if (bImportVertexColor)
	{
		ColorDataPtr = TangentVDataPtr + NumVertices * 3;
		AlphaDataPtr = ColorDataPtr + NumVertices * 3;
	}
	float* UVsDataPtr = (bImportVertexColor ? (AlphaDataPtr + NumVertices) : (TangentVDataPtr + NumVertices * 3));
	int32* LodDataPtr = (int32*)(UVsDataPtr + NumVertices * NumUVChannels * 3);
	char* MaterialDataPtr = (char*)(bShouldImportLodGroups ?
		(float*)(LodDataPtr + NumGroupsToImport * NumTriangles) : (UVsDataPtr + NumVertices * NumUVChannels * 3));
	int32* MaterialIndicesDataPtr = (MaterialAttribCompression == EHoudiniInputAttributeCompression::UniqueValue) ? nullptr :
		(((int32*)MaterialDataPtr) + NumMaterialChars / 4 + 2);  // First is num unique strings
	if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
	{
		*((int32*)MaterialDataPtr) = NumUniqueMaterialRefs;
		MaterialDataPtr += sizeof(int32);
	}
	for (const std::string& ImportMaterialRef : ImportMaterialRefs)
	{
		FMemory::Memcpy(MaterialDataPtr, ImportMaterialRef.c_str(), ImportMaterialRef.length());
		MaterialDataPtr += ImportMaterialRef.length() + 1;  // Add '\0'
	}

	size_t NumPrevVertices = 0;  // Houdini vertex
	size_t NumPrevTriangles = 0;
	for (int32 GroupIdx = 0; GroupIdx < NumGroupsToImport; ++GroupIdx)
	{
		const int32& LodIdx = IdxMeshes[GroupIdx].Key;
		const FSkeletalMeshLODRenderData& RenderMesh = *(IdxMeshes[GroupIdx].Value);

		const FStaticMeshVertexBuffer& VtxBuffer = RenderMesh.StaticVertexBuffers.StaticMeshVertexBuffer;
		const uint32 NumCurrUVChannels = VtxBuffer.GetNumTexCoords();

		const FColorVertexBuffer* ColorVertexBuffer = nullptr;
		if (bImportVertexColor)
		{
			if (SMC && SMC->LODInfo.IsValidIndex(LodIdx))
			{
				ColorVertexBuffer = SMC->LODInfo[LodIdx].OverrideVertexColors;
				if (ColorVertexBuffer && (ColorVertexBuffer->GetNumVertices() != VtxBuffer.GetNumVertices()))
					ColorVertexBuffer = nullptr;
			}

			if (!ColorVertexBuffer)
				ColorVertexBuffer = &RenderMesh.StaticVertexBuffers.ColorVertexBuffer;

			if (ColorVertexBuffer->GetNumVertices() != VtxBuffer.GetNumVertices())
				ColorVertexBuffer = nullptr;
		}

		size_t NumCurrTriangles = 0;
		for (const FSkelMeshRenderSection& Section : RenderMesh.RenderSections)
			NumCurrTriangles += Section.NumTriangles;

		if (MaterialAttribCompression == EHoudiniInputAttributeCompression::Indexing)
		{
			if (LodIdx >= 0)
			{
				for (const FSkelMeshRenderSection& Section : RenderMesh.RenderSections)
				{
					int32 ImportIdx = -1;
					if (const int32* FoundImportIdxPtr = ImportMatIdxMap.Find(Section.MaterialIndex))
						ImportIdx = *FoundImportIdxPtr;
					for (int32 MatTriIdx = Section.NumTriangles - 1; MatTriIdx >= 0; --MatTriIdx)
					{
						*MaterialIndicesDataPtr = ImportIdx;
						++MaterialIndicesDataPtr;
					}
				}
			}
			else  // Need not import material for collision geo, so all material refs is empty (-1)
			{
				for (int32 MatTriIdx = NumCurrTriangles - 1; MatTriIdx >= 0; --MatTriIdx)
					MaterialIndicesDataPtr[MatTriIdx] = -1;
				MaterialIndicesDataPtr += NumCurrTriangles;
			}
		}

		// Lods
		if (bShouldImportLodGroups)
		{
			int32* CurrLodDataPtr = LodDataPtr + GroupIdx * NumTriangles + NumPrevTriangles;
			FMemory::Memset(CurrLodDataPtr, 1, NumCurrTriangles * sizeof(int32));
		}

		// Attributes
		const FRawStaticIndexBuffer16or32Interface* RawIndices = RenderMesh.MultiSizeIndexContainer.GetIndexBuffer();
		for (uint32 TriIdx = 0; TriIdx < NumCurrTriangles; ++TriIdx)
		{
			for (uint32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
			{
				const uint32 RawIdx = RawIndices->Get(TriIdx * 3 + TriVtxIdx);  // Original RenderMesh Vertex Index
				const uint32 VtxIdx = TriIdx * 3 + 2 - TriVtxIdx;  // Houdini polygon vertex index

				const FVector3f Normal = VtxBuffer.VertexTangentZ(RawIdx);
				COPY_VERTEX_VECTOR3f_DATA(NormalDataPtr, Normal);

				const FVector4f Tangent = VtxBuffer.VertexTangentX(RawIdx);
				COPY_VERTEX_VECTOR3f_DATA(TangentUDataPtr, Tangent);

				const FVector3f Binormal = VtxBuffer.VertexTangentY(RawIdx);
				COPY_VERTEX_VECTOR3f_DATA(TangentVDataPtr, Binormal);

				if (ColorDataPtr && AlphaDataPtr)
				{
					const FColor VertexColor = ColorVertexBuffer ? ColorVertexBuffer->VertexColor(RawIdx) : FColor::White;
					float* CurrColorDataPtr = ColorDataPtr + (NumPrevVertices + VtxIdx) * 3;
					*CurrColorDataPtr = float(VertexColor.R) / 255.0f;
					*(CurrColorDataPtr + 1) = float(VertexColor.G) / 255.0f;
					*(CurrColorDataPtr + 2) = float(VertexColor.B) / 255.0f;
					*(AlphaDataPtr + NumPrevVertices + VtxIdx) = float(VertexColor.A) / 255.0f;
				}

				for (uint32 UVChannelIdx = 0; UVChannelIdx < NumUVChannels; ++UVChannelIdx)
				{
					if (UVChannelIdx < NumCurrUVChannels)
					{
						const FVector2f UV = VtxBuffer.GetVertexUV(RawIdx, UVChannelIdx);
						float* CurrUVsDataPtr = UVsDataPtr + (UVChannelIdx * NumVertices + NumPrevVertices + VtxIdx) * 3;
						*CurrUVsDataPtr = UV.X;
						*(CurrUVsDataPtr + 1) = 1.0f - UV.Y;
					}
			}
		}
	}

		NumPrevVertices += NumCurrTriangles * 3;
		NumPrevTriangles += NumCurrTriangles;
}

	if (InOutSHMInputNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GeoNodeId,
			FString::Printf(TEXT("mesh_%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), InOutSHMInputNodeId));

	return SHMGeoInput.HapiUpload(InOutSHMInputNodeId, SHM);
}

bool UHoudiniInputSkeletalMesh::HapiImportSkeleton(const USkeletalMesh* SKM,
	const FHoudiniInputSettings& Settings, const int32& GeoNodeId, int32& InOutSHMInputNodeId, size_t& InOutHandle)
{
	const FReferenceSkeleton& Skeleton = SKM->GetRefSkeleton();
	
	TArray<int32> Vertices;
	int32 NumSegs = 0;
	TArray<char> Names;
	TArray<char> SimpleCollisions;
	
	const TArray<FMeshBoneInfo>& BoneInfos = Skeleton.GetRawRefBoneInfo();
	const int32 NumPoints = BoneInfos.Num();

	TArray<USkeletalBodySetup*> BodySetups;
	const UPhysicsAsset* PhysicsAsset = SKM->GetPhysicsAsset();
	if ((Settings.CollisionImportMethod != EHoudiniMeshCollisionImportMethod::NoImportCollision) && IsValid(PhysicsAsset))
	{
		BodySetups.SetNumZeroed(BoneInfos.Num());
		for (const TObjectPtr<USkeletalBodySetup>& SkeletalBodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			const int32 BoneIdx = Skeleton.FindBoneIndex(SkeletalBodySetup->BoneName);
			if (BodySetups.IsValidIndex(BoneIdx))
				BodySetups[BoneIdx] = SkeletalBodySetup;
		}
	}
	const bool bImportCollision = !BodySetups.IsEmpty();

	for (int32 BoneIdx = 0; BoneIdx < BoneInfos.Num(); ++BoneIdx)
	{
		const FMeshBoneInfo& BoneInfo = BoneInfos[BoneIdx];
		if (BoneInfo.ParentIndex >= 0)
		{
			++NumSegs;
			Vertices.Append({ BoneInfo.ParentIndex, BoneIdx, HOUDINI_SHM_GEO_INPUT_POLYLINE });
		}

		const std::string BoneName = TCHAR_TO_UTF8(*BoneInfo.Name.ToString());
		Names.Append(BoneName.c_str(), BoneName.size() + 1);

		if (bImportCollision)
		{
			if (IsValid(BodySetups[BoneIdx]))
				AppendSimpleCollisionsToJsonStr(BodySetups[BoneIdx]->AggGeom, SimpleCollisions);
			else
				SimpleCollisions.Add('\0');
		}
	}

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumSegs, NumSegs * 2);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_TRANSFORM, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 9, NumPoints + NumPoints * 9);  // 3@transform
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_NAME, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, Names.Num() / 4 + 1);  // s@name
	if (bImportCollision)
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_SIMPLE_COLLISIONS, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Dict, 1, SimpleCollisions.Num() / 4 + 1);  // 3@transform

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)SKM), InOutHandle);

	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	FMemory::Memcpy(VertexDataPtr, Vertices.GetData(), size_t(Vertices.Num()) * sizeof(int32));
	float* TransformDataPtr = (float*)(VertexDataPtr + Vertices.Num());
	char* NameDataPtr = (char*)(TransformDataPtr + NumPoints * 9);
	FMemory::Memcpy(NameDataPtr, Names.GetData(), Names.Num() - 1);
	if (bImportCollision)
		FMemory::Memcpy((NameDataPtr + ((Names.Num() / 4 + 1) * sizeof(float))), SimpleCollisions.GetData(), SimpleCollisions.Num() - 1);

	for (int32 BoneIdx = 0; BoneIdx < BoneInfos.Num(); ++BoneIdx)
	{
		const FMatrix PoseMatrix = SKM->GetComposedRefPoseMatrix(BoneIdx);
		FMatrix HoudiniXform;
		HoudiniXform.M[0][0] = PoseMatrix.M[0][0];
		HoudiniXform.M[0][1] = PoseMatrix.M[0][2];
		HoudiniXform.M[0][2] = PoseMatrix.M[0][1];
		HoudiniXform.M[0][3] = PoseMatrix.M[0][3];

		HoudiniXform.M[1][0] = PoseMatrix.M[2][0];
		HoudiniXform.M[1][1] = PoseMatrix.M[2][2];
		HoudiniXform.M[1][2] = PoseMatrix.M[2][1];
		HoudiniXform.M[1][3] = PoseMatrix.M[2][3];

		HoudiniXform.M[2][0] = PoseMatrix.M[1][0];
		HoudiniXform.M[2][1] = PoseMatrix.M[1][2];
		HoudiniXform.M[2][2] = PoseMatrix.M[1][1];
		HoudiniXform.M[2][3] = PoseMatrix.M[1][3];

		HoudiniXform.M[3][0] = PoseMatrix.M[3][0] * POSITION_SCALE_TO_HOUDINI;
		HoudiniXform.M[3][1] = PoseMatrix.M[3][2] * POSITION_SCALE_TO_HOUDINI;
		HoudiniXform.M[3][2] = PoseMatrix.M[3][1] * POSITION_SCALE_TO_HOUDINI;
		HoudiniXform.M[3][3] = PoseMatrix.M[3][3];

		const FVector3f Position = FVector3f(HoudiniXform.GetOrigin());
		PositionDataPtr[0] = Position.X;
		PositionDataPtr[1] = Position.Y;
		PositionDataPtr[2] = Position.Z;
		PositionDataPtr += 3;

		TransformDataPtr[0] = HoudiniXform.M[0][0];
		TransformDataPtr[1] = HoudiniXform.M[0][1];
		TransformDataPtr[2] = HoudiniXform.M[0][2];
		TransformDataPtr[3] = HoudiniXform.M[1][0];
		TransformDataPtr[4] = HoudiniXform.M[1][1];
		TransformDataPtr[5] = HoudiniXform.M[1][2];
		TransformDataPtr[6] = HoudiniXform.M[2][0];
		TransformDataPtr[7] = HoudiniXform.M[2][1];
		TransformDataPtr[8] = HoudiniXform.M[2][2];
		TransformDataPtr += 9;
	}

	if (InOutSHMInputNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GeoNodeId,
			FString::Printf(TEXT("Skeleton_%s_%08X"), *SKM->GetName(), FPlatformTime::Cycles()), InOutSHMInputNodeId));

	return SHMGeoInput.HapiUpload(InOutSHMInputNodeId, SHM);
}

bool UHoudiniInputSkeletalMesh::HapiImport(const USkeletalMesh* SM, const USkeletalMeshComponent* SMC,
	const FHoudiniInputSettings& Settings, const int32& GeoNodeId,
	int32& InOutMeshInputNodeId, size_t& InOutMeshHandle, int32& InOutSkeletonInputNodeId, size_t& InOutSkeletonHandle)
{
	HOUDINI_FAIL_RETURN(HapiImportSkeleton(SM, Settings, GeoNodeId, InOutSkeletonInputNodeId, InOutSkeletonHandle));

	return (Settings.bImportRenderData ?
		HapiImportRenderData(SM, SMC, Settings, GeoNodeId, InOutMeshInputNodeId, InOutMeshHandle) :
		HapiImportMeshDescription(SM, SMC, Settings, GeoNodeId, InOutMeshInputNodeId, InOutMeshHandle));
}

void UHoudiniInputSkeletalMesh::SetAsset(USkeletalMesh* NewSkeletalMesh)
{
	if (SkeletalMesh != NewSkeletalMesh)
	{
		SkeletalMesh = NewSkeletalMesh;
		RequestReimport();
	}
}

bool UHoudiniInputSkeletalMesh::IsObjectExists() const
{
	return IsValid(SkeletalMesh.LoadSynchronous());
}

bool UHoudiniInputSkeletalMesh::HapiUpload()
{
	USkeletalMesh* SM = SkeletalMesh.LoadSynchronous();
	if (!IsValid(SM))
		return HapiDestroy();

	if (GetSettings().bImportAsReference)
	{
		if (MeshNodeId >= 0)  // Means previous import the whole geo, so we should destroy the prev nodes and create new node
			HapiDestroy();

		const bool bCreateNewNode = (SettingsNodeId < 0);
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("reference_%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
		
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(SettingsNodeId, FVector3f::ZeroVector));
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId, HAPI_ATTRIB_UNREAL_INSTANCE, FHoudiniEngineUtils::GetAssetReference(SM)));

		const FBox Box = SM->GetBounds().GetBox();
		const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
		const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddDictAttribute(SettingsNodeId, HAPI_ATTRIB_UNREAL_OBJECT_METADATA,
			FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y)));
		
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}
	else
	{
		if ((SettingsNodeId >= 0) && (MeshNodeId < 0))  // Means previous import as ref, so we should destroy the prev node and create new nodes
			HapiDestroy();

		HOUDINI_FAIL_RETURN(HapiImport(SM, nullptr, GetSettings(), GetGeoNodeId(), MeshNodeId, MeshHandle, SkeletonNodeId, SkeletonHandle));

		const bool bCreateNewSettingsNode = (SettingsNodeId < 0);
		if (bCreateNewSettingsNode)
		{
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInput::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("%s_%08X"), *SM->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
			HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInput::HapiConnectInput(SettingsNodeId, MeshNodeId, SkeletonNodeId));
		}

		FHoudiniEngineSetupKineFXInput MeshSettings;
		MeshSettings.AppendAttribute(HAPI_ATTRIB_UNREAL_INSTANCE, EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SM));
		HOUDINI_FAIL_RETURN(MeshSettings.HapiUpload(SettingsNodeId));
		if (bCreateNewSettingsNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
	}

	bHasChanged = false;

	return true;
}

bool UHoudiniInputSkeletalMesh::HapiDestroy()
{
	if (MeshNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MeshNodeId));

	if (SkeletonNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SkeletonNodeId));

	if (SettingsNodeId >= 0)
	{
		GetInput()->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
	}

	Invalidate();

	return true;
}

void UHoudiniInputSkeletalMesh::Invalidate()
{
	SettingsNodeId = -1;
	MeshNodeId = -1;
	SkeletonNodeId = -1;
	if (MeshHandle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(MeshHandle);
		MeshHandle = 0;
	}
	if (SkeletonHandle)
	{
		FHoudiniEngineUtils::CloseSharedMemoryHandle(SkeletonHandle);
		SkeletonHandle = 0;
	}
}
