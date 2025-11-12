// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputUtils.h"

#include "Landscape.h"
#include "EngineUtils.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniInputs.h"


FString FHoudiniOutputUtils::GetCookFolderPath(const AHoudiniNode* Node)
{
	return Node->GetCookFolderPath();
}

int32 FHoudiniOutputUtils::CurveAttributeEntryIdx(const HAPI_AttributeOwner& AttribOwner, const int32& VtxIdx, const int32& CurveIdx)
{
	switch (AttribOwner)
	{
	case HAPI_ATTROWNER_VERTEX:
	case HAPI_ATTROWNER_POINT: return VtxIdx;
	case HAPI_ATTROWNER_PRIM: return CurveIdx;
	case HAPI_ATTROWNER_DETAIL: return 0;
	}

	return -1;
}

bool FHoudiniOutputUtils::HapiGetSplitValues(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
	TArray<int32>& OutSplitKeys, TMap<int32, FString>& OutSplitValueMap, HAPI_AttributeOwner& InOutOwner)
{
    // First step, get SplitAttribName
    const HAPI_AttributeOwner SplitAttribNameOwner = FHoudiniEngineUtils::QueryAttributeOwner(
        AttribNames, AttribCounts, HAPI_ATTRIB_UNREAL_SPLIT_ATTR);

    HAPI_AttributeInfo AttribInfo;
    std::string SplitAttribName;
    if (SplitAttribNameOwner == HAPI_ATTROWNER_INVALID)
        return true;

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_UNREAL_SPLIT_ATTR, SplitAttribNameOwner, &AttribInfo));

	if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
	{
		HAPI_StringHandle SplitAttribNameSH;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_UNREAL_SPLIT_ATTR, &AttribInfo, &SplitAttribNameSH, 0, 1));

		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(SplitAttribNameSH, SplitAttribName));
	}

    // Second step, get split values
	if (InOutOwner != HAPI_ATTROWNER_INVALID)
	{
		if (!FHoudiniEngineUtils::IsAttributeExists(AttribNames, AttribCounts, SplitAttribName, InOutOwner))
			InOutOwner = HAPI_ATTROWNER_INVALID;
	}

	if (InOutOwner == HAPI_ATTROWNER_INVALID)
		InOutOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, AttribCounts, SplitAttribName);

    if (InOutOwner == HAPI_ATTROWNER_INVALID)
        return true;

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        SplitAttribName.c_str(), InOutOwner, &AttribInfo));

    OutSplitKeys.SetNumUninitialized(AttribInfo.count);

    if (!FHoudiniEngineUtils::IsArray(AttribInfo.storage) &&
		FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            SplitAttribName.c_str(), &AttribInfo, 1, OutSplitKeys.GetData(), 0, AttribInfo.count))
    else
    {
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            SplitAttribName.c_str(), &AttribInfo, OutSplitKeys.GetData(), 0, AttribInfo.count));

		const TArray<HAPI_StringHandle> UniqueSplitKeys = TSet<HAPI_StringHandle>(OutSplitKeys).Array();
		TArray<FString> UniqueSplitValues;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueSplitKeys, UniqueSplitValues));
		for (int32 SplitIdx = 0; SplitIdx < UniqueSplitKeys.Num(); ++SplitIdx)
			OutSplitValueMap.Add(UniqueSplitKeys[SplitIdx], UniqueSplitValues[SplitIdx]);
    }

    return true;
}

bool FHoudiniOutputUtils::HapiGetCurvePointIds(const int32& NodeId, const int32& PartId,
	const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX], TArray<int32>& OutPointIds)
{
    bool bFound = false;
    for (int32 AttribIdx = AttribCounts[HAPI_ATTROWNER_VERTEX];
        AttribIdx < AttribCounts[HAPI_ATTROWNER_VERTEX] + AttribCounts[HAPI_ATTROWNER_POINT]; ++AttribIdx)
    {
        if (AttribNames[AttribIdx] == HAPI_ATTRIB_CURVE_POINT_ID)
        {
            bFound = true;
            break;
        }
    }

    if (!bFound)
        return true;

    HAPI_AttributeInfo AttribInfo;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        HAPI_ATTRIB_CURVE_POINT_ID, HAPI_ATTROWNER_POINT, &AttribInfo));

    if (!FHoudiniEngineUtils::IsArray(AttribInfo.storage) &&
		(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int))
    {
        AttribInfo.tupleSize = 1;
        OutPointIds.SetNumUninitialized(AttribInfo.count);
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            HAPI_ATTRIB_CURVE_POINT_ID, &AttribInfo, 1, OutPointIds.GetData(), 0, AttribInfo.count));
    }

    return true;
}

bool FHoudiniOutputUtils::HapiGetPartialOutputModes(const int32& NodeId, const int32& PartId,
	TArray<int8>& OutData, HAPI_AttributeOwner& InOutOwner)
{
	static const auto GetPartialOutputModeLambda = [](const FUtf8StringView& AttribValue) -> int8
		{
			if ((UE::String::FindFirst(AttribValue, "remove", ESearchCase::IgnoreCase) != INDEX_NONE) ||
				(UE::String::FindFirst(AttribValue, "del", ESearchCase::IgnoreCase) != INDEX_NONE))
				return HAPI_PARTIAL_OUTPUT_MODE_REMOVE;
			else if ((UE::String::FindFirst(AttribValue, "partial", ESearchCase::IgnoreCase) != INDEX_NONE) ||
				(UE::String::FindFirst(AttribValue, "update", ESearchCase::IgnoreCase) != INDEX_NONE) || (UE::String::FindFirst(AttribValue, "mod", ESearchCase::IgnoreCase) != INDEX_NONE) ||
				(UE::String::FindFirst(AttribValue, "add", ESearchCase::IgnoreCase) != INDEX_NONE) || (UE::String::FindFirst(AttribValue, "append", ESearchCase::IgnoreCase) != INDEX_NONE))
				return HAPI_PARTIAL_OUTPUT_MODE_MODIFY;
			return HAPI_PARTIAL_OUTPUT_MODE_REPLACE;
		};

	return FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
		HAPI_ATTRIB_PARTIAL_OUTPUT_MODE, GetPartialOutputModeLambda, OutData, InOutOwner);
}

bool FHoudiniOutputUtils::GatherAllLandscapes(const AHoudiniNode* Node,
	TMap<FString, TArray<ALandscape*>>& OutInputLandscapesMap, TArray<ALandscape*>& OutLandscapes)
{
	for (TActorIterator<ALandscape> LandscapeIter(Node->GetWorld()); LandscapeIter; ++LandscapeIter)
	{
		ALandscape* Landscape = *LandscapeIter;
		if (IsValid(Landscape))
			OutLandscapes.Add(Landscape);
	}
	if (OutLandscapes.IsEmpty())
		return false;

	for (const UHoudiniInput* Input : Node->GetInputs())
	{
		if (Input->GetType() == EHoudiniInputType::World)
		{
			for (UHoudiniInputHolder* InputHolder : Input->Holders)  // Find UHoudiniInputLandscape and GetLandscape
			{
				UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(InputHolder);
				if (!IsValid(LandscapeInput))
					continue;

				ALandscape* InputLandscape = LandscapeInput->GetLandscape();
				if (IsValid(InputLandscape))
				{
					OutInputLandscapesMap.FindOrAdd(Input->GetInputName()).AddUnique(InputLandscape);
					OutLandscapes.Remove(InputLandscape);
				}
			}
		}
		else if (Input->GetType() == EHoudiniInputType::Mask)  // Find UHoudiniInputMask and GetLandscape
		{
			for (UHoudiniInputHolder* InputHolder : Input->Holders)
			{
				ALandscape* InputLandscape = Cast<UHoudiniInputMask>(InputHolder)->GetLandscape();
				if (IsValid(InputLandscape))
				{
					OutInputLandscapesMap.FindOrAdd(Input->GetInputName()).AddUnique(InputLandscape);
					OutLandscapes.Remove(InputLandscape);
				}
			}
		}
	}

	return true;
}

ALandscape* FHoudiniOutputUtils::FindTargetLandscape(const FString& LandscapeOutputTarget,
	const TMap<FString, TArray<ALandscape*>>& InputLandscapesMap, const TArray<ALandscape*>& Landscapes)  // Return nullptr if not found
{
	if (LandscapeOutputTarget.IsEmpty())  // if Target is empty, then we just use the first landscape in the inputs and world
	{
		for (const auto& InputLandscapes : InputLandscapesMap)
		{
			for (ALandscape* Landscape : InputLandscapes.Value)
				return Landscape;
		}

		return Landscapes[0];
	}

	// Firstly, find landscape by InputName, maybe LandscapeOutputTarget == UHoudiniInput::Name, and this input has import a landscape
	if (const TArray<ALandscape*>* FoundLandscapesPtr = InputLandscapesMap.Find(LandscapeOutputTarget))
		return (*FoundLandscapesPtr)[0];

	// Secondary, find landscape in InputLandscapes by ActorLabel and UObject::Name
	for (const auto& InputLandscapes : InputLandscapesMap)
	{
		for (ALandscape* Landscape : InputLandscapes.Value)
		{
			if ((Landscape->GetActorLabel(false) == LandscapeOutputTarget) || // Firstly, we should check landscape actor label
				(Landscape->GetFName().ToString() == LandscapeOutputTarget))  // Then check object name
				return Landscape;
		}
	}

	// Lastly, find landscape in Landscapes that not been imported, also by ActorLabel and UObject::Name
	for (ALandscape* Landscape : Landscapes)
	{
		if ((Landscape->GetActorLabel(false) == LandscapeOutputTarget) || // Firstly, we should check landscape actor label
			(Landscape->GetFName().ToString() == LandscapeOutputTarget))  // Then check object name
			return Landscape;
	}

	return nullptr;
}

void FHoudiniOutputUtils::NotifyLandscapeChanged(const FName& LandscapeName, const FName& EditLayerName, const FName& LayerName, const FIntRect& ChangedExtent)
{
	FOREACH_HOUDINI_INPUT_CHECK_CHANGED(if (Input->GetType() == EHoudiniInputType::World)
		{
			for (UHoudiniInputHolder* Holder : Input->Holders)
			{
				if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
				{
					if (LandscapeInput->GetLandscapeName() == LandscapeName)
						LandscapeInput->NotifyLayerChanged(EditLayerName, LayerName, ChangedExtent);
				}
			}
		}
	)
}

UHoudiniInputMask* FHoudiniOutputUtils::FindMaskInput(const AHoudiniNode* Node, const FString& MaskInputPath, FTransform& OutLandscapeTransform, FIntRect& OutLandscapeExtent)
{
	FString NodeTarget;
	FString MaskInputName;
	const AHoudiniNode* MaskNode = nullptr;
	if (MaskInputPath.Split(TEXT("/"), &NodeTarget, &MaskInputName))
	{
		for (const TWeakObjectPtr<AHoudiniNode>& CurrNode : FHoudiniEngine::Get().GetCurrentNodes())
		{
			if (CurrNode.IsValid() && ((CurrNode->GetName() == NodeTarget) || (CurrNode->GetActorLabel(false) == NodeTarget)))
			{
				MaskNode = CurrNode.Get();
				break;
			}
		}
	}
	else
		MaskNode = Node;

	if (IsValid(MaskNode))
	{
		for (const UHoudiniInput* Input : MaskNode->GetInputs())
		{
			if ((Input->GetType() == EHoudiniInputType::Mask) && Input->Holders.IsValidIndex(0) && (Input->GetInputName() == MaskInputPath))
			{
				if (UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Input->Holders[0]))
				{
					if (const ALandscape* Landscape = MaskInput->GetLandscape())
					{
						if (const ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
						{
							OutLandscapeTransform = Landscape->GetTransform();
							LandscapeInfo->GetLandscapeExtent(OutLandscapeExtent);
							return MaskInput;
						}
					}
				}

				break;
			}
		}
	}

	return nullptr;
}

void FHoudiniOutputUtils::WriteToMaskInput(UHoudiniInputMask* MaskInput, TConstArrayView<uint8> Data, const FIntRect& Extent)
{
	if (MaskInput->UpdateData(Data, Extent) != INVALID_LANDSCAPE_EXTENT)
		MaskInput->OnChangedDelegate.Broadcast(false);
	MaskInput->CommitData();
}

void FHoudiniOutputUtils::CloseData(const float* Data, const size_t& SHMHandle)
{
	if (!Data)
	{
		if (SHMHandle)
			FHoudiniEngineUtils::CloseSharedMemoryHandle(SHMHandle);
		return;
	}

	if (SHMHandle)
	{
		FHoudiniEngineUtils::UnmapSharedMemory(Data);
		FHoudiniEngineUtils::CloseSharedMemoryHandle(SHMHandle);
	}
	else
		FMemory::Free((void*)Data);
}
