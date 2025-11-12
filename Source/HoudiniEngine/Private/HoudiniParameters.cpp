// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniParameters.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniNode.h"


bool AHoudiniNode::HapiSyncAttributeMultiParameters(const bool& bUpdateDefaultCount) const
{
	for (const auto& AttribParmSet : GroupParmSetMap)
	{
		for (UHoudiniParameter* AttribParm : AttribParmSet.Value.PointAttribParms)
		{
			if (AttribParm->GetType() == EHoudiniParameterType::MultiParm)
				HOUDINI_FAIL_RETURN(Cast<UHoudiniMultiParameter>(AttribParm)->HapiSyncAttribute(bUpdateDefaultCount));
		}

		for (UHoudiniParameter* AttribParm : AttribParmSet.Value.PrimAttribParms)
		{
			if (AttribParm->GetType() == EHoudiniParameterType::MultiParm)
				HOUDINI_FAIL_RETURN(Cast<UHoudiniMultiParameter>(AttribParm)->HapiSyncAttribute(bUpdateDefaultCount));
		}
	}
	return true;
}

// As unreal string hash ignores case, we should overload our own GetTypeHash
struct FHoudiniParameterNameHolder
{
	FHoudiniParameterNameHolder(const FString* InStrPtr) : StrPtr(InStrPtr) {}
	const FString* StrPtr;

	bool operator==(const FHoudiniParameterNameHolder& OtherHolder) const
	{
		return StrPtr->Equals(*OtherHolder.StrPtr, ESearchCase::CaseSensitive);
	}
};

uint32 GetTypeHash(const FHoudiniParameterNameHolder& NameHolder)
{
	return FCrc::StrCrc32(*(*(NameHolder.StrPtr)));
}

bool AHoudiniNode::HapiUpdateParameters(const bool& bBeforeCook)
{
	const double StartTime = FPlatformTime::Seconds();

	if (Preset.IsValid())
	{
		// First, we should sync the count of all MultiParms after preset pending to load
		if (bBeforeCook)
		{
			FHoudiniParameterPresetHelper PresetHelper;
			for (const auto& Parm : *Preset)
			{
				if (Parm.Value.Type == EHoudiniGenericParameterType::MultiParm)
					PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Parm.Key.ToString())),
						std::string(TCHAR_TO_UTF8(*FString(Parm.Value.StringValue.IsEmpty() ? FString::FromInt(int32(Parm.Value.NumericValues.X)) : Parm.Value.StringValue))));
			}
			HOUDINI_FAIL_RETURN(PresetHelper.HapiSet(NodeId));
		}
		else  // HAPI BUG: Set multiparm by preset may crack hapi internal parm list, so we could only set by hapi 
		{
			for (const auto& Parm : *Preset)
			{
				if (Parm.Value.Type == EHoudiniGenericParameterType::MultiParm)
				{
					int CurrCount = 0;
					HAPI_Result HapiResult = FHoudiniApi::GetParmIntValue(FHoudiniEngine::Get().GetSession(), NodeId,
						TCHAR_TO_UTF8(*Parm.Key.ToString()), 0, &CurrCount);
					if (HapiResult != HAPI_RESULT_SUCCESS)  // Not found this parm
					{
						if (HAPI_SESSION_INVALID_RESULT(HapiResult))
							return false;
						continue;
					}
					
					const int32 DstCount = Parm.Value.StringValue.IsEmpty() ? int32(Parm.Value.NumericValues.X) : FCString::Atoi(*Parm.Value.StringValue);
					if (DstCount != CurrCount)
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), NodeId,
							TCHAR_TO_UTF8(*Parm.Key.ToString()), 0, DstCount));
				}
			}
		}
	}
	else if (bBeforeCook)
	{
		// First, we should sync the count of all MultiParms after just instantiate
		FHoudiniParameterPresetHelper PresetHelper;
		for (UHoudiniParameter* Parm : Parms)
		{
			if (Parm->GetType() == EHoudiniParameterType::MultiParm)
				PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Parm->GetParameterName())),
					std::string(TCHAR_TO_UTF8(*FString::FromInt(Cast<UHoudiniMultiParameter>(Parm)->GetInstanceCount()))));
			// We need NOT sync ramp-parms though they are also MultiParm, because we do not parse their children.
		}
		HOUDINI_FAIL_RETURN(PresetHelper.HapiSet(NodeId));
	}

	if (bBeforeCook)  // Always sync attrib multiparm after instantiating
		HOUDINI_FAIL_RETURN(HapiSyncAttributeMultiParameters(true));


	// -------- Update parameters and values ---------
	HAPI_NodeInfo NodeInfo;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo));
	GeoNodeId = (NodeInfo.type == HAPI_NODETYPE_OBJ) ? -1 : NodeInfo.parentId;

	// Get all int values
	TArray<int32> IntValues;
	if (NodeInfo.parmIntValueCount >= 1)
	{
		IntValues.SetNumUninitialized(NodeInfo.parmIntValueCount);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmIntValues(FHoudiniEngine::Get().GetSession(), NodeId,
			IntValues.GetData(), 0, NodeInfo.parmIntValueCount));
	}

	// Get all float values
	TArray<float> FloatValues;
	if (NodeInfo.parmFloatValueCount >= 1)
	{
		FloatValues.SetNumUninitialized(NodeInfo.parmFloatValueCount);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmFloatValues(FHoudiniEngine::Get().GetSession(), NodeId,
			FloatValues.GetData(), 0, NodeInfo.parmFloatValueCount));
	}

	TArray<HAPI_ParmInfo> ParmInfos;
	TArray<FString> AllStrings;
	TConstArrayView<FString> StringValues;
	TConstArrayView<FString> ParmNames, ParmLabels, ParmHelps;
	TConstArrayView<FString> ChoiceValues, ChoiceLabels;
	if (NodeInfo.parmCount >= 1)
	{
		// Get all string values and infos
		TArray<HAPI_StringHandle> AllSHs;
		// { StringValues, (ParmNames, ParmLabels, ParmHelps), (ChoiceValues, ChoiceLabels) }
		AllSHs.SetNumUninitialized(NodeInfo.parmStringValueCount + NodeInfo.parmCount * 3 + NodeInfo.parmChoiceCount * 2);
		if (NodeInfo.parmStringValueCount >= 1)
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmStringValues(FHoudiniEngine::Get().GetSession(), NodeId, false,
				AllSHs.GetData(), 0, NodeInfo.parmStringValueCount));

		ParmInfos.SetNumUninitialized(NodeInfo.parmCount);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), NodeId,
			ParmInfos.GetData(), 0, NodeInfo.parmCount));

		for (int32 ParmIndex = 0; ParmIndex < NodeInfo.parmCount; ++ParmIndex)
		{
			const HAPI_ParmInfo& ParmInfo = ParmInfos[ParmIndex];
			AllSHs[NodeInfo.parmStringValueCount + ParmIndex] = ParmInfo.nameSH;
			AllSHs[NodeInfo.parmStringValueCount + NodeInfo.parmCount + ParmIndex] = ParmInfo.labelSH;
			AllSHs[NodeInfo.parmStringValueCount + NodeInfo.parmCount * 2 + ParmIndex] = ParmInfo.helpSH;
		}

		TArray<HAPI_ParmChoiceInfo> ChoiceInfos;
		if (NodeInfo.parmChoiceCount >= 1)
		{
			ChoiceInfos.SetNumUninitialized(NodeInfo.parmChoiceCount);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmChoiceLists(FHoudiniEngine::Get().GetSession(), NodeId,
				ChoiceInfos.GetData(), 0, NodeInfo.parmChoiceCount));

			for (int32 ChoiceIndex = 0; ChoiceIndex < NodeInfo.parmChoiceCount; ++ChoiceIndex)
			{
				const HAPI_ParmChoiceInfo& ChoiceInfo = ChoiceInfos[ChoiceIndex];
				AllSHs[NodeInfo.parmStringValueCount + NodeInfo.parmCount * 3 + ChoiceIndex] = ChoiceInfo.valueSH;
				AllSHs[NodeInfo.parmStringValueCount + NodeInfo.parmCount * 3 + NodeInfo.parmChoiceCount + ChoiceIndex] = ChoiceInfo.labelSH;
			}
		}

		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(AllSHs, AllStrings));
		FString* AllStringsPtr = AllStrings.GetData();
		StringValues = TConstArrayView<FString>(AllStringsPtr, NodeInfo.parmStringValueCount);
		ParmNames = TConstArrayView<FString>(AllStringsPtr + NodeInfo.parmStringValueCount, NodeInfo.parmCount);
		ParmLabels = TConstArrayView<FString>(AllStringsPtr + NodeInfo.parmStringValueCount + NodeInfo.parmCount, NodeInfo.parmCount);
		ParmHelps = TConstArrayView<FString>(AllStringsPtr + NodeInfo.parmStringValueCount + NodeInfo.parmCount * 2, NodeInfo.parmCount);
		ChoiceValues = TConstArrayView<FString>(AllStringsPtr + NodeInfo.parmStringValueCount + NodeInfo.parmCount * 3, NodeInfo.parmChoiceCount);
		ChoiceLabels = TConstArrayView<FString>(AllStringsPtr + NodeInfo.parmStringValueCount + NodeInfo.parmCount * 3 + NodeInfo.parmChoiceCount, NodeInfo.parmChoiceCount);
	}


	// -------- Construct a name-parm map for quick find previous parm with same name --------
	TMap<FHoudiniParameterNameHolder, UHoudiniParameter*> PrevNameParmMap;
	for (UHoudiniParameter* Parm : Parms)
		PrevNameParmMap.Add(&Parm->GetParameterName(), Parm);
	for (const auto& AttribParmSet : GroupParmSetMap)
	{
		for (UHoudiniParameter* AttribParm : AttribParmSet.Value.PointAttribParms)
		{
			PrevNameParmMap.Add(&AttribParm->GetParameterName(), AttribParm);
			if (AttribParm->GetType() == EHoudiniParameterType::MultiParm)
			{
				for (UHoudiniParameter* ChildAttribParm : Cast<UHoudiniMultiParameter>(AttribParm)->GetChildAttributeParameters())
					PrevNameParmMap.Add(&ChildAttribParm->GetParameterName(), ChildAttribParm);
			}
		}
		for (UHoudiniParameter* AttribParm : AttribParmSet.Value.PrimAttribParms)
		{
			PrevNameParmMap.Add(&AttribParm->GetParameterName(), AttribParm);
			if (AttribParm->GetType() == EHoudiniParameterType::MultiParm)
			{
				for (UHoudiniParameter* ChildAttribParm : Cast<UHoudiniMultiParameter>(AttribParm)->GetChildAttributeParameters())
					PrevNameParmMap.Add(&ChildAttribParm->GetParameterName(), ChildAttribParm);
			}
		}
	}
	
	
	// We should NOT add parm directly on AHoudiniNode::Parms and AHoudiniNode::GroupParmSetMap, since the session may be interrupted at any time
	// So we should set them after all parms parsing finished
	TArray<UHoudiniParameter*> NewParms;
	TMap<FString, FHoudiniAttributeParameterSet> NewAttribParmSetMap;

	// For AttribParm
	TArray<TObjectPtr<UHoudiniParameter>>* CurrAttribParmArrayPtr = nullptr;  // Will be nullptr if AttribParms haven't started parse.
	TArray<int32> CurrAttribParmParentIdStack;

	UHoudiniMultiParameter* CurrAttribMultiParm = nullptr;  // Will be nullptr if attrib-MultiParms haven't started parse.
	TArray<int32> CurrAttribMultiParmParentIdStack;  // Will contains first folder

	// For all common parms
	UHoudiniParameterFolderList* CurrParmFolderList = nullptr;

	// For ramps
	const int32* CurrRampIdPtr = nullptr;
	int32 CurrFloatIndex = 0;

	// -------- Iter each current parm-info to create or update parms --------
	for (int32 ParmIdx = 0; ParmIdx < NodeInfo.parmCount; ++ParmIdx)
	{
		HAPI_ParmInfo& ParmInfo = ParmInfos[ParmIdx];
		if (ParmInfo.scriptType == HAPI_PRM_SCRIPT_TYPE_KEY_VALUE_DICT)  // TODO: HAPI does NOT support dict parm 
			continue;

		if ((ParmInfo.floatValuesIndex >= 0) &&
			((ParmInfo.type == HAPI_PARMTYPE_FLOAT) || (ParmInfo.type == HAPI_PARMTYPE_COLOR)))  // HAPI Bug: all floatValueIndex >= 0 after MultiParm inst inserted
			CurrFloatIndex = ParmInfo.floatValuesIndex + ParmInfo.size;
		// If is ramp, then set floatValuesIndex, so that UHoudiniParameterRamp_Base::UpdateValue could update all child values
		else if ((ParmInfo.type == HAPI_PARMTYPE_MULTIPARMLIST) && (ParmInfo.rampType != HAPI_RAMPTYPE_INVALID))
			ParmInfo.floatValuesIndex = CurrFloatIndex;
		
		// We need not parse ramps' children, the values have already recorded in ramp-parms directly
		if (CurrRampIdPtr && ParmInfo.parentId == *CurrRampIdPtr)
			continue;

		FString ParmName = ParmNames[ParmIdx];

		// Find head FolderList and Folder that contains AttribParms, we should skip parsing parm-info for them
		if (ParmInfo.type == HAPI_PARMTYPE_FOLDERLIST)
		{
			bool bIsPointAttribGroup = true;
			EHoudiniEditOptions* CurrEditOptionsPtr = nullptr;
			FString* CurrIdentifierNamePtr = nullptr;
			if (ParmName.RemoveFromEnd(TEXT(HAPI_PARM_SUFFIX_POINT_ATTRIB_FOLDER)))
			{
				FHoudiniAttributeParameterSet& CurrAttribParmSet = NewAttribParmSetMap.FindOrAdd(ParmName);
				CurrAttribParmArrayPtr = &CurrAttribParmSet.PointAttribParms;
				CurrEditOptionsPtr = &CurrAttribParmSet.PointEditOptions;
				CurrIdentifierNamePtr = &CurrAttribParmSet.PointIdentifierName;
			}
			else if (ParmName.RemoveFromEnd(TEXT(HAPI_PARM_SUFFIX_PRIM_ATTRIB_FOLDER)))
			{
				FHoudiniAttributeParameterSet& CurrAttribParmSet = NewAttribParmSetMap.FindOrAdd(ParmName);
				CurrAttribParmArrayPtr = &CurrAttribParmSet.PrimAttribParms;
				CurrEditOptionsPtr = &CurrAttribParmSet.PrimEditOptions;
				CurrIdentifierNamePtr = &CurrAttribParmSet.PrimIdentifierName;
				bIsPointAttribGroup = false;
			}

			if (CurrEditOptionsPtr)  // Means this is a attribute FolderList
			{
				// Reset/Set curr AttribParm variables
				CurrAttribParmParentIdStack.Empty();

				CurrAttribMultiParm = nullptr;
				CurrAttribMultiParmParentIdStack.Empty();

				// No need for parse this FolderList and its child folders, so we set CurrParmFolderList to nullptr
				CurrParmFolderList = nullptr;

				if (bBeforeCook)  // We need to update info from tags
					HOUDINI_FAIL_RETURN(UHoudiniParameterFolderList::HapiGetEditOptions(NodeId, ParmInfo, *CurrEditOptionsPtr, *CurrIdentifierNamePtr))
				else if (const FHoudiniAttributeParameterSet* FoundPrevParmSetPtr = GroupParmSetMap.Find(ParmName))  // Just copy from previous result
				{
					*CurrEditOptionsPtr = (bIsPointAttribGroup ? FoundPrevParmSetPtr->PointEditOptions : FoundPrevParmSetPtr->PrimEditOptions);
					*CurrEditOptionsPtr |= ParmInfo.invisible ? EHoudiniEditOptions::Hide : EHoudiniEditOptions::Show;
					*CurrIdentifierNamePtr = (bIsPointAttribGroup ? FoundPrevParmSetPtr->PointIdentifierName : FoundPrevParmSetPtr->PrimIdentifierName);
				}

				continue;
			}
		}
		else if (ParmInfo.type == HAPI_PARMTYPE_FOLDER)
		{
			if (CurrParmFolderList)  // HAPI Bug: parentId is -1 when parm-type is Folder
				ParmInfo.parentId = CurrParmFolderList->GetId();
			else // A FolderList always followed by a folder. If not, then means this folder is the head folder of AttribParms
			{
				CurrAttribParmParentIdStack.Add(ParmInfo.id);
				continue;
			}
		}
		

		// Check is AttribParm
		const bool bIsAttribParm = CurrAttribParmParentIdStack.Contains(ParmInfo.parentId);
		const bool bIsAttribMultiParmChild = CurrAttribMultiParmParentIdStack.Contains(ParmInfo.parentId);
		
		//if (ParmInfo.type == HAPI_PARMTYPE_NODE && (bIsAttribParm || bIsAttribMultiParmChild))   // Input will parse as asset parm with info import
		//	continue;

		// Try find previous parm with same name, for trying to reuse it.
		UHoudiniParameter* CurrParm = nullptr;
		if (UHoudiniParameter** FoundPrevParmPtr = PrevNameParmMap.Find(&ParmName))
			CurrParm = *FoundPrevParmPtr;
		HOUDINI_FAIL_RETURN(UHoudiniParameter::HapiCreateOrUpdate(this, ParmInfo, ParmName, ParmLabels[ParmIdx], ParmHelps[ParmIdx],
			IntValues, FloatValues, StringValues, ChoiceValues, ChoiceLabels,
			CurrParm, bBeforeCook, bIsAttribParm || bIsAttribMultiParmChild));
		

		// Parse FolderLists, ramps and AttribParms
		switch (CurrParm->GetType())
		{
		case EHoudiniParameterType::FolderList:
		{
			if (bIsAttribParm)
				CurrAttribParmParentIdStack.Add(CurrParm->GetId());
			else if (bIsAttribMultiParmChild)
				CurrAttribMultiParmParentIdStack.Add(CurrParm->GetId());

			CurrParmFolderList = (UHoudiniParameterFolderList*)CurrParm;
		}
		break;
		case EHoudiniParameterType::Folder:
		{
			if (bIsAttribParm)
				CurrAttribParmParentIdStack.Add(CurrParm->GetId());
			else if (bIsAttribMultiParmChild)
				CurrAttribMultiParmParentIdStack.Add(CurrParm->GetId());

			// Need record folder-type to FolderList, since parm-attrib need folder info to judge whether should have values
			CurrParmFolderList->SetFolderType(ParmInfo);
		}
		break;
		case EHoudiniParameterType::MultiParm:
		{
			if (bIsAttribParm && (!CurrAttribMultiParm || !CurrAttribMultiParmParentIdStack.Contains(CurrParm->GetParentId())))  // Do not support Multi-MultiParm within AttribParm group
			{
				CurrAttribMultiParm = (UHoudiniMultiParameter*)CurrParm;
				CurrAttribMultiParmParentIdStack = TArray<int32>{ CurrParm->GetId() };
				CurrAttribParmArrayPtr->Add(CurrParm);
			}
			else
				((UHoudiniMultiParameter*)CurrParm)->ChildAttribParmInsts.Empty();  // Not an attribute

			((UHoudiniMultiParameter*)CurrParm)->ResetChildAttributeParameters();
		}
		break;
		case EHoudiniParameterType::FloatRamp:
		case EHoudiniParameterType::ColorRamp:
			CurrRampIdPtr = &ParmInfo.id;
			break;
		}

		if (CurrParm == CurrAttribMultiParm)  // MultiParm is current attrib, we have already parsed it, so skip
			continue;

		// Parse AttribParm
		if (bIsAttribMultiParmChild)
			CurrAttribMultiParm->AddChildAttributeParameter(CurrParm);
		else
		{
			if (CurrAttribMultiParm)  // Reset attrib-MultiParm vars
			{
				CurrAttribMultiParm = nullptr;
				CurrAttribMultiParmParentIdStack.Empty();
			}
			
			if (bIsAttribParm)
				CurrAttribParmArrayPtr->Add(CurrParm);
			else if (CurrAttribParmArrayPtr)  // Reset AttribParm and AttribMultiParm vars
			{
				CurrAttribParmArrayPtr = nullptr;
				CurrAttribParmParentIdStack.Empty();

				CurrAttribMultiParm = nullptr;
				CurrAttribMultiParmParentIdStack.Empty();
			}
		}

		if (!bIsAttribParm && !bIsAttribMultiParmChild)  // Is a normal parms
		{
			NewParms.Add(CurrParm);
			
			// We should load operator path parm value from preset here, in order to let the AHoudiniNode::HapiUpdateInput to retrieve exactly objects to input
			// UHoudiniInput::HapiUpdateFromParameter called in AHoudiniNode::HapiUpdateInput will actually do this
			if (Preset.IsValid() && CurrParm->GetType() == EHoudiniParameterType::Input)
				Cast<UHoudiniParameterInput>(CurrParm)->TryLoadGeneric(*Preset);
		}
	}

	// Finally, backup value string, for ReDeltaInfo
	if (!bBeforeCook)
	{
		for (UHoudiniParameter* Parm : NewParms)
			Parm->UpdateBackupValueString();
	}

	// Finish parse
	Parms = NewParms;
	GroupParmSetMap = NewAttribParmSetMap;

	if (!GroupParmSetMap.IsEmpty() && (NodeInfo.inputCount == 1) && (NodeInfo.outputCount == 1) && (NodeInfo.type == HAPI_NODETYPE_SOP))
	{
		InputCount = 0;
		bOutputEditable = true;
	}
	else
	{
		InputCount = NodeInfo.inputCount;
		bOutputEditable = false;
	}

	const double TimeCost = FPlatformTime::Seconds() - StartTime;
	if (TimeCost > 0.005)
		UE_LOG(LogHoudiniEngine, Log, TEXT("%s: Update Parameters %.3f (s)"), *GetActorLabel(false), TimeCost);

	return true;
}

bool AHoudiniNode::GetGenericParameters(TMap<FName, FHoudiniGenericParameter>& OutParms) const
{
	for (int32 ParmIdx = 0; ParmIdx < Parms.Num(); ++ParmIdx)
		Parms[ParmIdx]->SaveGeneric(OutParms);

	return true;
}

FHoudiniGenericParameter AHoudiniNode::GetParameterValue(const FString& ParameterName) const
{
	for (const UHoudiniParameter* Parm : Parms)
	{
		if (Parm->GetParameterName() == ParameterName)
		{
			if (Parm->GetType() == EHoudiniParameterType::Input)
			{
				FHoudiniGenericParameter GenericParm;
				Cast<UHoudiniParameterInput>(Parm)->ConvertToGeneric(GenericParm);  // Do NOT use SaveGeneric, as only one GenericParm could return
				return GenericParm;
			}
			else
			{
				TMap<FName, FHoudiniGenericParameter> TempParms;
				Parm->SaveGeneric(TempParms);
				if (!TempParms.IsEmpty())
					return TempParms.begin()->Value;
			}
			break;
		}
	}

	return FHoudiniGenericParameter();
}

void AHoudiniNode::SetParameterValues(const TMap<FName, FHoudiniGenericParameter>& Parameters)
{
	if (!Preset.IsValid())
		Preset = MakeShared<const TMap<FName, FHoudiniGenericParameter>>(Parameters);
	else
	{
		TMap<FName, FHoudiniGenericParameter>* MutablePreset = const_cast<TMap<FName, FHoudiniGenericParameter>*>(Preset.Get());
		for (const TPair<FName, FHoudiniGenericParameter>& Parm : Parameters)
			MutablePreset->FindOrAdd(Parm.Key) = Parm.Value;
	}

	RequestCook();
}

bool UHoudiniParameter::GDisableAttributeActions = false;

bool UHoudiniParameter::HapiCreateOrUpdate(AHoudiniNode* Node,
	const HAPI_ParmInfo& InParmInfo, const FString& ParmName, const FString& ParmLabel, const FString& ParmHelp,
	const TArray<int32>& IntValues, const TArray<float>& FloatValues, TConstArrayView<FString> StringValues,
	TConstArrayView<FString> ChoiceValues, TConstArrayView<FString> ChoiceLabels,
	UHoudiniParameter*& InOutParm, const bool& bBeforeCook, const bool& bIsAttribute)
{
	bool bUpdateTags = bBeforeCook || !InOutParm;  // When before-cook or create a new parm
	bool bUpdateValues = !bBeforeCook && !bIsAttribute;  // If is a attribute, do NOT update the value
	bool bUpdateDefaultValues = bBeforeCook;


	// Get parameter type
	EHoudiniParameterType NewParmType = GetParameterTypeFromInfo(Node->GetNodeId(), InParmInfo);
	bool bImportInfo = false;
	if (bIsAttribute && (NewParmType == EHoudiniParameterType::Input))  // As Input cannot be generated on attrib panel, we should treat InputAttribParm as AssetParm with bImportInfo = true
	{
		NewParmType = EHoudiniParameterType::Asset;
		bImportInfo = true;
	}
	if ((NewParmType == EHoudiniParameterType::String) || (NewParmType == EHoudiniParameterType::StringChoice))
	{
		if (bUpdateTags)
		{
			HAPI_StringHandle TagValueSH;
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(), Node->GetNodeId(),
				InParmInfo.id, HAPI_PARM_TAG_UNREAL_REF, &TagValueSH))
			{
				FString UnrealRefTag;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, UnrealRefTag));
				bImportInfo = UnrealRefTag.Equals(TEXT("2")) || UnrealRefTag.Equals(TEXT("import_info"), ESearchCase::IgnoreCase);
				if (bImportInfo || UnrealRefTag.Equals(TEXT("1")))
					NewParmType = (NewParmType == EHoudiniParameterType::String) ? EHoudiniParameterType::Asset : EHoudiniParameterType::AssetChoice;
			}
		}
		else if (InOutParm && ((InOutParm->GetType() == EHoudiniParameterType::Asset) || (InOutParm->GetType() == EHoudiniParameterType::AssetChoice)))
		{
			// Means previous parm has ref tag, so we should also treat it as asset
			NewParmType = (NewParmType == EHoudiniParameterType::String) ? EHoudiniParameterType::Asset : EHoudiniParameterType::AssetChoice;
		}
	}


	// Check if parm type can be reuse due to - whether string is asset-parm won't be determined when parm-tag not be update.
	bool bCanReuse = InOutParm != nullptr;
	if (bCanReuse)
	{
		if (NewParmType != InOutParm->Type)
			bCanReuse = false;
	}


	// Create new parm and try reuse values of previous parm/Update previous parm
	const UHoudiniParameter* PrevParm = InOutParm;
	if (!bCanReuse)
	{
		// If create new, then need update all
		bUpdateTags = true;
		bUpdateValues = true;
		bUpdateDefaultValues = true;

		switch (NewParmType)
		{
		case EHoudiniParameterType::Separator: InOutParm = NewObject<UHoudiniParameterSeparator>(Node, FName(TEXT("ParmSeparator_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::Folder: InOutParm = NewObject<UHoudiniParameterFolder>(Node, FName(TEXT("ParmFolder_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::Button: InOutParm = NewObject<UHoudiniParameterButton>(Node, FName(TEXT("ParmButton_") + ParmName), RF_Public | RF_Transactional); break;

		// Int types
		case EHoudiniParameterType::Int: InOutParm = NewObject<UHoudiniParameterInt>(Node, FName(TEXT("ParmInt_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::IntChoice: InOutParm = NewObject<UHoudiniParameterIntChoice>(Node, FName(TEXT("ParmIntChoice_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::ButtonStrip: InOutParm = NewObject<UHoudiniParameterButtonStrip>(Node, FName(TEXT("ParmButtonStrip_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::Toggle: InOutParm = NewObject<UHoudiniParameterToggle>(Node, FName(TEXT("ParmToggle_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::FolderList: InOutParm = NewObject<UHoudiniParameterFolderList>(Node, FName(TEXT("ParmFolderList_") + ParmName), RF_Public | RF_Transactional); break;

		// Float types
		case EHoudiniParameterType::Float: InOutParm = NewObject<UHoudiniParameterFloat>(Node, FName(TEXT("ParmFloat_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::Color: InOutParm = NewObject<UHoudiniParameterColor>(Node, FName(TEXT("ParmColor_") + ParmName), RF_Public | RF_Transactional); break;

		// String types
		case EHoudiniParameterType::String: InOutParm = NewObject<UHoudiniParameterString>(Node, FName(TEXT("ParmString_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::StringChoice: InOutParm = NewObject<UHoudiniParameterStringChoice>(Node, FName(TEXT("ParmStringChoice_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::Asset: InOutParm = NewObject<UHoudiniParameterAsset>(Node, FName(TEXT("ParmAsset_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::AssetChoice: InOutParm = NewObject<UHoudiniParameterAssetChoice>(Node, FName(TEXT("ParmAssetChoice_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::Input: InOutParm = NewObject<UHoudiniParameterInput>(Node, FName(TEXT("ParmInput_") + ParmName), RF_Public | RF_Transactional); break;

		case EHoudiniParameterType::Label: InOutParm = NewObject<UHoudiniParameterLabel>(Node, FName(TEXT("ParmLabel_") + ParmName), RF_Public | RF_Transactional); break;

		// Extras
		case EHoudiniParameterType::MultiParm: InOutParm = NewObject<UHoudiniMultiParameter>(Node, FName(TEXT("MultiParm_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::FloatRamp: InOutParm = NewObject<UHoudiniParameterFloatRamp>(Node, FName(TEXT("ParmFloatRamp_") + ParmName), RF_Public | RF_Transactional); break;
		case EHoudiniParameterType::ColorRamp: InOutParm = NewObject<UHoudiniParameterColorRamp>(Node, FName(TEXT("ParmColorRamp_") + ParmName), RF_Public | RF_Transactional); break;
		}

		InOutParm->Type = NewParmType;
	}

	InOutParm->Name = ParmName;  // Force update parm name, as the case may be changed

	if (bUpdateTags && !bIsAttribute && InOutParm->Type == EHoudiniParameterType::FolderList)  // TODO: check why hapi folder list with collapsible parm not support set/get value
		bUpdateTags = false;

	// Update parm info
	InOutParm->Label = ParmLabel;
	InOutParm->Help = ParmHelp;
	InOutParm->HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	// Update values and default values
	switch (InOutParm->GetType())
	{
	case EHoudiniParameterType::Separator: break;
	case EHoudiniParameterType::Folder: break;
	case EHoudiniParameterType::Button: break;

	// Int types
	case EHoudiniParameterType::Int:
	{
		if (bUpdateValues) ((UHoudiniParameterInt*)InOutParm)->UpdateValues(IntValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterInt*)InOutParm)->UpdateDefaultValues(IntValues);
	}
	break;
	case EHoudiniParameterType::IntChoice:
	{
		if (bUpdateValues) ((UHoudiniParameterIntChoice*)InOutParm)->UpdateValue(IntValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterIntChoice*)InOutParm)->UpdateDefaultValue(IntValues);
		if (InParmInfo.useMenuItemTokenAsValue)
			((UHoudiniParameterIntChoice*)InOutParm)->UpdateChoices(InParmInfo.choiceIndex, InParmInfo.choiceCount, ChoiceValues, ChoiceLabels);
		else
			((UHoudiniParameterIntChoice*)InOutParm)->UpdateChoices(InParmInfo.choiceIndex, InParmInfo.choiceCount, ChoiceLabels);
	}
	break;
	case EHoudiniParameterType::ButtonStrip:
	{
		if (bUpdateValues) ((UHoudiniParameterButtonStrip*)InOutParm)->UpdateValue(IntValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterButtonStrip*)InOutParm)->UpdateDefaultValue(IntValues);
		((UHoudiniParameterButtonStrip*)InOutParm)->UpdateChoices(InParmInfo.choiceIndex, InParmInfo.choiceCount, ChoiceLabels);
	}
	break;
	case EHoudiniParameterType::Toggle:
	{
		if (bUpdateValues) ((UHoudiniParameterToggle*)InOutParm)->UpdateValue(IntValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterToggle*)InOutParm)->UpdateDefaultValue(IntValues);
	}
	break;
	case EHoudiniParameterType::FolderList:
	{
		if (bUpdateValues && InParmInfo.type == HAPI_PARMTYPE_FOLDERLIST_RADIO)
			((UHoudiniParameterFolderList*)InOutParm)->UpdateValue(IntValues);
	}
	break;
	
	// Float types
	case EHoudiniParameterType::Float:
	{
		if (bUpdateValues) ((UHoudiniParameterFloat*)InOutParm)->UpdateValues(FloatValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterFloat*)InOutParm)->UpdateDefaultValues(FloatValues);
	}
	break;
	case EHoudiniParameterType::Color:
	{
		if (bUpdateValues) ((UHoudiniParameterColor*)InOutParm)->UpdateValues(FloatValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterColor*)InOutParm)->UpdateDefaultValues(FloatValues);
	}
	break;

	// String types
	case EHoudiniParameterType::String:
	{
		if (bUpdateValues) ((UHoudiniParameterString*)InOutParm)->UpdateValue(StringValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterString*)InOutParm)->UpdateDefaultValue(StringValues);
	}
	break;
	case EHoudiniParameterType::StringChoice:
	{
		if (bUpdateValues) ((UHoudiniParameterStringChoice*)InOutParm)->UpdateValue(StringValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterStringChoice*)InOutParm)->UpdateDefaultValue(StringValues);
		((UHoudiniParameterStringChoice*)InOutParm)->UpdateChoices(InParmInfo.choiceIndex, InParmInfo.choiceCount, ChoiceValues, ChoiceLabels);
	}
	break;
	case EHoudiniParameterType::Asset:
	{
		if (bUpdateTags) ((UHoudiniParameterAsset*)InOutParm)->SetShouldImportInfo(bImportInfo);
		if (bUpdateValues) ((UHoudiniParameterAsset*)InOutParm)->UpdateValue(StringValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterAsset*)InOutParm)->UpdateDefaultValue(StringValues);
	}
	break;
	case EHoudiniParameterType::AssetChoice:
	{
		if (bUpdateValues) ((UHoudiniParameterAssetChoice*)InOutParm)->UpdateValue(StringValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterAssetChoice*)InOutParm)->UpdateDefaultValue(StringValues);
		((UHoudiniParameterAssetChoice*)InOutParm)->UpdateChoices(InParmInfo.choiceIndex, InParmInfo.choiceCount, ChoiceValues);
	}
	break;
	case EHoudiniParameterType::Input:
		if (bUpdateValues || bUpdateDefaultValues) ((UHoudiniParameterInput*)InOutParm)->UpdateValue(StringValues);
		break;
	case EHoudiniParameterType::Label:
		if (bUpdateValues || bUpdateDefaultValues) ((UHoudiniParameterLabel*)InOutParm)->UpdateValue(StringValues);
		break;

	// Extras
	case EHoudiniParameterType::MultiParm:
	{
		if (bUpdateValues) ((UHoudiniMultiParameter*)InOutParm)->UpdateInstanceCount(InParmInfo.instanceCount);
		if (bUpdateDefaultValues && PrevParm != InOutParm)  // Default count of MultiParms has been update via UHoudiniMultiParameter::HapiSync
			((UHoudiniMultiParameter*)InOutParm)->UpdateDefaultInstanceCount(InParmInfo.instanceCount);
	}
	break;
	case EHoudiniParameterType::FloatRamp:
	case EHoudiniParameterType::ColorRamp:
	{
		if (bUpdateValues) ((UHoudiniParameterRamp*)InOutParm)->UpdateValue(IntValues, FloatValues);
		if (bUpdateDefaultValues) ((UHoudiniParameterRamp*)InOutParm)->UpdateDefaultValue(IntValues, FloatValues);
	}
	break;
	}

	// Try reuse the previous values of legacy parm if possible
	if (!bIsAttribute && bBeforeCook && PrevParm && (PrevParm != InOutParm))
		InOutParm->ReuseValuesFromLegacyParameter(PrevParm);

	return true;
}

bool UHoudiniParameter::HapiGetUnit(const int32& NodeId, const int32& ParmId, FString& Unit)
{
	Unit.Empty();

	HAPI_StringHandle UnitSH;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(), NodeId, ParmId, "units", &UnitSH))
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(UnitSH, Unit));
		
		// Per second and per hour are the only "per" unit that unreal recognize
		Unit.ReplaceInline(TEXT("s-1"), TEXT("/s"));
		Unit.ReplaceInline(TEXT("h-1"), TEXT("/h"));

		// Houdini likes to add '1' on all the unit, so we'll remove all of them
		// except the '-1' that still needs to remain.
		Unit.ReplaceInline(TEXT("-1"), TEXT("--"));
		Unit.ReplaceInline(TEXT("1"), TEXT(""));
		Unit.ReplaceInline(TEXT("--"), TEXT("-1"));
	}

	return true;
}

bool UHoudiniParameter::UploadGeneric(FHoudiniParameterPresetHelper& PresetHelper,
	const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	if (LoadGeneric(Parms, bCompareWithDefault))
	{
		if (!Upload(PresetHelper))
			return false;
	}
	ResetModification();
	return true;
}

bool UHoudiniParameter::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	Id = InParmInfo.id;
	ParentId = InParmInfo.parentId;
	bVisible = !InParmInfo.invisible;
	bEnabled = !InParmInfo.disabled;
	Size = InParmInfo.size;
	MultiParmInstanceIndex = InParmInfo.isChildOfMultiParm ? InParmInfo.instanceNum : -1;

	ValueIndex = -1;
	switch (Type)
	{
	case EHoudiniParameterType::Separator: break;
	case EHoudiniParameterType::Folder: break;
	case EHoudiniParameterType::Button:
		ValueIndex = InParmInfo.intValuesIndex;
		break;

	// Int types
	case EHoudiniParameterType::Int:
	case EHoudiniParameterType::IntChoice:
	case EHoudiniParameterType::ButtonStrip:
	case EHoudiniParameterType::Toggle:
	case EHoudiniParameterType::FolderList:
		ValueIndex = InParmInfo.intValuesIndex;
		break;

	// Float types
	case EHoudiniParameterType::Float:
	case EHoudiniParameterType::Color:
		ValueIndex = InParmInfo.floatValuesIndex;
		break;

	// String types
	case EHoudiniParameterType::String:
	case EHoudiniParameterType::StringChoice:
	case EHoudiniParameterType::Asset:
	case EHoudiniParameterType::AssetChoice:
	case EHoudiniParameterType::Input:

	case EHoudiniParameterType::Label:
		ValueIndex = InParmInfo.stringValuesIndex;
		break;

	// Extras
	case EHoudiniParameterType::MultiParm:
	case EHoudiniParameterType::FloatRamp:
	case EHoudiniParameterType::ColorRamp:
		ValueIndex = InParmInfo.intValuesIndex;
		break;
	}

	return true;
}

bool UHoudiniParameter::CopySettingsFrom(const UHoudiniParameter* SrcParm)
{
	if (SrcParm->GetClass() != GetClass())
		return false;

	static const FName ValuePropName("Value");
	static const FName ValuesPropName("Values");

	for (TFieldIterator<FProperty> PropIter(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIter; ++PropIter)
	{
		if (const FProperty* Prop = *PropIter)
		{
			if ((Prop->GetFName() == ValuePropName) || (Prop->GetFName() == ValuesPropName))  // Value will update by attribute
				continue;

			const int32 PropOffset = Prop->GetOffset_ForInternal();
			Prop->CopyCompleteValue(((uint8*)this) + PropOffset, ((const uint8*)SrcParm) + PropOffset);
		}
	}
	return true;
}

const int32& UHoudiniParameter::GetNodeId() const
{
	// Since parameter's outer must be a AHoudiniNode, we could directly cast outer to AHoudiniNode
	return ((const AHoudiniNode*)GetOuter())->GetNodeId();
}

void UHoudiniParameter::SetModification(const EHoudiniParameterModification& InModification)
{
	Modification = InModification;

	if (Modification != EHoudiniParameterModification::None)
	{
		((AHoudiniNode*)GetOuter())->TriggerCookByParameter(this);
	}
}

EHoudiniParameterType UHoudiniParameter::GetParameterTypeFromInfo(const int32& NodeId, const HAPI_ParmInfo& InParmInfo)
{
	switch (InParmInfo.type)
	{
	case HAPI_PARMTYPE_INT: return (InParmInfo.scriptType == HAPI_PrmScriptType::HAPI_PRM_SCRIPT_TYPE_BUTTONSTRIP) ?
		EHoudiniParameterType::ButtonStrip : (InParmInfo.choiceCount ? EHoudiniParameterType::IntChoice : EHoudiniParameterType::Int);
	case HAPI_PARMTYPE_MULTIPARMLIST:
	{
		if (InParmInfo.rampType == HAPI_RAMPTYPE_FLOAT)
			return EHoudiniParameterType::FloatRamp;
		else if (InParmInfo.rampType == HAPI_RAMPTYPE_COLOR)
			return EHoudiniParameterType::ColorRamp;

		return EHoudiniParameterType::MultiParm;
	}
	case HAPI_PARMTYPE_TOGGLE: return EHoudiniParameterType::Toggle;
	case HAPI_PARMTYPE_BUTTON: return EHoudiniParameterType::Button;
			/// }@

			/// @{
	case HAPI_PARMTYPE_FLOAT: return EHoudiniParameterType::Float;
	case HAPI_PARMTYPE_COLOR: return EHoudiniParameterType::Color;
			/// @}

			/// @{
	case HAPI_PARMTYPE_STRING: return InParmInfo.choiceCount ? EHoudiniParameterType::StringChoice : EHoudiniParameterType::String;  // Judge is asset parm later!
	case HAPI_PARMTYPE_PATH_FILE: return EHoudiniParameterType::String;
	case HAPI_PARMTYPE_PATH_FILE_GEO: return EHoudiniParameterType::String;
	case HAPI_PARMTYPE_PATH_FILE_IMAGE: return EHoudiniParameterType::String;
			/// @}

	case HAPI_PARMTYPE_NODE: return EHoudiniParameterType::Input;

			/// @{
	case HAPI_PARMTYPE_FOLDERLIST: return EHoudiniParameterType::FolderList;
	case HAPI_PARMTYPE_FOLDERLIST_RADIO: return EHoudiniParameterType::FolderList;
			/// @}

			/// @{
	case HAPI_PARMTYPE_FOLDER: return EHoudiniParameterType::Folder;
	case HAPI_PARMTYPE_LABEL: return EHoudiniParameterType::Label;
	case HAPI_PARMTYPE_SEPARATOR: return EHoudiniParameterType::Separator;
	case HAPI_PARMTYPE_PATH_FILE_DIR: return EHoudiniParameterType::String;
	}

	check(false);  // Invalid type means something get wrong in houdini, so force crash
	return EHoudiniParameterType::Invalid;
}


// Button
bool UHoudiniParameterButton::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::SetValue)
	{
		static const int ButtonValueOn = 1;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			&ButtonValueOn, ValueIndex, 1));
	}
	
	return true;
}

bool UHoudiniParameterButton::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	if (const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name))
	{
		if ((GenericParm->Type == EHoudiniGenericParameterType::Int) && (GenericParm->NumericValues.X >= 1.0f))
		{
			Press();
			return true;
		}
	}
		
	return false;
}


// Int
FString UHoudiniParameterInt::GetValueString() const
{
	if (Values.Num() == 1)
		return FString::FromInt(Values[0]);

	FString ValueStr;
	for (int32 Idx = 0; Idx < Values.Num() - 1; ++Idx)
		ValueStr += FString::FromInt(Values[Idx]) + TEXT(",");
	return ValueStr + FString::FromInt(Values[Values.Num() - 1]);
}

bool UHoudiniParameterInt::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	Min = InParmInfo.hasMin ? int32(InParmInfo.min) : TNumericLimits<int32>::Lowest();
	Max = InParmInfo.hasMax ? int32(InParmInfo.max) : TNumericLimits<int32>::Max();
	UIMin = InParmInfo.hasUIMin ? int32(InParmInfo.UIMin) : TNumericLimits<int32>::Lowest();
	UIMax = InParmInfo.hasUIMax ? int32(InParmInfo.UIMax) : TNumericLimits<int32>::Max();
	
	Values.SetNum(Size);
	DefaultValues.SetNum(Size);

	if (bUpdateTags)
		HOUDINI_FAIL_RETURN(UHoudiniParameter::HapiGetUnit(GetNodeId(), GetId(), Unit))

	return true;
}

bool UHoudiniParameterInt::CopySettingsFrom(const UHoudiniParameter* SrcParm)
{
	if (UHoudiniParameter::CopySettingsFrom(SrcParm))
	{
		Values.SetNum(Cast<UHoudiniParameterInt>(SrcParm)->Values.Num());
		return true;
	}
	return false;
}

bool UHoudiniParameterInt::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	std::string ValuesStr;
	for (int32 TupleIdx = 0; TupleIdx < Values.Num(); ++TupleIdx)
	{
		ValuesStr += TCHAR_TO_UTF8(*FString::FromInt(Values[TupleIdx]));
		if (TupleIdx < (Values.Num() - 1))
			ValuesStr += HAPI_PRESET_VALUE_DELIM;
	}
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), ValuesStr);
	return true;
}

bool UHoudiniParameterInt::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			Values.GetData(), ValueIndex, Values.Num()))

	return true;
}

void UHoudiniParameterInt::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Int;
	const int32 TupleSize = FMath::Clamp(Size, 0, 4);
	for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
		GenericParm.NumericValues[TupleIdx] = float(Values[TupleIdx]);
}

bool UHoudiniParameterInt::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	if (const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name))
	{
		if (GenericParm->Type != EHoudiniGenericParameterType::Int &&
			GenericParm->Type != EHoudiniGenericParameterType::Float)
			return false;

		const TArray<int32> OrigValues = bCompareWithDefault ? DefaultValues : Values;
		const int32 TupleSize = FMath::Clamp(FMath::Min(GenericParm->Size, Size), 0, 4);
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			Values[TupleIdx] = int32(GenericParm->NumericValues[TupleIdx]);

		if (OrigValues != Values)
		{
			Modification = EHoudiniParameterModification::SetValue;
			return true;
		}
	}
	else
	{
		const TArray<int32> OrigValues = bCompareWithDefault ? DefaultValues : Values;
		bool bHasValueChanged = false;

		static const char* TupleSuffixes = "xyzw";
		const int32 TupleSize = Size <= 4 ? Size : 4;
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
		{
			FString TupleSuffix;
			TupleSuffix.AppendChar(TupleSuffixes[TupleIdx]);
			if (const FHoudiniGenericParameter* GenericParmTuple = Parms.Find(*(Name + TupleSuffix)))  // For the situation like "numx", "numy", "numz"
			{
				if ((GenericParmTuple->Type != EHoudiniGenericParameterType::Int) &&
					(GenericParmTuple->Type != EHoudiniGenericParameterType::Float))
					break;

				const int32 NewValue = int32(GenericParmTuple->NumericValues.X);
				Values[TupleIdx] = NewValue;
				if (OrigValues[TupleIdx] != NewValue)
					bHasValueChanged = true;
			}
			else
				break;
		}

		if (bHasValueChanged)
		{
			Modification = EHoudiniParameterModification::SetValue;
			return true;
		}
	}
	
	return false;
}


// IntChoice
bool UHoudiniParameterIntChoice::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*FString::FromInt(Value))));
	return true;
}

bool UHoudiniParameterIntChoice::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			&Value, ValueIndex, 1))

	return true;
}

void UHoudiniParameterIntChoice::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Int;
	GenericParm.NumericValues.X = float(Value);
}

bool UHoudiniParameterIntChoice::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if ((GenericParm->Type != EHoudiniGenericParameterType::Int) &&
		(GenericParm->Type != EHoudiniGenericParameterType::Float))
		return false;

	const int32 OrigValue = bCompareWithDefault ? DefaultValue : Value;
	Value = int32(GenericParm->NumericValues.X);

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


void UHoudiniParameterIntChoice::UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> AllChoiceValues, TConstArrayView<FString> AllChoiceLabels)
{
	Options.Empty();

	ChoiceValues.SetNum(ChoiceCount);
	ChoiceLabels.SetNum(ChoiceCount);
	for (int32 LocalChoiceIndex = 0; LocalChoiceIndex < ChoiceCount; ++LocalChoiceIndex)
	{
		ChoiceValues[LocalChoiceIndex] = FCString::Atoi(*AllChoiceValues[ChoiceIndex + LocalChoiceIndex]);
		ChoiceLabels[LocalChoiceIndex] = AllChoiceLabels[ChoiceIndex + LocalChoiceIndex];
	}
}

void UHoudiniParameterIntChoice::UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> AllChoiceLabels)
{
	Options.Empty();

	ChoiceValues.SetNum(ChoiceCount);
	ChoiceLabels.SetNum(ChoiceCount);
	for (int32 LocalChoiceIndex = 0; LocalChoiceIndex < ChoiceCount; ++LocalChoiceIndex)
	{
		ChoiceValues[LocalChoiceIndex] = LocalChoiceIndex;
		ChoiceLabels[LocalChoiceIndex] = AllChoiceLabels[ChoiceIndex + LocalChoiceIndex];
	}
}

const TArray<TSharedPtr<FString>>* UHoudiniParameterIntChoice::GetOptionsSource()
{
	Options.Empty();
	for (int32 ChoiceIdx = 0; ChoiceIdx < ChoiceLabels.Num(); ++ChoiceIdx)
		Options.Add(MakeShared<FString>(ChoiceLabels[ChoiceIdx]));
	return &Options;
}

TSharedPtr<FString> UHoudiniParameterIntChoice::GetChosen() const
{
	const int32 ChoiceIdx = (ChoiceValues.IsValidIndex(Value) && (ChoiceValues[Value] == Value)) ?
		Value : ChoiceValues.IndexOfByKey(Value);

	return Options.IsValidIndex(ChoiceIdx) ? Options[ChoiceIdx] : nullptr;
}

void UHoudiniParameterIntChoice::SetChosen(const TSharedPtr<FString>& Chosen)
{
	const int32 ChoiceIdx = Options.IndexOfByKey(Chosen);
	const int32 NewValue = ChoiceValues.IsValidIndex(ChoiceIdx) ? ChoiceValues[ChoiceIdx] : -1;
	if (Value == NewValue)
		return;

	Value = NewValue;
	SetModification(EHoudiniParameterModification::SetValue);
}


// ButtonStrip
bool UHoudiniParameterButtonStrip::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	bCanMultiSelect = InParmInfo.choiceListType == HAPI_CHOICELISTTYPE_TOGGLE;

	return true;
}

bool UHoudiniParameterButtonStrip::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*FString::FromInt(Value))));
	return true;
}

bool UHoudiniParameterButtonStrip::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			&Value, ValueIndex, 1))

	return true;
}

void UHoudiniParameterButtonStrip::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Int;
	GenericParm.NumericValues.X = float(Value);
}

bool UHoudiniParameterButtonStrip::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if ((GenericParm->Type != EHoudiniGenericParameterType::Int) &&
		(GenericParm->Type != EHoudiniGenericParameterType::Float))
		return false;

	const int32 OrigValue = bCompareWithDefault ? DefaultValue : Value;
	Value = int32(GenericParm->NumericValues.X);

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


// Toggle
bool UHoudiniParameterToggle::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*FString::FromInt(int32(Value)))));
	return true;
}

bool UHoudiniParameterToggle::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
	{
		const int IntValue = int(Value);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			&IntValue, ValueIndex, 1));
	}

	return true;
}

void UHoudiniParameterToggle::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Int;
	GenericParm.NumericValues.X = float(Value);
}

bool UHoudiniParameterToggle::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if (GenericParm->Type != EHoudiniGenericParameterType::Int &&
		GenericParm->Type != EHoudiniGenericParameterType::Float)
		return false;

	const bool OrigValue = bCompareWithDefault ? DefaultValue : Value;
	Value = bool(int32(GenericParm->NumericValues.X));

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


// FolderList
bool UHoudiniParameterFolderList::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	if (bUpdateTags)
	{
		HAPI_StringHandle TagValueSH;
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(),
			GetNodeId(), InParmInfo.id, HAPI_PARM_TAG_AS_TOGGLE, &TagValueSH))
		{
			FString TagValue;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, TagValue));
			FolderType = (TagValue == TEXT("1") || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)) ?
				EHoudiniFolderType::Toggle : EHoudiniFolderType::Invalid;
		}
	}

	return true;
}

bool UHoudiniParameterFolderList::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	if (HasValue())
		PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*FString::FromInt(Value))));
	return true;
}

bool UHoudiniParameterFolderList::HapiUpload()
{
	if (HasValue())
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			&Value, ValueIndex, 1))
	
	return true;
}

void UHoudiniParameterFolderList::SetFolderType(const HAPI_ParmInfo& FolderParmInfo)
{
	switch (FolderParmInfo.scriptType)
	{
	case HAPI_PRM_SCRIPT_TYPE_GROUPRADIO: FolderType = EHoudiniFolderType::Radio; return;
	case HAPI_PRM_SCRIPT_TYPE_GROUPCOLLAPSIBLE:
		if (FolderType != EHoudiniFolderType::Toggle) FolderType = EHoudiniFolderType::Collapsible; return;
	case HAPI_PRM_SCRIPT_TYPE_GROUPSIMPLE: FolderType = EHoudiniFolderType::Simple; return;
	default: FolderType = EHoudiniFolderType::Tabs; return;
	}
}

void UHoudiniParameterFolderList::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	if (HasValue())
	{
		FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
		GenericParm.Size = Size;
		GenericParm.Type = EHoudiniGenericParameterType::Int;
		GenericParm.NumericValues.X = float(Value);
	}
}

bool UHoudiniParameterFolderList::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	if (!HasValue())
		return false;

	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if (GenericParm->Type != EHoudiniGenericParameterType::Int &&
		GenericParm->Type != EHoudiniGenericParameterType::Float)
		return false;

	const int32 OrigValue = bCompareWithDefault ? 0 : Value;
	Value = FMath::Clamp(int32(GenericParm->NumericValues.X), 0, Size);

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}

bool UHoudiniParameterFolderList::HapiGetEditOptions(const int32& NodeId, const HAPI_ParmInfo& ParmInfo, EHoudiniEditOptions& OutEditOptions, FString& OutIdentifierName)
{
	OutEditOptions = EHoudiniEditOptions::None;

	OutEditOptions |= ParmInfo.invisible ? EHoudiniEditOptions::Hide : EHoudiniEditOptions::Show;

	//if (ParmInfo.tagCount <= 0)  // HAPI Bug: Do NOT use tagCount, as folder list's tagCount always return false
	//	return true;

	HAPI_StringHandle TagValueSH;
	HAPI_Result Result = FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(),
		NodeId, ParmInfo.id, HAPI_PARM_TAG_EDITABLE, &TagValueSH);
	if (HAPI_SESSION_INVALID_RESULT(Result))
		return false;

	if (Result == HAPI_RESULT_SUCCESS)
	{
		FString TagValue;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, TagValue));

		OutEditOptions |= (TagValue == TEXT("1") || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)) ?
			EHoudiniEditOptions::Enabled : EHoudiniEditOptions::Disabled;
	}


	Result = FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(),
		NodeId, ParmInfo.id, HAPI_PARM_TAG_ALLOW_ASSET_DROP, &TagValueSH);
	if (HAPI_SESSION_INVALID_RESULT(Result))
		return false;

	if (Result == HAPI_RESULT_SUCCESS)
	{
		FString TagValue;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, TagValue));

		if (TagValue == TEXT("1") || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
			OutEditOptions |= EHoudiniEditOptions::AllowAssetDrop;
	}


	Result = FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(),
		NodeId, ParmInfo.id, HAPI_PARM_TAG_COOK_ON_SELECT, &TagValueSH);
	if (HAPI_SESSION_INVALID_RESULT(Result))
		return false;

	if (Result == HAPI_RESULT_SUCCESS)
	{
		FString TagValue;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, TagValue));

		if (TagValue == TEXT("1") || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
			OutEditOptions |= EHoudiniEditOptions::CookOnSelect;
	}


	Result = FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(),
		NodeId, ParmInfo.id, HAPI_PARM_TAG_IDENTIFIER_NAME, &TagValueSH);
	if (HAPI_SESSION_INVALID_RESULT(Result))
		return false;

	if (Result == HAPI_RESULT_SUCCESS)
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, OutIdentifierName))

	return true;
}


// Float
FString UHoudiniParameterFloat::GetValueString() const
{
	if (Values.Num() == 1)
		return FString::SanitizeFloat(Values[0]);

	FString ValueStr;
	for (int32 Idx = 0; Idx < Values.Num() - 1; ++Idx)
		ValueStr += FString::SanitizeFloat(Values[Idx]) + TEXT(",");
	return ValueStr + FString::SanitizeFloat(Values[Values.Num() - 1]);
}

bool UHoudiniParameterFloat::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	Min = InParmInfo.hasMin ? InParmInfo.min : TNumericLimits<float>::Lowest();
	Max = InParmInfo.hasMax ? InParmInfo.max : TNumericLimits<float>::Max();
	UIMin = InParmInfo.hasUIMin ? InParmInfo.UIMin : TNumericLimits<float>::Lowest();
	UIMax = InParmInfo.hasUIMax ? InParmInfo.UIMax : TNumericLimits<float>::Max();

	Values.SetNum(Size);
	DefaultValues.SetNum(Size);

	if (bUpdateTags)
		HOUDINI_FAIL_RETURN(UHoudiniParameter::HapiGetUnit(GetNodeId(), GetId(), Unit))

	return true;
}

bool UHoudiniParameterFloat::CopySettingsFrom(const UHoudiniParameter* SrcParm)
{
	if (UHoudiniParameter::CopySettingsFrom(SrcParm))
	{
		Values.SetNum(Cast<UHoudiniParameterFloat>(SrcParm)->Values.Num());
		return true;
	}
	return false;
}

bool UHoudiniParameterFloat::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	std::string ValuesStr;
	for (int32 TupleIdx = 0; TupleIdx < Values.Num(); ++TupleIdx)
	{
		ValuesStr += TCHAR_TO_UTF8(*FString::SanitizeFloat(Values[TupleIdx]));
		if (TupleIdx < Values.Num() - 1)
			ValuesStr += HAPI_PRESET_VALUE_DELIM;
	}
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), ValuesStr);
	return true;
}

bool UHoudiniParameterFloat::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			Values.GetData(), ValueIndex, Size))

	return true;
}

void UHoudiniParameterFloat::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Float;
	const int32 TupleSize = FMath::Clamp(Size, 0, 4);
	for (int32 TupleIdx = 0; TupleIdx < FMath::Clamp(TupleSize, 0, 4); ++TupleIdx)
		GenericParm.NumericValues[TupleIdx] = Values[TupleIdx];
}

bool UHoudiniParameterFloat::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	if (const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name))
	{
		if (GenericParm->Type != EHoudiniGenericParameterType::Int &&
			GenericParm->Type != EHoudiniGenericParameterType::Float)
			return false;

		const TArray<float> OrigValues = bCompareWithDefault ? DefaultValues : Values;
		const int32 TupleSize = FMath::Clamp(FMath::Min(GenericParm->Size, Size), 0, 4);
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			Values[TupleIdx] = GenericParm->NumericValues[TupleIdx];

		if (OrigValues != Values)
		{
			Modification = EHoudiniParameterModification::SetValue;
			return true;
		}
	}
	else
	{
		const TArray<float> OrigValues = bCompareWithDefault ? DefaultValues : Values;
		bool bHasValueChanged = false;

		static const char* TupleSuffixes = "xyzw";
		const int32 TupleSize = Size <= 4 ? Size : 4;
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
		{
			FString TupleSuffix;
			TupleSuffix.AppendChar(TupleSuffixes[TupleIdx]);
			if (const FHoudiniGenericParameter* GenericParmTuple = Parms.Find(*(Name + TupleSuffix)))  // For the situation like "scalex", "scaley", "scalez"
			{
				if (GenericParmTuple->Type != EHoudiniGenericParameterType::Int &&
					GenericParmTuple->Type != EHoudiniGenericParameterType::Float)
					break;

				const float& NewValue = GenericParmTuple->NumericValues.X;
				Values[TupleIdx] = NewValue;
				if (OrigValues[TupleIdx] != NewValue)
					bHasValueChanged = true;
			}
			else
				break;
		}

		if (bHasValueChanged)
		{
			Modification = EHoudiniParameterModification::SetValue;
			return true;
		}
	}

	return false;
}


// Color
bool UHoudiniParameterColor::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	std::string ValuesStr =
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.R))) + HAPI_PRESET_VALUE_DELIM +
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.G))) + HAPI_PRESET_VALUE_DELIM +
		std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.B)));
	if (Size >= 4)
		ValuesStr += HAPI_PRESET_VALUE_DELIM + std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value.A)));
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), ValuesStr);
	return true;
}

bool UHoudiniParameterColor::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
	{
		float Values[4] = { Value.R, Value.G, Value.B, Value.A };
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmFloatValues(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			Values, ValueIndex, Size));
	}

	return true;
}

void UHoudiniParameterColor::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Float;
	GenericParm.NumericValues.X = Value.R;
	GenericParm.NumericValues.Y = Value.G;
	GenericParm.NumericValues.Z = Value.B;
	GenericParm.NumericValues.W = (Size >= 4) ? Value.A : 1.0f;
}

bool UHoudiniParameterColor::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if (GenericParm->Type != EHoudiniGenericParameterType::Int &&
		GenericParm->Type != EHoudiniGenericParameterType::Float)
		return false;

	const FLinearColor OrigValue = bCompareWithDefault ? DefaultValue : Value;
	Value.R = GenericParm->NumericValues.X;
	Value.G = GenericParm->NumericValues.Y;
	Value.B = GenericParm->NumericValues.Z;
	if (Size >= 4) Value.A = GenericParm->NumericValues.W;

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


// String
void UHoudiniParameterString::ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm)
{
	if ((LegacyParm->GetType() == EHoudiniParameterType::StringChoice) ||
		(LegacyParm->GetType() == EHoudiniParameterType::Asset) || (LegacyParm->GetType() == EHoudiniParameterType::AssetChoice))
	{
		Value = LegacyParm->GetValueString();
		SetModification(LegacyParm);
	}
}

bool UHoudiniParameterString::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	switch (InParmInfo.type)
	{
	case HAPI_PARMTYPE_PATH_FILE: PathType = EHoudiniPathParameterType::File; break;
	case HAPI_PARMTYPE_PATH_FILE_GEO: PathType = EHoudiniPathParameterType::Geo; break;
	case HAPI_PARMTYPE_PATH_FILE_IMAGE: PathType = EHoudiniPathParameterType::Image; break;
	case HAPI_PARMTYPE_PATH_FILE_DIR: PathType = EHoudiniPathParameterType::Directory; break;
	default: PathType = EHoudiniPathParameterType::Invalid; break;
	}

	return true;
}

bool UHoudiniParameterString::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*(Value.Contains(TEXT(" ")) ? (TEXT("\"") + Value + TEXT("\"")) : Value))));
	return true;
}

bool UHoudiniParameterString::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)));
	}
	else
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Value), Id, 0));
	}

	return true;
}

void UHoudiniParameterString::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::String;
	GenericParm.StringValue = Value;
}

bool UHoudiniParameterString::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	const FString OrigValue = bCompareWithDefault ? DefaultValue : Value;
	if (GenericParm->Type == EHoudiniGenericParameterType::String)
		Value = GenericParm->StringValue;
	else if (GenericParm->Type == EHoudiniGenericParameterType::Object)
		Value = FHoudiniEngineUtils::GetAssetReference(GenericParm->ObjectValue.ToSoftObjectPath());

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


// StringChoice
void UHoudiniParameterStringChoice::ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm)
{
	if ((LegacyParm->GetType() == EHoudiniParameterType::String) ||
		(LegacyParm->GetType() == EHoudiniParameterType::Asset) || (LegacyParm->GetType() == EHoudiniParameterType::AssetChoice))
	{
		Value = LegacyParm->GetValueString();
		SetModification(LegacyParm);
	}
}

bool UHoudiniParameterStringChoice::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	ChoiceListType = EHoudiniChoiceListType::Normal;
	switch(InParmInfo.choiceListType)
	{
	case HAPI_CHOICELISTTYPE_NONE:	
	case HAPI_CHOICELISTTYPE_NORMAL: // Menu Only, Single Selection
	case HAPI_CHOICELISTTYPE_MINI:  // Mini Menu Only, Single Selection
		ChoiceListType = EHoudiniChoiceListType::Normal;
		break;
	case HAPI_CHOICELISTTYPE_REPLACE:  ChoiceListType = EHoudiniChoiceListType::Replace; break;  // Field + Single Selection Menu
	case HAPI_CHOICELISTTYPE_TOGGLE: ChoiceListType = EHoudiniChoiceListType::Toggle; break;  // Field + Multiple Selection Menu
	}

	return true;
}

bool UHoudiniParameterStringChoice::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*(Value.Contains(TEXT(" ")) ? (TEXT("\"") + Value + TEXT("\"")) : Value))));
	return true;
}

bool UHoudiniParameterStringChoice::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)));
	}
	else
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Value), Id, 0));
	}

	return true;
}

void UHoudiniParameterStringChoice::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::String;
	GenericParm.StringValue = Value;
}

bool UHoudiniParameterStringChoice::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	const FString OrigValue = bCompareWithDefault ? DefaultValue : Value;
	if (GenericParm->Type == EHoudiniGenericParameterType::String)
		Value = GenericParm->StringValue;
	else if (GenericParm->Type == EHoudiniGenericParameterType::Object)
		Value = FHoudiniEngineUtils::GetAssetReference(GenericParm->ObjectValue.ToSoftObjectPath());

	if (OrigValue != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


void UHoudiniParameterStringChoice::UpdateChoices(const int32& ChoiceIndex, const int32& ChoiceCount, TConstArrayView<FString> AllChoiceValues, TConstArrayView<FString> AllChoiceLabels)
{
	ChoiceValues.SetNum(ChoiceCount);
	ChoiceLabels.SetNum(ChoiceCount);
	for (int32 LocalChoiceIndex = 0; LocalChoiceIndex < ChoiceCount; ++LocalChoiceIndex)
	{
		ChoiceValues[LocalChoiceIndex] = AllChoiceValues[ChoiceIndex + LocalChoiceIndex];
		ChoiceLabels[LocalChoiceIndex] = AllChoiceLabels[ChoiceIndex + LocalChoiceIndex];
	}
}

const TArray<TSharedPtr<FString>>* UHoudiniParameterStringChoice::GetOptionsSource()
{
	Options.Empty();
	for (int32 ChoiceIdx = 0; ChoiceIdx < ChoiceLabels.Num(); ++ChoiceIdx)
		Options.Add(MakeShared<FString>(ChoiceLabels[ChoiceIdx]));
	return &Options;
}

TSharedPtr<FString> UHoudiniParameterStringChoice::GetChosen() const
{
	const int32 ChoiceIdx = ChoiceValues.IndexOfByKey(Value);
	return Options.IsValidIndex(ChoiceIdx) ? Options[ChoiceIdx] : nullptr;
}

void UHoudiniParameterStringChoice::SetChosen(const TSharedPtr<FString>& Chosen)
{
	const int32 ChoiceIdx = Options.IndexOfByKey(Chosen);

	if (ChoiceListType == EHoudiniChoiceListType::Toggle)
	{
		if (!ChoiceValues.IsValidIndex(ChoiceIdx))
			return;

		const FString& ChosenValue = ChoiceValues[ChoiceIdx];
		if (Value.IsEmpty())
			Value = ChosenValue;
		else
		{
			TArray<FString> Values;
			Value.ParseIntoArray(Values, TEXT(" "));
			const int32 ChosenValueIdx = Values.Find(ChosenValue);
			if (Values.IsValidIndex(ChosenValueIdx))  // If found, then we should remove this chosen from value
			{
				Value.Empty();
				Values.RemoveAt(ChosenValueIdx);
				for (int32 ValueIdx = 0; ValueIdx < Values.Num(); ++ValueIdx)
					Value += ((ValueIdx == Values.Num() - 1) ? Values[ValueIdx] : (Values[ValueIdx] + TEXT(" ")));
			}
			else
				Value += TEXT(" ") + ChosenValue;
		}
	}
	else
	{
		if (ChoiceValues.IsValidIndex(ChoiceIdx))
			Value = ChoiceValues[ChoiceIdx];
		else  // Should never happen
			Value.Empty();
	}

	SetModification(EHoudiniParameterModification::SetValue);
}


// Asset
void UHoudiniParameterAsset::ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm)
{
	if (LegacyParm->GetType() == EHoudiniParameterType::String || LegacyParm->GetType() == EHoudiniParameterType::StringChoice)
	{
		if (!IS_ASSET_PATH_INVALID(LegacyParm->GetValueString()))
		{
			Value = LegacyParm->GetValueString();
			SetModification(LegacyParm);
		}
	}
	else if (LegacyParm->GetType() == EHoudiniParameterType::AssetChoice)
	{
		Value = Cast<UHoudiniParameterAssetChoice>(LegacyParm)->GetAssetPath();
		SetModification(LegacyParm);
	}
}

bool UHoudiniParameterAsset::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);
	
	HAPI_StringHandle TagValueSH;

	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
		Id, HAPI_PARM_TAG_UNREAL_REF_FILTER, &TagValueSH))
	{
		FString FilterPattern;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, FilterPattern));

		FHoudiniEngineUtils::ParseFilterPattern(FilterPattern, Filters, InvertedFilters);
	}
	else
	{
		Filters.Empty();
		InvertedFilters.Empty();
	}

	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
		Id, HAPI_PARM_TAG_UNREAL_REF_CLASS, &TagValueSH))
	{
		FString FilterPattern;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, FilterPattern));

		FHoudiniEngineUtils::ParseFilterPattern(FilterPattern, AllowClassNames, DisallowClassNames);
	}
	else
	{
		AllowClassNames.Empty();
		DisallowClassNames.Empty();
	}

	return true;
}

#define RETURN_ASSET_PATH_STRING_WITH_INFO(ASSET_PATH) if (!ASSET_PATH.IsValid())\
		return FString();\
	if (bImportInfo)\
	{\
		FString AssetRefStr;\
		UObject* Asset = ASSET_PATH.TryLoad();\
		if (IsValid(Asset))\
		{\
			AssetRefStr = FHoudiniEngineUtils::GetAssetReference(Asset);\
			FString InfoStr;\
			UHoudiniParameterInput::GetObjectInfo(Asset, InfoStr);\
			if (!InfoStr.IsEmpty())\
				AssetRefStr += TEXT(";") + InfoStr;\
		}\
		return AssetRefStr;\
	}\
	return FHoudiniEngineUtils::GetAssetReference(ASSET_PATH);

FString UHoudiniParameterAsset::GetValueString() const
{
	RETURN_ASSET_PATH_STRING_WITH_INFO(Value);
}

bool UHoudiniParameterAsset::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*GetValueString())));
	return true;
}

bool UHoudiniParameterAsset::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*GetValueString()), Id, 0))

	return true;
}

void UHoudiniParameterAsset::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::Object;
	GenericParm.ObjectValue = Value;
	if (bImportInfo)
	{
		UObject* Asset = Value.TryLoad();
		if (IsValid(Asset))
			UHoudiniParameterInput::GetObjectInfo(Asset, GenericParm.StringValue);
	}
}

FORCEINLINE static FSoftObjectPath ParseAssetPathFromString(const FString& StringValue)
{
	if (IS_ASSET_PATH_INVALID(StringValue))
		return FSoftObjectPath();

	int32 SplitIdx = 0;
	if (StringValue.FindChar(TCHAR(';'), SplitIdx))
		return StringValue.Left(SplitIdx);

	return StringValue;
}

bool UHoudiniParameterAsset::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if (GenericParm->Type != EHoudiniGenericParameterType::String &&
		GenericParm->Type != EHoudiniGenericParameterType::Object)
		return false;

	const FSoftObjectPath OrigPath = bCompareWithDefault ? DefaultValue : Value;
	Value = (GenericParm->Type == EHoudiniGenericParameterType::String) ?
		ParseAssetPathFromString(GenericParm->StringValue) : GenericParm->ObjectValue.ToSoftObjectPath();
	
	if (OrigPath != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}

void UHoudiniParameterAsset::UpdateValue(TConstArrayView<FString> AllStringValues)
{
	Value = ParseAssetPathFromString(AllStringValues[ValueIndex]);
}

void UHoudiniParameterAsset::UpdateDefaultValue(TConstArrayView<FString> AllStringValues)
{
	DefaultValue = ParseAssetPathFromString(AllStringValues[ValueIndex]);
}


// AssetChoice
void UHoudiniParameterAssetChoice::ReuseValuesFromLegacyParameter(const UHoudiniParameter* LegacyParm)
{
	if ((LegacyParm->GetType() == EHoudiniParameterType::String) || (LegacyParm->GetType() == EHoudiniParameterType::StringChoice))
	{
		if (!IS_ASSET_PATH_INVALID(LegacyParm->GetValueString()))
		{
			Value = LegacyParm->GetValueString();
			SetModification(LegacyParm);
		}
	}
	else if (LegacyParm->GetType() == EHoudiniParameterType::Asset)
	{
		Value = Cast<UHoudiniParameterAsset>(LegacyParm)->GetAssetPath();
		SetModification(LegacyParm);
	}
}

bool UHoudiniParameterAssetChoice::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	PresetHelper.Append(std::string(TCHAR_TO_UTF8(*Name)), std::string(TCHAR_TO_UTF8(*GetValueString())));
	return true;
}

bool UHoudiniParameterAssetChoice::HapiUpload()
{
	if (Modification == EHoudiniParameterModification::RevertToDefault)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)))
	else
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmStringValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*GetValueString()), Id, 0))

	return true;
}

void UHoudiniParameterAssetChoice::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::String;
	GenericParm.ObjectValue = Value;
}


void UHoudiniParameterAssetChoice::UpdateValue(TConstArrayView<FString> AllStringValues)
{
	Value = ParseAssetPathFromString(AllStringValues[ValueIndex]);
}

void UHoudiniParameterAssetChoice::UpdateDefaultValue(TConstArrayView<FString> AllStringValues)
{
	DefaultValue = ParseAssetPathFromString(AllStringValues[ValueIndex]);
}

bool UHoudiniParameterAssetChoice::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if ((GenericParm->Type != EHoudiniGenericParameterType::String) &&
		(GenericParm->Type != EHoudiniGenericParameterType::Object))
		return false;

	const FSoftObjectPath OrigPath = bCompareWithDefault ? DefaultValue : Value;
	Value = (GenericParm->Type == EHoudiniGenericParameterType::String) ?
		(IS_ASSET_PATH_INVALID(GenericParm->StringValue) ? FSoftObjectPath() : GenericParm->StringValue) :
		GenericParm->ObjectValue.ToSoftObjectPath();

	if (OrigPath != Value)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


// MultiParm
bool UHoudiniMultiParameter::HapiSyncAttribute(const bool& bUpdateDefaultCount)
{
	const int32& NodeId = GetNodeId();

	// Try to get MultiParm default count by parm-name, maybe not found if .hda modified.
	int32 CurrCount = 0;
	HAPI_Result Result = FHoudiniApi::GetParmIntValue(FHoudiniEngine::Get().GetSession(), NodeId,
		TCHAR_TO_UTF8(*Name), 0, &CurrCount);

	// So do NOT print to log, As maybe just parm name not found.
	if (Result != HAPI_RESULT_SUCCESS)
		return !(HAPI_SESSION_INVALID_RESULT(Result));
	
	if (bUpdateDefaultCount)
		DefaultCount = CurrCount;

	if (CurrCount != 1)  // If already be 1, then we need NOT call hapi to set value
	{
		// Try to set attrib-MultiParm count to 1 by name, maybe not found if .hda modified.
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), NodeId,
			TCHAR_TO_UTF8(*Name), 0, 1));
	}

	return true;
}

bool UHoudiniMultiParameter::HapiUpload()
{
	switch (Modification)
	{
	case EHoudiniParameterModification::SetValue:
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmIntValue(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name), 0, Count));
		break;
	}
	case EHoudiniParameterModification::RevertToDefault:
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RevertParmToDefaults(FHoudiniEngine::Get().GetSession(), GetNodeId(),
			TCHAR_TO_UTF8(*Name)));
		break;
	}
	case EHoudiniParameterModification::InsertInst:
	{
		ModificationIndices.Sort();
		for (int32 n = ModificationIndices.Num() - 1; n >= 0; --n)
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::InsertMultiparmInstance(FHoudiniEngine::Get().GetSession(), GetNodeId(),
				Id, ModificationIndices[n]));
		ModificationIndices.Empty();
		break;
	}
	case EHoudiniParameterModification::RemoveInst:
	{
		ModificationIndices.Sort();
		for (int32 n = ModificationIndices.Num() - 1; n >= 0; --n)
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::RemoveMultiparmInstance(FHoudiniEngine::Get().GetSession(), GetNodeId(),
				Id, ModificationIndices[n]));
		ModificationIndices.Empty();
		break;
	}
	}

	ResetModification();
	
	return true;
}

void UHoudiniMultiParameter::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);

	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::MultiParm;
	GenericParm.StringValue = FString::FromInt(Count);
	
	if (ChildAttribParms.IsValidIndex(0))
	{
		const int32 StartMultiParmIdx = ChildAttribParms[0]->GetMultiParmInstanceIndex();
		for (const UHoudiniParameter* AttribParmInst : ChildAttribParmInsts)
		{
			TMap<FName, FHoudiniGenericParameter> CurrGenericParm;
			AttribParmInst->SaveGeneric(CurrGenericParm);
			if (!CurrGenericParm.IsEmpty())
				InOutParms.Add(*FString::Printf(TEXT("%s_%d"), *AttribParmInst->GetParameterName(),
					AttribParmInst->GetMultiParmInstanceIndex() - StartMultiParmIdx), CurrGenericParm.begin()->Value);
		}
	}
}

bool UHoudiniMultiParameter::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)  // Only for attrib
{
	if (ChildAttribParms.IsEmpty())  // Only for AttribMultiParm
		return false;

	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if (GenericParm->StringValue.IsEmpty() || !GenericParm->StringValue.IsNumeric())
		return false;

	bool bValueChanged = false;
	const int32 NewCount = FCString::Atoi(*GenericParm->StringValue);
	if (Count != NewCount)
	{
		SetInstanceCount(NewCount, false);  // We should set num first
		bValueChanged = true;
	}

	const int32 StartMultiParmIdx = ChildAttribParms[0]->GetMultiParmInstanceIndex();
	for (UHoudiniParameter* AttribParmInst : ChildAttribParmInsts)
	{
		if (const FHoudiniGenericParameter* ChildGenericParm = Parms.Find(*FString::Printf(TEXT("%s_%d"),
			*AttribParmInst->GetParameterName(), AttribParmInst->GetMultiParmInstanceIndex() - StartMultiParmIdx)))
		{
			TMap<FName, FHoudiniGenericParameter> CurrGenericParm;
			CurrGenericParm.Add(*AttribParmInst->GetParameterName(), *ChildGenericParm);
			if (AttribParmInst->LoadGeneric(CurrGenericParm, false))  // As this is an attribute, so always compare with values
				bValueChanged = true;
		}
	}

	return bValueChanged;
}

void UHoudiniMultiParameter::InsertInstance(const int32& InstIdx)
{
	if (ChildAttribParms.IsEmpty())  // Treat as normal MultiParm
		ModificationIndices.Add(InstIdx);
	else  // Should be an AttribMultiParm
	{
		++Count;

		class FHouParmInstanceIndexModifier : public UHoudiniParameter
		{
		public:
			FORCEINLINE void Increase() { ++MultiParmInstanceIndex; }
			FORCEINLINE void Set(const int32& InstIdx) { MultiParmInstanceIndex = InstIdx; }
		};

		int32 InsertIdxPos = -1;
		for (int32 ChildIdx = 0; ChildIdx < ChildAttribParmInsts.Num(); ++ChildIdx)
		{
			const int32& CurrInstIdx = ChildAttribParmInsts[ChildIdx]->GetMultiParmInstanceIndex();
			if (CurrInstIdx == InstIdx && InsertIdxPos < 0)
				InsertIdxPos = ChildIdx;
			
			if (CurrInstIdx >= InstIdx)
				((FHouParmInstanceIndexModifier*)ChildAttribParmInsts[ChildIdx])->Increase();
		}

		if (ChildAttribParmInsts.IsValidIndex(InsertIdxPos))
		{
			TArray<UHoudiniParameter*> NewAttribParms;
			for (UHoudiniParameter* AttribParm : ChildAttribParms)
			{
				UHoudiniParameter* NewAttribParm = DuplicateObject<UHoudiniParameter>(AttribParm, AttribParm->GetOuter());
				NewAttribParms.Add(NewAttribParm);
				((FHouParmInstanceIndexModifier*)NewAttribParm)->Set(InstIdx);
			}
			ChildAttribParmInsts.Insert(NewAttribParms, InsertIdxPos);
		}
	}

	SetModification(EHoudiniParameterModification::InsertInst);
}

void UHoudiniMultiParameter::RemoveInstance(const int32& InstIdx)
{
	if (ChildAttribParms.IsEmpty())  // treat as normal MultiParm
		ModificationIndices.Add(InstIdx);
	else  // Should be an AttribMultiParm
	{
		--Count;

		ChildAttribParmInsts.RemoveAll([&InstIdx](const UHoudiniParameter* AttribParm){ return AttribParm->GetMultiParmInstanceIndex() == InstIdx; });

		class FHouParmInstanceIndexModifier : public UHoudiniParameter
		{
		public:
			FORCEINLINE void Decrease() { ++MultiParmInstanceIndex; }
		};

		for (int32 ChildIdx = 0; ChildIdx < ChildAttribParmInsts.Num(); ++ChildIdx)
		{
			if (ChildAttribParmInsts[ChildIdx]->GetMultiParmInstanceIndex() > InstIdx)
				((FHouParmInstanceIndexModifier*)ChildAttribParmInsts[ChildIdx])->Decrease();
		}
	}

	SetModification(EHoudiniParameterModification::RemoveInst);
}

void UHoudiniMultiParameter::SetInstanceCount(const int32& InNewCount, const bool& bNotifyChanged)
{
	if (Count == InNewCount)
		return;
	
	if (!ChildAttribParms.IsEmpty())  // Should be a AttribMultiParm
	{
		if (Count < InNewCount)
		{
			class FHouParmInstanceIndexModifier : public UHoudiniParameter
			{
			public:
				FORCEINLINE void Plus(const int32& DupIdx) { MultiParmInstanceIndex += DupIdx; }
			};

			for (int32 DupIdx = Count; DupIdx < InNewCount; ++DupIdx)
			{
				for (UHoudiniParameter* AttribParm : ChildAttribParms)
				{
					UHoudiniParameter* NewAttribParm = DuplicateObject<UHoudiniParameter>(AttribParm, AttribParm->GetOuter());
					((FHouParmInstanceIndexModifier*)NewAttribParm)->Plus(DupIdx);
					ChildAttribParmInsts.Add(NewAttribParm);
				}
			}
		}
		else if (Count > InNewCount)
		{
			const int32 StartInstIdx = ChildAttribParms[0]->GetMultiParmInstanceIndex() + InNewCount;
			for (int32 ChildIdx = 0; ChildIdx < ChildAttribParmInsts.Num(); ++ChildIdx)
			{
				if (ChildAttribParmInsts[ChildIdx]->GetMultiParmInstanceIndex() == StartInstIdx)
				{
					ChildAttribParmInsts.SetNum(ChildIdx);
					break;
				}
			}
		}
	}

	Count = InNewCount;
	if (bNotifyChanged)
		SetModification(EHoudiniParameterModification::SetValue);
}


// Ramp
bool UHoudiniParameterRamp::HapiUpdateFromInfo(const HAPI_ParmInfo& InParmInfo, const bool& bUpdateTags)
{
	UHoudiniParameter::HapiUpdateFromInfo(InParmInfo, bUpdateTags);

	// Normally, HAPI_ParmInfo::floatValuesIndex won't be set if is a ramp(MultiParm), but we've set it before paring.
	StartFloatValueIndex = InParmInfo.floatValuesIndex;
	return true;
}

bool UHoudiniParameterRamp::HapiUpload()
{
	ResetModification();

	FHoudiniParameterPresetHelper PresetHelper;
	Upload(PresetHelper);
	return PresetHelper.HapiSet(GetNodeId());
}

void UHoudiniParameterRamp::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	FHoudiniGenericParameter& GenericParm = InOutParms.Add(*Name);
	GenericParm.Size = Size;
	GenericParm.Type = EHoudiniGenericParameterType::String;
	GenericParm.StringValue = GetValueString();
}

ERichCurveInterpMode UHoudiniParameterRamp::ToInterpMode(const int32& InBasis)
{
	switch (InBasis)
	{
	case 0: return RCIM_Constant;  // hou.rampBasis.Constant: Holds the value constant until the next key.
	case 1: return RCIM_Linear;  // hou.rampBasis.Linear: Does a linear (straight line) interpolation between keys.
	case 2:  // hou.rampBasis.CatmullRom: Interpolates smoothly between the keys. See Catmull-Rom_spline .
	case 3:  // hou.rampBasis.MonotoneCubic: Another smooth interpolation that ensures that there is no overshoot. For example, if a key's value is smaller than the values in the adjacent keys, this type ensures that the interpolated value is never less than the key's value.
	case 4:  // hou.rampBasis.Bezier: Cubic Bezier curve that interpolates every third control point and uses the other points to shape the curve. See Bezier curve .
	case 5:  // hou.rampBasis.BSpline: Cubic curve where the control points influence the shape of the curve locally (that is, they influence only a section of the curve). See B-Spline .
	case 6: return RCIM_Cubic;  // hou.rampBasis.Hermite: Cubic Hermite curve that interpolates the odd control points, while even control points control the tangent at the previous interpolation point. See Hermite spline .
	}

	return RCIM_Linear;
}

ERichCurveInterpMode UHoudiniParameterRamp::ToInterpMode(const FString& InBasisStr)
{
	if (InBasisStr.Contains(TEXT("linear")))
		return RCIM_Linear;
	else if (InBasisStr.Contains(TEXT("constant")))
		return RCIM_Constant;

	return RCIM_Cubic;
}

int32 UHoudiniParameterRamp::ToBasisEnum(const ERichCurveInterpMode& InInterpMode)
{
	switch (InInterpMode)
	{
	case RCIM_Constant: return 0;
	case RCIM_Linear: return 1;
	case RCIM_Cubic: return 2;
	}

	return 0;
}

FString UHoudiniParameterRamp::ToBasisString(const ERichCurveInterpMode& InInterpMode)
{
	switch (InInterpMode)
	{
	case RCIM_Constant: return TEXT("constant");
	case RCIM_Linear: return TEXT("linear");
	case RCIM_Cubic: return TEXT("catmull-rom");
	}

	return TEXT("linear");
}

#define RESET_EICH_CURVE_KEY_TANGENT Key.ArriveTangent = 0.0f; \
	Key.ArriveTangentWeight = 0.0f; \
	Key.LeaveTangent = 0.0f; \
	Key.LeaveTangentWeight = 0.0f; \
	Key.TangentMode = RCTM_Auto; \
	Key.TangentWeightMode = RCTWM_WeightedNone;


// FloatRamp
bool UHoudiniParameterFloatRamp::bIsRampEditorDragging = true;

bool UHoudiniParameterFloatRamp::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	if (Name.IsEmpty())
		return true;

	const int32 NumKeys = Value.Keys.Num();
	PresetHelper.Append(TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*FString::FromInt(NumKeys)));

	const TCHAR LastChar = Name[Name.Len() - 1];
	const bool bEndWithNum = (TCHAR('0') <= LastChar) && (LastChar <= TCHAR('9'));
	for (int32 KeyIdx = 0; KeyIdx < NumKeys; ++KeyIdx)
	{
		const FRichCurveKey& Key = Value.Keys[KeyIdx];
		const std::string KeyPrefix = std::string(TCHAR_TO_UTF8(*((bEndWithNum ? (Name + TEXT("_")) : Name) + FString::FromInt(KeyIdx + 1))));
		PresetHelper.Append(KeyPrefix + "pos", std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Key.Time))));
		PresetHelper.Append(KeyPrefix + "value", std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Key.Value))));
		PresetHelper.Append(KeyPrefix + "interp", std::string(TCHAR_TO_UTF8(*ToBasisString(Key.InterpMode))));
	}
	return true;
}

bool UHoudiniParameterFloatRamp::LoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms, const bool& bCompareWithDefault)
{
	const FHoudiniGenericParameter* GenericParm = Parms.Find(*Name);
	if (!GenericParm)
		return false;

	if (GenericParm->Type != EHoudiniGenericParameterType::String)
		return false;

	const TArray<FRichCurveKey> OrigKeys = bCompareWithDefault ? DefaultValue.Keys : Value.Keys;
	ParseFromString(GenericParm->StringValue);
	
	if (OrigKeys != Value.Keys)
	{
		Modification = EHoudiniParameterModification::SetValue;
		return true;
	}
	return false;
}


void UHoudiniParameterFloatRamp::UpdateKeys(FRichCurve& InOutCurve, const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) const
{
	TArray<FRichCurveKey>& Keys = InOutCurve.Keys;
	Keys.SetNum(AllIntValues[ValueIndex]);
	for (int32 KeyIdx = 0; KeyIdx < Keys.Num(); ++KeyIdx)
	{
		FRichCurveKey& Key = Keys[KeyIdx];
		Key.Time = AllFloatValues[StartFloatValueIndex + KeyIdx * 2];
		Key.Value = AllFloatValues[StartFloatValueIndex + KeyIdx * 2 + 1];
		Key.InterpMode = UHoudiniParameterRamp::ToInterpMode(AllIntValues[ValueIndex + 1 + KeyIdx]);
		
		RESET_EICH_CURVE_KEY_TANGENT;
	}

	InOutCurve.AutoSetTangents();
}

FString UHoudiniParameterFloatRamp::ConvertRichCurve(const FRichCurve& FloatCurve)
{
	FString RampStr(TEXT("{\"colortype\":\"RGB\",\"points\":["));
	RampStr += FString::FromInt(FloatCurve.Keys.Num());
	for (const FRichCurveKey& Key : FloatCurve.Keys)
	{
		RampStr += FString::Printf(TEXT(",{\"t\":%f,\"rgba\":[%f,1,1,1],\"basis\":\"%s\"}"),
			Key.Time, Key.Value, *UHoudiniParameterRamp::ToBasisString(Key.InterpMode));
	}
	RampStr += TEXT("]}");
	return RampStr;
}

void UHoudiniParameterFloatRamp::ParseString(FRichCurve& OutCurve, const FString& RampStr)
{
	TArray<FRichCurveKey>& Keys = OutCurve.Keys;
	Keys.Empty();
	TArray<FString> PointStrs;
	RampStr.ParseIntoArray(PointStrs, TEXT("{"));
	for (int32 PointIdx = 1; PointIdx < PointStrs.Num(); ++PointIdx)
	{
		const FString& PointStr = PointStrs[PointIdx];
		FString CurrValueStr;
		bool bValueParsing = false;
		TArray<FString> ValueStrs;
		for (const TCHAR& Char : PointStr)
		{
			if ((Char == TCHAR(':')) || (Char == TCHAR('[')))
				bValueParsing = true;
			else if (bValueParsing)
			{
				if ((Char == TCHAR(',')) || Char == (TCHAR('}')))
				{
					ValueStrs.Add(CurrValueStr);
					bValueParsing = false;
					CurrValueStr.Empty();
				}
				else if (Char != TCHAR('\"'))
					CurrValueStr.AppendChar(Char);
			}
		}

		FRichCurveKey Key;
		Key.Time = ValueStrs.IsValidIndex(0) ? FCString::Atof(*ValueStrs[0]) : 0.0f;
		Key.Value = ValueStrs.IsValidIndex(1) ? FCString::Atof(*ValueStrs[1]) : 0.0f;
		Key.InterpMode = ValueStrs.IsValidIndex(2) ? UHoudiniParameterRamp::ToInterpMode(ValueStrs[2]) : ERichCurveInterpMode::RCIM_Linear;

		RESET_EICH_CURVE_KEY_TANGENT;

		Keys.Add(Key);
	}

	OutCurve.AutoSetTangents();
}

void UHoudiniParameterFloatRamp::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	for (FRichCurveKey& Key : Value.Keys)
	{
		Key.Time = FMath::Clamp(Key.Time, 0.0f, 1.0f);
		Key.Value = FMath::Clamp(Key.Value, 0.0f, 1.0f);
	}

	if (!bIsRampEditorDragging)
	{
		SetModification(EHoudiniParameterModification::SetValue);
		bIsDefault = false;
	}

	bIsRampEditorDragging = false;
}

// Color-Ramp
bool UHoudiniParameterColorRamp::Upload(FHoudiniParameterPresetHelper& PresetHelper) const
{
	if (Name.IsEmpty())
		return true;

	const int32 NumKeys = Value[0].Keys.Num();
	PresetHelper.Append(TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*FString::FromInt(NumKeys)));

	const TCHAR LastChar = Name[Name.Len() - 1];
	const bool bEndWithNum = (TCHAR('0') <= LastChar) && (LastChar <= TCHAR('9'));
	for (int32 KeyIdx = 0; KeyIdx < NumKeys; ++KeyIdx)
	{
		const FRichCurveKey& Key = Value[0].Keys[KeyIdx];
		const std::string KeyPrefix = std::string(TCHAR_TO_UTF8(*((bEndWithNum ? (Name + TEXT("_")) : Name) + FString::FromInt(KeyIdx + 1))));
		PresetHelper.Append(KeyPrefix + "pos", std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Key.Time))));
		PresetHelper.Append(KeyPrefix + "c", std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Key.Value))) + HAPI_PRESET_VALUE_DELIM +
			std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value[1].Keys[KeyIdx].Value))) + HAPI_PRESET_VALUE_DELIM +
			std::string(TCHAR_TO_UTF8(*FString::SanitizeFloat(Value[2].Keys[KeyIdx].Value))));  // Check whether houdini only support RGB rather than RGBA
		PresetHelper.Append(KeyPrefix + "interp", std::string(TCHAR_TO_UTF8(*ToBasisString(Key.InterpMode))));
	}
	return true;
}

void UHoudiniParameterColorRamp::UpdateKeys(FRichCurve InOutCurves[4], const TArray<int32>& AllIntValues, const TArray<float>& AllFloatValues) const
{
	const int32& NumKeys = AllIntValues[ValueIndex];
	for (int32 ChannelIdx = 0; ChannelIdx < 4; ++ChannelIdx)
		InOutCurves[ChannelIdx].Keys.SetNum(NumKeys);

	for (int32 KeyIdx = 0; KeyIdx < NumKeys; ++KeyIdx)
	{
		const float& KeyTime = AllFloatValues[StartFloatValueIndex + KeyIdx * 4];  // 1 + 3, key and color(3)
		const ERichCurveInterpMode KeyInterp = UHoudiniParameterRamp::ToInterpMode(AllIntValues[ValueIndex + 1 + KeyIdx]);
		for (int32 ChannelIdx = 0; ChannelIdx < 4; ++ChannelIdx)
		{
			FRichCurveKey& Key = InOutCurves[ChannelIdx].Keys[KeyIdx];
			Key.Time = KeyTime;
			Key.Value = (ChannelIdx < 3) ? AllFloatValues[StartFloatValueIndex + KeyIdx * 4 + 1 + ChannelIdx] : 1.0f;  // We only need 3 channels for houdini color ramp
			Key.InterpMode = KeyInterp;

			RESET_EICH_CURVE_KEY_TANGENT;
		}
	}

	for (int32 ChannelIdx = 0; ChannelIdx < 4; ++ChannelIdx)
		InOutCurves[ChannelIdx].AutoSetTangents();
}

FString UHoudiniParameterColorRamp::ConvertRichCurve(const FRichCurve FloatCurves[4])
{
	FString RampStr(TEXT("{\"colortype\":\"RGB\",\"points\":["));
	const int32 NumKeys = FloatCurves[0].Keys.Num();
	RampStr += FString::FromInt(NumKeys);
	for (int32 KeyIdx = 0; KeyIdx < NumKeys; ++KeyIdx)
	{
		const FRichCurveKey& Key = FloatCurves[0].Keys[KeyIdx];
		RampStr += FString::Printf(TEXT(",{\"t\":%f,\"rgba\":[%f,%f,%f,1],\"basis\":\"%s\"}"),
			Key.Time, Key.Value, FloatCurves[1].Keys[KeyIdx].Value, FloatCurves[2].Keys[KeyIdx].Value,
			*UHoudiniParameterRamp::ToBasisString(Key.InterpMode));
	}
	RampStr += TEXT("]}");
	return RampStr;
}

void UHoudiniParameterColorRamp::ParseString(FRichCurve OutCurves[4], const FString& RampStr)
{
	for (int32 TupleIdx = 0; TupleIdx < 4; ++TupleIdx)
		OutCurves[TupleIdx].Keys.Empty();

	TArray<FString> PointStrs;
	RampStr.ParseIntoArray(PointStrs, TEXT("{"));
	for (int32 PointIdx = 1; PointIdx < PointStrs.Num(); ++PointIdx)
	{
		const FString& PointStr = PointStrs[PointIdx];
		FString CurrValueStr;
		bool bValueParsing = false;
		bool bParsingColor = false;
		TArray<FString> ValueStrs;
		for (const TCHAR& Char : PointStr)
		{
			if (Char == TCHAR(':'))
				bValueParsing = true;
			else if (Char == TCHAR('['))
				bParsingColor = true;
			else if (bValueParsing)
			{
				if (Char == TCHAR(']'))
				{
					bValueParsing = false;
					bParsingColor = false;
					ValueStrs.Add(CurrValueStr);
					CurrValueStr.Empty();
				}
				else if ((Char == TCHAR(',')) || (Char == TCHAR('}')))
				{
					ValueStrs.Add(CurrValueStr);
					if (!bParsingColor)
						bValueParsing = false;
					CurrValueStr.Empty();
				}
				else if (Char != TCHAR('\"'))
					CurrValueStr.AppendChar(Char);
			}
		}

		FRichCurveKey Key;
		RESET_EICH_CURVE_KEY_TANGENT;

		Key.Time = ValueStrs.IsValidIndex(0) ? FCString::Atof(*ValueStrs[0]) : 0.0f;
		Key.InterpMode = ValueStrs.IsValidIndex(5) ? UHoudiniParameterRamp::ToInterpMode(ValueStrs[5]) : ERichCurveInterpMode::RCIM_Linear;

		for (int32 TupleIdx = 0; TupleIdx < 4; ++TupleIdx)
		{
			Key.Value = ValueStrs.IsValidIndex(TupleIdx + 1) ? FCString::Atof(*ValueStrs[TupleIdx + 1]) : 0.0f;
			OutCurves[TupleIdx].Keys.Add(Key);
		}
	}

	for (int32 TupleIdx = 0; TupleIdx < 4; ++TupleIdx)
		OutCurves[TupleIdx].AutoSetTangents();
}


static const FName RedCurveName(TEXT("R"));
static const FName GreenCurveName(TEXT("G"));
static const FName BlueCurveName(TEXT("B"));
static const FName AlphaCurveName(TEXT("A"));

static FRichCurve EmptyAlphaCurve;

TArray<FRichCurveEditInfoConst> UHoudiniParameterColorRamp::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(FRichCurveEditInfoConst(&Value[0], RedCurveName));
	Curves.Add(FRichCurveEditInfoConst(&Value[1], GreenCurveName));
	Curves.Add(FRichCurveEditInfoConst(&Value[2], BlueCurveName));
	Curves.Add(FRichCurveEditInfoConst(&EmptyAlphaCurve, AlphaCurveName));
	return Curves;
}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
void UHoudiniParameterColorRamp::GetCurves(TAdderReserverRef<FRichCurveEditInfoConst> Curves) const
{
	Curves.Add(FRichCurveEditInfoConst(&Value[0], RedCurveName));
	Curves.Add(FRichCurveEditInfoConst(&Value[1], GreenCurveName));
	Curves.Add(FRichCurveEditInfoConst(&Value[2], BlueCurveName));
	Curves.Add(FRichCurveEditInfoConst(&EmptyAlphaCurve, AlphaCurveName));
}
#endif
TArray<FRichCurveEditInfo> UHoudiniParameterColorRamp::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(FRichCurveEditInfo(&Value[0], RedCurveName));
	Curves.Add(FRichCurveEditInfo(&Value[1], GreenCurveName));
	Curves.Add(FRichCurveEditInfo(&Value[2], BlueCurveName));
	Curves.Add(FRichCurveEditInfo(&EmptyAlphaCurve, AlphaCurveName));
	return Curves;
}

void UHoudiniParameterColorRamp::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	for (int32 ChannelIdx = 0; ChannelIdx < 4; ++ChannelIdx)
	{
		for (FRichCurveKey& Key : Value[ChannelIdx].Keys)
		{
			Key.Time = FMath::Clamp(Key.Time, 0.0f, 1.0f);
			Key.Value = (ChannelIdx < 3) ? FMath::Clamp(Key.Value, 0.0f, 1.0f) : 1.0f;  // We only need 3 channels for houdini color ramp
		}
	}
	
	Modification = EHoudiniParameterModification::SetValue;  // Just mark changed, do NOT trigger node to cook
}

FLinearColor UHoudiniParameterColorRamp::GetLinearColorValue(float InTime) const
{
	FLinearColor Result;

	Result.R = Value[0].Eval(InTime);
	Result.G = Value[1].Eval(InTime);
	Result.B = Value[2].Eval(InTime);
	Result.A = 1.0f;

	return Result;
}
