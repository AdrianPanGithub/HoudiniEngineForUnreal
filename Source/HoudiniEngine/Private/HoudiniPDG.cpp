// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniNode.h"

#include "Tasks/Task.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniOutput.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

bool AHoudiniNode::HapiUpdatePDG()
{
	// Get all TOP nodes but not schedulers
	int32 TopNodeCount = 0;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ComposeChildNodeList(FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_NODETYPE_TOP, HAPI_NODEFLAGS_TOP_NONSCHEDULER, true, &TopNodeCount));

    if (TopNodeCount <= 0)
        return true;

    TArray<int32> TopNodeIds;
    TopNodeIds.SetNumUninitialized(TopNodeCount);
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetComposedChildNodeList(FHoudiniEngine::Get().GetSession(), NodeId,
        TopNodeIds.GetData(), TopNodeCount));

    TArray<FHoudiniTopNode> NewTopNodes;
    for (const int32& TopNodeId : TopNodeIds)
    {
        FString TopNodePath;
        FString TopNodeLabel;
        {
            HAPI_StringHandle NodePathSH;
            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetNodePath(FHoudiniEngine::Get().GetSession(),
                TopNodeId, NodeId, &NodePathSH));

            HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(NodePathSH, TopNodePath));

            int32 SplitIdx = -1;
            if (TopNodePath.FindLastChar(TCHAR('/'), SplitIdx))
                TopNodeLabel = TopNodePath.RightChop(SplitIdx + 1);
            else
                TopNodeLabel = TopNodePath;
        }

        const bool bIsTopOutputNode = TopNodeLabel.StartsWith(HOUDINI_ENGINE_PDG_OUTPUT_NODE_LABEL_PREFIX);
        const bool bIsTopNode = bIsTopOutputNode ? false : TopNodeLabel.StartsWith(HOUDINI_ENGINE_PDG_NODE_LABEL_PREFIX);
        if (!bIsTopNode && !bIsTopOutputNode)
            continue;

        const int32 FoundOldTopNodeIdx = TopNodes.IndexOfByPredicate(
            [TopNodePath](const FHoudiniTopNode& OldTopNode) { return TopNodePath == OldTopNode.Path; });
        FHoudiniTopNode NewTopNode;
        if (TopNodes.IsValidIndex(FoundOldTopNodeIdx))
        {
            NewTopNode = TopNodes[FoundOldTopNodeIdx];
            TopNodes.RemoveAt(FoundOldTopNodeIdx);
        }
        else
            NewTopNode.Path = TopNodePath;
        NewTopNode.NodeId = TopNodeId;

        if (bIsTopNode)  // Is not a output node, then it should NOT has outputs
            NewTopNode.Destroy();

        NewTopNodes.Add(NewTopNode);
    }

    for (FHoudiniTopNode& TopNode : TopNodes)
        TopNode.Destroy();

    TopNodes = NewTopNodes;

	return true;
}

bool AHoudiniNode::HasTopNodePendingCook() const
{
    for (const FHoudiniTopNode& TopNode : TopNodes)
    {
        if (TopNode.NeedCook())
            return true;
    }

    return false;
}

void AHoudiniNode::AsyncCookPDG()
{
    FHoudiniEngine::Get().StartHoudiniTask();

    UE::Tasks::Launch(UE_SOURCE_LOCATION, [&]
        {
            for (FHoudiniTopNode& TopNode : TopNodes)
            {
                if (TopNode.NeedCook())
                {
                    AsyncTask(ENamedThreads::GameThread, [&]
                        {
                            FHoudiniEngine::Get().HoudiniAsyncTaskMessageEvent.Broadcast(
                                FText::FromString(GetActorLabel(false) + TEXT(" - PDG Cooking:\n") + TopNode.Path));
                        });

                    TopNode.HapiCook(this);
                }
            }

            AsyncTask(ENamedThreads::GameThread, [&]
                {
                    this->CleanupSplitActors();
                    FHoudiniEngine::Get().FinishHoudiniTask();
                    FHoudiniEngine::Get().FinishHoudiniAsyncTaskMessage();
                });
        });
}

bool FHoudiniTopNode::IsOutput() const
{
    int32 SplitIdx = -1;
    if (Path.FindLastChar(TCHAR('/'), SplitIdx))
        return Path.RightChop(SplitIdx + 1).StartsWith(HOUDINI_ENGINE_PDG_OUTPUT_NODE_LABEL_PREFIX);
    return Path.StartsWith(HOUDINI_ENGINE_PDG_OUTPUT_NODE_LABEL_PREFIX);
}

class FHoudiniOutputNameAccessor : public UHoudiniOutput
{
public:
    FORCEINLINE void Set(const FString& NewName) { Name = NewName; }

    FORCEINLINE void Reset() { Name.Empty(); }
};

bool FHoudiniTopNode::HapiCook(AHoudiniNode* Node)
{
    // Mark all output name empty, means pending destroy
    for (UHoudiniOutput* Output : Outputs)
        ((FHoudiniOutputNameAccessor*)Output)->Reset();
    
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CookPDG(FHoudiniEngine::Get().GetSession(), NodeId, 0, 0));
    
    Task = EPDGTaskType::Cooking;

    // While its cooking, check pdg events for each graph context,
    // until cook has finished or errored
    int32 WorkItemOutputIdx = 0;
    const bool bIsOutput = IsOutput();

    TMap<HAPI_PDG_WorkItemId, HAPI_PDG_WorkItemState> WorkItemStateMap;
    for (;;)
    {
        FPlatformProcess::SleepNoStats(0.1f);

        // Always query the number of graph contexts each time
        int NumContexts = 0;
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetPDGGraphContextsCount(FHoudiniEngine::Get().GetSession(), &NumContexts));
        TArray<HAPI_StringHandle> ContextNames;
        ContextNames.SetNumUninitialized(NumContexts);
        TArray<int32> ContextIds;
        ContextIds.SetNumUninitialized(NumContexts);
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetPDGGraphContexts(FHoudiniEngine::Get().GetSession(),
            ContextNames.GetData(), ContextIds.GetData(), 0, NumContexts));

        if (Task == EPDGTaskType::Pause)
        {
            for (const int32& ContextId : ContextIds)
                HAPI_SESSION_FAIL_RETURN(FHoudiniApi::PausePDGCook(FHoudiniEngine::Get().GetSession(), ContextId));
            Task = EPDGTaskType::None;
            return true;
        }
        else if (Task == EPDGTaskType::Cancel)
        {
            for (const int32& ContextId : ContextIds)
                HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CancelPDGCook(FHoudiniEngine::Get().GetSession(), ContextId));
            Task = EPDGTaskType::None;
            return true;
        }

        bool bFinished = false;
        for (const int32& ContextId : ContextIds)
        {
            // Check for new events
            TArray<HAPI_PDG_EventInfo> EventInfos;
            EventInfos.SetNumUninitialized(32);
            int drained = 0, leftOver = 0;
            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetPDGEvents(FHoudiniEngine::Get().GetSession(), ContextId,
                EventInfos.GetData(), 32, &drained, &leftOver));
            // Loop over the acquired events
            for (int i = 0; i < drained; i++)
            {
                switch (EventInfos[i].eventType)
                {
                case HAPI_PDG_EVENT_WORKITEM_ADD:
                case HAPI_PDG_EVENT_WORKITEM_REMOVE:
                case HAPI_PDG_EVENT_COOK_WARNING: break;
                case HAPI_PDG_EVENT_COOK_ERROR:
                case HAPI_PDG_EVENT_COOK_COMPLETE:
                {
                    bFinished = true;
                    Task = EPDGTaskType::None;
                }
                break;
                case HAPI_PDG_EVENT_WORKITEM_STATE_CHANGE:
                {
                    const HAPI_PDG_WorkItemId& WorkItemId = EventInfos[i].workItemId;

                    switch ((HAPI_PDG_WorkitemState)EventInfos[i].currentState)
                    {
                    case HAPI_PDG_WORKITEM_COOKED_SUCCESS:
                    case HAPI_PDG_WORKITEM_COOKED_CACHE:
                    if (bIsOutput)
                    {
                        HAPI_PDG_WorkItemState& WorkItemState = WorkItemStateMap.FindOrAdd(WorkItemId);
                        if (WorkItemState == HAPI_PDG_WORKITEM_COOKED_SUCCESS)
                            break;
                        WorkItemState = HAPI_PDG_WORKITEM_COOKED_SUCCESS;

                        HAPI_PDG_WorkItemInfo WorkItemInfo;
                        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetWorkItemInfo(FHoudiniEngine::Get().GetSession(),
                            ContextId, WorkItemId, &WorkItemInfo));
                        if (WorkItemInfo.outputFileCount > 0)
                        {
                            TArray<HAPI_PDG_WorkItemOutputFile> OutputFiles;
                            OutputFiles.SetNum(WorkItemInfo.outputFileCount);
                            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetWorkItemOutputFiles(FHoudiniEngine::Get().GetSession(), NodeId,
                                WorkItemId, OutputFiles.GetData(), WorkItemInfo.outputFileCount));

                            TArray<HAPI_StringHandle> FilePathSHs;
                            for (const HAPI_PDG_WorkItemOutputFile& OutputFile : OutputFiles)
                                FilePathSHs.Add(OutputFile.filePathSH);

                            TArray<std::string> FilePaths;
                            HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(FilePathSHs, FilePaths));
                            AsyncTask(ENamedThreads::GameThread, [this, Node, FilePaths, WorkItemOutputIdx]
                                {
                                    int32 FileInputNodeId = -1;
                                    for (int32 FileIdx = 0; FileIdx < FilePaths.Num(); ++FileIdx)
                                        HOUDINI_FAIL_INVALIDATE_RETURN(this->HapiUpdateOutputs(Node,
                                            FString::Printf(TEXT("%d_%d"), WorkItemOutputIdx, FileIdx), FilePaths[FileIdx], FileInputNodeId));

                                    HAPI_NodeInfo NodeInfo;
                                    FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), FileInputNodeId, &NodeInfo);
                                    FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeInfo.parentId);
                                });

                            ++WorkItemOutputIdx;
                        }
                    }
                    else
                        WorkItemStateMap.FindOrAdd(WorkItemId) = HAPI_PDG_WORKITEM_COOKED_SUCCESS;
                    break;
                    case HAPI_PDG_WORKITEM_COOKED_FAIL: WorkItemStateMap.FindOrAdd(WorkItemId) = HAPI_PDG_WORKITEM_COOKED_FAIL; break;
                    case HAPI_PDG_WORKITEM_COOKING: WorkItemStateMap.FindOrAdd(WorkItemId) = HAPI_PDG_WORKITEM_COOKING; break;
                    case HAPI_PDG_WORKITEM_WAITING: WorkItemStateMap.FindOrAdd(WorkItemId) = HAPI_PDG_WORKITEM_WAITING; break;
                    }
                }
                break;
                default:
                    break;
                }
            }
        }

        ResetStates();
        for (const auto& WorkItemState : WorkItemStateMap)
        {
            switch (WorkItemState.Value)
            {
            case HAPI_PDG_WORKITEM_COOKED_FAIL:  ++NumErrorWorkItems; break;
            case HAPI_PDG_WORKITEM_COOKED_SUCCESS:  ++NumCompletedWorkItems; break;
            case HAPI_PDG_WORKITEM_COOKING:  ++NumRunningWorkItems; break;
            case HAPI_PDG_WORKITEM_WAITING:  ++NumWaitingWorkItems; break;
            }
        }

        if (bFinished)
            break;
    } 

    // Clean up outputs that OutputName is empty (means is stale and pending destroy)
    AsyncTask(ENamedThreads::GameThread, [this]
        {
            Outputs.RemoveAll([](UHoudiniOutput* Output)
                {
                    if (Output->GetOutputName().IsEmpty())
                    {
                        Output->Destroy();
                        return true;
                    }
                    return false;
                });
        });

	return true;
}

bool FHoudiniTopNode::HapiUpdateOutputs(AHoudiniNode* Node, const FString& PDGOutputIdentifier, const std::string& FilePath, int32& InOutFileInputNodeId)
{
    if (!FPaths::FileExists(FilePath.c_str()))
        return true;

    if (InOutFileInputNodeId < 0)
        HOUDINI_FAIL_RETURN(FHoudiniSopFile::HapiCreateNode(-1, FString(), InOutFileInputNodeId));

    HOUDINI_FAIL_RETURN(FHoudiniSopFile::HapiLoadFile(InOutFileInputNodeId, FilePath.c_str()));
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CookNode(FHoudiniEngine::Get().GetSession(), InOutFileInputNodeId, nullptr));

    HAPI_GeoInfo GeoInfo;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), InOutFileInputNodeId, &GeoInfo));
    
    const TArray<TSharedPtr<IHoudiniOutputBuilder>>& OutputBuilders = FHoudiniEngine::Get().GetOutputBuilders();

    struct FHoudiniOutputDesc
    {
        FHoudiniOutputDesc(UHoudiniOutput* InOutput, const HAPI_PartInfo& PartInfo) : Output(InOutput)
        {
            PartInfos.Add(PartInfo);
        }

        UHoudiniOutput* Output;  // Should never be nullptr
        TArray<HAPI_PartInfo> PartInfos;
    };

    const FString OutputName = FHoudiniEngineUtils::GetValidatedString(Path) + PDGOutputIdentifier;

    TArray<FHoudiniOutputDesc> OutputDescs;
    TMap<TSharedPtr<IHoudiniOutputBuilder>, TArray<HAPI_PartInfo>> OutputConverters;
    for (int32 PartIdx = 0; PartIdx < GeoInfo.partCount; ++PartIdx)
    {
        HAPI_PartInfo PartInfo;
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartIdx, &PartInfo));
        for (int32 BuilderIdx = OutputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
        {
            const TSharedPtr<IHoudiniOutputBuilder>& OutputBuilder = OutputBuilders[BuilderIdx];
            bool bIsValid = false;
            bool bShouldHoldByOutput = false;
            HOUDINI_FAIL_RETURN(OutputBuilder->HapiIsPartValid(GeoInfo.nodeId, PartInfo, bIsValid, bShouldHoldByOutput));
            if (bIsValid)
            {
                if (bShouldHoldByOutput)
                {
                    const TSubclassOf<UHoudiniOutput> OutputClass = OutputBuilder->GetClass();
                    if (FHoudiniOutputDesc* FoundOutputDescPtr = OutputDescs.FindByPredicate(
                        [OutputClass](const FHoudiniOutputDesc& OutputDesc) { return OutputDesc.Output->GetClass() == OutputClass; }))
                        FoundOutputDescPtr->PartInfos.Add(PartInfo);
                    else
                    {
                        UHoudiniOutput* FoundOutput = nullptr;
                        for (UHoudiniOutput* Output : Outputs)
                        {
                            if (Output->GetOutputName().IsEmpty() && Output->GetClass() == OutputClass)
                            {
                                ((FHoudiniOutputNameAccessor*)Output)->Set(OutputName);  // Set name to prevent from destroy
                                FoundOutput = Output;
                                break;
                            }
                        }

                        if (!FoundOutput)
                        {
                            FoundOutput = NewObject<UHoudiniOutput>(Node, OutputClass, MakeUniqueObjectName(Node, OutputClass, "HoudiniPDGOutput"), RF_Public | RF_Transactional);
                            Outputs.Add(FoundOutput);
                            ((FHoudiniOutputNameAccessor*)FoundOutput)->Set(OutputName);  // Set name to prevent from destroy
                        }

                        OutputDescs.Add(FHoudiniOutputDesc(FoundOutput, PartInfo));
                    }
                }
                else
                    OutputConverters.FindOrAdd(OutputBuilder).Add(PartInfo);

                break;
            }
        }
    }

    for (const auto& OutputConverter : OutputConverters)
        HOUDINI_FAIL_RETURN(OutputConverter.Key->HapiRetrieve(Node, OutputName, GeoInfo, OutputConverter.Value));

    for (const FHoudiniOutputDesc& OutputDesc : OutputDescs)
        HOUDINI_FAIL_RETURN(OutputDesc.Output->HapiUpdate(GeoInfo, OutputDesc.PartInfos));

    return true;
}

void FHoudiniTopNode::Invalidate()
{
    NodeId = -1;
    Task = EPDGTaskType::None;
    ResetStates();
}

void FHoudiniTopNode::Destroy()
{
    class FHoudiniCacheActorMap : public FHoudiniEngine
    {
    public:
        FORCEINLINE void Cache() { CacheNameActorMap(); }
    };

    ((FHoudiniCacheActorMap*)&FHoudiniEngine::Get())->Cache();

    for (const UHoudiniOutput* Output : Outputs)
        Output->Destroy();

    Outputs.Empty();
}

bool FHoudiniTopNode::HapiDirty() const
{
    const_cast<FHoudiniTopNode*>(this)->ResetStates();

    if (NodeId >= 0)
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DirtyPDGNode(FHoudiniEngine::Get().GetSession(), NodeId, true));

    return true;
}

bool FHoudiniTopNode::HapiDirtyAll() const
{
    const_cast<FHoudiniTopNode*>(this)->Destroy();

    return HapiDirty();
}

#undef LOCTEXT_NAMESPACE
