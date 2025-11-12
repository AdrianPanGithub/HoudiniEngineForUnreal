// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniNodeFactory.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniNode.h"


#define LOCTEXT_NAMESPACE "HoudiniEngine"

UHoudiniNodeFactory::UHoudiniNodeFactory()
{
	NewActorClass = AHoudiniNode::StaticClass();
}

bool UHoudiniNodeFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!FHoudiniEngine::Get().AllowEdit())
	{
		OutErrorMsg = LOCTEXT("Error_SessionWorking", "Houdini Node(s) are cooking");
	}

	if (AssetData.IsValid() && AssetData.IsInstanceOf(UHoudiniAsset::StaticClass()))
		return true;

	OutErrorMsg = LOCTEXT("Error_HoudiniAssetInvalid", "A valid Houdini asset must be specified.");

	return false;
}

void UHoudiniNodeFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	if (NewActor->bIsEditorPreviewActor)
		return;

	NewActor->SetFolderPath(HOUDINI_LOCTEXT_NAMESPACE);
	Cast<AHoudiniNode>(NewActor)->Initialize(Cast<UHoudiniAsset>(Asset));
}

UObject* UHoudiniNodeFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	if (IsValid(ActorInstance))
	{
		if (AHoudiniNode* Node = Cast<AHoudiniNode>(ActorInstance))
			return Node->GetAsset();
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
