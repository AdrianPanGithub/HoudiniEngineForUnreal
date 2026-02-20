// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedFoliageActor.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniAttribute.h"

#define ENABLE_INSTANCEDSKINNEDMESH_OUTPUT ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
#define WAIT_SKELETAL_MESH_COMPILATION (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION == 7) && (ENGINE_PATCH_VERSION == 0)  // Bug in 5.7.0, the 5.7.1 Has been fixed
#if ENABLE_INSTANCEDSKINNEDMESH_OUTPUT
#if WAIT_SKELETAL_MESH_COMPILATION
#include "AssetCompilingManager.h"
#endif
#ifdef UE_EXPERIMENTAL
#pragma push_macro("UE_EXPERIMENTAL")
#undef UE_EXPERIMENTAL
#define UE_EXPERIMENTAL(VERSION, MESSAGE)
#endif

#include "Components/InstancedSkinnedMeshComponent.h"

#pragma pop_macro("UE_EXPERIMENTAL")
#endif


bool FHoudiniInstancerOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = ((PartInfo.type == HAPI_PARTTYPE_INSTANCER) ||  // Instantiate packed mesh
		((PartInfo.type == HAPI_PARTTYPE_MESH) && (PartInfo.faceCount == 0) && (PartInfo.pointCount >= 1)));  // Instantiate by attribute
	bOutShouldHoldByOutput = true;

	return true;
}

UMeshComponent* FHoudiniInstancedMeshOutput::Find(const AHoudiniNode* Node) const
{
	return Find_Internal<false>(Component, Node);
}

UMeshComponent* FHoudiniInstancedMeshOutput::CreateOrUpdate(AHoudiniNode* Node,
	const TSubclassOf<UMeshComponent>& Class, const FString& InSplitValue, const bool& bInSplitActor)
{
	USceneComponent* SMC = Component.IsValid() ? Component.Get() : nullptr;
	CreateOrUpdateComponent(Node, SMC, Class, InSplitValue, bInSplitActor, false);
	Component = (UMeshComponent*)SMC;
	return (UMeshComponent*)SMC;
}

void FHoudiniInstancedMeshOutput::Destroy(const AHoudiniNode* Node) const
{
	DestroyComponent(Node, Component, false);
	Component.Reset();
}


bool FHoudiniFoliageOutput::IsComponentValid(const AHoudiniNode* Node) const
{
	if (Component.IsValid())
		return true;

	AActor* FoundSplitActor = nullptr;
	UActorComponent* FoundComponent = FindComponent(Node, FoundSplitActor, false);
	Component = FoundComponent;

	return IsValid(FoundComponent);
}

bool FHoudiniFoliageOutput::Create(AHoudiniNode* Node, UObject* Instance,
	const FString& GeoName, const FString& InSplitValue, const bool& bInSplitActor,
	const TArray<int32>& PointIndices, const TArray<FTransform>& Transforms)
{
	ClearFoliageInstances(Node);  // We should clear the previous FoliageInstances first
	FoliageActorHolders.Empty();

	UFoliageType_InstancedStaticMesh* FT = Cast<UFoliageType_InstancedStaticMesh>(Instance);
	if (!IsValid(FT))
	{
		if (UStaticMesh* SM = Cast<UStaticMesh>(Instance))
		{
			FT = FHoudiniEngineUtils::FindOrCreateAsset<UFoliageType_InstancedStaticMesh>(
				Node->GetCookFolderPath() + (InSplitValue.IsEmpty() ?
					FString::Printf(TEXT("FT_%s_%s_%08X"), *FHoudiniEngineUtils::GetValidatedString(GeoName), *(Instance->GetName()), GetTypeHash(Instance->GetPathName())) :
					FString::Printf(TEXT("FT_%s_%s_%s_%08X"), *FHoudiniEngineUtils::GetValidatedString(GeoName), *InSplitValue, *(Instance->GetName()), GetTypeHash(Instance->GetPathName()))));
			FT->SetStaticMesh(SM);
		}
		else
			return false;
	}

	USceneComponent* BaseComponent = Component.IsValid() ? Cast<USceneComponent>(Component) : nullptr;
	CreateOrUpdateComponent(Node, BaseComponent, USceneComponent::StaticClass(), InSplitValue, bInSplitActor, false);
	Component = BaseComponent;


	TArray<FFoliageInstance> FoliageInstances;
	FoliageInstances.SetNum(PointIndices.Num());
	TMap<AInstancedFoliageActor*, TArray<const FFoliageInstance*>> IFAInstancesMap;
	UWorld* World = Node->GetWorld();
	for (int32 InstIdx = 0; InstIdx < FoliageInstances.Num(); ++InstIdx)
	{
		FFoliageInstance& FoliageInstance = FoliageInstances[InstIdx];

		FoliageInstance.BaseComponent = BaseComponent;

		const FTransform& Transform = Transforms[PointIndices[InstIdx]];
		FoliageInstance.Location = Transform.GetLocation();
		FoliageInstance.Rotation = Transform.GetRotation().Rotator();
		FoliageInstance.DrawScale3D = (FVector3f)Transform.GetScale3D();

		if (AInstancedFoliageActor* IFA =
			AInstancedFoliageActor::Get(Node->GetWorld(), true, Node->GetLevel(), FoliageInstance.Location))
			IFAInstancesMap.FindOrAdd(IFA).Add(&FoliageInstance);
	}

	for (const auto& IFAInstances : IFAInstancesMap)
	{
		AInstancedFoliageActor* IFA = IFAInstances.Key;
		FoliageActorHolders.Add(IFA);

		FFoliageInfo* Info = nullptr;
		UFoliageType* FoliageSettings = IFA->AddFoliageType(FT, &Info);
		Info->AddInstances(FoliageSettings, IFAInstances.Value);
		Info->Refresh(false, true);
	}

	return true;
}

void FHoudiniFoliageOutput::ClearFoliageInstances(const AHoudiniNode* Node) const
{
	if (!FoliageActorHolders.IsEmpty())
	{
		AActor* FoundSplitActor = nullptr;
		UActorComponent* BaseComponent = Component.IsValid() ? Component.Get() : FindComponent(Node, FoundSplitActor, false);
		if (!IsValid(BaseComponent))
			return;

		//AInstancedFoliageActor::DeleteInstancesForComponent(Node->GetWorld(), BaseComponent);
		// Do NOT used the method above, as we should consider the situation that some IFA has been unloaded.
		for (const FHoudiniActorHolder& FoliageActorHolder : FoliageActorHolders)
		{
			AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(FoliageActorHolder.Load());
			if (!IsValid(IFA))
				continue;

			IFA->Modify();
			IFA->DeleteInstancesForComponent(BaseComponent);
		}
	}
}

void FHoudiniFoliageOutput::Destroy(AHoudiniNode* Node) const
{
	ClearFoliageInstances(Node);

	DestroyComponent(Node, Cast<USceneComponent>(Component), false);
	Component.Reset();
}


int32 FHoudiniInstancedComponentOutput::GetMatchScore(const TSubclassOf<USceneComponent>& InComponentClass, const int32& NumInsts) const
{
	if (ComponentClass != InComponentClass)
		return -1;

	return FMath::Abs(ComponentHolders.Num() - NumInsts);
}

const AActor* FHoudiniInstancedComponentOutput::GetParentActorNameComponentMap(const AHoudiniNode* Node, TMap<FName, USceneComponent*>& OutNameComponentMap) const
{
	const AActor* FoundSplitActor = bSplitActor ? FindSplitActor(Node, false) : nullptr;
	const AActor* ParentActor = bSplitActor ? FoundSplitActor : Node;
	for (UActorComponent* Component : ParentActor->GetComponents())
	{
		if (IsValid(Component) && Component->GetClass() == ComponentClass)
			OutNameComponentMap.FindOrAdd(Component->GetFName(), Cast<USceneComponent>(Component));
	}

	return ParentActor;
}

bool FHoudiniInstancedComponentOutput::HasValidAndCleanup(const AHoudiniNode* Node)
{
	TMap<FName, USceneComponent*> NameComponentMap;
	const AActor* ParentActor = GetParentActorNameComponentMap(Node, NameComponentMap);
	if (!IsValid(ParentActor))
	{
		ComponentHolders.Empty();
		return false;
	}

	// Find all components that could be reused
	ComponentHolders.RemoveAll([&](const FHoudiniInstancedComponentHolder& ComponentHolder)
		{
			return !ComponentHolder.Component.IsValid() && !NameComponentMap.Contains(ComponentHolder.ComponentName);
		});

	return !ComponentHolders.IsEmpty();
}

bool FHoudiniInstancedComponentOutput::Update(const TSubclassOf<USceneComponent>& InComponentClass, AHoudiniNode* Node,
	const FString& InSplitValue, const bool& bInSplitActor, const int32& NumComponents, TArray<USceneComponent*>& OutComponents)
{
	if (ComponentClass != InComponentClass)
	{
		Destroy(Node);
		ComponentHolders.Empty();
	}

	TMap<FName, USceneComponent*> NameComponentMap;
	const AActor* PreviousParentActor = ComponentHolders.IsEmpty() ? nullptr : GetParentActorNameComponentMap(Node, NameComponentMap);

	bSplitActor = bInSplitActor;
	SplitValue = InSplitValue;
	ComponentClass = InComponentClass;

	AActor* SplitActor = bSplitActor ? FindOrCreateSplitActor(Node, false) : nullptr;
	AActor* ParentActor = bSplitActor ? SplitActor : Node;

	if (!ComponentHolders.IsEmpty() && (ParentActor != PreviousParentActor))
	{
		UE_LOG(LogHoudiniEngine, Warning, TEXT("Component owners does NOT matched, please check your code"));  // We should NOT rename it, as re-attach component may crack the actor package
		for (const FHoudiniInstancedComponentHolder& ComponentHolder : ComponentHolders)
		{
			USceneComponent* SC = nullptr;
			if (ComponentHolder.Component.IsValid())
				SC = ComponentHolder.Component.Get();
			else if (USceneComponent** FoundComponentPtr = NameComponentMap.Find(ComponentHolder.ComponentName))
				SC = *FoundComponentPtr;

			FHoudiniEngineUtils::DestroyComponent(SC);
		}
		ComponentHolders.Empty();
	}

	// Find all components that could be reused
	for (const FHoudiniInstancedComponentHolder& ComponentHolder : ComponentHolders)
	{
		if (ComponentHolder.Component.IsValid())
			OutComponents.Add(ComponentHolder.Component.Get());
		else if (USceneComponent** FoundComponentPtr = NameComponentMap.Find(ComponentHolder.ComponentName))
			OutComponents.Add(*FoundComponentPtr);
	}

	// Destroy all useless components
	if (OutComponents.Num() > NumComponents)
	{
		for (int32 SCIdx = OutComponents.Num() - 1; SCIdx >= NumComponents; --SCIdx)
			FHoudiniEngineUtils::DestroyComponent(OutComponents[SCIdx]);
		OutComponents.SetNum(NumComponents);
	}
	
	// Create new components
	if (OutComponents.Num() < NumComponents)
	{
		for (int32 SCIdx = OutComponents.Num(); SCIdx < NumComponents; ++SCIdx)
			OutComponents.Add(FHoudiniEngineUtils::CreateComponent(ParentActor, InComponentClass));
	}

	// Set all components transforms and properties
	ComponentHolders.SetNum(OutComponents.Num());
	for (int32 SCIdx = 0; SCIdx < OutComponents.Num(); ++SCIdx)
	{
		USceneComponent* SC = OutComponents[SCIdx];
		
		ComponentHolders[SCIdx].ComponentName = SC->GetFName();
		ComponentHolders[SCIdx].Component = SC;
	}

	return true;
}

void FHoudiniInstancedComponentOutput::Destroy(const AHoudiniNode* Node) const
{
	TMap<FName, USceneComponent*> NameComponentMap;
	const AActor* ParentActor = nullptr;

	for (const FHoudiniInstancedComponentHolder& ComponentHolder : ComponentHolders)
	{
		if (ComponentHolder.Component.IsValid())
			FHoudiniEngineUtils::DestroyComponent(ComponentHolder.Component.Get());
		else
		{
			if (!ParentActor)
				ParentActor = GetParentActorNameComponentMap(const_cast<AHoudiniNode*>(Node), NameComponentMap);

			if (USceneComponent** FoundComponentPtr = NameComponentMap.Find(ComponentHolder.ComponentName))
				FHoudiniEngineUtils::DestroyComponent(*FoundComponentPtr);
		}
	}
}


bool FHoudiniInstancedActorOutput::HasValidAndCleanup()
{
	ActorHolders.RemoveAll([](const FHoudiniActorHolder& ActorHolder)
		{
			return !IsValid(ActorHolder.Load());
		});

	return !ActorHolders.IsEmpty();
}

int32 FHoudiniInstancedActorOutput::GetMatchScore(const UObject* Instance, const int32& NumInsts) const
{
	if (Reference != FSoftObjectPath(Instance))
		return -1;

	return FMath::Abs(ActorHolders.Num() - NumInsts);
}

bool FHoudiniInstancedActorOutput::Update(const AHoudiniNode* Node, const FTransform& SplitTransform, const FString& InSplitValue, UObject* Instance, AActor*& InOutRefActor,
	const TArray<int32>& PointIndices, const TArray<FTransform>& Transforms, TFunctionRef<void(AActor*, const int32& ElemIdx)> PostFunc, const bool& bCustomFolderPath)
{
	Reference = Instance;
	SplitValue = InSplitValue;

	if (!InOutRefActor)
		InOutRefActor = Cast<AActor>(Instance);  // Maybe Instance is a actor

	if (!InOutRefActor)
	{
		if (ActorHolders.IsValidIndex(0))
			InOutRefActor = ActorHolders[0].Load();
	}

	const FString& NodeLabel = Node->GetActorLabel(false);
	const FName FolderPath = *(TEXT(HOUDINI_NODE_OUTLINER_FOLDER) / NodeLabel / (InSplitValue.IsEmpty() ? TEXT("Instances") : InSplitValue.Replace(TEXT("/"), TEXT("_"))));

#if WITH_EDITOR
	if (!IsValid(InOutRefActor))
	{
		TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject(Node->GetLevel(), Instance, false, RF_NoFlags, nullptr, NAME_None);
		if (NewActors.IsValidIndex(0) && IsValid(NewActors[0]))
		{
			InOutRefActor = NewActors[0];
			InOutRefActor->SetFlags(RF_Transactional);  // Recover Transaction feature
			ActorHolders.Add(FHoudiniActorHolder(InOutRefActor));
		}
		else
			return false;
	}
#else
	if (!IsValid(InOutRefActor))
		return false;
#endif

	const int32 NumInsts = PointIndices.Num();
	if (NumInsts < ActorHolders.Num())
	{
		// We should consider the situation that actor may be destroyed manually, so some ActorHolders are empty, we should collect valid actors firstly
		TArray<FHoudiniActorHolder> NewActorHolders;
		for (const FHoudiniActorHolder& ActorHolder : ActorHolders)
		{
			if (NewActorHolders.Num() >= NumInsts)
				ActorHolder.Destroy();
			else
			{
				AActor* Actor = ActorHolder.Load();
				if (IsValid(Actor))
					NewActorHolders.Add(FHoudiniActorHolder(Actor));
			}
		}
		ActorHolders = NewActorHolders;
	}

	UWorld* World = Node->GetWorld();
	FActorSpawnParameters SpawnParm;
	SpawnParm.Template = InOutRefActor;
	
	const int32 NumPreviousActor = ActorHolders.Num();
	if (NumInsts > NumPreviousActor)
	{
		for (int32 InstIdx = NumPreviousActor; InstIdx < NumInsts; ++InstIdx)
		{
			AActor* NewActor = World->SpawnActor(InOutRefActor->GetClass(), &FTransform::Identity, SpawnParm);
			if (!InOutRefActor->GetActorLabel(false).IsEmpty())
				NewActor->SetActorLabel(InOutRefActor->GetActorLabel(false));
			ActorHolders.Add(NewActor);
		}
	}
	
	const bool bSplitTransformIdenty = SplitTransform.Equals(FTransform::Identity);
	for (int32 InstIdx = 0; InstIdx < ActorHolders.Num(); ++InstIdx)
	{
		const int32& PointIdx = PointIndices[InstIdx];
		const FTransform ActorTransform = Transforms.IsEmpty() ? SplitTransform :
			(bSplitTransformIdenty ? Transforms[PointIdx] : (Transforms[PointIdx] * SplitTransform));

		FHoudiniActorHolder& ActorHolder = ActorHolders[InstIdx];
		AActor* Actor = ActorHolder.Load();
		if (!IsValid(Actor))  // Actor has been destroyed manually, so we need spawn new actors
		{
			Actor = World->SpawnActor(InOutRefActor->GetClass(), &ActorTransform, SpawnParm);
			if (!InOutRefActor->GetActorLabel(false).IsEmpty())
				Actor->SetActorLabel(InOutRefActor->GetActorLabel(false));
			ActorHolder = Actor;
		}
		else if (!Actor->GetActorTransform().Equals(ActorTransform))
		{
			Actor->SetActorTransform(ActorTransform);
			Actor->Modify();
		}
		
		PostFunc(Actor, PointIdx);

		const FName OrigFolderPath = Actor->GetFolderPath();
		if ((OrigFolderPath.IsNone() || !bCustomFolderPath) && OrigFolderPath != FolderPath)
			Actor->SetFolderPath(FolderPath);
	}

	return true;
}

void FHoudiniInstancedActorOutput::TransformActors(const FMatrix& DeltaXform) const
{
	for (const FHoudiniActorHolder& ActorHolder : ActorHolders)
	{
		if (AActor* Actor = ActorHolder.Load())
		{
			if (Actor->HasValidRootComponent())
			{
				FMatrix ActorXform = Actor->GetActorTransform().ToMatrixWithScale() * DeltaXform;
				Actor->SetActorTransform(FTransform(ActorXform));
				Actor->Modify();
			}
		}
	}
}

void FHoudiniInstancedActorOutput::Destroy() const
{
	for (const FHoudiniActorHolder& ActorHolder : ActorHolders)
		ActorHolder.Destroy();
}

FHoudiniGeometryCollectionOutput::FHoudiniGeometryCollectionOutput(AGeometryCollectionActor* InActor)
{
	Actor = InActor;
	if (IsValid(InActor))
		ActorName = InActor->GetFName();

	FHoudiniEngine::Get().RegisterActor(InActor);
}

AGeometryCollectionActor* FHoudiniGeometryCollectionOutput::Load() const
{
	if (Actor.IsValid())
		return Actor.Get();

	AGeometryCollectionActor* FoundActor = Cast<AGeometryCollectionActor>(FHoudiniEngine::Get().GetActorByName(ActorName));
	if (FoundActor)
		Actor = FoundActor;

	return FoundActor;
}

AGeometryCollectionActor* FHoudiniGeometryCollectionOutput::Update(const FString& InSplitValue, const FString& OutputName, const AHoudiniNode* Node, UGeometryCollection*& OutGC)
{
	const FString GCName = InSplitValue.IsEmpty() ? (TEXT("GC_") + FHoudiniEngineUtils::GetValidatedString(OutputName)) :
		(TEXT("GC_") + FHoudiniEngineUtils::GetValidatedString(OutputName) + TEXT("_") + InSplitValue);

	if (!Actor.IsValid())
		Actor = Node->GetWorld()->SpawnActor<AGeometryCollectionActor>();

	if (Actor->GetActorLabel(false) != GCName)
		Actor->SetActorLabel(GCName);

	const FName FolderPath = *(TEXT(HOUDINI_NODE_OUTLINER_FOLDER) / Node->GetActorLabel(false) /
		(InSplitValue.IsEmpty() ? TEXT("GeometryCollection") : InSplitValue.Replace(TEXT("/"), TEXT("_"))));
	if (Actor->GetFolderPath() != FolderPath)
		Actor->SetFolderPath(FolderPath);
	
	const FTransform NodeTransform = Node->GetActorTransform();
	if (!Actor->GetActorTransform().Equals(NodeTransform))
		Actor->SetActorTransform(NodeTransform);

	OutGC = FHoudiniEngineUtils::FindOrCreateAsset<UGeometryCollection>(Node->GetCookFolderPath() + GCName);

	Actor->GetGeometryCollectionComponent()->RestCollection = OutGC;

	OutGC->Reset();
	OutGC->GeometrySource.Empty();
	OutGC->Modify();

	SplitValue = InSplitValue;

	return Actor.Get();
}

void FHoudiniGeometryCollectionOutput::TransformActor(const FMatrix& DeltaXform) const
{
	if (AActor* GCActor = Load())
	{
		if (GCActor->HasValidRootComponent())
		{
			FMatrix ActorXform = GCActor->GetActorTransform().ToMatrixWithScale() * DeltaXform;
			GCActor->SetActorTransform(FTransform(ActorXform));
			GCActor->Modify();
		}
	}
}

void FHoudiniGeometryCollectionOutput::Destroy() const
{
	FHoudiniEngine::Get().DestroyActorByName(ActorName);
}


static bool HapiGetCustomFloatInfos(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
	TArray<int8>& OutNumCustomFloats, HAPI_AttributeOwner& OutNumCustomFloatsOwner, TArray<std::string>& OutCustomFloatNames, TArray<HAPI_AttributeInfo>& OutCustomFloatAttribInfos)
{
	// Get i@unreal_num_custom_floats
	OutNumCustomFloatsOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
		AttribCounts, HAPI_ATTRIB_UNREAL_INSTANCE_NUM_CUSTOM_FLOATS);
	if (OutNumCustomFloatsOwner == HAPI_ATTROWNER_INVALID)
		return true;

	HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
		HAPI_ATTRIB_UNREAL_INSTANCE_NUM_CUSTOM_FLOATS, OutNumCustomFloats, OutNumCustomFloatsOwner));
	if (OutNumCustomFloats.IsEmpty())
		return true;

	// Get the maximun NumCustomFloats, we may could retrieve less CustomFloats data based on MaxNumCustomFloats
	int8 MaxNumCustomFloats = 0;
	for (const int8& NumCustomFloats : OutNumCustomFloats)
	{
		if (MaxNumCustomFloats < NumCustomFloats)
			MaxNumCustomFloats = NumCustomFloats;
	}

	// Get f@unreal_per_instance_custom_dataX
	TArray<TPair<int32, std::string>> CustomFloatIndicators;
	for (const std::string& AttribName : AttribNames)
	{
		if (AttribName.starts_with(HAPI_ATTRIB_UNREAL_INSTANCE_CUSTOM_DATA_PREFIX))
		{
			const int32 CustomFloatIdx =
				FCStringAnsi::Atoi(AttribName.c_str() + strlen(HAPI_ATTRIB_UNREAL_INSTANCE_CUSTOM_DATA_PREFIX));
			if (!CustomFloatIndicators.ContainsByPredicate([CustomFloatIdx](const TPair<int32, std::string>& Lod) { return Lod.Key == CustomFloatIdx; }))
				CustomFloatIndicators.Add(TPair<int32, std::string>(CustomFloatIdx, AttribName));
		}
	}
	CustomFloatIndicators.Sort();

	for (const TPair<int32, std::string>& CustomFloatIndicator : CustomFloatIndicators)
	{
		const int32& CustomIdx = CustomFloatIndicator.Key;
		if (CustomIdx < 0)
			continue;

		if (OutCustomFloatNames.Num() >= MaxNumCustomFloats)  // We need NOT to retrieve more CustomFloats, the MaxNumCustomFloats has arrived
			break;

		const std::string& CustomFloatName = CustomFloatIndicator.Value;
		HAPI_AttributeOwner CustomOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, AttribCounts, CustomFloatName);
		if (CustomOwner == HAPI_ATTROWNER_INVALID)
			continue;

		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			CustomFloatName.c_str(), CustomOwner, &AttribInfo));
		if (AttribInfo.storage == HAPI_STORAGETYPE_FLOAT || AttribInfo.storage == HAPI_STORAGETYPE_FLOAT64)
		{
			OutCustomFloatNames.Add(CustomFloatName);
			OutCustomFloatAttribInfos.Add(AttribInfo);
		}
	}

	// We have known how many CustomFloats this part holds, so we should clamp OutNumCustomFloats from 0 - MaxNumCustomFloats
	MaxNumCustomFloats = OutCustomFloatNames.Num();
	for (int8& NumCustomFloats : OutNumCustomFloats)
		NumCustomFloats = FMath::Clamp(NumCustomFloats, 0, MaxNumCustomFloats);

	return true;
}


enum class EHoudiniInstancerOutputMode : int8
{
	Auto = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_AUTO,  // Depend on instance count, When just a single point without custom floats
	ISMC = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ISMC,  // Force to generate InstancedStaticMeshComponent
	HISMC = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_HISMC,  // Force to generate HierarchicalInstancedStaticMeshComponent
	Components = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_COMPONENTS, // From USceneComponent classes, such as Blueprint, PointLightComponent etc.
	Actors = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ACTORS,  // From Blueprint, DecalMaterial, PointLight etc.
	Foliage = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_FOLIAGE,
	GeometryCollection = HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_CHAOS
};

FORCEINLINE static EHoudiniInstancerOutputMode GetInstancerType(const UObject* Object)
{
	if (const UStaticMesh* SM = Cast<UStaticMesh>(Object))
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
		return (!SM->IsNaniteEnabled() && (SM->GetNumLODs() >= 2)) ?
#else
		return (!SM->NaniteSettings.bEnabled && SM->GetNumLODs() >= 2) ?
#endif
			EHoudiniInstancerOutputMode::HISMC : EHoudiniInstancerOutputMode::Auto;
#if ENABLE_INSTANCEDSKINNEDMESH_OUTPUT
	else if (Object->IsA<USkeletalMesh>())
		return EHoudiniInstancerOutputMode::ISMC;
#endif
	else if (Object->GetClass() == UFoliageType_InstancedStaticMesh::StaticClass())
		return EHoudiniInstancerOutputMode::Foliage;
	else if (const UClass* InstancedClass = Cast<UClass>(Object))
	{
		if (InstancedClass->IsChildOf<USceneComponent>())
			return EHoudiniInstancerOutputMode::Components;
	}

	return EHoudiniInstancerOutputMode::Actors;
}

FORCEINLINE static void RefineInstancerType(EHoudiniInstancerOutputMode& InOutInstancerType, int8& InOutNumCustomFloats, const EHoudiniInstancerOutputMode& TargetInstancerType)
{
	if ((InOutNumCustomFloats >= 1) &&
		(InOutInstancerType == EHoudiniInstancerOutputMode::Auto))  // If has per instance data, then we should treat it is
		InOutInstancerType = EHoudiniInstancerOutputMode::ISMC;

	if (InOutInstancerType <= EHoudiniInstancerOutputMode::HISMC)
	{
		if (TargetInstancerType != EHoudiniInstancerOutputMode::Components)
			InOutInstancerType = EHoudiniInstancerOutputMode(FMath::Max(int8(InOutInstancerType), int8(TargetInstancerType)));
	}

	if (InOutInstancerType >= EHoudiniInstancerOutputMode::Components)  // Actors instancer do NOT have CustomFloats
		InOutNumCustomFloats = 0;
}

static bool HapiGetInstanceOutputModes(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
	TArray<int8>& OutInstanceOutputModes, HAPI_AttributeOwner& OutInstanceOutputModeOwner)
{
	static const auto GetInstanceOutputModeLambda = [](const FUtf8StringView& AttribValue) -> int8
		{
			if (AttribValue.StartsWith("inst") || AttribValue.StartsWith("force_inst") || AttribValue.StartsWith("ism"))
				return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ISMC;
			else if (AttribValue.StartsWith("hierarch") || AttribValue.StartsWith("hism"))
				return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_HISMC;
			else if (AttribValue.StartsWith("comp"))
				return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_COMPONENTS;
			else if ((UE::String::FindFirst(AttribValue, "actor", ESearchCase::IgnoreCase) != INDEX_NONE))
				return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ACTORS;
			else if (AttribValue.StartsWith("foli"))
				return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_FOLIAGE;
			else if (AttribValue.StartsWith("chao") || AttribValue.StartsWith("gc") || AttribValue.StartsWith("geo"))
				return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_CHAOS;
			return HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_AUTO;
		};

	OutInstanceOutputModeOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
		AttribCounts, HAPI_ATTRIB_UNREAL_OUTPUT_INSTANCE_TYPE);
	HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
		HAPI_ATTRIB_UNREAL_OUTPUT_INSTANCE_TYPE, GetInstanceOutputModeLambda, OutInstanceOutputModes, OutInstanceOutputModeOwner));

	if (!OutInstanceOutputModes.IsEmpty())
		return true;

	// Also compatible with deprecated attribs
	auto HapiGetInstanceOutputModesDeprecatedLambda = [&](const char* AttribName, const int8& Mode, const TCHAR* ModeStr) -> bool
		{
			OutInstanceOutputModeOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
				AttribCounts, AttribName);
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
				AttribName, OutInstanceOutputModes, OutInstanceOutputModeOwner));
			if (!OutInstanceOutputModes.IsEmpty())
			{
				UE_LOG(LogHoudiniEngine, Error, TEXT("%s is deprecated,\nuse i@unreal_instance_output_mode = %d or s@unreal_instance_output_mode = \"%s\" instead"),
					UTF8_TO_TCHAR(AttribName), int32(Mode), ModeStr);

				for (int8& InstanceOutputMode : OutInstanceOutputModes)
				{
					if (InstanceOutputMode >= 1)
						InstanceOutputMode = Mode;
				}
			}

			return true;
		};

	// Foliage
	HOUDINI_FAIL_RETURN(HapiGetInstanceOutputModesDeprecatedLambda(
		HAPI_UNREAL_ATTRIB_FOLIAGE_INSTANCER, HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_FOLIAGE, TEXT("foliage")));

	if (!OutInstanceOutputModes.IsEmpty())
		return true;

	// HISMC
	HOUDINI_FAIL_RETURN(HapiGetInstanceOutputModesDeprecatedLambda(
		HAPI_UNREAL_ATTRIB_HIERARCHICAL_INSTANCED_SM, HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_HISMC, TEXT("hierarchical")));

	if (!OutInstanceOutputModes.IsEmpty())
		return true;

	// ISMC
	return HapiGetInstanceOutputModesDeprecatedLambda(
		HAPI_UNREAL_ATTRIB_FORCE_INSTANCER, HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_ISMC, TEXT("instance"));
}

template<typename TAssetClass>
FORCEINLINE static TAssetClass* LoadAssetFromHoudiniString(const std::string& Str)
{
	FString AssetRefStr = UTF8_TO_TCHAR(Str.c_str());
	int32 SplitIdx;
	if (AssetRefStr.FindChar(TCHAR(';'), SplitIdx))
		AssetRefStr.LeftInline(SplitIdx);
	TAssetClass* LoadedAsset = LoadObject<TAssetClass>(nullptr, *AssetRefStr, nullptr, LOAD_Quiet | LOAD_NoWarn);
	return (IsValid(LoadedAsset) ? LoadedAsset : nullptr);
}

bool UHoudiniOutputInstancer::HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputInstancer);

	struct FHoudiniInstancerIndicesHolder
	{
		FHoudiniInstancerIndicesHolder(const int8& UpdateMode, const FString& InSplitValue) :
			PartialOutputMode(UpdateMode), SplitValue(InSplitValue) {}

		int8 PartialOutputMode = 0;
		FString SplitValue;

		// < < <InstancerOutputMode, NumCustomFloats>, AssetToInstantiate > , Indices >
		TMap<TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>, TArray<int32>> AssetIndicesMap;
	};

	struct FHoudiniInstancerPart
	{
		FHoudiniInstancerPart(const HAPI_PartInfo& PartInfo) : Info(PartInfo)
		{
			if (Info.instancedPartCount <= 0)  // This part should be treated as attribute instancer, so we should not read the vertex and prim attributes
			{
				Info.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
				Info.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
			}
		}

		HAPI_PartInfo Info;
		TArray<std::string> AttribNames;
		TArray<std::string> CustomFloatNames;
		TArray<HAPI_AttributeInfo> CustomFloatAttribInfos;
		TMap<int32, FHoudiniInstancerIndicesHolder> SplitInstancerMap;
	};

	// -------- Retrieve all part data --------
	TArray<FHoudiniInstancerPart> Parts;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
		Parts.Add(FHoudiniInstancerPart(PartInfo));

	AHoudiniNode* Node = GetNode();
	const int32& NodeId = GeoInfo.nodeId;
	
	bool bPartialUpdate = false;

	HAPI_AttributeInfo AttribInfo;
	for (FHoudiniInstancerPart& Part : Parts)
	{
		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;

		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, Part.AttribNames));
		const TArray<std::string>& AttribNames = Part.AttribNames;


		TArray<int8> NumCustomFloatsData;
		HAPI_AttributeOwner NumCustomFloatsOwner = HAPI_ATTROWNER_INVALID;
		HOUDINI_FAIL_RETURN(HapiGetCustomFloatInfos(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			NumCustomFloatsData, NumCustomFloatsOwner, Part.CustomFloatNames, Part.CustomFloatAttribInfos));


		TArray<int32> SplitKeys;  // Maybe int or HAPI_StringHandle
		HAPI_AttributeOwner SplitAttribOwner = HAPI_ATTROWNER_POINT;
		TMap<HAPI_StringHandle, FString> SplitValueMap;
		HOUDINI_FAIL_RETURN(FHoudiniOutputUtils::HapiGetSplitValues(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			SplitKeys, SplitValueMap, SplitAttribOwner));
		const bool bHasSplitValues = !SplitKeys.IsEmpty();


		HAPI_AttributeOwner PartialOutputModeOwner = bHasSplitValues ? FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
			PartInfo.attributeCounts, HAPI_ATTRIB_PARTIAL_OUTPUT_MODE) : HAPI_ATTROWNER_INVALID;
		TArray<int8> PartialOutputModes;
		HOUDINI_FAIL_RETURN(FHoudiniOutputUtils::HapiGetPartialOutputModes(NodeId, PartId, PartialOutputModes, PartialOutputModeOwner));


		// Get the data that could judge what type of instance we should generate, Foliage > HISMC > ISMC
		TArray<int8> InstanceOutputModes;
		HAPI_AttributeOwner InstanceOutputModeOwner;
		HOUDINI_FAIL_RETURN(HapiGetInstanceOutputModes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			InstanceOutputModes, InstanceOutputModeOwner));

		if (PartInfo.instancedPartCount >= 1)  // Should instantiate other mesh part
		{
			TArray<HAPI_PartId> InstancedPartIds;
			InstancedPartIds.SetNumUninitialized(PartInfo.instancedPartCount);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetInstancedPartIds(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				InstancedPartIds.GetData(), 0, PartInfo.instancedPartCount));

			TArray<UStaticMesh*> InstancedSMs;
			bool bMaybePDGOutput = true;
			for (const UHoudiniOutput* Output : GetNode()->GetOutputs())
			{
				if ((Output->GetClass() == UHoudiniOutputMesh::StaticClass()) && (Output->GetOutputName() == Name))
				{
					Cast<UHoudiniOutputMesh>(Output)->GetInstancedStaticMeshes(InstancedPartIds, InstancedSMs);
					bMaybePDGOutput = false;
					break;
				}

				if (Output == this)  // this output is in node outputs, NOT PDG
					bMaybePDGOutput = false;
			}
			if (InstancedSMs.IsEmpty() && bMaybePDGOutput)  // Also try find packed mesh in PDG outputs
			{
				for (const FHoudiniTopNode& TopNode : GetNode()->GetTopNodes())
				{
					for (const UHoudiniOutput* Output : TopNode.Outputs)
					{
						if ((Output->GetClass() == UHoudiniOutputMesh::StaticClass()) && (Output->GetOutputName() == Name))
						{
							Cast<UHoudiniOutputMesh>(Output)->GetInstancedStaticMeshes(InstancedPartIds, InstancedSMs);
							break;
						}
					}
				}
			}

			// If packed InstancedSMs is empty, but we have PartialOutputModes, then means probably we will remove some split values
			if (InstancedSMs.IsEmpty() && PartialOutputModes.IsEmpty())
				continue;

			TMap<int32, FHoudiniInstancerIndicesHolder>& SplitMap = Part.SplitInstancerMap;  // SplitValue, <MeshOutputMesh, GroupIdx>, Triangles
			FHoudiniInstancerIndicesHolder AllInstancer(HAPI_PARTIAL_OUTPUT_MODE_REPLACE, FString());
			TMap<TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>, TArray<int32>>& AllAssetIndicesMap = AllInstancer.AssetIndicesMap;  // Only for "no split values" condition
			for (int32 PointIdx = 0; PointIdx < PartInfo.instanceCount; ++PointIdx)
			{
				// Judge PartialOutputMode, if remove && previous NOT set, then we will NOT parse the GroupIdx
				const int32 SplitKey = bHasSplitValues ? SplitKeys[POINT_ATTRIB_ENTRY_IDX(SplitAttribOwner, PointIdx)] : 0;
				FHoudiniInstancerIndicesHolder* FoundHolderPtr = bHasSplitValues ? SplitMap.Find(SplitKey) : nullptr;

				const int8 PartialOutputMode = FMath::Clamp(PartialOutputModes.IsEmpty() ? HAPI_PARTIAL_OUTPUT_MODE_REPLACE :
					PartialOutputModes[POINT_ATTRIB_ENTRY_IDX(SplitAttribOwner, PointIdx)], HAPI_PARTIAL_OUTPUT_MODE_REPLACE, HAPI_PARTIAL_OUTPUT_MODE_REMOVE);

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
						SplitMap.Add(SplitKey, FHoudiniInstancerIndicesHolder(HAPI_PARTIAL_OUTPUT_MODE_REMOVE, GET_SPLIT_VALUE_STR));
						continue;
					}
				}

				int8 NumCustomFloats = NumCustomFloatsData.IsEmpty() ? 0 : NumCustomFloatsData[POINT_ATTRIB_ENTRY_IDX(NumCustomFloatsOwner, PointIdx)];
				EHoudiniInstancerOutputMode InstanceType = EHoudiniInstancerOutputMode::Auto;
				RefineInstancerType(InstanceType, NumCustomFloats,  // If NumCustomFloats >= 1, then type will > EHoudiniInstancerOutputMode::ISMC
					EHoudiniInstancerOutputMode(InstanceOutputModes.IsEmpty() ? HAPI_UNREAL_OUTPUT_INSTANCE_TYPE_AUTO : InstanceOutputModes[POINT_ATTRIB_ENTRY_IDX(InstanceOutputModeOwner, PointIdx)]));
				
				// Finally, add point index to the specify group
				for (UObject* Instance : InstancedSMs)
				{
					const TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*> InstanceIdentifier = IsValid(Instance) ?
						TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>(TPair<EHoudiniInstancerOutputMode, int8>(InstanceType, NumCustomFloats), Instance) :
						TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>();
					if (bHasSplitValues)  // Split Instancer by SplitValueSHs
					{
						if (!FoundHolderPtr)
							FoundHolderPtr = &SplitMap.Add(SplitKey, FHoudiniInstancerIndicesHolder(PartialOutputMode, GET_SPLIT_VALUE_STR));
						FoundHolderPtr->AssetIndicesMap.FindOrAdd(InstanceIdentifier).Add(PointIdx);
					}
					else
						AllAssetIndicesMap.FindOrAdd(InstanceIdentifier).Add(PointIdx);
				}
			}

			if (!bHasSplitValues)
				SplitMap.Add(0, AllInstancer);  // We just add AllLodTrianglesMap to SplitMeshMap
		}
		else
		{
			HAPI_AttributeOwner InstanceOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
				PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_INSTANCE);
			TArray<UObject*> Instances;
			TArray<EHoudiniInstancerOutputMode> InstanceTypes;
			if (InstanceOwner != HAPI_ATTROWNER_INVALID)
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_ATTRIB_UNREAL_INSTANCE, InstanceOwner, &AttribInfo));

				if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
				{
					TArray<HAPI_StringHandle> InstanceRefSHs;
					InstanceRefSHs.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_UNREAL_INSTANCE, &AttribInfo, InstanceRefSHs.GetData(), 0, AttribInfo.count));

					TMap<HAPI_StringHandle, TPair<UObject*, EHoudiniInstancerOutputMode>> SHInstanceInfoMap;
					if (!FHoudiniEngineUtils::HapiConvertStringHandles(InstanceRefSHs, [](FUtf8StringView& StrView) -> TPair<UObject*, EHoudiniInstancerOutputMode>
						{
							UObject* Instance = nullptr;
							if (!StrView.IsEmpty())  // Do NOT use "IS_ASSET_PATH_INVALID", as strings like "PointLight", "SplineMeshComponent" also can be instantiated
							{
								int32 SplitIdx;
								if (StrView.FindChar(UTF8CHAR(';'), SplitIdx))  // UHoudiniParameterAsset support import asset ref with info, and append by ';', so we need to support it here
									StrView.LeftInline(SplitIdx);
								const FString InstanceAssetRef(StrView);

								Instance = LoadObject<UObject>(nullptr, *InstanceAssetRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
								if (!Instance)  // Maybe is the class name, like "PointLight", "SplineMeshComponent"
									Instance = FindFirstObject<UClass>(*InstanceAssetRef, EFindFirstObjectOptions::NativeFirst);
							}
							return (IsValid(Instance) ? TPair<UObject*, EHoudiniInstancerOutputMode>(Instance, GetInstancerType(Instance)) :
								TPair<UObject*, EHoudiniInstancerOutputMode>(nullptr, EHoudiniInstancerOutputMode::Auto));
						}, SHInstanceInfoMap))
						return false;

					Instances.SetNumUninitialized(AttribInfo.count);
					InstanceTypes.SetNumUninitialized(AttribInfo.count);
					for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
					{
						const TPair<UObject*, EHoudiniInstancerOutputMode>& InstanceInfo = SHInstanceInfoMap[InstanceRefSHs[ElemIdx]];
						Instances[ElemIdx] = InstanceInfo.Key;
						InstanceTypes[ElemIdx] = InstanceInfo.Value;
					}
				}
			}


			TMap<int32, FHoudiniInstancerIndicesHolder>& SplitMap = Part.SplitInstancerMap;  // SplitValue, <MeshOutputMesh, GroupIdx>, Triangles
			FHoudiniInstancerIndicesHolder AllInstancer(HAPI_PARTIAL_OUTPUT_MODE_REPLACE, FString());
			TMap<TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>, TArray<int32>>& AllAssetIndicesMap = AllInstancer.AssetIndicesMap;  // Only for "no split values" condition
			for (int32 PointIdx = 0; PointIdx < PartInfo.pointCount; ++PointIdx)
			{
				// Judge PartialOutputMode, if remove && previous NOT set, then we will NOT parse the GroupIdx
				const int32 SplitKey = bHasSplitValues ? SplitKeys[POINT_ATTRIB_ENTRY_IDX(SplitAttribOwner, PointIdx)] : 0;
				FHoudiniInstancerIndicesHolder* FoundHolderPtr = bHasSplitValues ? SplitMap.Find(SplitKey) : nullptr;

				const int8 PartialOutputMode = FMath::Clamp(PartialOutputModes.IsEmpty() ? HAPI_PARTIAL_OUTPUT_MODE_REPLACE :
					PartialOutputModes[POINT_ATTRIB_ENTRY_IDX(SplitAttribOwner, PointIdx)], HAPI_PARTIAL_OUTPUT_MODE_REPLACE, HAPI_PARTIAL_OUTPUT_MODE_REMOVE);

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
						SplitMap.Add(SplitKey, FHoudiniInstancerIndicesHolder(HAPI_PARTIAL_OUTPUT_MODE_REMOVE, GET_SPLIT_VALUE_STR));
						continue;
					}
				}

				int8 NumCustomFloats = NumCustomFloatsData.IsEmpty() ? 0 : NumCustomFloatsData[POINT_ATTRIB_ENTRY_IDX(NumCustomFloatsOwner, PointIdx)];
				EHoudiniInstancerOutputMode InstanceType = InstanceTypes.IsEmpty() ? EHoudiniInstancerOutputMode::Auto : InstanceTypes[POINT_ATTRIB_ENTRY_IDX(InstanceOwner, PointIdx)];
				EHoudiniInstancerOutputMode SpecifiedInstanceType = InstanceOutputModes.IsEmpty() ? InstanceType : EHoudiniInstancerOutputMode(InstanceOutputModes[POINT_ATTRIB_ENTRY_IDX(InstanceOutputModeOwner, PointIdx)]);
				UObject* Instance = Instances.IsEmpty() ? nullptr : Instances[POINT_ATTRIB_ENTRY_IDX(InstanceOwner, PointIdx)];
				switch (SpecifiedInstanceType)
				{
				case EHoudiniInstancerOutputMode::GeometryCollection:  // Only StaticMesh can construct Chaos GeometryCollection and Foliage
				case EHoudiniInstancerOutputMode::Foliage: { if (!IsValid(Instance) || !Instance->IsA<UStaticMesh>()) SpecifiedInstanceType = InstanceType; } break;
				}
				RefineInstancerType(InstanceType, NumCustomFloats, SpecifiedInstanceType);  // If NumCustomFloats >= 1, then type will > EHoudiniInstancerOutputMode::ISMC
				
				// Finally, add point index to the specify group
				const TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*> InstanceIdentifier = IsValid(Instance) ?
					TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>(TPair<EHoudiniInstancerOutputMode, int8>(InstanceType, NumCustomFloats), Instance) :
					TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>();
				if (bHasSplitValues)  // Split Instancer by SplitValueSHs
				{
					if (!FoundHolderPtr)
						FoundHolderPtr = &SplitMap.Add(SplitKey, FHoudiniInstancerIndicesHolder(PartialOutputMode, GET_SPLIT_VALUE_STR));
					FoundHolderPtr->AssetIndicesMap.FindOrAdd(InstanceIdentifier).Add(PointIdx);
				}
				else
					AllAssetIndicesMap.FindOrAdd(InstanceIdentifier).Add(PointIdx);
			}

			if (!bHasSplitValues)
				SplitMap.Add(0, AllInstancer);  // We just add AllLodTrianglesMap to SplitMeshMap
		}
	}


	// -------- Update output holders --------
	TDoubleLinkedList<FHoudiniInstancedMeshOutput*> OldInstancedMeshOutputs;
	TArray<FHoudiniInstancedMeshOutput> NewInstancedMeshOutputs;
	TDoubleLinkedList<FHoudiniInstancedComponentOutput*> OldInstancedComponentOutputs;
	TArray<FHoudiniInstancedComponentOutput> NewInstancedComponentOutputs;
	TDoubleLinkedList<FHoudiniInstancedActorOutput*> OldInstancedActorOutputs;
	TArray<FHoudiniInstancedActorOutput> NewInstancedActorOutputs;
	TDoubleLinkedList<FHoudiniFoliageOutput*> OldFoliageOutputs;
	TArray<FHoudiniFoliageOutput> NewFoliageOutputs;
	TDoubleLinkedList<FHoudiniGeometryCollectionOutput*> OldGeometryCollectionOutputs;
	TArray<FHoudiniGeometryCollectionOutput> NewGeometryCollectionOutputs;

	if (bPartialUpdate)
	{
		TSet<FString> ModifySplitValues;
		TSet<FString> RemoveSplitValues;
		for (FHoudiniInstancerPart& Part : Parts)
		{
			for (TMap<int32, FHoudiniInstancerIndicesHolder>::TIterator SplitIter(Part.SplitInstancerMap); SplitIter; ++SplitIter)
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

		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, InstancedMeshOutputs,
			[Node](const FHoudiniInstancedMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldInstancedMeshOutputs, NewInstancedMeshOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, InstancedComponentOutputs,
			[Node](FHoudiniInstancedComponentOutput& Holder) { return Holder.HasValidAndCleanup(Node); }, OldInstancedComponentOutputs, NewInstancedComponentOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, InstancedActorOutputs,
			[](FHoudiniInstancedActorOutput& Holder) { return Holder.HasValidAndCleanup(); }, OldInstancedActorOutputs, NewInstancedActorOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, FoliageOutputs,
			[Node](const FHoudiniFoliageOutput& Holder) { return Holder.IsComponentValid(Node); }, OldFoliageOutputs, NewFoliageOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, GeometryCollectionOutputs,
			[](const FHoudiniGeometryCollectionOutput& Holder) { return IsValid(Holder.Load()); }, OldGeometryCollectionOutputs, NewGeometryCollectionOutputs);
	}
	else
	{
		// Collect valid old output holders for reuse
		FHoudiniOutputUtils::UpdateOutputHolders(InstancedMeshOutputs,
			[Node](const FHoudiniInstancedMeshOutput& Holder) { return IsValid(Holder.Find(Node)); }, OldInstancedMeshOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(InstancedComponentOutputs,
			[Node](FHoudiniInstancedComponentOutput& Holder) { return Holder.HasValidAndCleanup(Node); }, OldInstancedComponentOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(InstancedActorOutputs,
			[](FHoudiniInstancedActorOutput& Holder) { return Holder.HasValidAndCleanup(); }, OldInstancedActorOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(FoliageOutputs,
			[Node](const FHoudiniFoliageOutput& Holder) { return Holder.IsComponentValid(Node); }, OldFoliageOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(GeometryCollectionOutputs,
			[](const FHoudiniGeometryCollectionOutput& Holder) { return IsValid(Holder.Load()); }, OldGeometryCollectionOutputs);
	}

	// Remove null object
	for (FHoudiniInstancerPart& Part : Parts)
	{
		for (TMap<int32, FHoudiniInstancerIndicesHolder>::TIterator SplitIter(Part.SplitInstancerMap); SplitIter; ++SplitIter)
		{
			SplitIter->Value.AssetIndicesMap.Remove(TPair<TPair<EHoudiniInstancerOutputMode, int8>, UObject*>());
			if (SplitIter->Value.AssetIndicesMap.IsEmpty())
				SplitIter.RemoveCurrent();
		}
	}


	// -------- Update Instancer data --------
	TMap<UObject*, AActor*> InstanceActorMap;
	TMap<FString, UGeometryCollection*> SplitGCMap;
	TArray<UGeometryCollectionComponent*> NewGCCs;
	TMap<AActor*, TArray<FString>> ActorPropertyNamesMap;  // Use to avoid Set the same property in same SplitActor twice
	for (const FHoudiniInstancerPart& Part : Parts)
	{
		if (Part.SplitInstancerMap.IsEmpty())
			continue;

		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;
		const TArray<std::string>& AttribNames = Part.AttribNames;


		TArray<FTransform> Transforms;
		{
			const int32& PointCount = (PartInfo.instancedPartCount >= 1) ? PartInfo.instanceCount : PartInfo.pointCount;

			TArray<HAPI_Transform> HapiTransforms;
			HapiTransforms.SetNumUninitialized(PointCount);
			if (PartInfo.instancedPartCount >= 1)
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetInstancerPartTransforms(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_SRT, HapiTransforms.GetData(), 0, PointCount))
			else
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetInstanceTransformsOnPart(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_SRT, HapiTransforms.GetData(), 0, PointCount))

			Transforms.SetNumUninitialized(PointCount);
			for (int32 PointIdx = 0; PointIdx < PointCount; ++PointIdx)
			{
				const HAPI_Transform& HapiTransform = HapiTransforms[PointIdx];
				FTransform& Transform = Transforms[PointIdx];
				Transform.SetLocation(FVector(HapiTransform.position[0], HapiTransform.position[2], HapiTransform.position[1]) * POSITION_SCALE_TO_UNREAL_F);
				Transform.SetRotation(FQuat(HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[1], -HapiTransform.rotationQuaternion[3]));
				Transform.SetScale3D(FVector(HapiTransform.scale[0], HapiTransform.scale[2], HapiTransform.scale[1]));
			}
		}
		
		HAPI_AttributeOwner SplitActorsOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_SPLIT_ACTORS);
		TArray<int8> bSplitActors;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
			HAPI_ATTRIB_UNREAL_SPLIT_ACTORS, bSplitActors, SplitActorsOwner));

		// Retrieve custom floats for ISMC
		const TArray<HAPI_AttributeInfo>& CustomFloatAttribInfos = Part.CustomFloatAttribInfos;
		TArray<TArray<float>> CustomFloatsData;
		CustomFloatsData.SetNum(CustomFloatAttribInfos.Num());
		for (int32 CustomFloatIdx = 0; CustomFloatIdx < CustomFloatAttribInfos.Num(); ++CustomFloatIdx)
		{
			HAPI_AttributeInfo CustomFloatAttribInfo = CustomFloatAttribInfos[CustomFloatIdx];
			CustomFloatAttribInfo.tupleSize = 1;
			TArray<float>& CustomFloats = CustomFloatsData[CustomFloatIdx];
			CustomFloats.SetNumUninitialized(CustomFloatAttribInfo.count);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				Part.CustomFloatNames[CustomFloatIdx].c_str(), &CustomFloatAttribInfo, -1, CustomFloats.GetData(), 0, CustomFloatAttribInfo.count));
		}

		// Retrieve UProperties
		TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

		FHoudiniAttribute* ActorLocationAttrib = nullptr;
		if (const TSharedPtr<FHoudiniAttribute>* ActorLocationAttribPtr = PropAttribs.FindByPredicate([](const TSharedPtr<FHoudiniAttribute>& Attrib)
			{ return Attrib->GetAttributeName().Equals(HOUDINI_PROPERTY_ACTOR_LOCATION, ESearchCase::IgnoreCase); }))
			ActorLocationAttrib = ActorLocationAttribPtr->Get();

		const bool bCustomFolderPath = PropAttribs.ContainsByPredicate([](const TSharedPtr<FHoudiniAttribute>& Attrib)
			{
				return Attrib->GetAttributeName().Equals(TEXT("FolderPath"), ESearchCase::IgnoreCase);
			});

		// We need to split ISMCs by override material attribute
		FHoudiniAttribute* MatAttrib = nullptr;
		if (const TSharedPtr<FHoudiniAttribute>* MatAttribPtr = PropAttribs.FindByPredicate(
			[](const TSharedPtr<FHoudiniAttribute>& Attrib) { return Attrib->GetAttributeName().Equals(HOUDINI_PROPERTY_MATERIALS, ESearchCase::IgnoreCase); }))
			MatAttrib = MatAttribPtr->Get();
		if (MatAttrib)
		{
			const HAPI_StorageType& Storage = MatAttrib->GetStorage();
			if ((Storage != HAPI_STORAGETYPE_STRING) && (Storage != HAPI_STORAGETYPE_STRING_ARRAY))
				MatAttrib = nullptr;
		}

		class FHoudiniStringAttributDataAccessor : public FHoudiniAttribute
		{
		public:
			const TArray<int32>& GetIntValues() const { return IntValues; }
			const TArray<std::string>& GetStringValues() const { return StringValues; }
		};

		TArray<UMaterialInterface*> Mats;
		TConstArrayView<int32> MatArrayCounts;
		auto ParseMatAttribLambda = [&]()
			{
				if (Mats.IsEmpty() && MatArrayCounts.IsEmpty())
				{
					const TArray<int32>& StrIndices = static_cast<FHoudiniStringAttributDataAccessor*>(MatAttrib)->GetIntValues();
					
					TArray<UMaterialInterface*> UniqueMats;
					for (const std::string& Str : static_cast<FHoudiniStringAttributDataAccessor*>(MatAttrib)->GetStringValues())
						UniqueMats.Add(LoadAssetFromHoudiniString<UMaterialInterface>(Str));
					if (StrIndices.IsEmpty())  // Only one string
						Mats = UniqueMats;
					else
					{
						Mats.SetNumUninitialized(StrIndices.Num());
						for (int32 ElemIdx = 0; ElemIdx < StrIndices.Num(); ++ElemIdx)
							Mats[ElemIdx] = UniqueMats[StrIndices[ElemIdx]];

						if (MatAttrib->GetStorage() == HAPI_STORAGETYPE_STRING_ARRAY)
							MatArrayCounts = static_cast<FHoudiniArrayAttribute*>(MatAttrib)->Counts;
					}
				}
			};

#if ENABLE_INSTANCEDSKINNEDMESH_OUTPUT
		// We need to split ISKMCs by override TransformProvider attribute
		FHoudiniAttribute* TransformProviderAttrib = nullptr;
		if (const TSharedPtr<FHoudiniAttribute>* TransformProviderAttribPtr = PropAttribs.FindByPredicate(
			[](const TSharedPtr<FHoudiniAttribute>& Attrib) { return (Attrib->GetAttributeName() == HOUDINI_PROPERTY_TRANSFORMER_PROVIDER); }))
			TransformProviderAttrib = TransformProviderAttribPtr->Get();

		if (TransformProviderAttrib)
		{
			const HAPI_StorageType& Storage = TransformProviderAttrib->GetStorage();
			if ((Storage != HAPI_STORAGETYPE_STRING) && (Storage != HAPI_STORAGETYPE_STRING_ARRAY))
				TransformProviderAttrib = nullptr;
		}

		TArray<UTransformProviderData*> TransformProviders;
		auto ParseTransformProviderAttribLambda = [&]()
			{
				if (TransformProviders.IsEmpty())
				{
					const TArray<int32>& StrIndices = static_cast<FHoudiniStringAttributDataAccessor*>(TransformProviderAttrib)->GetIntValues();

					TArray<UTransformProviderData*> UniqueTPs;
					for (const std::string& Str : static_cast<FHoudiniStringAttributDataAccessor*>(TransformProviderAttrib)->GetStringValues())
						UniqueTPs.Add(LoadAssetFromHoudiniString<UTransformProviderData>(Str));
					if (StrIndices.IsEmpty())  // Only one string
						TransformProviders = UniqueTPs;
					else
					{
						TransformProviders.SetNumUninitialized((TransformProviderAttrib->GetOwner() == HAPI_ATTROWNER_DETAIL) ? 1 : Part.Info.pointCount);
						if (TransformProviderAttrib->GetStorage() == HAPI_STORAGETYPE_STRING)
						{
							for (int32 ElemIdx = 0; ElemIdx < TransformProviders.Num(); ++ElemIdx)
								TransformProviders[ElemIdx] = UniqueTPs[StrIndices[ElemIdx]];
						}
						else if (TransformProviderAttrib->GetStorage() == HAPI_STORAGETYPE_STRING_ARRAY)
						{
							const TArray<int32>& ElemArrayCounts = static_cast<FHoudiniArrayAttribute*>(TransformProviderAttrib)->Counts;
							for (int32 ElemIdx = 0; ElemIdx < ElemArrayCounts.Num(); ++ElemIdx)
								TransformProviders[ElemIdx] = (ElemArrayCounts[ElemIdx] <= 0) ? nullptr :
								UniqueTPs[StrIndices[((ElemIdx <= 0) ? 0 : ElemArrayCounts[ElemIdx - 1])]];
						}
					}
				}
			};
#endif

		for (const auto& SplitInstancer : Part.SplitInstancerMap)
		{
			const FString& SplitValue = SplitInstancer.Value.SplitValue;

			for (const auto& AssetIndices : SplitInstancer.Value.AssetIndicesMap)
			{
				EHoudiniInstancerOutputMode InstanceType = AssetIndices.Key.Key.Key;
				const TArray<int32>& PointIndices = AssetIndices.Value;
				UObject* Instance = AssetIndices.Key.Value;
				const int32& MainPointIdx = PointIndices[0];
				bool bSplitActor = false;
				if (!bSplitActors.IsEmpty())
					bSplitActor = bSplitActors[POINT_ATTRIB_ENTRY_IDX(SplitActorsOwner, MainPointIdx)] >= 1;


				switch (InstanceType)
				{
				case EHoudiniInstancerOutputMode::Auto:
				case EHoudiniInstancerOutputMode::ISMC:
				case EHoudiniInstancerOutputMode::HISMC:
				{
					const int8& NumCustomFloats = AssetIndices.Key.Key.Value;
					if (UStaticMesh* SM = Cast<UStaticMesh>(Instance))
					{
						auto FindOrCreateInstanceComponentLambda = [&](const TArray<int32>& InstPointIndices, const bool& bReverseCull, const TArray<UMaterialInterface*>& OverrideMaterials)
							{
								const int32& InstMainPointIdx = InstPointIndices[0];

								if (InstanceType == EHoudiniInstancerOutputMode::Auto && ((InstPointIndices.Num() >= 2) || (NumCustomFloats >= 1)))
									InstanceType = EHoudiniInstancerOutputMode::ISMC;

								TSubclassOf<UStaticMeshComponent> SMCClass = nullptr;
								switch (InstanceType)
								{
								case EHoudiniInstancerOutputMode::Auto: SMCClass = UStaticMeshComponent::StaticClass(); break;
								case EHoudiniInstancerOutputMode::ISMC: SMCClass = UInstancedStaticMeshComponent::StaticClass(); break;
								case EHoudiniInstancerOutputMode::HISMC: SMCClass = UHierarchicalInstancedStaticMeshComponent::StaticClass(); break;
								}

								FHoudiniInstancedMeshOutput NewISMOutput;
								if (FHoudiniInstancedMeshOutput* FoundISMOutput = FHoudiniOutputUtils::FindOutputHolder(OldInstancedMeshOutputs,
									[&](const FHoudiniInstancedMeshOutput* ISMOutput)
									{
										if (ISMOutput->CanReuse(SplitValue, bSplitActor))
										{
											if (const UMeshComponent* SMC = ISMOutput->Find(Node))
											{
												if (SMC->GetClass() == SMCClass)
													return Cast<UStaticMeshComponent>(SMC)->GetStaticMesh() == SM;
											}
										}

										return false;
									}))
									NewISMOutput = *FoundISMOutput;

								UStaticMeshComponent* NewSMC = Cast<UStaticMeshComponent>(NewISMOutput.CreateOrUpdate(GetNode(), SMCClass, SplitValue, bSplitActor));
								NewSMC->SetStaticMesh(SM);
								UInstancedStaticMeshComponent* NewISMC = Cast<UInstancedStaticMeshComponent>(NewSMC);

								// Set uproperties
								for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
								{
									const FString& PropertyName = PropAttrib->GetAttributeName();
									if (NewISMC && (PropertyName == HOUDINI_PROPERTY_INSTANCE_START_CULL_DISTANCE))
									{
										TArray<int32> IntData = PropAttrib->GetIntData(POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx));
										if (IntData.IsValidIndex(0))
											NewISMC->InstanceStartCullDistance = IntData[0];
										continue;
									}
									else if (NewISMC && (PropertyName == HOUDINI_PROPERTY_INSTANCE_END_CULL_DISTANCE))
									{
										TArray<int32> IntData = PropAttrib->GetIntData(POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx));
										if (IntData.IsValidIndex(0))
											NewISMC->InstanceEndCullDistance = IntData[0];
										continue;
									}

									if (PropertyName == HOUDINI_PROPERTY_MATERIALS)
									{
										const int32 NumMats = FMath::Min(OverrideMaterials.Num(), NewSMC->GetNumMaterials());
										for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
										{
											if (OverrideMaterials[MatIdx])
												NewSMC->SetMaterial(MatIdx, OverrideMaterials[MatIdx]);
										}
										continue;
									}

									PropAttrib->SetObjectPropertyValues(NewSMC, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx));
								}

								if (NewISMC)
								{
									// Clear Transform
									if (!NewISMC->GetRelativeTransform().Equals(FTransform::Identity))
										NewISMC->SetRelativeTransform(FTransform::Identity);

									struct FHoudiniComponentNoCollisionScope
									{
										UStaticMeshComponent* Component = nullptr;
										FName CollisionProfileName;
										ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
										bool bUseDefaultCollision = false;

										FHoudiniComponentNoCollisionScope(UStaticMeshComponent* InComponent)
										{
											if (IsValid(InComponent))
											{
												Component = InComponent;
												CollisionEnabled = Component->GetCollisionEnabled();
												if (CollisionEnabled != ECollisionEnabled::NoCollision)
												{
													bUseDefaultCollision = Component->bUseDefaultCollision;
													CollisionProfileName = Component->GetCollisionProfileName();
													Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
												}
											}
										}

										~FHoudiniComponentNoCollisionScope()
										{
											if (IsValid(Component))
											{
												if (CollisionEnabled != ECollisionEnabled::NoCollision)
												{
													if (CollisionProfileName != UCollisionProfile::CustomCollisionProfileName)
														Component->SetCollisionProfileName(CollisionProfileName);
													else
														Component->SetCollisionEnabled(CollisionEnabled);

													if (bUseDefaultCollision)
														Component->bUseDefaultCollision = true;
												}
											}
										}
									};

									{
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
										// Disable collision when output HISMC, to have better performance.  // TODO: check out why?
										FHoudiniComponentNoCollisionScope NoCollisionScope(Cast<UHierarchicalInstancedStaticMeshComponent>(NewISMC));
#endif

										auto UpdateInstancesLambda = [&](const int32& NumInsts)
											{
												if (NumInsts >= 1)
												{
													TArray<FTransform> TransformsToUpdate;
													TransformsToUpdate.Reserve(NumInsts);
													for (int32 InstIdx = 0; InstIdx < NumInsts; ++InstIdx)
														TransformsToUpdate.Add(Transforms[InstPointIndices[InstIdx]]);
													NewISMC->BatchUpdateInstancesTransforms(0, TransformsToUpdate);
												}
											};

										const int32 NumOldInsts = NewISMC->GetInstanceCount();
										const int32 NumNewInsts = InstPointIndices.Num();

										if (NumOldInsts < NumNewInsts)
										{
											UpdateInstancesLambda(NumOldInsts);

											TArray<FTransform> TransformsToCreate;
											TransformsToCreate.Reserve(NumNewInsts - NumOldInsts);
											for (int32 InstIdx = NumOldInsts; InstIdx < NumNewInsts; ++InstIdx)
												TransformsToCreate.Add(Transforms[InstPointIndices[InstIdx]]);
											NewISMC->AddInstances(TransformsToCreate, false);
										}
										else if (NumOldInsts == NumNewInsts)
										{
											UpdateInstancesLambda(NumNewInsts);
										}
										else if (NumOldInsts > NumNewInsts)
										{
											TArray<int32> InstIndicesToRemove;
											InstIndicesToRemove.Reserve(NumOldInsts - NumNewInsts);
											for (int32 InstIdx = NumOldInsts - 1; InstIdx >= NumNewInsts; --InstIdx)
												InstIndicesToRemove.Add(InstIdx);

											// Disable collision when remove instances, to have better performance.  // TODO: check out why?
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
											FHoudiniComponentNoCollisionScope RemoveNoCollisionScope(NoCollisionScope.Component ? nullptr : NewISMC);  // ensure only once
#else
											FHoudiniComponentNoCollisionScope RemoveNoCollisionScope(NewISMC);
#endif

											if (!InstIndicesToRemove.IsEmpty())
											{
												NewISMC->SelectedInstances.Empty();  // Clear selected instance to avoid crash
												NewISMC->RemoveInstances(InstIndicesToRemove);
											}

											UpdateInstancesLambda(NumNewInsts);
										}

										// Set Custom Floats
										NewISMC->SetNumCustomDataFloats(int32(NumCustomFloats));
										if (NumCustomFloats >= 1)
										{
											for (int32 InstIdx = 0; InstIdx < NumNewInsts; ++InstIdx)
											{
												TArray<float> InstCustomFloats;
												for (int8 CustomFloatIdx = 0; CustomFloatIdx < NumCustomFloats; ++CustomFloatIdx)
													InstCustomFloats.Add(CustomFloatsData[CustomFloatIdx][POINT_ATTRIB_ENTRY_IDX(CustomFloatAttribInfos[CustomFloatIdx].owner, InstPointIndices[InstIdx])]);
												NewISMC->SetCustomData(InstIdx, InstCustomFloats);
											}
										}
									}  // EndNoCollisionScope

									NewISMC->SetReverseCulling(bReverseCull);
								}
								else if (!NewSMC->GetRelativeTransform().Equals(Transforms[InstMainPointIdx]))
									NewSMC->SetRelativeTransform(Transforms[InstMainPointIdx]);

								SET_SPLIT_ACTOR_UPROPERTIES(NewISMOutput, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx), false);

								NewSMC->Modify();

								NewInstancedMeshOutputs.Add(NewISMOutput);
							};

						if (MatAttrib && (MatAttrib->GetOwner() != HAPI_ATTROWNER_DETAIL))
						{
							const int32 NumMatSlots = SM->GetStaticMaterials().Num();
							ParseMatAttribLambda();
							if (MatAttrib->GetStorage() == HAPI_STORAGETYPE_STRING)
							{
								TMap<TPair<bool, UMaterialInterface*>, TArray<int32>> MatPointIndicesMap;
								for (const int32& PointIdx : PointIndices)
								{
									const FVector InstScale = Transforms[PointIdx].GetScale3D();
									MatPointIndicesMap.FindOrAdd(TPair<bool, UMaterialInterface*>(
										bool((int32(InstScale.X < 0.0) + int32(InstScale.Y < 0.0) + int32(InstScale.Z < 0.0)) % 2), Mats[PointIdx])).Add(PointIdx);
								}
								for (const auto& MatPointIndices : MatPointIndicesMap)
									FindOrCreateInstanceComponentLambda(MatPointIndices.Value, MatPointIndices.Key.Key, TArray<UMaterialInterface*>{ MatPointIndices.Key.Value });
							}
							else
							{
								TMap<TPair<bool, TArray<UMaterialInterface*>>, TArray<int32>> MatPointIndicesMap;
								for (const int32& PointIdx : PointIndices)
								{
									const FVector InstScale = Transforms[PointIdx].GetScale3D();
									TPair<bool, TArray<UMaterialInterface*>> Identifier;
									Identifier.Key = bool((int32(InstScale.X < 0.0) + int32(InstScale.Y < 0.0) + int32(InstScale.Z < 0.0)) % 2);
									if (MatArrayCounts.IsEmpty())
										Identifier.Value.Add(Mats[PointIdx]);
									else
									{
										const int32 StartDataIdx = ((PointIdx <= 0) ? 0 : MatArrayCounts[PointIdx - 1]);
										Identifier.Value.Append(Mats.GetData() + StartDataIdx, FMath::Min(MatArrayCounts[PointIdx] - StartDataIdx, NumMatSlots));
									}
									MatPointIndicesMap.FindOrAdd(Identifier).Add(PointIdx);
								}

								for (const auto& MatPointIndices : MatPointIndicesMap)
									FindOrCreateInstanceComponentLambda(MatPointIndices.Value, MatPointIndices.Key.Key, MatPointIndices.Key.Value);
							}
						}
						else
						{
							TArray<int32> PositiveIndices; PositiveIndices.Reserve(PointIndices.Num());  // Most like to be
							TArray<int32> NegativeIndices;
							for (const int32& PointIdx : PointIndices)
							{
								const FVector InstScale = Transforms[PointIdx].GetScale3D();
								if ((int32(InstScale.X < 0.0) + int32(InstScale.Y < 0.0) + int32(InstScale.Z < 0.0)) % 2)
									NegativeIndices.Add(PointIdx);
								else
									PositiveIndices.Add(PointIdx);
							}

							if (MatAttrib)  // Must on detail
								ParseMatAttribLambda();

							if (!PositiveIndices.IsEmpty())
								FindOrCreateInstanceComponentLambda(PositiveIndices, false, Mats);
							if (!NegativeIndices.IsEmpty())
								FindOrCreateInstanceComponentLambda(NegativeIndices, true, Mats);
						}
					}
#if ENABLE_INSTANCEDSKINNEDMESH_OUTPUT
					else if (USkeletalMesh* SKM = Cast<USkeletalMesh>(Instance))
					{
						auto FindOrCreateISKMCLambda = [&](const TArray<int32>& InstPointIndices, const TArray<UMaterialInterface*>& OverrideMaterials, UTransformProviderData* TransformProvider)
							{
								const int32& InstMainPointIdx = InstPointIndices[0];
								FHoudiniInstancedMeshOutput NewISKMOutput;
								if (FHoudiniInstancedMeshOutput* FoundISKMOutput = FHoudiniOutputUtils::FindOutputHolder(OldInstancedMeshOutputs,
									[&](const FHoudiniInstancedMeshOutput* ISMOutput)
									{
										if (ISMOutput->CanReuse(SplitValue, bSplitActor))
										{
											if (const UMeshComponent* MC = ISMOutput->Find(Node))
												return (MC->GetClass() == UInstancedSkinnedMeshComponent::StaticClass()) &&
													(Cast<UInstancedSkinnedMeshComponent>(MC)->GetSkinnedAsset() == SKM);
										}

										return false;
									}))
									NewISKMOutput = *FoundISKMOutput;

								UInstancedSkinnedMeshComponent* NewISKMC = Cast<UInstancedSkinnedMeshComponent>(
									NewISKMOutput.CreateOrUpdate(GetNode(), UInstancedSkinnedMeshComponent::StaticClass(), SplitValue, bSplitActor));
								if (NewISKMC->GetInstanceCount() >= 1)
									NewISKMC->ClearInstances();
								else
									NewISKMC->SetCollisionProfileName(TEXT("NoCollision"));  // Disable collision by default
								NewISKMC->SetSkinnedAsset(SKM);

								// Set uproperties
								int32 InstanceStartCullDistance = 0;
								int32 InstanceEndCullDistance = 0;
								NewISKMC->GetCullDistances(InstanceStartCullDistance, InstanceEndCullDistance);
								for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
								{
									const FString& PropertyName = PropAttrib->GetAttributeName();
									if (PropertyName == HOUDINI_PROPERTY_MATERIALS)
									{
										const int32 NumMats = FMath::Min(OverrideMaterials.Num(), NewISKMC->GetNumMaterials());
										for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
										{
											if (OverrideMaterials[MatIdx])
												NewISKMC->SetMaterial(MatIdx, OverrideMaterials[MatIdx]);
										}
										continue;
									}
									else if (PropertyName == HOUDINI_PROPERTY_TRANSFORMER_PROVIDER)
									{
										NewISKMC->SetTransformProvider(TransformProvider);
										continue;
									}
									if (PropertyName == HOUDINI_PROPERTY_INSTANCE_START_CULL_DISTANCE)
									{
										TArray<int32> IntData = PropAttrib->GetIntData(POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx));
										if (IntData.IsValidIndex(0))
											InstanceStartCullDistance = IntData[0];
										continue;
									}
									else if (PropertyName == HOUDINI_PROPERTY_INSTANCE_END_CULL_DISTANCE)
									{
										TArray<int32> IntData = PropAttrib->GetIntData(POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx));
										if (IntData.IsValidIndex(0))
											InstanceEndCullDistance = IntData[0];
										continue;
									}

									PropAttrib->SetObjectPropertyValues(NewISKMC, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx));
								}
								NewISKMC->SetCullDistances(InstanceStartCullDistance, InstanceEndCullDistance);

								// Clear Transform
								if (!NewISKMC->GetRelativeTransform().Equals(FTransform::Identity))
									NewISKMC->SetRelativeTransform(FTransform::Identity);


								TArray<FTransform> InstTransforms;
								InstTransforms.Reserve(InstPointIndices.Num());
								TArray<int32> InstAnimIdcs;
								InstAnimIdcs.Reserve(InstPointIndices.Num());
								for (const int32& PtIdx : InstPointIndices)
								{
									InstTransforms.Add(Transforms[PtIdx]);
									InstAnimIdcs.Add(-1);
								}

								TArray<FPrimitiveInstanceId> InstIds = NewISKMC->AddInstances(InstTransforms, InstAnimIdcs, NumCustomFloats >= 1);
								// Set Custom Floats
								NewISKMC->SetNumCustomDataFloats(int32(NumCustomFloats));
								if (NumCustomFloats >= 1)
								{
									for (int32 InstIdx = 0; InstIdx < InstPointIndices.Num(); ++InstIdx)
									{
										TArray<float> InstCustomFloats;
										for (int8 CustomFloatIdx = 0; CustomFloatIdx < NumCustomFloats; ++CustomFloatIdx)
											InstCustomFloats.Add(CustomFloatsData[CustomFloatIdx][POINT_ATTRIB_ENTRY_IDX(CustomFloatAttribInfos[CustomFloatIdx].owner, InstPointIndices[InstIdx])]);
										NewISKMC->SetCustomData(InstIds[InstIdx], InstCustomFloats);
									}
								}
								SET_SPLIT_ACTOR_UPROPERTIES(NewISKMOutput, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), InstMainPointIdx), false);

								NewISKMC->MarkRenderStateDirty();
								NewISKMC->UpdateBounds();
								NewISKMC->OptimizeInstanceData();
								//NewISKMC->Modify();

								NewInstancedMeshOutputs.Add(NewISKMOutput);
							};

						if (MatAttrib)
							ParseMatAttribLambda();
						if (TransformProviderAttrib)
							ParseTransformProviderAttribLambda();

						if (MatAttrib && (MatAttrib->GetOwner() != HAPI_ATTROWNER_DETAIL))
						{
							const int32 NumMatSlots = SKM->GetMaterials().Num();
							if (TransformProviderAttrib && (TransformProviderAttrib->GetOwner() != HAPI_ATTROWNER_DETAIL))
							{
								if (MatAttrib->GetStorage() == HAPI_STORAGETYPE_STRING)
								{
									TMap<TPair<UMaterialInterface*, UTransformProviderData*>, TArray<int32>> MatPointIndicesMap;
									for (const int32& PointIdx : PointIndices)
										MatPointIndicesMap.FindOrAdd(TPair<UMaterialInterface*, UTransformProviderData*>(
											Mats[PointIdx], TransformProviders[PointIdx])).Add(PointIdx);
									for (const auto& MatPointIndices : MatPointIndicesMap)
										FindOrCreateISKMCLambda(MatPointIndices.Value, TArray<UMaterialInterface*>{ MatPointIndices.Key.Key }, MatPointIndices.Key.Value);
								}
								else
								{
									TMap<TPair<TArray<UMaterialInterface*>, UTransformProviderData*>, TArray<int32>> MatPointIndicesMap;
									for (const int32& PointIdx : PointIndices)
									{
										if (MatArrayCounts.IsEmpty())
											MatPointIndicesMap.FindOrAdd(TPair<TArray<UMaterialInterface*>, UTransformProviderData*>(
												TArray<UMaterialInterface*>{ Mats[PointIdx] }, TransformProviders[PointIdx])).Add(PointIdx);
										else
										{
											const int32 StartDataIdx = ((PointIdx <= 0) ? 0 : MatArrayCounts[PointIdx - 1]);
											MatPointIndicesMap.FindOrAdd(TPair<TArray<UMaterialInterface*>, UTransformProviderData*>(
												TArray<UMaterialInterface*>(Mats.GetData() + StartDataIdx, FMath::Min(MatArrayCounts[PointIdx] - StartDataIdx, NumMatSlots)), TransformProviders[PointIdx])).Add(PointIdx);
										}
									}

									for (const auto& MatPointIndices : MatPointIndicesMap)
										FindOrCreateISKMCLambda(MatPointIndices.Value, MatPointIndices.Key.Key, MatPointIndices.Key.Value);
								}
							}
							else
							{
								UTransformProviderData* TransformProvider = TransformProviders.IsValidIndex(0) ? TransformProviders[0] : nullptr;
								if (MatAttrib->GetStorage() == HAPI_STORAGETYPE_STRING)
								{
									TMap<UMaterialInterface*, TArray<int32>> MatPointIndicesMap;
									for (const int32& PointIdx : PointIndices)
										MatPointIndicesMap.FindOrAdd(Mats[PointIdx]).Add(PointIdx);
									for (const auto& MatPointIndices : MatPointIndicesMap)
										FindOrCreateISKMCLambda(MatPointIndices.Value, TArray<UMaterialInterface*>{ MatPointIndices.Key }, TransformProvider);
								}
								else
								{
									TMap<TArray<UMaterialInterface*>, TArray<int32>> MatPointIndicesMap;
									for (const int32& PointIdx : PointIndices)
									{
										if (MatArrayCounts.IsEmpty())
											MatPointIndicesMap.FindOrAdd(TArray<UMaterialInterface*>{ Mats[PointIdx] }).Add(PointIdx);
										else
										{
											const int32 StartDataIdx = ((PointIdx <= 0) ? 0 : MatArrayCounts[PointIdx - 1]);
											MatPointIndicesMap.FindOrAdd(TArray<UMaterialInterface*>(Mats.GetData() + StartDataIdx, FMath::Min(MatArrayCounts[PointIdx] - StartDataIdx, NumMatSlots))).Add(PointIdx);
										}
									}

									for (const auto& MatPointIndices : MatPointIndicesMap)
										FindOrCreateISKMCLambda(MatPointIndices.Value, MatPointIndices.Key, TransformProvider);
								}
							}
						}
						else if (TransformProviderAttrib && (TransformProviderAttrib->GetOwner() != HAPI_ATTROWNER_DETAIL))
						{
							TMap<UTransformProviderData*, TArray<int32>> TPPointIndicesMap;
							for (const int32& PointIdx : PointIndices)
								TPPointIndicesMap.FindOrAdd(TransformProviders[PointIdx]).Add(PointIdx);
							for (const auto& TPPointIndices : TPPointIndicesMap)
								FindOrCreateISKMCLambda(TPPointIndices.Value, TArray<UMaterialInterface*>{}, TPPointIndices.Key);
						}
						else
							FindOrCreateISKMCLambda(PointIndices, Mats, TransformProviders.IsValidIndex(0) ? TransformProviders[0] : nullptr);
#if WAIT_SKELETAL_MESH_COMPILATION
						FAssetCompilingManager::Get().FinishCompilationForObjects({ SKM });
#endif
					}
#endif
				}
				break;
				case EHoudiniInstancerOutputMode::Foliage:
				{
					FHoudiniFoliageOutput NewFoliageOutput;
					if (FHoudiniFoliageOutput* FoundFoliageOutput = FHoudiniOutputUtils::FindOutputHolder(OldFoliageOutputs,
						[&](const FHoudiniFoliageOutput* FoliageOutput) { return FoliageOutput->CanReuse(SplitValue, bSplitActor); }))
						NewFoliageOutput = *FoundFoliageOutput;

					if (NewFoliageOutput.Create(GetNode(), Instance, Name, SplitValue, bSplitActor, PointIndices, Transforms))
						NewFoliageOutputs.Add(NewFoliageOutput);
					else
						NewFoliageOutput.Destroy(GetNode());
				}
				break;
				case EHoudiniInstancerOutputMode::Components:
				{
					// Try to find the most matched InstancedComponentOutput
					TSubclassOf<USceneComponent> ComponentClass = Cast<UClass>(Instance);
					TDoubleLinkedList<FHoudiniInstancedComponentOutput*>::TDoubleLinkedListNode* MostMatchedListNode = nullptr;
					int32 MinScore = -1;
					for (auto OldOutputIter = OldInstancedComponentOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
					{
						if (OldOutputIter->GetValue()->CanReuse(SplitValue, bSplitActor))
						{
							const int32 Score = OldOutputIter->GetValue()->GetMatchScore(ComponentClass, PointIndices.Num());
							if (Score == 0)  // Perfect matched, we should stop searching here
							{
								MostMatchedListNode = OldOutputIter;
								break;
							}
							if (Score > 0 && (MinScore < 0 || MinScore > Score))
							{
								MostMatchedListNode = OldOutputIter;
								MinScore = Score;
							}
						}
					}

					FHoudiniInstancedComponentOutput NewInstancedComponentOutput;
					if (MostMatchedListNode)
					{
						NewInstancedComponentOutput = *MostMatchedListNode->GetValue();
						OldInstancedComponentOutputs.RemoveNode(MostMatchedListNode);
					}

					TArray<USceneComponent*> Components;
					NewInstancedComponentOutput.Update(ComponentClass, GetNode(), SplitValue, bSplitActor, PointIndices.Num(), Components);
					for (int32 InstIdx = 0; InstIdx < PointIndices.Num(); ++InstIdx)
					{
						USceneComponent* SC = Components[InstIdx];
						const int32& PointIdx = PointIndices[InstIdx];
						SC->SetRelativeTransform(Transforms[PointIdx]);

						for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
							PropAttrib->SetObjectPropertyValues(SC, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), PointIdx));

						SC->Modify();
					}

					NewInstancedComponentOutputs.Add(NewInstancedComponentOutput);
				}
				break;
				case EHoudiniInstancerOutputMode::Actors:
				{
					// Try to find the most matched InstancedActorOutput
					TDoubleLinkedList<FHoudiniInstancedActorOutput*>::TDoubleLinkedListNode* MostMatchedListNode = nullptr;
					int32 MinScore = -1;
					for (auto OldOutputIter = OldInstancedActorOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
					{
						const int32 Score = OldOutputIter->GetValue()->GetMatchScore(Instance, PointIndices.Num());
						if (Score == 0)  // Perfect matched, we should stop searching here
						{
							MostMatchedListNode = OldOutputIter;
							break;
						}
						if ((Score > 0) && ((MinScore < 0) || (MinScore > Score)))
						{
							MostMatchedListNode = OldOutputIter;
							MinScore = Score;
						}
					}
					
					FHoudiniInstancedActorOutput NewInstancedActorOutput;
					if (MostMatchedListNode)
					{
						NewInstancedActorOutput = *MostMatchedListNode->GetValue();
						OldInstancedActorOutputs.RemoveNode(MostMatchedListNode);
					}
					
					FTransform SplitTransform = Node->GetActorTransform();
					if (ActorLocationAttrib)
					{
						const TArray<float> FloatData = ActorLocationAttrib->GetFloatData(PointIndices[0]);
						if (FloatData.Num() >= 3)
							SplitTransform.SetLocation(FVector(double(FloatData[0])* POSITION_SCALE_TO_UNREAL, double(FloatData[2])* POSITION_SCALE_TO_UNREAL, double(FloatData[1])* POSITION_SCALE_TO_UNREAL));
					}
					if (NewInstancedActorOutput.Update(Node, SplitTransform, SplitValue, Instance, InstanceActorMap.FindOrAdd(Instance),
						PointIndices, Transforms, [&](AActor* Actor, const int32& PointIdx)
							{
								for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
								{
									if (!PropAttrib->GetAttributeName().Equals(HOUDINI_PROPERTY_ACTOR_LOCATION, ESearchCase::IgnoreCase))  // We should not translate actors here
										PropAttrib->SetObjectPropertyValues(Actor, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), PointIdx));
								}

								USceneComponent* RootComponent = Actor->GetRootComponent();
								if (RootComponent && RootComponent->GetClass() != USceneComponent::StaticClass())  // We should also set RootComponent properties, like PointLight. however, if is SceneComponent, then we need NOT to set
								{
									for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
										PropAttrib->SetObjectPropertyValues(RootComponent, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), PointIdx));
								}
							}, bCustomFolderPath))
						NewInstancedActorOutputs.Add(NewInstancedActorOutput);
					else
						NewInstancedActorOutput.Destroy();
				}
				break;
				case EHoudiniInstancerOutputMode::GeometryCollection:
				if (UStaticMesh* SM = Cast<UStaticMesh>(Instance)){
					UGeometryCollection* GC = nullptr;
					if (UGeometryCollection** FoundGCPtr = SplitGCMap.Find(SplitValue))
						GC = *FoundGCPtr;

					if (!IsValid(GC))
					{
						FHoudiniGeometryCollectionOutput NewGCOutput;
						for (auto OldOutputIter = OldGeometryCollectionOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
						{
							if (OldOutputIter->GetValue()->GetSplitValue() == SplitValue || (OldOutputIter == OldGeometryCollectionOutputs.GetTail()))  // last one will be used
							{
								NewGCOutput = *OldOutputIter->GetValue();
								OldGeometryCollectionOutputs.RemoveNode(OldOutputIter);
								break;
							}
						}
						AGeometryCollectionActor* GCActor = NewGCOutput.Update(SplitValue, Name, Node, GC);
						SET_OBJECT_UPROPERTIES(GCActor, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), MainPointIdx));
						SET_OBJECT_UPROPERTIES(GCActor->GetGeometryCollectionComponent(), POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), MainPointIdx));
						SET_OBJECT_UPROPERTIES(GC, POINT_ATTRIB_ENTRY_IDX(PropAttrib->GetOwner(), MainPointIdx));

						NewGeometryCollectionOutputs.Add(NewGCOutput);
						NewGCCs.Add(GCActor->GetGeometryCollectionComponent());
						SplitGCMap.Add(SplitValue, GC);
					}
					
					for (const int32& PointIdx : PointIndices)
					{
						FGeometryCollectionSource GCSource(SM, Transforms[PointIdx], {});
						for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
						{
							const HAPI_AttributeOwner& PropAttribOwner = PropAttrib->GetOwner();
							PropAttrib->SetStructPropertyValues(&GCSource, FGeometryCollectionSource::StaticStruct(),
								POINT_ATTRIB_ENTRY_IDX(PropAttribOwner, PointIdx));
						}
						GC->GeometrySource.Add(GCSource);
						FGeometryCollectionEngineConversion::AppendStaticMesh(SM, {}, Transforms[PointIdx], GC, false/*bReindexMaterials*/, false, false, false);
					}
				}
				break;
				}
			}
		}
	}


	// -------- Post-processing --------
	// Destroy old outputs, like this->Destroy()
	for (const FHoudiniInstancedMeshOutput* OldISMOutput : OldInstancedMeshOutputs)
		OldISMOutput->Destroy(Node);
	OldInstancedMeshOutputs.Empty();

	for (const FHoudiniInstancedComponentOutput* OldInstancedComponentOutput : OldInstancedComponentOutputs)
		OldInstancedComponentOutput->Destroy(Node);
	OldInstancedComponentOutputs.Empty();

	for (const FHoudiniInstancedActorOutput* OldInstancedActorOutput : OldInstancedActorOutputs)
		OldInstancedActorOutput->Destroy();
	OldInstancedActorOutputs.Empty();

	for (const FHoudiniFoliageOutput* OldFoliageOutput : OldFoliageOutputs)
		OldFoliageOutput->Destroy(Node);
	OldFoliageOutputs.Empty();

	for (const FHoudiniGeometryCollectionOutput* OldGeometryCollectionOutput : OldGeometryCollectionOutputs)
		OldGeometryCollectionOutput->Destroy();
	OldGeometryCollectionOutputs.Empty();

	for (const auto& SplitGC : SplitGCMap)
	{
		UGeometryCollection* GC = SplitGC.Value;

		// Ensure we have a SizeSpecificData
		if (GC->SizeSpecificData.IsEmpty())
			GC->SizeSpecificData.Add(FGeometryCollectionSizeSpecificData());
		
		GC->InitializeMaterials();
		GC->InvalidateCollection();
		GC->CreateSimulationData();  // Otherwise asset cannot save
		GC->RebuildRenderData();
	}

#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
	// TODO: Improve me!
	// Somehow, in 5.4 - the Component doesnt seem to update properly if we set the GC too early
	// (which is what we used to do) - 	resetting the whole GC as temporary fix
	for (UGeometryCollectionComponent* GCC : NewGCCs)  // Force refresh to allow gcc display
		GCC->SetRestCollection(GCC->GetRestCollection());
#endif

	// Update output holders
	InstancedMeshOutputs = NewInstancedMeshOutputs;
	InstancedComponentOutputs = NewInstancedComponentOutputs;
	InstancedActorOutputs = NewInstancedActorOutputs;
	FoliageOutputs = NewFoliageOutputs;
	GeometryCollectionOutputs = NewGeometryCollectionOutputs;

	return true;
}

void UHoudiniOutputInstancer::Destroy() const
{
	for (const FHoudiniInstancedMeshOutput& OldISMOutput : InstancedMeshOutputs)
		OldISMOutput.Destroy(GetNode());

	for (const FHoudiniInstancedComponentOutput& OldInstancedComponentOutput : InstancedComponentOutputs)
		OldInstancedComponentOutput.Destroy(GetNode());

	for (const FHoudiniInstancedActorOutput& OldInstancedActorOutput : InstancedActorOutputs)
		OldInstancedActorOutput.Destroy();

	for (const FHoudiniFoliageOutput& OldFoliageOutput : FoliageOutputs)
		OldFoliageOutput.Destroy(GetNode());

	for (const FHoudiniGeometryCollectionOutput& OldGCOutput : GeometryCollectionOutputs)
		OldGCOutput.Destroy();
}

void UHoudiniOutputInstancer::CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const
{
	for (const FHoudiniInstancedMeshOutput& ISMOutput : InstancedMeshOutputs)
	{
		if (ISMOutput.IsSplitActor())
			InOutSplitValues.FindOrAdd(ISMOutput.GetSplitValue());
	}

	for (const FHoudiniInstancedComponentOutput& ICOutput : InstancedComponentOutputs)
	{
		if (ICOutput.IsSplitActor())
			InOutSplitValues.FindOrAdd(ICOutput.GetSplitValue());
	}

	// We need NOT to add InstancedActorOutputs, because they did NOT attach to any SplitActors

	for (const FHoudiniFoliageOutput& FoliageOutput : FoliageOutputs)
	{
		if (FoliageOutput.IsSplitActor())
			InOutSplitValues.FindOrAdd(FoliageOutput.GetSplitValue());
	}
}

void UHoudiniOutputInstancer::OnNodeTransformed(const FMatrix& DeltaXform) const
{
	for (const FHoudiniInstancedActorOutput& IAOutput : InstancedActorOutputs)
		IAOutput.TransformActors(DeltaXform);

	for (const FHoudiniGeometryCollectionOutput& GCOutput : GeometryCollectionOutputs)
		GCOutput.TransformActor(DeltaXform);
}

void UHoudiniOutputInstancer::DestroyStandaloneActors() const
{
	for (const FHoudiniInstancedActorOutput& IAOutput : InstancedActorOutputs)
		IAOutput.Destroy();

	for (const FHoudiniGeometryCollectionOutput& GCOutput : GeometryCollectionOutputs)
		GCOutput.Destroy();
}
