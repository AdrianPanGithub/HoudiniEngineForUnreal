// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HoudiniAsset.generated.h"


class AHoudiniNode;

UCLASS()
class HOUDINIENGINE_API UHoudiniAsset : public UObject
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TArray<uint8> LibraryBuffer;  // Buffer containing the raw HDA OTL data.

	UPROPERTY(EditAnyWhere)
	FFilePath FilePath;

	FDateTime LastEditTime = FDateTime();  // To record the .hda file edit time, if file was edited, we should automatically rebuild it

	TArray<TWeakObjectPtr<const AHoudiniNode>> InstantiatedNodes;  // For current asset file exclusively

	TArray<FString> AvailableAssetNames;

	FORCEINLINE void MarkAsUnloaded()
	{
		LastEditTime = FDateTime();
		InstantiatedNodes.Empty();
		AvailableAssetNames.Empty();
	}

public:
	void SetFilePath(const FString& InFilePath);  // Will reset all temps, for mark this hda as Unloaded.

	bool LoadBuffer();  // Return true if file exists, else false

	bool HapiLoad(TArray<FString>& OutAvailableAssetNames);  // Load asset library, will also refresh UHoudiniAsset::AvailableAssetNames

	bool NeedLoad();  // Check if asset has been loaded in current session

	FORCEINLINE void RegisterInstantiatedNode(const TWeakObjectPtr<const AHoudiniNode>& Node) { InstantiatedNodes.AddUnique(Node); }

	FORCEINLINE void UnregisterInstantiatedNode(const TWeakObjectPtr<const AHoudiniNode>& Node) { InstantiatedNodes.Remove(Node); }

	FORCEINLINE bool IsNodeInstantiated(const TWeakObjectPtr<const AHoudiniNode>& Node) const { return InstantiatedNodes.Contains(Node); }

	FORCEINLINE const TArray<TWeakObjectPtr<const AHoudiniNode>>& GetInstantiatedNodes() const { return InstantiatedNodes; }

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#else
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
