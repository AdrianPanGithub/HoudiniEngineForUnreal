// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"

#include "HoudiniEngine.h"
#include "HoudiniApi.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineCommon.h"
#include "HoudiniNode.h"
#include "HoudiniAttribute.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniCurvesComponent.h"
#include "HoudiniMeshComponent.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

bool AHoudiniNode::HapiUpdateOutputs(const TArray<FString>& GeoNames, const TArray<HAPI_GeoInfo>& GeoInfos, const TArray<TArray<HAPI_PartInfo>>& GeoPartInfos)
{
	if (GeoNames.IsEmpty())
		return true;

	const double StartTime = FPlatformTime::Seconds();
	
	FHoudiniEngine::Get().HoudiniMainTaskMessageEvent.Broadcast(0.5f, LOCTEXT("HoudiniOutputMessage", "Process Houdini Outputs..."));

	FHoudiniAttribute::ClearBlueprintProperties();


	struct FHoudiniOutputDesc
	{
		FHoudiniOutputDesc(const int32& InGeoIdx, const TSubclassOf<UHoudiniOutput>& InClass,
			UHoudiniOutput* InOutput, const HAPI_PartInfo& PartInfo) :
			GeoIdx(InGeoIdx), Class(InClass), Output(InOutput) { PartInfos.Add(PartInfo); }

		const int32 GeoIdx;
		const TSubclassOf<UHoudiniOutput> Class;
		UHoudiniOutput* Output = nullptr;
		TArray<HAPI_PartInfo> PartInfos;
	};

	struct FHoudiniOutputConverter
	{
		FHoudiniOutputConverter(const int32& InGeoIdx, const HAPI_PartInfo& PartInfo) :
			GeoIdx(InGeoIdx) { PartInfos.Add(PartInfo); }

		const int32 GeoIdx;
		TArray<HAPI_PartInfo> PartInfos;
	};

	class FHoudiniOutputNameModifier : public UHoudiniOutput
	{
	public:
		FORCEINLINE void Set(const FString& NewName) { Name = NewName; }
	};


	// -------- Build all outputs --------
	const TArray<TSharedPtr<IHoudiniOutputBuilder>>& OutputBuilders = FHoudiniEngine::Get().GetOutputBuilders();
	TArray<FHoudiniOutputDesc> OutputDescs;
	TMap<TPair<FString, TSharedPtr<IHoudiniOutputBuilder>>, FHoudiniOutputConverter> OutputConverters;
	for (int32 GeoIdx = 0; GeoIdx < GeoInfos.Num(); ++GeoIdx)
	{
		const FString& GeoName = GeoNames[GeoIdx];
		const HAPI_GeoInfo& GeoInfo = GeoInfos[GeoIdx];
		const TArray<HAPI_PartInfo>& PartInfos = GeoPartInfos[GeoIdx];
		for (const HAPI_PartInfo& PartInfo : PartInfos)
		{
			for (int32 BuilderIdx = OutputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
			{
				const TSharedPtr<IHoudiniOutputBuilder>& OutputBuilder = OutputBuilders[BuilderIdx];
				bool bIsValid = false;
				bool bShouldHoldByOutput = false;
				HOUDINI_FAIL_RETURN(OutputBuilder->HapiIsPartValid(GeoInfo.nodeId, PartInfo, bIsValid, bShouldHoldByOutput))
				if (bIsValid)
				{
					if (bShouldHoldByOutput)
					{
						const TSubclassOf<UHoudiniOutput> OutputClass = OutputBuilder->GetClass();
						if (!OutputClass)
						{
							UE_LOG(LogHoudiniEngine, Error, TEXT("Please override your IHoudiniOutputBuilder::GetClass() when bShouldHoldByOutput == true"));
							continue;  // Try other builders
						}
						const int32 FoundOutputDescIdx = OutputDescs.IndexOfByPredicate([GeoIdx, OutputClass](const FHoudiniOutputDesc& OutputDesc)
							{
								return ((OutputDesc.GeoIdx == GeoIdx) && (OutputClass == OutputDesc.Class));
							});
						if (OutputDescs.IsValidIndex(FoundOutputDescIdx))
							OutputDescs[FoundOutputDescIdx].PartInfos.Add(PartInfo);
						else
						{
							// Try find corresponding old output so that we could reuse
							const int32 FoundOldOutputIdx = Outputs.IndexOfByPredicate([GeoName, OutputClass](const UHoudiniOutput* Output)
								{
									return Output->GetOutputName() == GeoName && OutputClass == Output->GetClass();
								});

							if (Outputs.IsValidIndex(FoundOldOutputIdx))
							{
								OutputDescs.Add(FHoudiniOutputDesc(GeoIdx, OutputClass, Outputs[FoundOldOutputIdx], PartInfo));
								Outputs.RemoveAt(FoundOldOutputIdx);
							}
							else
								OutputDescs.Add(FHoudiniOutputDesc(GeoIdx, OutputClass, nullptr, PartInfo));
						}
					}
					else
					{
						const TPair<FString, TSharedPtr<IHoudiniOutputBuilder>> ConverterIdentifier(GeoName, OutputBuilder);
						if (FHoudiniOutputConverter* ConverterPtr = OutputConverters.Find(ConverterIdentifier))
							ConverterPtr->PartInfos.Add(PartInfo);
						else
							OutputConverters.Add(ConverterIdentifier, FHoudiniOutputConverter(GeoIdx, PartInfo));
					}

					break;
				}
			}
		}
	}

	// Create new outputs
	for (FHoudiniOutputDesc& OutputDesc : OutputDescs)
	{
		if (OutputDesc.Output)
			continue;

		const int32 FoundOldOutputIdx = Outputs.IndexOfByPredicate([&](UHoudiniOutput* Output)  // Just find old output by type, we should reuse it anyway
			{
				if (Output->GetClass() == OutputDesc.Class)
				{
					((FHoudiniOutputNameModifier*)Output)->Set(GeoNames[OutputDesc.GeoIdx]);
					return true;
				}
				return false;
			});

		if (Outputs.IsValidIndex(FoundOldOutputIdx))
		{
			OutputDesc.Output = Outputs[FoundOldOutputIdx];
			Outputs.RemoveAt(FoundOldOutputIdx);
		}
		else  // Cannot reuse old outputs, we should just create a new output
		{
			OutputDesc.Output = NewObject<UHoudiniOutput>(this, OutputDesc.Class, MakeUniqueObjectName(this, OutputDesc.Class), RF_Public | RF_Transactional);
			((FHoudiniOutputNameModifier*)OutputDesc.Output)->Set(GeoNames[OutputDesc.GeoIdx]);
		}
	}

	// Destroy the old outputs
	for (UHoudiniOutput* OldOutput : Outputs)
		OldOutput->Destroy();
	Outputs.Empty();

	// Append new outputs
	for (const FHoudiniOutputDesc& OutputDesc : OutputDescs)
		Outputs.Add(OutputDesc.Output);

	// -------- Output assets first, as they may be used by uproperties, or material parameters --------
	for (int32 BuilderIdx = OutputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
	{
		const TSharedPtr<IHoudiniOutputBuilder>& OutputBuilder = OutputBuilders[BuilderIdx];
		for (const auto& OutputConverter : OutputConverters)
		{
			if (OutputConverter.Key.Value == OutputBuilder)
				HOUDINI_FAIL_RETURN(OutputBuilder->HapiRetrieve(this, GeoNames[OutputConverter.Value.GeoIdx], GeoInfos[OutputConverter.Value.GeoIdx], OutputConverter.Value.PartInfos));
		}
	}

	// -------- Then output mesh, as they may be packed mesh and instancer will need them --------
	for (int32 BuilderIdx = OutputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
	{
		const TSharedPtr<IHoudiniOutputBuilder>& OutputBuilder = OutputBuilders[BuilderIdx];
		for (const FHoudiniOutputDesc& OutputDesc : OutputDescs)
		{
			if (OutputDesc.Class == OutputBuilder->GetClass())
				HOUDINI_FAIL_RETURN(OutputDesc.Output->HapiUpdate(GeoInfos[OutputDesc.GeoIdx], OutputDesc.PartInfos));
		}
	}

	FHoudiniAttribute::ClearBlueprintProperties();

	FHoudiniEngine::Get().FinishHoudiniMainTaskMessage();

	UE_LOG(LogHoudiniEngine, Log, TEXT("%s: OUTPUT %.3f (s)"), *GetActorLabel(false), FPlatformTime::Seconds() - StartTime);

	return true;
}


template<typename THoudiniEditableGeometryComponent>
FORCEINLINE static bool HapiUploadEditableGeometryGroups(const TMap<FString, TArray<THoudiniEditableGeometryComponent*>>& GroupEditGeosMap, const int32& GeoNodeId, const int32& MergeNodeId,
	TArray<TPair<int32, size_t>>& InOutNewNodeIdHandles, TArray<TPair<int32, size_t>>& InOutOldNodeIdHandles)
{
	for (const auto& GroupEditGeos : GroupEditGeosMap)
	{
		int32 EditGeoNodeId = -1;
		size_t EditGeoHandle = 0;
		if (InOutOldNodeIdHandles.IsValidIndex(0))  // Try reuse the first of previous input nodes, to keep order
		{
			EditGeoNodeId = InOutOldNodeIdHandles[0].Key;
			EditGeoHandle = InOutOldNodeIdHandles[0].Value;
			InOutOldNodeIdHandles.RemoveAt(0);
		}

		if (EditGeoNodeId < 0)
		{
			HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GeoNodeId,
				FString::Printf(TEXT("editgeo_%08X"), FPlatformTime::Cycles()), EditGeoNodeId));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
				MergeNodeId, InOutNewNodeIdHandles.Num(), EditGeoNodeId, 0));
		}

		HOUDINI_FAIL_RETURN(THoudiniEditableGeometryComponent::HapiUpload(GroupEditGeos.Value, EditGeoNodeId, EditGeoHandle));

		InOutNewNodeIdHandles.Add(TPair<int32, size_t>(EditGeoNodeId, EditGeoHandle));
	}

	return true;
}

bool AHoudiniNode::HapiUploadEditableOutputs()
{
	if (bOutputEditable)
	{
		const double StartTime = FPlatformTime::Seconds();

		if (DeltaInfoNodeId < 0)
		{
			HOUDINI_FAIL_RETURN(FHoudiniSopAttribCreate::HapiCreateNode(GeoNodeId, FString(), DeltaInfoNodeId));
			HOUDINI_FAIL_RETURN(FHoudiniSopAttribCreate::HapiSetupDeltaInfoAttribute(DeltaInfoNodeId));
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
				NodeId, 0, DeltaInfoNodeId, 0));
		}

		if (MergeNodeId < 0)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),
				GeoNodeId, "merge", nullptr, false, &MergeNodeId));

			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
				DeltaInfoNodeId, 0, MergeNodeId, 0));
		}

		// We should separate mesh and curve to input, as curves has more intrinsic attributes that meshes does NOT have.
		TMap<FString, TArray<UHoudiniCurvesComponent*>> GroupCurvesMap;
		TMap<FString, TArray<UHoudiniMeshComponent*>> GroupMeshesMap;
		for (UHoudiniOutput* Output : Outputs)
		{
			if (UHoudiniOutputMesh* MeshOutput = Cast<UHoudiniOutputMesh>(Output))
				MeshOutput->CollectEditMeshes(GroupMeshesMap);
			else if (UHoudiniOutputCurve* CurveOutput = Cast<UHoudiniOutputCurve>(Output))
				CurveOutput->CollectEditCurves(GroupCurvesMap);
		}

		TArray<TPair<int32, size_t>> NewNodeIdHandles;
		HOUDINI_FAIL_RETURN(HapiUploadEditableGeometryGroups(GroupCurvesMap, GeoNodeId, MergeNodeId,
			NewNodeIdHandles, EditGeoNodeIdHandles));  // DeltaInfos in EditCurves has been empty
		HOUDINI_FAIL_RETURN(HapiUploadEditableGeometryGroups(GroupMeshesMap, GeoNodeId, MergeNodeId,
			NewNodeIdHandles, EditGeoNodeIdHandles));  // DeltaInfos in EditMeshes has been empty

		// Clear and destroy all legacy nodes
		for (const auto& NodeIdHandle : EditGeoNodeIdHandles)
		{
			if (NodeIdHandle.Value)
				FHoudiniEngineUtils::CloseSharedMemoryHandle(NodeIdHandle.Value);
			if (NodeIdHandle.Key >= 0)
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeIdHandle.Key));
		}

		// Update node ids
		EditGeoNodeIdHandles = NewNodeIdHandles;

		HOUDINI_FAIL_RETURN(FHoudiniSopAttribCreate::HapiSetDeltaInfo(DeltaInfoNodeId, DeltaInfo.IsEmpty() ? ReDeltaInfo : DeltaInfo));

		const double TimeCost = FPlatformTime::Seconds() - StartTime;
		if (TimeCost > 0.001)
			UE_LOG(LogHoudiniEngine, Log, TEXT("%s: Feedback Editable Outputs %.3f (s)"), *GetActorLabel(false), FPlatformTime::Seconds() - StartTime);
	}

	return true;
}


const bool& UHoudiniOutput::CanHaveEditableOutput() const
{
	class FHouNodeCanHaveEditableOutput : public AHoudiniNode
	{
	public:
		FORCEINLINE const bool& Get() const { return bOutputEditable; }
	};

	return ((FHouNodeCanHaveEditableOutput*)GetOuter())->Get();
}

bool UHoudiniOutput::HapiGetEditableGroups(TArray<FString>& OutEditableGroupNames, TArray<TArray<int32>>& OutEditableGroupships, bool& bOutIsMainGeoEditable,
	const int32& NodeId, const HAPI_PartInfo& PartInfo, const TArray<std::string>& PrimGroupNames) const
{
	bOutIsMainGeoEditable = false;
	for (const auto& GroupParmSet : GetNode()->GetGroupParameterSetMap())
	{
		const std::string EditableGroupName = TCHAR_TO_UTF8(*GroupParmSet.Key);
		if (EditableGroupName == HAPI_GROUP_MAIN_GEO)  // We need NOT get "main_geo" groupship, because the rest of all will be "main_geo", and will have GroupIdx = -1
			bOutIsMainGeoEditable = true;
		else if (PrimGroupNames.Contains(EditableGroupName))
		{
			TArray<int32> EditableGroupship;
			bool bGroupshipConst = false;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetGroupMembership(NodeId, PartInfo.id,
				PartInfo.faceCount, PartInfo.isInstanced, HAPI_GROUPTYPE_PRIM, EditableGroupName.c_str(),
				EditableGroupship, bGroupshipConst));

			if (EditableGroupship[0] || !bGroupshipConst)
			{
				OutEditableGroupships.Add(EditableGroupship);
				OutEditableGroupNames.Add(GroupParmSet.Key);
			}
		}
	}

	return true;
}


AActor* FHoudiniSplittableOutput::FindSplitActor(const AHoudiniNode* Node, const bool& bIsEditableGeometry) const
{
	class FHoudiniNodeSplitActorAccessor : public AHoudiniNode
	{
	public:
		AActor* GetSplitActor(const FString& InSplitValue, const bool& bIsEditableGeometry) const
		{
			const FHoudiniActorHolder* FoundActorHolder = bIsEditableGeometry ?
				SplitEditableActorMap.Find(InSplitValue) : SplitActorMap.Find(InSplitValue);

			return FoundActorHolder ? FoundActorHolder->Load() : nullptr;
		}
	};

	return ((const FHoudiniNodeSplitActorAccessor*)Node)->GetSplitActor(SplitValue, bIsEditableGeometry);
}

AActor* FHoudiniSplittableOutput::FindOrCreateSplitActor(AHoudiniNode* Node, const bool& bIsEditableGeometry) const
{
	class FHoudiniNodeSplitActorAccessor : public AHoudiniNode
	{
	public:
		AActor* GetOrSpawnSplitActor(const FString& InSplitValue, const bool& bIsEditableGeometry)
		{
			const FHoudiniActorHolder* FoundActorHolder = bIsEditableGeometry ?
				SplitEditableActorMap.Find(InSplitValue) : SplitActorMap.Find(InSplitValue);

			AActor* SplitActor = FoundActorHolder ? FoundActorHolder->Load() : nullptr;

			const FString& NodeLabel = GetActorLabel(false);

			if (bIsEditableGeometry)
			{
				if (SplitActor && SplitActor->IsA<AHoudiniNodeProxy>())
					return SplitActor;

				SplitActor = GetWorld()->SpawnActor<AHoudiniNodeProxy>();
				SplitActor->SetActorLabel(InSplitValue.IsEmpty() ?
					(NodeLabel.IsEmpty() ? FHoudiniEngineUtils::GetValidatedString(SelectedOpName) : NodeLabel) : InSplitValue);

				SplitActor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
				SplitEditableActorMap.FindOrAdd(InSplitValue) = FHoudiniActorHolder(SplitActor);
			}
			else
			{
				if (SplitActor)
					return SplitActor;

				SplitActor = GetWorld()->SpawnActor<AActor>();
				USceneComponent* SplitRoot = NewObject<USceneComponent>(SplitActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
				SplitActor->SetRootComponent(SplitRoot);
				SplitRoot->RegisterComponent();
				SplitActor->SetActorLabel(InSplitValue.IsEmpty() ?
					(NodeLabel.IsEmpty() ? FHoudiniEngineUtils::GetValidatedString(SelectedOpName) : NodeLabel) : InSplitValue);

				// Copy node transform to split actor
				const FTransform NodeTransform = GetActorTransform();
				if (!NodeTransform.Equals(FTransform::Identity))
					SplitActor->SetActorTransform(NodeTransform);

				SplitActor->SetFolderPath(FName(TEXT(HOUDINI_NODE_OUTLINER_FOLDER) /
					(NodeLabel.IsEmpty() ? FHoudiniEngineUtils::GetValidatedString(SelectedOpName) : NodeLabel)));
				SplitActorMap.FindOrAdd(InSplitValue) = FHoudiniActorHolder(SplitActor);
			}

			return SplitActor;
		}
	};

	return ((FHoudiniNodeSplitActorAccessor*)Node)->GetOrSpawnSplitActor(SplitValue, bIsEditableGeometry);
}

AActor* AHoudiniNode::Bake() const
{
	AActor* BakeActor = nullptr;
	for (UActorComponent* Component : GetComponents())
	{
		if (IsValid(Component) && !Component->IsA<UHoudiniEditableGeometry>() && (Component->GetClass() != USceneComponent::StaticClass()))
		{
			if (!BakeActor)
			{
				BakeActor = GetWorld()->SpawnActor<AActor>();
				USceneComponent* Root = NewObject<USceneComponent>(BakeActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
				BakeActor->SetRootComponent(Root);
				Root->RegisterComponent();
				BakeActor->SetActorLabel(GetActorLabel(false) + TEXT("_Bake"));
				BakeActor->SetFolderPath(HOUDINI_ENGINE "/Bake");
				const FTransform& NodeTransform = GetActorTransform();
				if (!NodeTransform.Equals(FTransform::Identity))
					BakeActor->SetActorTransform(NodeTransform);
			}

			UActorComponent* NewComponent = DuplicateObject(Component, BakeActor);
			NewComponent->CreationMethod = EComponentCreationMethod::Instance;
			if (USceneComponent* NewSC = Cast<USceneComponent>(NewComponent))
			{
				NewSC->SetMobility(BakeActor->GetRootComponent()->Mobility);
				NewSC->AttachToComponent(BakeActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			}
			NewComponent->OnComponentCreated();
			NewComponent->RegisterComponent();
			BakeActor->AddInstanceComponent(NewComponent);
		}
	}
	return BakeActor;
}


UActorComponent* FHoudiniComponentOutput::FindComponent(const AHoudiniNode* Node, AActor*& OutFoundSplitActor, const bool& bIsEditableGeometry) const
{
	if (ComponentName.IsNone())
		return nullptr;

	OutFoundSplitActor = bSplitActor ? FindSplitActor(Node, bIsEditableGeometry) : nullptr;
	const AActor* ParentActor = bSplitActor ? OutFoundSplitActor : Node;
	if (!IsValid(ParentActor))  // Means the split actor has been deleted manually
		return nullptr;

	for (UActorComponent* Component : ParentActor->GetComponents())
	{
		if (Component->GetFName() == ComponentName)
			return Component;
	}

	return nullptr;
}

void FHoudiniComponentOutput::CreateOrUpdateComponent(AHoudiniNode* Node, USceneComponent*& InOutSC, const TSubclassOf<USceneComponent>& Class,
	const FString& InSplitValue, const bool& bInSplitActor, const bool& bIsEditableGeometry)
{
	AActor* ParentActor = nullptr;
	if (!InOutSC)
		InOutSC = Cast<USceneComponent>(FindComponent(Node, ParentActor, bIsEditableGeometry));

	if (InOutSC && (InOutSC->GetClass() != Class))  // Check UClass
		InOutSC = nullptr;

	// We could update split info here, as component has been find upon
	SplitValue = InSplitValue;
	bSplitActor = bInSplitActor;

	if (bInSplitActor)
	{
		if (!ParentActor)
			ParentActor = FindOrCreateSplitActor(Node, bIsEditableGeometry);
	}
	else
		ParentActor = Node;

	if (InOutSC && (InOutSC->GetOwner() != ParentActor))
	{
		UE_LOG(LogHoudiniEngine, Warning, TEXT("%s owner does NOT matched, please check your code"), *InOutSC->GetPathName());  // We should NOT rename it, as re-attach component may crack the actor package
		FHoudiniEngineUtils::DestroyComponent(InOutSC);
		InOutSC = nullptr;  // To mark as need create
	}
	
	if (!InOutSC)
	{
		InOutSC = FHoudiniEngineUtils::CreateComponent(ParentActor, Class);
		ComponentName = InOutSC->GetFName();
	}
}

void FHoudiniComponentOutput::DestroyComponent(const AHoudiniNode* Node, const TWeakObjectPtr<USceneComponent>& Component, const bool& bIsEditableGeometry) const
{
	AActor* FoundSplitActor = nullptr;
	USceneComponent* ExistSC = Component.IsValid() ? Component.Get() : Cast<USceneComponent>(FindComponent(Node, FoundSplitActor, bIsEditableGeometry));
	if (ExistSC)
		FHoudiniEngineUtils::DestroyComponent(ExistSC);
}

#undef LOCTEXT_NAMESPACE
