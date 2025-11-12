// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"

#include "HoudiniNodeFactory.generated.h"


class AHoudiniNode;

UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniNodeFactory : public UActorFactory
{
	GENERATED_BODY()

	UHoudiniNodeFactory();

public:
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;

	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;

	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;

	//virtual void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;
};
