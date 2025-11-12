// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "PrimitiveSceneProxy.h"


class UHoudiniMeshComponent;
class FHoudiniMeshRenderBufferSet;


class HOUDINIENGINE_API FHoudiniMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FHoudiniMeshSceneProxy(UHoudiniMeshComponent* InMeshComponent);

	~FHoudiniMeshSceneProxy();

protected:
	UHoudiniMeshComponent* MeshComponent;

	TArray<FHoudiniMeshRenderBufferSet*> RenderBufferSets;

	FCriticalSection AllocatedSetsLock;  // Use to control access to AllocatedBufferSets 

	void Build();

public:
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	//virtual bool CanBeOccluded() const override { return true; }

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize()); }

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
};
