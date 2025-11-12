// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniMeshSceneProxy.h"

#include "Materials/MaterialRenderProxy.h"
#include "MaterialDomain.h"
#include "DynamicMeshBuilder.h"

#include "HoudiniMeshComponent.h"


//#include "Runtime/GeometryFramework/Private/Components/DynamicMeshSceneProxy.h"
//#include "Runtime/Engine/Public/StaticMeshSceneProxy.h"

class FHoudiniMeshRenderBufferSet
{
public:
	FHoudiniMeshRenderBufferSet(ERHIFeatureLevel::Type FeatureLevelType) :
		VertexFactory(FeatureLevelType, "FHoudiniMeshRenderBufferSet")
	{
		StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
		StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(true);
	}

	FPositionVertexBuffer PositionVertexBuffer;

	FDynamicMeshIndexBuffer32 IndexBuffer;

	FStaticMeshVertexBuffer StaticMeshVertexBuffer;

	FColorVertexBuffer ColorVertexBuffer;

	FLocalVertexFactory VertexFactory;

	// Copy from FMeshRenderBufferSet
	static void InitOrUpdateResource(FRenderResource* Resource)
	{
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		if (!Resource->IsInitialized())
		{
			Resource->InitResource(RHICmdList);
		}
		else
		{
			Resource->UpdateRHI(RHICmdList);
		}
	}
	
	// Copy from FMeshRenderBufferSet
	void Upload()
	{
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		InitOrUpdateResource(&this->PositionVertexBuffer);
		InitOrUpdateResource(&this->StaticMeshVertexBuffer);
		InitOrUpdateResource(&this->ColorVertexBuffer);

		FLocalVertexFactory::FDataType Data;
		this->PositionVertexBuffer.BindPositionVertexBuffer(&this->VertexFactory, Data);
		this->StaticMeshVertexBuffer.BindTangentVertexBuffer(&this->VertexFactory, Data);
		this->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&this->VertexFactory, Data);
		// currently no lightmaps support
		//this->StaticMeshVertexBuffer.BindLightMapVertexBuffer(&this->VertexFactory, Data, LightMapIndex);
		this->ColorVertexBuffer.BindColorVertexBuffer(&this->VertexFactory, Data);
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
		this->VertexFactory.SetData(RHICmdList, Data);
#else
		this->VertexFactory.SetData(Data);
#endif
		InitOrUpdateResource(&this->VertexFactory);
		PositionVertexBuffer.InitResource(RHICmdList);
		StaticMeshVertexBuffer.InitResource(RHICmdList);
		ColorVertexBuffer.InitResource(RHICmdList);
		VertexFactory.InitResource(RHICmdList);
		IndexBuffer.InitResource(RHICmdList);
	}

	~FHoudiniMeshRenderBufferSet()
	{
		PositionVertexBuffer.ReleaseResource();
		StaticMeshVertexBuffer.ReleaseResource();
		ColorVertexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		if (IndexBuffer.IsInitialized())
		{
			IndexBuffer.ReleaseResource();
		}
	}
};


FHoudiniMeshSceneProxy::FHoudiniMeshSceneProxy(UHoudiniMeshComponent* InMeshComponent) : FPrimitiveSceneProxy(InMeshComponent),
	MeshComponent(InMeshComponent)
{
	Build();
}

FHoudiniMeshSceneProxy::~FHoudiniMeshSceneProxy()
{
	for (FHoudiniMeshRenderBufferSet* RenderBufferSet : RenderBufferSets)
	{
		ENQUEUE_RENDER_COMMAND(FHoudiniMeshRenderBufferSetDestroy)(
			[RenderBufferSet](FRHICommandListImmediate& RHICmdList)
			{
				delete RenderBufferSet;
			});
	}

	MeshComponent = nullptr;
}

#define GET_MESH_ATTRIB_DATA(OWNER, ATTRIB_DATA) switch (OWNER) \
	{ \
	case EHoudiniAttributeOwner::Vertex: return (ATTRIB_DATA)[VtxIdx]; \
	case EHoudiniAttributeOwner::Point: return (ATTRIB_DATA)[PtIdx]; \
	case EHoudiniAttributeOwner::Prim: return (ATTRIB_DATA)[TriIdx]; \
	} \
	return (ATTRIB_DATA)[0];

void FHoudiniMeshSceneProxy::Build()
{
	class FHoudiniMeshDataAccessor : public UHoudiniMeshComponent
	{
	public:
		const FVector3f& GetNormal(const int32& VtxIdx, const int32& PtIdx, const int32& TriIdx) const
		{
			GET_MESH_ATTRIB_DATA(NormalOwner, NormalData);
		}

		void TryComputeTangent(EHoudiniAttributeOwner& DerivedTangentUOwner, TArray<FVector3f>& DerivedTangentUData,
			EHoudiniAttributeOwner& DerivedTangentVOwner, TArray<FVector3f>& DerivedTangentVData)
		{
			if (TangentUData.IsEmpty() || TangentVData.IsEmpty())  // We should compute both tangentu and tangentv
			{
				DerivedTangentUOwner = NormalOwner;
				DerivedTangentVOwner = NormalOwner;
				const int32 NumNormalData = NormalData.Num();
				DerivedTangentUData.SetNumUninitialized(NumNormalData);
				DerivedTangentVData.SetNumUninitialized(NumNormalData);
				for (int32 DataIdx = 0; DataIdx < NumNormalData; ++DataIdx)
				{
					NormalData[DataIdx].FindBestAxisVectors(DerivedTangentUData[DataIdx], DerivedTangentUData[DataIdx]);
				}
			}

			// TODO: shall we support calculate tangentv data only
		}

		const FVector3f& GetTangentU(const int32& VtxIdx, const int32& PtIdx, const int32& TriIdx,
			const EHoudiniAttributeOwner& DerivedTangentUOwner, const TArray<FVector3f>& DerivedTangentUData) const
		{
			GET_MESH_ATTRIB_DATA(DerivedTangentUData.IsEmpty() ? TangentUOwner : DerivedTangentUOwner,
				DerivedTangentUData.IsEmpty() ? TangentUData : DerivedTangentUData);
		}

		const FVector3f& GetTangentV(const int32& VtxIdx, const int32& PtIdx, const int32& TriIdx,
			const EHoudiniAttributeOwner& DerivedTangentVOwner, const TArray<FVector3f>& DerivedTangentVData) const
		{
			GET_MESH_ATTRIB_DATA(DerivedTangentVData.IsEmpty() ? TangentVOwner : DerivedTangentVOwner,
				DerivedTangentVData.IsEmpty() ? TangentVData : DerivedTangentVData);
		}

		const FVector2f& GetUV(const int32& VtxIdx, const int32& PtIdx, const int32& TriIdx, const int32& UVIdx) const
		{
			if (!UVs.IsValidIndex(UVIdx))
				return FVector2f::ZeroVector;

			GET_MESH_ATTRIB_DATA(UVs[UVIdx].Owner, UVs[UVIdx].Data);
		}

		const FColor& GetColor(const int32& VtxIdx, const int32& PtIdx, const int32& TriIdx) const
		{
			if (ColorData.IsEmpty())
				return FColor::White;

			GET_MESH_ATTRIB_DATA(ColorOwner, ColorData);
		}

		FORCEINLINE const TArray<FHoudiniMeshSection>& GetSections() const { return Sections; }
		FORCEINLINE int32 GetNumUVs() const { return UVs.Num(); }
	};


	EHoudiniAttributeOwner DerivedTangentUOwner;
	TArray<FVector3f> DerivedTangentUData;
	EHoudiniAttributeOwner DerivedTangentVOwner;
	TArray<FVector3f> DerivedTangentVData;
	((FHoudiniMeshDataAccessor*)MeshComponent)->TryComputeTangent(DerivedTangentUOwner, DerivedTangentUData,
		DerivedTangentVOwner, DerivedTangentVData);

	const TArray<FVector3f>& Positions = MeshComponent->GetPositions();
	const TArray<FIntVector>& Triangles = MeshComponent->GetTriangles();
	const int32 NumUVs = ((FHoudiniMeshDataAccessor*)MeshComponent)->GetNumUVs();
	for (const FHoudiniMeshSection& Section : ((FHoudiniMeshDataAccessor*)MeshComponent)->GetSections())
	{
		FHoudiniMeshRenderBufferSet* BufferSet = new FHoudiniMeshRenderBufferSet(GetScene().GetFeatureLevel());
		
		const int32& NumVertices = Section.TriangleIndices.Num() * 3;
		BufferSet->StaticMeshVertexBuffer.Init(NumVertices, FMath::Max(NumUVs, 1));  // must have at least one tex coord
		BufferSet->ColorVertexBuffer.Init(NumVertices);
		BufferSet->PositionVertexBuffer.Init(NumVertices);
		BufferSet->IndexBuffer.Indices.SetNumUninitialized(NumVertices);

		for (int32 SecTriIdx = 0; SecTriIdx < Section.TriangleIndices.Num(); ++SecTriIdx)
		{
			const int32 TriIdx = Section.TriangleIndices[SecTriIdx];
			const FIntVector& Triangle = Triangles[TriIdx];
			for (int32 TriVtxIdx = 0; TriVtxIdx < 3; ++TriVtxIdx)
			{
				const int32& PtIdx = Triangle[TriVtxIdx];
				const int32 VtxIdx = TriIdx * 3 + 2 - TriVtxIdx;
				const int32 SecVtxIdx = SecTriIdx * 3 + 2 - TriVtxIdx;

				BufferSet->PositionVertexBuffer.VertexPosition(SecVtxIdx) = Positions[PtIdx];
				BufferSet->StaticMeshVertexBuffer.SetVertexTangents(SecVtxIdx,
					((FHoudiniMeshDataAccessor*)MeshComponent)->GetTangentU(VtxIdx, PtIdx, TriIdx, DerivedTangentUOwner, DerivedTangentUData),
					((FHoudiniMeshDataAccessor*)MeshComponent)->GetTangentV(VtxIdx, PtIdx, TriIdx, DerivedTangentVOwner, DerivedTangentVData),
					((FHoudiniMeshDataAccessor*)MeshComponent)->GetNormal(VtxIdx, PtIdx, TriIdx));
				if (NumUVs >= 1)
				{
					for (int32 UVIdx = 0; UVIdx < NumUVs; ++UVIdx)
						BufferSet->StaticMeshVertexBuffer.SetVertexUV(SecVtxIdx, UVIdx,
							((FHoudiniMeshDataAccessor*)MeshComponent)->GetUV(VtxIdx, PtIdx, TriIdx, UVIdx));
				}
				else
					BufferSet->StaticMeshVertexBuffer.SetVertexUV(SecVtxIdx, 0, FVector2f::ZeroVector);
					
				BufferSet->ColorVertexBuffer.VertexColor(SecVtxIdx) = ((FHoudiniMeshDataAccessor*)MeshComponent)->GetColor(VtxIdx, PtIdx, TriIdx);
				BufferSet->IndexBuffer.Indices[SecVtxIdx] = SecTriIdx * 3 + TriVtxIdx;
			}
		}

		RenderBufferSets.Add(BufferSet);

		ENQUEUE_RENDER_COMMAND(FHoudiniMeshSceneProxyInitialize)(
			[BufferSet](FRHICommandListImmediate& RHICmdList)
			{
				BufferSet->Upload();
			});
	}
}

FPrimitiveViewRelevance FHoudiniMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;

	Result.bDrawRelevance = IsShown(View);
	Result.bDynamicRelevance = true;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	
	
	// Copy from MeshComponent::GetMaterialRelevance
	FMaterialRelevance MaterialRelevance;
	for (int32 ElementIndex = 0; ElementIndex < MeshComponent->GetNumMaterials(); ElementIndex++)
	{
		UMaterialInterface const* MaterialInterface = MeshComponent->GetMaterial(ElementIndex);
		if (!MaterialInterface)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
		MaterialRelevance |= MaterialInterface->GetRelevance_Concurrent(View->GetShaderPlatform());
#else
		MaterialRelevance |= MaterialInterface->GetRelevance_Concurrent(View->GetFeatureLevel());
#endif
	}
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	//MeshComponent->GetMaterialRelevance(View->GetFeatureLevel()).SetPrimitiveViewRelevance(Result);


	Result.bVelocityRelevance = IsMovable() && (Result.bOpaque >= 1) && (Result.bRenderInMainPass >= 1);

	return Result;
}

void FHoudiniMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const bool bWireframe = (AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe);
	FMaterialRenderProxy* WireframeMaterialProxy = nullptr;
	if (bWireframe)
	{
		FColoredMaterialRenderProxy* WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
			FLinearColor(0.6f, 0.6f, 0.6f)
		);
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		WireframeMaterialProxy = WireframeMaterialInstance;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];
		
		for (int32 SecIdx = 0; SecIdx < RenderBufferSets.Num(); ++SecIdx)
		{
			const FHoudiniMeshRenderBufferSet* RenderBufferSet = RenderBufferSets[SecIdx];
			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &RenderBufferSet->IndexBuffer;
			//Mesh.bWireframe = bWireframe;
			//Mesh.bDisableBackfaceCulling = bWireframe;		// todo: doing this would be more consistent w/ other meshes in wireframe mode, but it is problematic for modeling tools - perhaps should be configurable
			Mesh.VertexFactory = &RenderBufferSet->VertexFactory;
			
			UMaterialInterface* SectionMaterial = MeshComponent->GetMaterial(SecIdx);
			Mesh.MaterialRenderProxy = bWireframe ? WireframeMaterialProxy : (SectionMaterial ? SectionMaterial->GetRenderProxy() : nullptr);

			//BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = RenderBufferSet->IndexBuffer.Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = RenderBufferSet->PositionVertexBuffer.GetNumVertices() - 1;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			// if this is a wireframe draw pass then we do not want to apply View Mode Overrides
			//Mesh.bCanApplyViewModeOverrides = (bWireframe) ? false : this->bEnableViewModeOverrides;
			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}
