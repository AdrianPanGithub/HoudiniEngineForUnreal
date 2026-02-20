// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniNode.h"

#include "Tasks/Task.h"

#include "HoudiniAsset.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineCommon.h"
#include "HoudiniEngineSettings.h"
#include "HoudiniInputs.h"
#include "HoudiniOutputs.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniApi.h"
#include "HoudiniCurvesComponent.h"
#include "HoudiniMeshComponent.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

#define FOREACH_HOUDINI_NODE_OUTPUT(FUNC) for (const UHoudiniOutput* Output : Outputs)\
			{\
				FUNC\
			}\
			for (const FHoudiniTopNode& TopNode : TopNodes)\
			{\
				for (const UHoudiniOutput* Output : TopNode.Outputs)\
				{\
					FUNC\
				}\
			}\

AHoudiniNode::AHoudiniNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCanBeDamaged(false);
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, USceneComponent::GetDefaultSceneRootVariableName());
}

void AHoudiniNode::BroadcastEvent(const EHoudiniNodeEvent NodeEvent)
{
	if (FHoudiniEngine::Get().HoudiniNodeEvents.IsBound())
		FHoudiniEngine::Get().HoudiniNodeEvents.Broadcast(this, NodeEvent);
	if (HoudiniNodeEvents.IsBound())
		HoudiniNodeEvents.Broadcast(NodeEvent);
}

void AHoudiniNode::Initialize(UHoudiniAsset* InAsset)
{
	Asset = InAsset;
	FHoudiniEngine::Get().RegisterNode(this);

	RequestRebuild();
}

AHoudiniNode* AHoudiniNode::Create(UWorld* World, UHoudiniAsset* Asset, const bool bTemporary)
{
	if (!IsValid(Asset))
		return nullptr;

	FActorSpawnParameters SpawnParms;
	if (bTemporary)
	{
		SpawnParms.bTemporaryEditorActor = true;
		SpawnParms.ObjectFlags |= RF_Transient;
	}
	AHoudiniNode* NewNode = World->SpawnActor<AHoudiniNode>(SpawnParms);
	if (!NewNode)
	{
		UE_LOG(LogHoudiniEngine, Error, TEXT("Failed to spawn HoudiniNode!"));
		return nullptr;
	}
	NewNode->bIsSpatiallyLoaded = false;  // AHoudiniNode always used as a tool for the whole map
	NewNode->SetFolderPath(HoudiniNodeDefaultFolderPath);

	TSet<FString> AllNodeLabels;
	for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())
	{
		if (Node.IsValid())
			AllNodeLabels.Add(Node->GetActorLabel(false));
	}

	const FString AssetName = Asset->GetName();
	for (int32 Idx = 0; Idx <= FHoudiniEngine::Get().GetCurrentNodes().Num(); ++Idx)  // Find a unique idx, and append to asset name as node label
	{
		const FString NodeLabel = AssetName + ((Idx == 0) ? FString() : FString::FromInt(Idx + 1));
		if (!AllNodeLabels.Contains(NodeLabel))
		{
			NewNode->SetActorLabel(NodeLabel, false);
			break;
		}
	}

	NewNode->Initialize(Asset);
	return NewNode;
}

AHoudiniNode* AHoudiniNode::InstantiateHoudiniAsset(const UObject* WorldContextObject, UHoudiniAsset* Asset, const bool bTemporary)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		return Create(World, Asset, bTemporary);

	return nullptr;
}

void AHoudiniNode::InvalidateEditableGeometryFeedback()
{
	MergeNodeId = -1;
	DeltaInfoNodeId = -1;
	for (const auto& EditGeoNodeIdHandle : EditGeoNodeIdHandles)
	{
		const size_t& EditGeoHandle = EditGeoNodeIdHandle.Value;
		if (EditGeoHandle)
			FHoudiniEngineUtils::CloseSharedMemoryHandle(EditGeoHandle);
	}
	EditGeoNodeIdHandles.Empty();
}

void AHoudiniNode::Invalidate()
{
	bRebuildBeforeCook = true;
	RequestCookMethod = EHoudiniNodeRequestCookMethod::None;
	
	if (NodeId < 0)  // This node haven't been instantiated yet
		return;

	NodeId = -1;
	GeoNodeId = -1;
	InvalidateEditableGeometryFeedback();

	for (UHoudiniInput* Input : Inputs)
		Input->Invalidate();

	for (FHoudiniTopNode& TopNode : TopNodes)
		TopNode.Invalidate();
}

bool AHoudiniNode::NeedInstantiate()
{
	if (NodeId < 0)
		return true;

	if (bRebuildBeforeCook)
		return true;

	if (Asset->NeedLoad())
		return true;

	if (!Asset->IsNodeInstantiated(this))
		return true;

	return false;
}

bool AHoudiniNode::HapiCookUpstream(bool& bOutHasUpstreamCooking) const
{
	bOutHasUpstreamCooking = false;
	for (const UHoudiniInput* Input : Inputs)
	{
		if (Input->GetType() != EHoudiniInputType::Node)
			continue;

		for (const UHoudiniInputHolder* Holder : Input->Holders)
		{
			if (const UHoudiniInputNode* InputNode = Cast<UHoudiniInputNode>(Holder))
			{
				AHoudiniNode* UpstreamNode = InputNode->GetNode();
				if (!IsValid(UpstreamNode))
					continue;

				if (UpstreamNode->NeedCook() || UpstreamNode->NodeId < 0)  // Upstream haven't been instantiated yet
				{
					bOutHasUpstreamCooking = true;
					return UpstreamNode->HapiAsyncProcess(false);
				}
			}
		}
	}

	return true;
}

bool AHoudiniNode::HapiAsyncProcess(const bool& bAfterInstantiating)
{
	if (!IsValid(Asset))  // Check if HDA is valid or deleted manually
		return true;

	Modify();  // Ensure that this actor's package is dirty and can be saved

	if (!bAfterInstantiating)
	{
		// Check whether we need to wait upstream nodes to cook first
		bool bHasUpstreamCooking = false;
		HOUDINI_FAIL_RETURN(HapiCookUpstream(bHasUpstreamCooking));
		if (bHasUpstreamCooking)
			return true;

		BroadcastEvent(EHoudiniNodeEvent::StartCook);

		if (NeedInstantiate())  // Instantiate
		{
			FHoudiniEngine::Get().StartHoudiniTask();
			FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FText::FromString(GetActorLabel(false) + ": Start Instantiate"));
			UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]
				{
					HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiInstantiate());  // INSTANTIATE !!!

					AsyncTask(ENamedThreads::GameThread, [this]
						{
							FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();
							FHoudiniEngine::Get().FinishHoudiniTask();

							if (this->GetNodeId() < 0)  // Failed to instantiate
								return;

							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdatePDG());  // Update top nodes here, Assume that top node will NOT changed by cook

							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateParameters(true));  // Only update parms, defaults and tags, do NOT update parm values
							bool bHasNewInputsPendingUpload = false;  // Useless here
							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateInputs(true, bHasNewInputsPendingUpload));  // Update operator path inputs by parm name
							this->BroadcastEvent(EHoudiniNodeEvent::FinishInstantiate);
							
							if (bHasNewInputsPendingUpload)  // Maybe a new node input parsed, we should cook input node(upstream) first
							{
								bool bHasUpstreamCooking = false;
								HOUDINI_FAIL_INVALIDATE_RETURN(HapiCookUpstream(bHasUpstreamCooking));
								if (bHasUpstreamCooking)
									return;
							}

							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiAsyncProcess(true));  // Continue to cook
						});
				});

			return true;  // HapiAsyncCook will continue in AsyncTask(ENamedThreads::GameThread upon
		}
		else if (Preset.IsValid())  // If just instantiated, then we should NOT update parameters and inputs again
		{
			HOUDINI_FAIL_RETURN(this->HapiUpdateParameters(false));
			bool bHasNewInputsPendingUpload = false;
			HOUDINI_FAIL_RETURN(this->HapiUpdateInputs(false, bHasNewInputsPendingUpload));
		}
	}

	// Actual cook process
	HOUDINI_FAIL_RETURN(this->HapiUploadInputsAndParameters());
	HOUDINI_FAIL_RETURN(this->HapiUploadEditableOutputs());
	if (bCookOnParameterChanged || (RequestCookMethod == EHoudiniNodeRequestCookMethod::Force))
	{
		FHoudiniEngine::Get().StartHoudiniTask();
		FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FText::FromString(GetActorLabel(false) + TEXT(": Start Cook")));
		FHoudiniEngine::Get().LimitEngineFrameRate();
		UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]
			{
				TArray<FString> GeoNames;
				TArray<HAPI_GeoInfo> GeoInfos;
				TArray<TArray<HAPI_PartInfo>> GeoPartInfos;
				HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiCook(GeoNames, GeoInfos, GeoPartInfos));  // COOK !!!

				AsyncTask(ENamedThreads::GameThread, [this, GeoNames, GeoInfos, GeoPartInfos]
					{
						FHoudiniEngine::Get().RecoverEngineFrameRate();
						FHoudiniEngine::Get().FinishHoudiniTask();

						HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateParameters(false));  // Only update parms and values, do NOT update defaults and tags
						bool bHasNewInputsPendingUpload = false;
						HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateInputs(false, bHasNewInputsPendingUpload));  // Update operator path inputs by parm value then name

						if (bHasNewInputsPendingUpload)  // We should execute cook process once again
						{
							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUploadInputsAndParameters());
							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUploadEditableOutputs());  // TODO: we really need this?

							FHoudiniEngine::Get().StartHoudiniTask();
							FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FText::FromString(GetActorLabel(false) + ": Start Cook"));

							UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]
								{
									TArray<FString> GeoNames;
									TArray<HAPI_GeoInfo> GeoInfos;
									TArray<TArray<HAPI_PartInfo>> GeoPartInfos;
									HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiCook(GeoNames, GeoInfos, GeoPartInfos));  // COOK !!!

									AsyncTask(ENamedThreads::GameThread, [this, GeoNames, GeoInfos, GeoPartInfos]
										{
											FHoudiniEngine::Get().FinishHoudiniTask();

											HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateParameters(false));  // Only update parms and values, do NOT update defaults and tags
											bool bHasNewInputsPendingUpload = false;  // We should not parse it again
											HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateInputs(false, bHasNewInputsPendingUpload));  // Update operator path inputs by parm value then name

											HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateOutputs(GeoNames, GeoInfos, GeoPartInfos));
											
											if (this->HasTopNodePendingCook())
												this->AsyncCookPDG();
											else
												FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();

											this->FinishCook();
										});
								});
						}
						else
						{
							HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateOutputs(GeoNames, GeoInfos, GeoPartInfos));
							
							if (this->HasTopNodePendingCook())
								this->AsyncCookPDG();
							else
								FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();

							this->FinishCook();
						}
					});
			});
	}
	else
	{
		HOUDINI_FAIL_RETURN(this->HapiUpdateParameters(false));  // Only update parms and values, do NOT update defaults and tags
		bool bHasNewInputsPendingUpload = false;
		HOUDINI_FAIL_RETURN(this->HapiUpdateInputs(false, bHasNewInputsPendingUpload));  // Update operator path inputs by parm value then name

		if (bHasNewInputsPendingUpload)  // We should execute actual cook process once again
		{
			HOUDINI_FAIL_RETURN(this->HapiUploadInputsAndParameters());
			HOUDINI_FAIL_RETURN(this->HapiUploadEditableOutputs());
			HOUDINI_FAIL_RETURN(this->HapiUpdateParameters(false));  // Only update parms and values, do NOT update defaults and tags
			HOUDINI_FAIL_RETURN(this->HapiUpdateInputs(false, bHasNewInputsPendingUpload));  // Update operator path inputs by parm value then name
		}

		this->FinishCook();
	}

#if 0  // Sync Cook Flow
	if (NeedInstantiate())
	{
		HapiInstantiate();  // INSTANTIATE !!!
		HapiUpdateParameters(true);  // Only update parms, defaults and tags, do NOT update parm values
		HapiUpdateInputs(true);  // Update operator path inputs by parm name
	}
	else if (Preset.IsValid())
	{
		HapiUpdateParameters(false);  // Only update parms, defaults and tags, do NOT update parm values
		HapiUpdateInputs(false);  // Update operator path inputs by parm name
	}

	for (int32 CookNum = 0; CookNum < 2; ++CookNum)
	{
		HapiUploadInputsAndParameters();
		if (bCookOnParameterChange)
		{
			HapiUploadEditableOutputs();
			HapiCookNodes();  // COOK !!!
		}
		HapiUpdateParameters(false);  // Only update parms and values, do NOT update defaults and tags
		HapiUpdateInputs(false);  // Update operator path inputs by parm value then name
		if (!HasInputsPendingUpload())  // We should execute actual cook process once again
			break;
	}

	if (bCookOnParameterChange)
		HapiUpdateOutputs();

	FinishCook();
#endif

	return true;
}

void AHoudiniNode::CleanupSplitActors()
{
	if (!SplitActorMap.IsEmpty() || !SplitEditableActorMap.IsEmpty())
	{
		TSet<FString> SplitValues;
		TSet<FString> EditableSplitValues;
		FOREACH_HOUDINI_NODE_OUTPUT(Output->CollectActorSplitValues(SplitValues, EditableSplitValues););

		for (auto SplitActorIter = SplitActorMap.CreateIterator(); SplitActorIter; ++SplitActorIter)
		{
			const FString& CurrSplitValue = SplitActorIter->Key;
			if (!SplitValues.Contains(CurrSplitValue))
			{
				SplitActorIter->Value.Destroy();
				SplitActorIter.RemoveCurrent();
			}
		}

		for (auto SplitActorIter = SplitEditableActorMap.CreateIterator(); SplitActorIter; ++SplitActorIter)
		{
			const FString& CurrSplitValue = SplitActorIter->Key;
			if (!EditableSplitValues.Contains(CurrSplitValue))
			{
				SplitActorIter->Value.Destroy();
				SplitActorIter.RemoveCurrent();
			}
		}
	}
}

void AHoudiniNode::FinishCook()
{
	bRebuildBeforeCook = false;
	RequestCookMethod = EHoudiniNodeRequestCookMethod::None;

	// -------- Cleanup the useless SplitActors --------
	CleanupSplitActors();

	BroadcastEvent(EHoudiniNodeEvent::FinishCook);

	NotifyDownstreamCookFinish();

	DeltaInfo.Empty();
}

bool AHoudiniNode::HapiDestroy()
{
	if (!FHoudiniEngine::Get().IsNodeRegistered(this))
		return true;

	FHoudiniEngine::Get().UnregisterNode(this);

	BroadcastEvent(EHoudiniNodeEvent::Destroy);

	if (NodeId < 0)  // We need NOT to destroy nodes before instantiation
		return true;
	
	Asset->UnregisterInstantiatedNode(this);

	// Destroy all nodes in houdini session
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), GeoNodeId >= 0 ? GeoNodeId : NodeId));

	for (UHoudiniInput* Input : Inputs)
	{
		if (Input->GetGeoNodeId() >= 0)
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), Input->GetGeoNodeId()));
	}
	// We need NOT to remove this node from upstream node's downstream set, as these will removed after next trigger downstream cook

	// Invalidate all SHM Handles and NodeIds
	Invalidate();

	return true;
}

void AHoudiniNode::PostLoad()
{
	Super::PostLoad();

	FHoudiniEngine::Get().RegisterNode(this);
}

void AHoudiniNode::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	FHoudiniEngine::Get().RegisterNode(this);
}

#if WITH_EDITOR
void AHoudiniNode::PostEditUndo()
{
	Super::PostEditUndo();

	if (!FHoudiniEngine::Get().IsNodeRegistered(this))  // This node may deleted then undo to recover it, then we need register it again
		FHoudiniEngine::Get().RegisterNode(this);
}
#endif

void AHoudiniNode::Destroyed()
{
	if (GIsRunning)
	{
		HOUDINI_FAIL_INVALIDATE(HapiDestroy());

		// Notify mask inputs has been destroyed
		for (UHoudiniInput* Input : Inputs)
		{
			if (Input->Holders.IsValidIndex(0))
			{
				if (Input->GetType() == EHoudiniInputType::Mask)
				{
					if (UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Input->Holders[0]))
						MaskInput->Destroy();
				}
				else if (Input->GetType() == EHoudiniInputType::Curves)
				{
					if (UHoudiniInputCurves* CurvesInput = Cast<UHoudiniInputCurves>(Input->Holders[0]))
						CurvesInput->Modify();  // As CurvesInput refs a CurvesComponent, we must modify it so that the ref will retain after undo
				}
			}
		}

		// Destroy output actors
		for (const auto& SplitEditableActor : SplitEditableActorMap)
			SplitEditableActor.Value.Destroy();

		// Check whether has standalone output actors
		bool bHasStandaloneOutputActors = !SplitActorMap.IsEmpty();
		if (!bHasStandaloneOutputActors)
		{
			FOREACH_HOUDINI_NODE_OUTPUT(if (!bHasStandaloneOutputActors && Output->HasStandaloneActors())
				{
					bHasStandaloneOutputActors = true;
					break;
				});
		}

		if (bHasStandaloneOutputActors)
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("HoudiniNodeDestroyConfirm", "Also Destroy All Output Actors With Houdini Node?")) == EAppReturnType::Yes)
			{
				for (const auto& SplitActor : SplitActorMap)
					SplitActor.Value.Destroy();

				FOREACH_HOUDINI_NODE_OUTPUT(if (Output->HasStandaloneActors()) Output->DestroyStandaloneActors(); );
			}
		}
	}

	Super::Destroyed();
}

bool AHoudiniNode::CanDeleteSelectedActor(FText& OutReason) const
{
	if (!FHoudiniEngine::Get().AllowEdit())
	{
		OutReason = LOCTEXT("NodeActorDestroy", "Houdini Session Is Working");
		return false;
	}

	return true;
}

void FHoudiniEngine::RegisterNodeMovedDelegate()
{
	if (!OnNodeMovedHandle.IsValid())
	{
		OnNodeMovedHandle = GEngine->OnActorMoved().AddLambda([](AActor* Actor)
			{
				if (const AHoudiniNode* Node = Cast<AHoudiniNode>(Actor))
					HOUDINI_FAIL_INVALIDATE_RETURN(Node->HapiOnTransformed());
			});
	}
}

bool AHoudiniNode::HapiOnTransformed() const
{
	const FTransform NodeTransform = GetActorTransform();
	if (NodeTransform.Equals(LastTransform))
	{
		LastTransform = NodeTransform;
		return true;
	}

	const FMatrix DeltaXform = LastTransform.ToInverseMatrixWithScale() * NodeTransform.ToMatrixWithScale();

	// Apply transform to all split actors
	for (const auto& SplitActor : SplitActorMap)
	{
		if (AActor* Actor = SplitActor.Value.Load())
		{
			Actor->SetActorTransform(FTransform(Actor->GetActorTransform().ToMatrixWithScale() * DeltaXform));
			Actor->Modify();
		}
	}

	// Apply transform to all output actors
	for (const UHoudiniOutput* Output : Outputs)
		Output->OnNodeTransformed(DeltaXform);
	for (const FHoudiniTopNode& TopNode : TopNodes)
	{
		for (const UHoudiniOutput* Output : TopNode.Outputs)
			Output->OnNodeTransformed(DeltaXform);
	}

	LastTransform = NodeTransform;

	// Apply transform to node in session if this node has been instantiated
	const int32& ObjectNodeId = GeoNodeId >= 0 ? GeoNodeId : NodeId;
	if (ObjectNodeId >= 0)
	{
		const HAPI_TransformEuler HapiTransform = FHoudiniEngineUtils::ConvertTransform(NodeTransform);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetObjectTransform(FHoudiniEngine::Get().GetSession(),
			ObjectNodeId, &HapiTransform));
	}

	return true;
}

void FHoudiniEngine::UnregisterNodeMovedDelegate()
{
	if (IsValid(GEngine) && OnNodeMovedHandle.IsValid())
		GEngine->OnActorMoved().Remove(OnNodeMovedHandle);

	OnNodeMovedHandle.Reset();
}

bool AHoudiniNode::NeedWaitUpstreamCookFinish() const
{
	for (UHoudiniInput* Input : Inputs)
	{
		for (UHoudiniInputHolder* Holder : Input->Holders)
		{
			UHoudiniInputNode* InputNode = Cast<UHoudiniInputNode>(Holder);
			if (!IsValid(InputNode))
				continue;

			AHoudiniNode* UpstreamNode = InputNode->GetNode();
			if (!IsValid(UpstreamNode))
				continue;

			if (UpstreamNode->NeedCook())
				return true;

			if (UpstreamNode->NodeId < 0)  // Upstream haven't been instantiated yet
			{
				UpstreamNode->RequestCook();
				return true;
			}
		}
	}

	return false;
}

void AHoudiniNode::NotifyDownstreamCookFinish()
{
	for (const TWeakObjectPtr<AHoudiniNode>& DownstreamNode : FHoudiniEngine::Get().GetCurrentNodes())
	{
		if (DownstreamNode.IsValid())
		{
			for (UHoudiniInput* Input : DownstreamNode->Inputs)
			{
				if ((Input->GetType() != EHoudiniInputType::Node) || !Input->Holders.IsValidIndex(0))
					continue;

				if (GetFName() == Cast<UHoudiniInputNode>(Input->Holders[0])->GetNodeActorName())
				{
					if (DownstreamNode->bCookOnUpstreamChanged && Input->GetSettings().bCheckChanged)
					{
						Input->Holders[0]->MarkChanged(true);
						if (DeltaInfo.IsEmpty())
							DownstreamNode->RequestCook();
						else  // If DeltaInfo has value, means this node had changed
							DownstreamNode->TriggerCookByInput(Input);  // So we should record downstream node's delta info by this input
						
						break;  // Found, so stop iterate inputs
					}
				}
			}
		}
	}
}

bool AHoudiniNode::HapiInstantiate()
{
	const double StartTime = FPlatformTime::Seconds();

	HOUDINI_FAIL_RETURN(Asset->HapiLoad(AvailableOpNames));
	
	if (AvailableOpNames.IsEmpty())
		return true;

	if (!AvailableOpNames.Contains(SelectedOpName))
		SelectedOpName = AvailableOpNames[0];


	// We should delete the previous node, and clear editable output input shm
	if (NodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), GeoNodeId >= 0 ? GeoNodeId : NodeId));

		for (UHoudiniInput* Input : Inputs)
		{
			if (Input->GetGeoNodeId() >= 0)
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), Input->GetGeoNodeId()));
		}

		Invalidate();
	}

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(), -1, TCHAR_TO_UTF8(*SelectedOpName),
		TCHAR_TO_UTF8(*(FHoudiniEngineUtils::GetValidatedString(SelectedOpName) + TEXT("_") + GetIdentifier())), false, &NodeId));
	
	if (GetDefault<UHoudiniEngineSettings>()->bVerbose)
	{
		for (int32 Iter = 0;; ++Iter)
		{
			FPlatformProcess::SleepNoStats(0.1f);

			int Status = HAPI_STATE_STARTING_COOK;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status));

			if (Status == HAPI_STATE_READY)
				break;
			else if (Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
			{
				FString StatusString;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStatusString(
					HAPI_STATUS_COOK_RESULT, HAPI_STATUSVERBOSITY_ERRORS, StatusString));
				UE_LOG(LogHoudiniEngine, Error, TEXT("%s Instantiate Error:\n%s"), *GetActorLabel(false), *StatusString);
				break;
			}

			if ((Iter % 5) == 1)
			{
				FString StatusString;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStatusString(
					HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS, StatusString));
				StatusString += GetActorLabel(false) + TEXT(": ") + StatusString;
				AsyncTask(ENamedThreads::GameThread, [StatusString]
					{
						FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FText::FromString(StatusString));
					});
			}
		}
	}

	AsyncTask(ENamedThreads::GameThread, [this]
		{
			FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(
				FText::FromString(this->GetActorLabel(false) + TEXT(": Instantiate Finished")));
		});


	Asset->RegisterInstantiatedNode(this);

	// -------- Get asset infos --------
	HAPI_AssetInfo AssetInfo;
	HAPI_Result Result = FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), NodeId, &AssetInfo);
	if (HAPI_SESSION_INVALID_RESULT(Result))
		return false;

	if (Result != HAPI_RESULT_SUCCESS)  // Means this node has not been created succussfully, just invalidate and skip cook
	{
		// If is a sop node, maybe the parent geo node has been created, then we should also delete it.
		int32 ParentNodeId = -1;
		Result = FHoudiniApi::GetNodeFromPath(FHoudiniEngine::Get().GetSession(), -1,
			TCHAR_TO_UTF8(*(TEXT("/obj/") + FHoudiniEngineUtils::GetValidatedString(SelectedOpName) + TEXT("_") + GetIdentifier())), &ParentNodeId);
		if ((Result == HAPI_RESULT_SUCCESS) && (ParentNodeId >= 0))
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), ParentNodeId));

		Invalidate();

		UE_LOG(LogHoudiniEngine, Error, TEXT("Failed to instantiate %s"), *SelectedOpName);

		return true;
	}

	if (FHoudiniEngine::Get().IsSessionSync() && AssetInfo.objectNodeId >= 0)  // We need turn off the object node, to avoid auto cook when session sync
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(), AssetInfo.objectNodeId, 0));

	TArray<FString> AssetInfoStrings;
	HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(
		TArray<HAPI_StringHandle>{ AssetInfo.labelSH, AssetInfo.helpTextSH, AssetInfo.helpURLSH }, AssetInfoStrings));
	Label = AssetInfoStrings[0];
	HelpText = AssetInfoStrings[1];
	HelpURL = AssetInfoStrings[2];
	
	// Set node transform
	if (AssetInfo.objectNodeId >= 0)
	{
		const FTransform NodeTransform = GetActorTransform();
		if (!NodeTransform.Equals(FTransform::Identity))
		{
			const HAPI_TransformEuler HapiTransform = FHoudiniEngineUtils::ConvertTransform(NodeTransform);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetObjectTransform(FHoudiniEngine::Get().GetSession(),
				AssetInfo.objectNodeId, &HapiTransform));
		}
	}

	UE_LOG(LogHoudiniEngine, Log, TEXT("%s: INSTANTIATE %.3f (s)"), *GetActorLabel(false), FPlatformTime::Seconds() - StartTime);

	return true;
}

bool AHoudiniNode::HapiCook(TArray<FString>& OutGeoNames, TArray<HAPI_GeoInfo>& OutGeoInfos, TArray<TArray<HAPI_PartInfo>>& OutGeoPartInfos)
{
	const double StartTime = FPlatformTime::Seconds();

	// -------- Gather all output nodes --------
	TArray<int32> OutputNodeIds;
	if (GeoNodeId >= 0)  // Sop
	{
		int32 NumOutputs;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetOutputGeoCount(FHoudiniEngine::Get().GetSession(), NodeId, &NumOutputs));

		if (NumOutputs <= 0)  // We should just cook this node
			OutputNodeIds.Add(NodeId);
		else
		{
			TArray<HAPI_GeoInfo> GeoInfos;
			GeoInfos.SetNumUninitialized(NumOutputs);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetOutputGeoInfos(FHoudiniEngine::Get().GetSession(),
				NodeId, GeoInfos.GetData(), NumOutputs));

			for (const HAPI_GeoInfo& GeoInfo : GeoInfos)
				OutputNodeIds.Add(GeoInfo.nodeId);
		}
	}
	else  // Object
	{
		int32 NumOutputs;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ComposeChildNodeList(FHoudiniEngine::Get().GetSession(),
			NodeId, HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_DISPLAY, true, &NumOutputs));

		if (NumOutputs >= 1)
		{
			OutputNodeIds.SetNumUninitialized(NumOutputs);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetComposedChildNodeList(FHoudiniEngine::Get().GetSession(),
				NodeId, OutputNodeIds.GetData(), NumOutputs));
		}
	}
	

	// -------- COOK --------
	for (const int32& OutputNodeId : OutputNodeIds)
	{
		if (FHoudiniEngine::Get().IsNullSession())  // Session has been stopped
			return false;

		try
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), OutputNodeId, nullptr));
		}
		catch (...)
		{
			UE_LOG(LogHoudiniEngine, Warning, TEXT("%s: Cook Interupted"), *GetActorLabel(false));
			return false;
		}
		
		if (GetDefault<UHoudiniEngineSettings>()->bVerbose)
		{
			FPlatformProcess::SleepNoStats(0.04f);

			for (int32 Iter = 0;; ++Iter)
			{
				if (FHoudiniEngine::Get().IsNullSession())  // Session has been stopped
					return false;

				int Status = HAPI_STATE_STARTING_COOK;
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetStatus(FHoudiniEngine::Get().GetSession(), HAPI_STATUS_COOK_STATE, &Status));

				if (Status == HAPI_STATE_READY)
					break;
				else if (Status == HAPI_STATE_READY_WITH_FATAL_ERRORS || Status == HAPI_STATE_READY_WITH_COOK_ERRORS)
				{
					FString StatusString;
					HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStatusString(
						HAPI_STATUS_COOK_RESULT, HAPI_STATUSVERBOSITY_ERRORS, StatusString));
					UE_LOG(LogHoudiniEngine, Error, TEXT("%s Cook Error:\n%s"), *GetActorLabel(false), *StatusString);
					break;
				}

				if ((Iter % 5) == 1)
				{
					FString StatusString;
					HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStatusString(
						HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS, StatusString));
					StatusString = GetActorLabel(false) + TEXT(": ") + StatusString;
					AsyncTask(ENamedThreads::GameThread, [StatusString]
						{
							FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(FText::FromString(StatusString));
						});
				}

				FPlatformProcess::SleepNoStats(0.1f);
			}
		}
	}

	if (FHoudiniEngine::Get().IsNullSession())  // Session has been stopped
		return false;

	// -------- Gather all output GeoInfos and Retrieve PartInfos --------
	if (GeoNodeId >= 0)  // Sop
	{
		int32 NumOutputs;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetOutputGeoCount(FHoudiniEngine::Get().GetSession(), NodeId, &NumOutputs));

		if (NumOutputs <= 0)  
		{
			OutGeoInfos.AddUninitialized();
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), NodeId, &OutGeoInfos.Last()));
			OutGeoNames.Add(FString());
		}
		else
		{
			OutGeoInfos.SetNumUninitialized(NumOutputs);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetOutputGeoInfos(FHoudiniEngine::Get().GetSession(),
				NodeId, OutGeoInfos.GetData(), NumOutputs));

			TArray<HAPI_StringHandle> GeoNameSHs;
			GeoNameSHs.SetNumUninitialized(OutGeoInfos.Num());
			for (int32 GeoIdx = 0; GeoIdx < OutGeoInfos.Num(); ++GeoIdx)
				GeoNameSHs[GeoIdx] = OutGeoInfos[GeoIdx].nameSH;

			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(GeoNameSHs, OutGeoNames));
		}
	}
	else  // Object
	{
		int32 NumOutputs;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ComposeChildNodeList(FHoudiniEngine::Get().GetSession(),
			NodeId, HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_DISPLAY, true, &NumOutputs));

		if (NumOutputs >= 1)
		{
			TArray<HAPI_NodeId> ChildNodeIds;
			ChildNodeIds.SetNumUninitialized(NumOutputs);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetComposedChildNodeList(FHoudiniEngine::Get().GetSession(),
				NodeId, ChildNodeIds.GetData(), NumOutputs));

			for (const HAPI_NodeId& ChildNodeId : ChildNodeIds)
			{
				OutGeoInfos.AddUninitialized();
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(),
					ChildNodeId, &OutGeoInfos.Last()));
				HAPI_StringHandle ChildNodePathSH;
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetNodePath(FHoudiniEngine::Get().GetSession(),
					ChildNodeId, NodeId, &ChildNodePathSH));
				FString GeoName;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(ChildNodePathSH, GeoName));
				OutGeoNames.Add(FHoudiniEngineUtils::GetValidatedString(GeoName));  // Remove '/'
			}
		}
	}

	AsyncTask(ENamedThreads::GameThread, [this]
		{
			FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(
				FText::FromString(this->GetActorLabel(false) + TEXT(": Cook Finished")));
		});

	OutGeoPartInfos.SetNum(OutGeoInfos.Num());
	for (int32 GeoIdx = 0; GeoIdx < OutGeoInfos.Num(); ++GeoIdx)
	{
		const HAPI_GeoInfo& GeoInfo = OutGeoInfos[GeoIdx];
		TArray<HAPI_PartInfo>& PartInfos = OutGeoPartInfos[GeoIdx];
		for (int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)
		{
			PartInfos.AddUninitialized();
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(),
				GeoInfo.nodeId, PartIdx, &PartInfos.Last()));
		}
	}
	
	UE_LOG(LogHoudiniEngine, Log, TEXT("%s: COOK %.3f (s)"), *GetActorLabel(false), FPlatformTime::Seconds() - StartTime);

	return true;
}


static bool IsParameterInAttributeSet(const TArray<UHoudiniParameter*>& AttribParms, const UHoudiniParameter* ChangedParm)
{
	for (UHoudiniParameter* AttribParm : AttribParms)
	{
		if (AttribParm == ChangedParm)
			return true;

		if (AttribParm->GetType() == EHoudiniParameterType::MultiParm)
		{
			if (Cast<UHoudiniMultiParameter>(AttribParm)->ChildAttribParmInsts.Contains(ChangedParm))
				return true;
		}
	}

	return false;
}

void AHoudiniNode::TriggerCookByParameter(const UHoudiniParameter* ChangedParm)
{
	// First, check is attrib-parm, then update parm-attrib.
	// Vars for attrib-parm
	FString AttribGroupName;
	EHoudiniAttributeOwner AttribOwner = EHoudiniAttributeOwner::Invalid;

	for (const auto& GroupName_AttribParmSet : GroupParmSetMap)
	{
		if (IsParameterInAttributeSet(GroupName_AttribParmSet.Value.PointAttribParms, ChangedParm))
		{
			if (UHoudiniParameter::GDisableAttributeActions)
				return;

			AttribGroupName = GroupName_AttribParmSet.Key;
			AttribOwner = EHoudiniAttributeOwner::Point;
			break;
		}
		else if (IsParameterInAttributeSet(GroupName_AttribParmSet.Value.PrimAttribParms, ChangedParm))
		{
			if (UHoudiniParameter::GDisableAttributeActions)
				return;

			AttribGroupName = GroupName_AttribParmSet.Key;
			AttribOwner = EHoudiniAttributeOwner::Prim;
			break;
		}
	}

	if (AttribOwner == EHoudiniAttributeOwner::Point || AttribOwner == EHoudiniAttributeOwner::Prim)  // is attrib-parm
	{
		bool bSuccess = false;
		for (const UHoudiniInput* Input : Inputs)  // Find EditGeo in Inputs
		{
			if (Input->GetType() != EHoudiniInputType::Curves || !Input->Holders.IsValidIndex(0))
				continue;

			UHoudiniCurvesComponent* CurvesComponent = Cast<UHoudiniInputCurves>(Input->Holders[0])->GetCurvesComponent();
			if (CurvesComponent->UpdateAttribute(ChangedParm, AttribOwner))
			{
				TriggerCookByInput(Input);
				bSuccess = true;
				break;
			}
		}

		for (UHoudiniOutput* Output : Outputs)
		{
			if (bSuccess)
				break;

			if (UHoudiniOutputCurve* CurveOutput = Cast<UHoudiniOutputCurve>(Output))
			{
				TMap<FString, TArray<UHoudiniCurvesComponent*>> GroupCurvesMap;
				CurveOutput->CollectEditCurves(GroupCurvesMap);
				for (const auto& GroupCurves : GroupCurvesMap)
				{
					for (UHoudiniCurvesComponent* HCC : GroupCurves.Value)
					{
						if (HCC->UpdateAttribute(ChangedParm, AttribOwner))
						{
							TriggerCookByEditableGeometry(HCC);
							bSuccess = true;
							break;
						}
					}

					if (bSuccess)
						break;
				}
			}
			else if (UHoudiniOutputMesh* MeshOutput = Cast<UHoudiniOutputMesh>(Output))
			{
				TMap<FString, TArray<UHoudiniMeshComponent*>> GroupMeshesMap;
				MeshOutput->CollectEditMeshes(GroupMeshesMap);
				for (const auto& GroupMeshes : GroupMeshesMap)
				{
					for (UHoudiniMeshComponent* HMC : GroupMeshes.Value)
					{
						if (HMC->UpdateAttribute(ChangedParm, AttribOwner))
						{
							TriggerCookByEditableGeometry(HMC);
							bSuccess = true;
							break;
						}
					}

					if (bSuccess)
						break;
				}
			}
		}

		// TODO: check if we really need to canel modification on attrib-parm
		const_cast<UHoudiniParameter*>(ChangedParm)->ResetModification();
	}
	else
	{
		const FString DeltaInfoPrefix = GetActorLabel() + DELTAINFO_PARAMETER + ChangedParm->GetParameterName() + TEXT("/");
		AppendDeltaInfos(DeltaInfoPrefix + ChangedParm->GetBackupValueString(), DeltaInfoPrefix + ChangedParm->GetValueString());
		RequestCook();
	}
}

void AHoudiniNode::TriggerCookByInput(const UHoudiniInput* ChangedInput)
{
	if (!GetDefault<UHoudiniEngineSettings>()->bCookOnInputChanged)
		return;

	const FString DeltaInfoPrefix = GetActorLabel() +
		(ChangedInput->IsParameter() ? DELTAINFO_PARAMETER : DELTAINFO_INPUT) + ChangedInput->GetInputName() + TEXT("/");
	AppendDeltaInfos(DeltaInfoPrefix + TEXT("1"), DeltaInfoPrefix + TEXT("0"));

	RequestCook();
}

void AHoudiniNode::AppendDeltaInfos(const FString& NewDeltaInfo, const FString& NewReDeltaInfo)
{
	if (DeltaInfo.IsEmpty())
	{
		DeltaInfo = NewDeltaInfo;
		ReDeltaInfo = NewReDeltaInfo;
	}
	else  // TODO: join the delta info, e.g. "he_parm_test/parameter input/aaa bbb|curve_input/0|1|1"
	{
		DeltaInfo = NewDeltaInfo;
		ReDeltaInfo = NewReDeltaInfo;
	}
}

void AHoudiniNode::TriggerCookByEditableGeometry(const UHoudiniEditableGeometry* ChangedEditGeo)
{
	if (ChangedEditGeo->IsA<UHoudiniCurvesComponent>())  // CurvesComponent maybe as both input/edit curve, so we should try find it in inputs
	{
		for (const UHoudiniInput* Input : Inputs)
		{
			if ((Input->GetType() == EHoudiniInputType::Curves) && Input->Holders.IsValidIndex(0))
			{
				if (Cast<UHoudiniInputCurves>(Input->Holders[0])->GetCurvesComponent() == ChangedEditGeo)  // ChangedEditGeo is input curves
				{
					if (Input->GetSettings().bCheckChanged && GetDefault<UHoudiniEngineSettings>()->bCookOnInputChanged)
						TriggerCookByInput(Input);
					else
					{
						Input->Holders[0]->MarkChanged(true);  // Let node know this input should reimport when next cook
						const_cast<UHoudiniEditableGeometry*>(ChangedEditGeo)->ResetDeltaInfo();  // Allow selecting and editing
					}

					return;
				}
			}
		}
	}

	// ChangedEditGeo is output EditGeo
	class FHoudiniEditGeoDeltaInfoAccessor : UHoudiniEditableGeometry
	{
	public:
		FORCEINLINE const FString& GetDeltaInfo() const { return DeltaInfo; }
		FORCEINLINE const FString& GetReDeltaInfo() const { return ReDeltaInfo; }
	};

	// We just override delta info if EditGeo changed
	DeltaInfo = ((const FHoudiniEditGeoDeltaInfoAccessor*)ChangedEditGeo)->GetDeltaInfo();
	ReDeltaInfo = ((const FHoudiniEditGeoDeltaInfoAccessor*)ChangedEditGeo)->GetReDeltaInfo();

	RequestCook();
}

void AHoudiniNode::SetCookOnParameterChanged(const bool bInCookOnParameterChanged)
{
	if (bCookOnParameterChanged != bInCookOnParameterChanged)
	{
		const bool bPrevDeferredCook = DeferredCook();
		bCookOnParameterChanged = bInCookOnParameterChanged;
		if (bPrevDeferredCook != DeferredCook())
			BroadcastEvent(EHoudiniNodeEvent::RefreshEditorOnly);
	}
}

void AHoudiniNode::SetCookOnUpstreamChanged(const bool bInCookOnUpstreamChanged)
{
	if (bCookOnUpstreamChanged != bInCookOnUpstreamChanged)
	{
		const bool bPrevDeferredCook = DeferredCook();
		bCookOnUpstreamChanged = bInCookOnUpstreamChanged;
		if (bPrevDeferredCook != DeferredCook())
			BroadcastEvent(EHoudiniNodeEvent::RefreshEditorOnly);
	}
}

FString AHoudiniNode::GetCookFolderPath() const
{
	return GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder + HOUDINI_ENGINE_COOK_FOLDER_PATH +
		FHoudiniEngineUtils::GetValidatedString(SelectedOpName) / GetIdentifier() + TEXT("/");
}

FString AHoudiniNode::GetBakeFolderPath() const
{
	return GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder + HOUDINI_ENGINE_BAKE_FOLDER_PATH +
		FHoudiniEngineUtils::GetValidatedString(SelectedOpName) / GetIdentifier() + TEXT("/");
}

FString AHoudiniNode::GetPresetPathFilter() const
{
	return HOUDINI_ENGINE_PRESET_FOLDER_PATH + FHoudiniEngineUtils::GetValidatedString(SelectedOpName);
}

bool AHoudiniNode::LinkSplitActor(const FString& SplitValue, AActor* SplitActor)
{
	bool bFound = false;
	if (IsValid(SplitActor))
	{
		if (FHoudiniActorHolder* FoundActorPtr = SplitActorMap.Find(SplitValue))
		{
			bFound = true;
			*FoundActorPtr = SplitActor;
		}
		else
			SplitActorMap.Add(SplitValue, SplitActor);
	}
	else if (SplitActorMap.Remove(SplitValue) >= 1)
		bFound = true;

	return !bFound;
}

AHoudiniNodeProxy::AHoudiniNodeProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCanBeDamaged(false);
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, USceneComponent::GetDefaultSceneRootVariableName());
}

#undef LOCTEXT_NAMESPACE
