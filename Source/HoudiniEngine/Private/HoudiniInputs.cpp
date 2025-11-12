// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputs.h"

#include "EngineUtils.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniNode.h"
#include "HoudiniParameter.h"
#include "HoudiniCurvesComponent.h"


bool AHoudiniNode::HapiUpdateInputs(const bool& bBeforeCook, bool& bOutHasNewInputsPendingUpload)
{
	const double StartTime = FPlatformTime::Seconds();

	bOutHasNewInputsPendingUpload = false;

	// -------- Identify inputs to node-input and parm-input --------
	TArray<UHoudiniInput*> NodeInputs, ParmInputs;
	for (UHoudiniInput* Input : Inputs)
	{
		if (Input->IsParameter())
			ParmInputs.Add(Input);
		else
			NodeInputs.Add(Input);
	}
	Inputs.Empty();


	// -------- Update node-inputs --------
	// Node-inputs update should only happen before cook
	if (bOutputEditable)  // Means we should ignore all node-inputs
	{
		for (UHoudiniInput* Input : NodeInputs)
			HOUDINI_FAIL_RETURN(Input->HapiDestroy());
	}
	else if (GeoNodeId >= 0)  // Is a Sop
	{
		if (bBeforeCook)  // Only parse NodeInputs before cook, because NodeInputs are static, may only be changed when hda changed
		{
			TArray<FString> AllInputNames;
			// InputCount has already been set in HapiParseParameters
			for (int32 InputIdx = 0; InputIdx < InputCount; ++InputIdx)
			{
				HAPI_StringHandle InputNameSH;
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetNodeInputName(FHoudiniEngine::Get().GetSession(), NodeId, InputIdx, &InputNameSH));
				FString InputName;
				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(InputNameSH, InputName));
				AllInputNames.Add(InputName);
			}

			// Found by name first
			Inputs.SetNumZeroed(InputCount);
			for (int32 InputIdx = 0; InputIdx < InputCount; ++InputIdx)
			{
				const FString& InputName = AllInputNames[InputIdx];
				if (UHoudiniInput** FoundInputPtr = NodeInputs.FindByPredicate(
					[InputName](const UHoudiniInput* Input) { return Input->GetInputName() == InputName; }))
					Inputs[InputIdx] = *FoundInputPtr;
			}

			// Found by index, else create new inputs
			for (int32 InputIdx = 0; InputIdx < InputCount; ++InputIdx)
			{
				if (!Inputs[InputIdx])
				{
					if (NodeInputs.IsValidIndex(InputIdx) && !Inputs.Contains(NodeInputs[InputIdx]))
						Inputs[InputIdx] = NodeInputs[InputIdx];
				}

				if (!Inputs[InputIdx])
				{
					Inputs[InputIdx] = NewObject<UHoudiniInput>(this,
						MakeUniqueObjectName(this, UHoudiniInput::StaticClass(), FName("Input")), RF_Public | RF_Transactional);
				}

				Inputs[InputIdx]->UpdateNodeInput(InputIdx, AllInputNames[InputIdx]);
			}
		}
		else
			Inputs = NodeInputs;
	}


	// -------- Update parm-inputs --------
	if (bBeforeCook)
	{
		TMap<FString, UHoudiniInput*> NameInputMap;  // We just use parm-name to identify inputs before cook.
		for (UHoudiniInput* Input : ParmInputs)
			NameInputMap.Add(Input->GetInputName(), Input);

		for (UHoudiniParameter* Parm : Parms)
		{
			if (Parm->GetType() != EHoudiniParameterType::Input)
				continue;

			if (UHoudiniInput** FoundInputPtr = NameInputMap.Find(Parm->GetParameterName()))
			{
				UHoudiniInput* Input = *FoundInputPtr;
				HOUDINI_FAIL_RETURN(Input->HapiUpdateFromParameter(Cast<UHoudiniParameterInput>(Parm), true));
				Inputs.Add(Input);

				NameInputMap.Remove(Parm->GetParameterName());
			}
			else
			{
				UHoudiniInput* Input = NewObject<UHoudiniInput>(this,
					MakeUniqueObjectName(this, UHoudiniInput::StaticClass(), FName("InputParm")), RF_Public | RF_Transactional);
				HOUDINI_FAIL_RETURN(Input->HapiUpdateFromParameter(Cast<UHoudiniParameterInput>(Parm), true));  // Will parse the parm's value into objects to input if parm has value
				
				// Judge whether input parm value is parsed, do NOT use "Input->Holders.IsEmpty", as Content input may has a null holder by default
				if (Input->Holders.ContainsByPredicate([](const UHoudiniInputHolder* Holder) { return Holder != nullptr; }))
					bOutHasNewInputsPendingUpload = true;
				
				Inputs.Add(Input);
			}
		}

		// -------- Destroy useless inputs --------
		for (const auto& Input : NameInputMap)
			HOUDINI_FAIL_RETURN(Input.Value->HapiDestroy());
	}
	else
	{
		// If has value but not found, then we should destroy it and create a new Input, which will parse the parm's value into objects to input
		TMap<FString, UHoudiniInput*> NameEmptyInputMap;  // If parm-value is empty, then we should use parm-name ro identify.
		TMap<FString, UHoudiniInput*> ValueInputMap;  // We should use parm-value to identify inputs after cook.
		for (UHoudiniInput* Input : ParmInputs)
		{
			if (Input->GetValue().IsEmpty())
				NameEmptyInputMap.Add(Input->GetInputName(), Input);
			else
				ValueInputMap.Add(Input->GetValue(), Input);
		}

		for (UHoudiniParameter* Parm : Parms)
		{
			if (Parm->GetType() != EHoudiniParameterType::Input)
				continue;

			UHoudiniParameterInput* InputParm = Cast<UHoudiniParameterInput>(Parm);
			const bool bIsParmValueEmpty = InputParm->GetValue().IsEmpty();
			
			if (UHoudiniInput** FoundInputPtr = (bIsParmValueEmpty ? NameEmptyInputMap.Find(Parm->GetParameterName()) : ValueInputMap.Find(InputParm->GetValue())))
			{
				UHoudiniInput* Input = *FoundInputPtr;
				HOUDINI_FAIL_RETURN(Input->HapiUpdateFromParameter(InputParm, false));
				Inputs.Add(Input);

				if (bIsParmValueEmpty)
					NameEmptyInputMap.Remove(Parm->GetParameterName());
				else
					ValueInputMap.Remove(InputParm->GetValue());
			}
			else
			{
				UHoudiniInput* Input = NewObject<UHoudiniInput>(this,
					MakeUniqueObjectName(this, UHoudiniInput::StaticClass(), FName("InputParm_")), RF_Public | RF_Transactional);
				HOUDINI_FAIL_RETURN(Input->HapiUpdateFromParameter(InputParm, true));  // Will parse the parm's value into objects to input if parm has value
				
				// Judge whether input parm value is parsed, do NOT use "Input->Holders.IsEmpty()", as Content input may has a null holder by default
				if (Input->Holders.ContainsByPredicate([](const UHoudiniInputHolder* Holder) { return Holder != nullptr; }))
					bOutHasNewInputsPendingUpload = true;

				Inputs.Add(Input);
			}
		}

		// -------- Destroy useless inputs --------
		for (const auto& Input : NameEmptyInputMap)
			HOUDINI_FAIL_RETURN(Input.Value->HapiDestroy());

		for (const auto& Input : ValueInputMap)
			HOUDINI_FAIL_RETURN(Input.Value->HapiDestroy());
	}


	// -------- Update InputHolders --------
	for (UHoudiniInput* Input : Inputs)
	{
		switch (Input->GetType())
		{
		case EHoudiniInputType::Curves:
		{
			const TArray<UHoudiniInputHolder*>& Holders = Input->Holders;
			if (Holders.IsValidIndex(0))
			{
				Cast<UHoudiniInputCurves>(Holders[0])->GetCurvesComponent()->InputUpdate(this, Input->GetInputName(),
					Input->GetSettings().bDefaultCurveClosed, Input->GetSettings().DefaultCurveType, Input->GetSettings().DefaultCurveColor);
			}
		}
		break;
		}
	}

	const double TimeCost = FPlatformTime::Seconds() - StartTime;
	if (TimeCost > 0.001)
		UE_LOG(LogHoudiniEngine, Log, TEXT("%s: Update Inputs %.3f (s)"), *GetActorLabel(false), TimeCost);

	return true;
}

bool AHoudiniNode::HapiUploadInputsAndParameters()
{
	const double StartTime = FPlatformTime::Seconds();

	// We should upload inputs first, as upload parms may change the ParmInputs' parmId
	for (UHoudiniInput* Input : Inputs)
		HOUDINI_FAIL_RETURN(Input->HapiUpload());  // Will check whether this input truly need reimport, then upload

	const double InputUploadedTime = FPlatformTime::Seconds();

	if (Preset.IsValid())
	{
		TArray<UHoudiniParameter*> ParmsToUpload;
		FHoudiniParameterPresetHelper PresetHelper;
		for (UHoudiniParameter* Parm : Parms)
		{
			if (!Parm->UploadGeneric(PresetHelper, *Preset, bRebuildBeforeCook))
				ParmsToUpload.Add(Parm);
		}
		HOUDINI_FAIL_RETURN(PresetHelper.HapiSet(NodeId));

		// Reversed order to upload parms, as some parms may change the parm count
		for (int32 ParmIdx = ParmsToUpload.Num() - 1; ParmIdx >= 0; --ParmIdx)
		{
			HOUDINI_FAIL_RETURN(ParmsToUpload[ParmIdx]->HapiUpload());
			ParmsToUpload[ParmIdx]->ResetModification();
		}
	}
	else if (bRebuildBeforeCook)  // Should upload all parm values that are NOT in default
	{
		FHoudiniParameterPresetHelper PresetHelper;
		TArray<UHoudiniParameter*> ParmsToUpload;
		for (UHoudiniParameter* Parm : Parms)
		{
			// We need NOT to upload values in default because all values haven't been set yet.
			// But we should update changed parms, like pressed button, or MultiParm insert/remove
			if (!Parm->IsInDefault() || (Parm->HasChanged() && !Parm->IsPendingRevert()))
			{
				if (Parm->Upload(PresetHelper))
					Parm->ResetModification();
				else  // Should HapiUpload later
					ParmsToUpload.Add(Parm);
			}
		}
		HOUDINI_FAIL_RETURN(PresetHelper.HapiSet(NodeId));  // We should upload parm first, as multiparm insts may insert or remove later

		// Reversed order to upload parms, as some parms may change the parm count
		for (int32 ParmIdx = ParmsToUpload.Num() - 1; ParmIdx >= 0; --ParmIdx)
		{
			HOUDINI_FAIL_RETURN(ParmsToUpload[ParmIdx]->HapiUpload());
			ParmsToUpload[ParmIdx]->ResetModification();
		}
	}
	else  // Only upload changed parm values
	{
		// Reversed order to upload parms, as some parms may change the parm count
		for (int32 ParmIdx = Parms.Num() - 1; ParmIdx >= 0; --ParmIdx)
		{
			UHoudiniParameter* Parm = Parms[ParmIdx];
			if (Parm->HasChanged())
			{
				HOUDINI_FAIL_RETURN(Parm->HapiUpload());
				Parm->ResetModification();
			}
		}
	}

	Preset.Reset();  // Set Preset to nullptr, to avoid load preset twice

	if (bRebuildBeforeCook)  // We should sync AttribMultiParm here, to allow HapiUpdateParameters(false) to retrieve ChildAttribParms
		HOUDINI_FAIL_RETURN(HapiSyncAttributeMultiParameters(false));

	const double ParmUploadedTime = FPlatformTime::Seconds();

	// ByteMask inputs are binding a set of IntParmInsts, so we should upload it at last
	for (UHoudiniInput* Input : Inputs)
	{
		if (Input->GetType() == EHoudiniInputType::Mask && Input->Holders.Num() == 1)
			HOUDINI_FAIL_RETURN(Cast<UHoudiniInputMask>(Input->Holders[0])->HapiUploadData());
	}

	const double MaskUploadedTime = FPlatformTime::Seconds();
	
	{
		const double TimeCost = InputUploadedTime - StartTime + MaskUploadedTime - ParmUploadedTime;
		if (TimeCost > 0.001)
			UE_LOG(LogHoudiniEngine, Log, TEXT("%s: Upload Changed Inputs %.3f (s)"), *GetActorLabel(false), TimeCost);
	}
	
	{
		const double TimeCost = ParmUploadedTime - InputUploadedTime;
		if (TimeCost > 0.001)
			UE_LOG(LogHoudiniEngine, Log, TEXT("%s: Upload Changed Parameters %.3f (s)"), *GetActorLabel(false), TimeCost);
	}

	return true;
}

UHoudiniInput* AHoudiniNode::GetInputByName(const FString& Name, const bool bIsOperatorPathParameter) const
{
	for (UHoudiniInput* Input : Inputs)
	{
		if ((Input->IsParameter() == bIsOperatorPathParameter) && Input->GetInputName() == Name)
			return Input;
	}

	return nullptr;
}


void UHoudiniParameterInput::SaveGeneric(TMap<FName, FHoudiniGenericParameter>& InOutParms) const
{
	if (Input->GetType() == EHoudiniInputType::Content)
	{
		int32 ValidHolderIdx = 0;
		for (const UHoudiniInputHolder* Holder : Input->Holders)
		{
			if (!IsValid(Holder))
				continue;

			UObject* Asset = Holder->GetObject().LoadSynchronous();
			if (IsValid(Asset))
			{
				FHoudiniGenericParameter& GenericParm = InOutParms.Add(*(ValidHolderIdx == 0 ? Name : (Name + TEXT("_Holder") + FString::FromInt(ValidHolderIdx))));
				GenericParm.Type = EHoudiniGenericParameterType::Object;
				GenericParm.ObjectValue = Asset;
				GetObjectInfo(Asset, GenericParm.StringValue);

				++ValidHolderIdx;
			}
		}
	}

	// TODO: Other input type
}

void UHoudiniParameterInput::ConvertToGeneric(FHoudiniGenericParameter& OutGenericParm) const
{
	if (!Input.IsValid())
		return;

	OutGenericParm.Type = EHoudiniGenericParameterType::String;
	OutGenericParm.Size = 0;
	OutGenericParm.StringValue.Empty();

	// Convert holders input one single string
	for (const UHoudiniInputHolder* Holder : Input->Holders)
	{
		if (!IsValid(Holder))
			continue;

		if (const UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(Holder))
			OutGenericParm.StringValue += ActorInput->GetActorName().ToString() + TEXT(";");
		else if (const UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
			OutGenericParm.StringValue += LandscapeInput->GetLandscapeName().ToString() + TEXT(";");
		else if (const UHoudiniInputCurves* CurvesInput = Cast<UHoudiniInputCurves>(Holder))
		{
			FJsonDataBag DataBag;
			DataBag.JsonObject = CurvesInput->GetCurvesComponent()->ConvertToJson();
			OutGenericParm.StringValue = DataBag.ToJson();
			return;
		}
		else if (const UHoudiniInputNode* NodeInput = Cast<UHoudiniInputNode>(Holder))
		{
			OutGenericParm.StringValue = NodeInput->GetNodeActorName().ToString();
			return;
		}
		else if (const UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Holder))
		{
			OutGenericParm.StringValue = MaskInput->GetLandscapeName().ToString();
			return;
		}
		else  // Content inputs
		{
			const FString AssetRef = FHoudiniEngineUtils::GetAssetReference(Holder->GetObject().ToSoftObjectPath());
			if (!AssetRef.IsEmpty())
				OutGenericParm.StringValue += AssetRef + TEXT(";");
		}
	}
	OutGenericParm.StringValue.RemoveFromEnd(TEXT(";"));
}

void UHoudiniParameterInput::TryLoadGeneric(const TMap<FName, FHoudiniGenericParameter>& Parms)
{
	// Just set value, then value will parse by UHoudiniInput::HapiUpdateFromParameter called in AHoudiniNode::HapiUpdateInput, will actually do this
	for (int32 HolderIdx = 0; HolderIdx < Parms.Num(); ++HolderIdx)
	{
		if (const FHoudiniGenericParameter* FoundGenericParm = Parms.Find(*(HolderIdx == 0 ? Name : (Name + TEXT("_Holder") + FString::FromInt(HolderIdx)))))
		{
			if (HolderIdx == 0)
				Value.Empty();

			if ((HolderIdx == 0) && (FoundGenericParm->Type == EHoudiniGenericParameterType::String) &&
				!FoundGenericParm->StringValue.IsEmpty())
			{
				Value = FoundGenericParm->StringValue;
				return;
			}

			UObject* Asset = FoundGenericParm->ObjectValue.LoadSynchronous();
			if (IsValid(Asset))
			{
				if (!Value.IsEmpty())
					Value += TEXT(";");
				Value += Asset->GetPathName();
			}
		}
		else
			break;
	}
}

void UHoudiniParameterInput::GetObjectInfo(const UObject* Object, FString& OutInfoStr)
{
	const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& Builders = FHoudiniEngine::Get().GetContentInputBuilders();
	for (int32 BuilderIdx = Builders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
	{
		if (Builders[BuilderIdx]->GetInfo(Object, OutInfoStr))
			break;
	}
}

void UHoudiniInput::ForAll(TFunctionRef<void(UHoudiniInput*)> Fn)
{
	for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())
	{
		if (Node.IsValid())
		{
			for (UHoudiniInput* Input : Node->GetInputs())
				Fn(Input);
		}
	}
}

EHoudiniInputType UHoudiniInput::ParseTypeFromString(const FString& InputName, bool& bOutTypeSpecified)
{
	EHoudiniInputType Type = EHoudiniInputType::Content;

	if (InputName.Contains(TEXT("Content")) || InputName.Contains(TEXT("Object")))
		bOutTypeSpecified = true;
	if (InputName.Contains(TEXT("Curve")))
		Type = EHoudiniInputType::Curves;
	else if (InputName.Contains(TEXT("Node")) || InputName.Contains(TEXT("hda")))
		Type = EHoudiniInputType::Node;
	else if (InputName.Contains(TEXT("World")) || InputName.Contains(TEXT("Actor")) ||
		InputName.Contains(TEXT("Landscape")) || InputName.Contains(TEXT("Terrain")))
		Type = EHoudiniInputType::World;
	else if (InputName.Contains(TEXT("Mask")) || InputName.Contains(TEXT("Brush")))
		Type = EHoudiniInputType::Mask;

	if (Type != EHoudiniInputType::Content)  // If type is NOT default(Content), means the type has been specified by InputName
		bOutTypeSpecified = true;

	return Type;
}

const int32& UHoudiniInput::GetParentNodeId() const
{
	// Since inputs' outer must be a AHoudiniNode, we could directly cast outer to AHoudiniNode
	return ((const AHoudiniNode*)GetOuter())->GetNodeId();
}

void UHoudiniInput::SetType(const EHoudiniInputType& InNewType)
{
	if (Type == InNewType)
		return;

	Type = InNewType;  // Update type before destroy holders

	bPendingClear = false;
	for (UHoudiniInputHolder* Holder : Holders)
	{
		if (IsValid(Holder))
		{
			bPendingClear = true;
			Holder->Destroy();  // We will hapi-destroy the input "geo" node, so at here, we just destroy holder and reset holders
		}
	}

	// Reset Holders
	Holders.Empty();
	if (InNewType == EHoudiniInputType::Content)
		Holders.SetNumZeroed((Settings.NumHolders <= 1) ? 1 : Settings.NumHolders);  // Add default null holders for content input as placeholders

	if (bPendingClear)
		((AHoudiniNode*)GetOuter())->TriggerCookByInput(this);
	else
		((AHoudiniNode*)GetOuter())->BroadcastEvent(EHoudiniNodeEvent::RefreshEditorOnly);
}

void UHoudiniInput::UpdateNodeInput(const int32& InputIdx, const FString& InputName)
{
	if (NodeInputIdx < 0)
	{
		bool bTypeSpecified = false;
		Type = ParseTypeFromString(InputName, bTypeSpecified);

		if (Type == EHoudiniInputType::Content)
			Holders.SetNumZeroed(1);
	}

	NodeInputIdx = InputIdx;
	Name = InputName;
}

bool UHoudiniInput::HapiUpdateFromParameter(UHoudiniParameterInput* Parm, const bool& bUpdateTags)
{
	NodeInputIdx = -1;

	if (bUpdateTags)
		HOUDINI_FAIL_RETURN(Settings.HapiParseFromParameterTags(this, GetParentNodeId(), Parm->GetId()));

	if (Name.IsEmpty())  // Means this is a new input
	{
		Name = Parm->GetParameterName();  // Must update name here so that CurvesComponent could find its attributes by this Name

		const auto ForAllLevelActorsLambda = [&](const FString& ActorNameOrLabel, TFunctionRef<bool(AActor*)> Func) -> void
			{
				for (TActorIterator<AActor> ActorIter(GetWorld()); ActorIter; ++ActorIter)
				{
					AActor* Actor = *ActorIter;
					if (IsValid(Actor) && ((Actor->GetActorLabel(false) == ActorNameOrLabel) || (Actor->GetName() == ActorNameOrLabel)))
					{
						if (Func(Actor))
							break;
					}
				}
			};

		bool bTypeSpecified = false;
		Type = ParseTypeFromString(Parm->GetLabel(), bTypeSpecified);

		if (bTypeSpecified && (Type == EHoudiniInputType::World) && (Settings.ActorFilterMethod != EHoudiniActorFilterMethod::Selection))
			HOUDINI_FAIL_RETURN(HapiReimportActorsByFilter())
		else if (!Parm->GetValue().IsEmpty())
		{
			if (bTypeSpecified && (Type == EHoudiniInputType::Curves))
			{
				TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Parm->GetValue());
				TSharedPtr<FJsonObject> JsonCurves;
				if (FJsonSerializer::Deserialize(JsonReader, JsonCurves))
				{
					if (!Holders.IsValidIndex(0))
						Holders.Add(UHoudiniInputCurves::Create(this));

					Cast<UHoudiniInputCurves>(Holders[0])->GetCurvesComponent()->AppendFromJson(JsonCurves);
				}
			}
			else
			{
				TArray<ALandscape*> FoundLandscapes;  // There maybe some ALandscapeStreamingProxy cause import one ALandscape multiple time, so we should record the imported landscapes
				TArray<FString> AssetRefs;
				Parm->GetValue().ParseIntoArray(AssetRefs, TEXT(";"));
				for (const FString& AssetRef : AssetRefs)
				{
					if (bTypeSpecified)
					{
						switch (Type)
						{
						case EHoudiniInputType::Content:
						if (!IS_ASSET_PATH_INVALID(AssetRef))
						{
							UObject* FoundObject = LoadObject<UObject>(nullptr, *AssetRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
							if (IsValid(FoundObject))
							{
								if (AActor* FoundActor = Cast<AActor>(FoundObject))  // Actor also import as asset
								{
									if (FoundActor->GetWorld() == ((AHoudiniNode*)GetOuter())->GetWorld())
									{
										if (ALandscape* FoundLandscape = UHoudiniInputLandscape::TryCast(FoundActor))
										{
											if (!FoundLandscapes.Contains(FoundLandscape))
											{
												FoundLandscapes.AddUnique(FoundLandscape);
												Holders.Add(UHoudiniInputLandscape::Create(this, FoundLandscape));
											}
										}
										else
											Holders.Add(UHoudiniInputActor::Create(this, FoundActor));
									}
								}
								else
								{
									const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& InputBuilders = FHoudiniEngine::Get().GetContentInputBuilders();
									for (int32 BuilderIdx = InputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
									{
										if (UHoudiniInputHolder* NewHolder = InputBuilders[BuilderIdx]->CreateOrUpdate(this, FoundObject, nullptr))
										{
											Holders.Add(NewHolder);
											break;
										}
									}
								}
							}
						}
						break;
						case EHoudiniInputType::World:
						{
							ForAllLevelActorsLambda(AssetRef, [&](AActor* Actor)
								{
									if (ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor))
									{
										if (!FoundLandscapes.Contains(Landscape))
										{
											Holders.Add(UHoudiniInputLandscape::Create(this, Landscape));
											FoundLandscapes.AddUnique(Landscape);
										}
									}
									else
										Holders.Add(UHoudiniInputActor::Create(this, Actor));
									return true;
								});
						}
						break;
						case EHoudiniInputType::Node: if (Holders.IsEmpty())  // Node Input can only have one holder
						{
							for (const TWeakObjectPtr<AHoudiniNode>& Node : FHoudiniEngine::Get().GetCurrentNodes())
							{
								if (Node.IsValid() && (Node->GetActorLabel(false) == AssetRef || Node->GetName() == AssetRef) &&
									CanNodeInput(Node.Get()))
								{
									Holders.Add(UHoudiniInputNode::Create(this, Node.Get()));
									break;
								}
							}
						}
						break;
						case EHoudiniInputType::Mask: if (Holders.IsEmpty())  // Mask Input can only have one holder
						{
							ForAllLevelActorsLambda(AssetRef, [&](AActor* Actor)
								{
									if (ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor))
									{
										Holders.Add(UHoudiniInputMask::Create(this, Landscape));
										return true;
									}
									return false;
								});
						}
						break;
						}
					}
					else  // Type not specified yet, we should get input type from asset type
					{
						if (!IS_ASSET_PATH_INVALID(AssetRef))  // is an asset path, we should just treat it as content input
						{
							UObject* FoundObject = LoadObject<UObject>(nullptr, *AssetRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
							if (IsValid(FoundObject))
							{
								if (AActor* FoundActor = Cast<AActor>(FoundObject))  // Actor also import as asset
								{
									if (FoundActor->GetWorld() == ((AHoudiniNode*)GetOuter())->GetWorld())
									{
										if (ALandscape* FoundLandscape = UHoudiniInputLandscape::TryCast(FoundActor))
										{
											FoundLandscapes.AddUnique(FoundLandscape);
											Holders.Add(UHoudiniInputLandscape::Create(this, FoundLandscape));
										}
										else
											Holders.Add(UHoudiniInputActor::Create(this, FoundActor));

										Type = EHoudiniInputType::World;
										bTypeSpecified = true;
									}
								}
								else
								{
									const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& InputBuilders = FHoudiniEngine::Get().GetContentInputBuilders();
									for (int32 BuilderIdx = InputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
									{
										if (UHoudiniInputHolder* NewHolder = InputBuilders[BuilderIdx]->CreateOrUpdate(this, FoundObject, nullptr))
										{
											Holders.Add(NewHolder);
											Type = EHoudiniInputType::Content;
											bTypeSpecified = true;
											break;
										}
									}
								}
							}
						}	
						else  // Try to traverse all actor to find the corresponding
						{
							ForAllLevelActorsLambda(AssetRef, [&](AActor* Actor)
								{
									AHoudiniNode* Node = Cast<AHoudiniNode>(Actor);
									if (Node && CanNodeInput(Node))  // Ensure there are NO node graph loop
									{
										Holders.Add(UHoudiniInputNode::Create(this, Node));
										Type = EHoudiniInputType::Node;
									}
									else
									{
										if (ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor))
										{
											Holders.Add(UHoudiniInputLandscape::Create(this, Landscape));
											FoundLandscapes.Add(Landscape);
										}
										else
											Holders.Add(UHoudiniInputActor::Create(this, Actor));
										Type = EHoudiniInputType::World;
									}

									bTypeSpecified = true;
									return true;
								});
						}
					}
				}
			}
		}

		if (Type == EHoudiniInputType::Content && Holders.IsEmpty())
			Holders.SetNumZeroed(1);
	}
	else if (bUpdateTags &&
		(Type == EHoudiniInputType::World) && (Settings.ActorFilterMethod != EHoudiniActorFilterMethod::Selection))  // Check if actor filter changed, then we may need reimport actors
		HOUDINI_FAIL_RETURN(HapiReimportActorsByFilter());

	NodeInputIdx = -1;
	Name = Parm->GetParameterName();
	Parm->SetInput(this);

	// Set content input default holder counts
	if (Type == EHoudiniInputType::Content)
	{
		if (Settings.NumHolders >= 1)
		{
			if (Holders.Num() > Settings.NumHolders)
			{
				for (int32 HolderIdx = Holders.Num() - 1; HolderIdx >= Settings.NumHolders; --HolderIdx)
				{
					if (IsValid(Holders[HolderIdx]))
						HOUDINI_FAIL_RETURN(Holders[HolderIdx]->HapiDestroy());
				}
			}

			Holders.SetNumZeroed(Settings.NumHolders);
		}
		else if (Holders.IsEmpty())
			Holders.Add(nullptr);
	}

	return true;
}


bool UHoudiniInput::HapiDestroy()
{
	for (UHoudiniInputHolder* Holder : Holders)
	{
		if (IsValid(Holder))
			Holder->Destroy();  // The parent geo node will be destroyed, so we just call UHoudiniInputHolder::Destroy()
	}

	if (!IsParameter() && MergeNodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DisconnectNodeInput(FHoudiniEngine::Get().GetSession(),
			GetParentNodeId(), NodeInputIdx));
	}

	MergeNodeId = -1;
	MergedNodeCount = 0;
	bPendingClear = false;

	if (GeoNodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), GeoNodeId));
		GeoNodeId = -1;
	}

	return true;
}

bool UHoudiniInput::HasValidHolders() const
{
	for (UHoudiniInputHolder* Holder : Holders)
	{
		if (IsValid(Holder))
			return true;
	}

	return false;
}

void UHoudiniInput::Invalidate()
{
	MergeNodeId = -1;
	GeoNodeId = -1;
	MergedNodeCount = 0;
	bPendingClear = false;

	for (UHoudiniInputHolder* Holder : Holders)
	{
		if (IsValid(Holder))
		{
			Holder->Invalidate();
			Holder->MarkChanged(true);  // Must mark as true
		}
	}
}

bool UHoudiniInput::HapiClear()
{
	Value.Empty();

	if (IsParameter())  // Set operator path to MergeNode
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmNodeValue(FHoudiniEngine::Get().GetSession(), GetParentNodeId(),
			TCHAR_TO_UTF8(*Name), -1));  // Clear parameter value anyway, as parameter may has default value
	}
	else
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DisconnectNodeInput(FHoudiniEngine::Get().GetSession(),
			GetParentNodeId(), NodeInputIdx));
	}

	if (GeoNodeId >= 0)
	{
		const int32 PrevGeoNodeId = GeoNodeId;

		Invalidate();

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), PrevGeoNodeId));
	}

	return true;
}

bool UHoudiniInput::HapiUpload()
{
	const bool bInputPendingClear = bPendingClear;
	bPendingClear = false;

	if (bInputPendingClear)  // We should try to destroy input
		return HapiClear();  // Holders has been reset in UHoudiniInput::RequestClear() or SetType()

	HOUDINI_FAIL_RETURN(HapiCleanupHolders());

	const int32 PreviousNodeId = MergeNodeId;
	if (Type == EHoudiniInputType::Node)
	{
		if (Holders.IsValidIndex(0))
		{
			if (const AHoudiniNode* UpstreamNode = Cast<UHoudiniInputNode>(Holders[0])->GetNode())
				MergeNodeId = UpstreamNode->GetNodeId();
			Holders[0]->MarkChanged(false);
		}
	}
	else
	{
		if (HasValidHolders())
		{
			if (GeoNodeId < 0)  // Create a geo node to contain the input nodes
			{
				const FString GeoNodeLabel = FHoudiniEngineUtils::GetValidatedString(((AHoudiniNode*)GetOuter())->GetActorLabel(false)) +
					TEXT("_") + Name + FString::Printf(TEXT("_%08X"), FPlatformTime::Cycles());
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),
					-1, "Object/geo", TCHAR_TO_UTF8(*GeoNodeLabel), false, &GeoNodeId));

				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetNodeDisplay(FHoudiniEngine::Get().GetSession(),
					GeoNodeId, 0));
			}

			if ((Type == EHoudiniInputType::Content || Type == EHoudiniInputType::World) && MergeNodeId < 0)  // As content and world input could have multiple input objects, we need a merge node to connect all of them
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CreateNode(FHoudiniEngine::Get().GetSession(),
					GeoNodeId, "merge", nullptr, false, &MergeNodeId));
			}
		}

		for (UHoudiniInputHolder* Holder : Holders)
		{
			if (IsValid(Holder) && Holder->HasChanged())
				HOUDINI_FAIL_RETURN(Holder->HapiUpload());
		}
	}

	// Finally, connect merge node to input/operator-path
	if (MergeNodeId >= 0)
	{
		if (!HasValidHolders())
			return HapiClear();

		if (MergeNodeId != PreviousNodeId)  // Only when merge node created or changed then we need reconnect it to parent node
		{
			if (IsParameter())  // Set operator path to MergeNode
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::SetParmNodeValue(FHoudiniEngine::Get().GetSession(), GetParentNodeId(),
					TCHAR_TO_UTF8(*Name), MergeNodeId));

				HAPI_StringHandle ValueSH;
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmStringValue(FHoudiniEngine::Get().GetSession(), GetParentNodeId(),
					TCHAR_TO_UTF8(*Name), 0, false, &ValueSH));

				HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(ValueSH, Value));
			}
			else  // Node input
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(), GetParentNodeId(),
					NodeInputIdx, MergeNodeId, 0));
			}
		}
	}

	return true;
}

bool UHoudiniInput::HapiConnectToMergeNode(const int32& NodeId)
{
	if (Type == EHoudiniInputType::Content || Type == EHoudiniInputType::World)
	{
		if (MergeNodeId >= 0)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::ConnectNodeInput(FHoudiniEngine::Get().GetSession(),
				MergeNodeId, MergedNodeCount, NodeId, 0));
			++MergedNodeCount;
		}
	}
	else
		MergeNodeId = NodeId;  // As the other input types only has one holder, so we just merge node to NodeId

	return true;
}

bool UHoudiniInput::HapiDisconnectFromMergeNode(const int32& NodeId)
{
	if (Type == EHoudiniInputType::Content || Type == EHoudiniInputType::World)
	{
		for (int32 InputIdx = 0; InputIdx < 9999; ++InputIdx)
		{
			int32 ConnectedNodeId = -1;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::QueryNodeInput(FHoudiniEngine::Get().GetSession(),
				MergeNodeId, InputIdx, &ConnectedNodeId));
			if (ConnectedNodeId < 0)
				break;

			if (ConnectedNodeId == NodeId)
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DisconnectNodeInput(FHoudiniEngine::Get().GetSession(),
					MergeNodeId, InputIdx));
				NotifyMergedNodeDestroyed();
				break;
			}
		}
	}
	return true;
}

bool UHoudiniInput::HapiCleanupHolders()
{
	switch (Type)
	{
	case EHoudiniInputType::Content:
	{
		for (int32 HolderIdx = 0; HolderIdx < Holders.Num(); ++HolderIdx)
		{
			if (IsValid(Holders[HolderIdx]) && !Holders[HolderIdx]->IsObjectExists())
			{
				HOUDINI_FAIL_RETURN(Holders[HolderIdx]->HapiDestroy());
				Holders[HolderIdx] = nullptr;
			}
		}
	}
	break;
	}

	return true;
}

void UHoudiniInput::RequestClear()
{
	if (Holders.IsEmpty())
		return;

	bPendingClear = false;
	for (UHoudiniInputHolder* Holder : Holders)
	{
		if (IsValid(Holder))
		{
			bPendingClear = true;
			Holder->Destroy();
		}
	}
	Holders.Empty();

	if (bPendingClear)
	{
		GetNode()->TriggerCookByInput(this);
	}
}

bool UHoudiniInput::HapiRemoveHolder(const int32& RemoveHolderIdx, const bool& bShrink)
{
	if (!Holders.IsValidIndex(RemoveHolderIdx))
		return true;

	UHoudiniInputHolder* Holder = Holders[RemoveHolderIdx];
	if (!IsValid(Holder))  // If this holder is not valid, then we just remove this idx from UHoudiniInput::Holders
	{
		if (bShrink)
			Holders.RemoveAt(RemoveHolderIdx);
		return true;
	}

	bool bHasValidHolder = false;  // Check whether there is any valid holder remains
	for (int32 HolderIdx = 0; HolderIdx < Holders.Num(); ++HolderIdx)
	{
		if (HolderIdx != RemoveHolderIdx && IsValid(Holders[HolderIdx]))
		{
			bHasValidHolder = true;
			break;
		}
	}

	if (bShrink)
	{
		if (!bHasValidHolder)  // If No valid holder remains, then we just clear this input
		{
			RequestClear();
			return true;
		}

		Holders.RemoveAt(RemoveHolderIdx);
	}
	else
		Holders[RemoveHolderIdx] = nullptr;

	HOUDINI_FAIL_RETURN(Holder->HapiDestroy());
	Holder->MarkChanged(true);  // Make sure this holder is marked as changed after undo
	GetNode()->TriggerCookByInput(this);

	return true;
}


bool UHoudiniInput::CanNodeInput(const AHoudiniNode* InputNode) const
{
	const AHoudiniNode* Node = (const AHoudiniNode*)(GetOuter());
	if (Node == InputNode)
		return false;

	class FHoudiniNodeAccessor : public AHoudiniNode
	{
	public:
		bool CheckDownstream(const AHoudiniNode* UpstreamNode, const int32 Depth = 0) const  // if InputNode is in downstream nodes, means: if we set it to Input, then will cause a loop 
		{
			const TArray<TWeakObjectPtr<AHoudiniNode>>& AllNodes = FHoudiniEngine::Get().GetCurrentNodes();
			if (Depth > AllNodes.Num())  // Restrict recursive depth
				return true;

			for (const TWeakObjectPtr<AHoudiniNode>& Node : AllNodes)
			{
				if (!Node.IsValid() || (Node == this))
					continue;

				for (const UHoudiniInput* Input : Node->GetInputs())
				{
					if ((Input->GetType() == EHoudiniInputType::Node) && Input->Holders.IsValidIndex(0) &&
						(GetFName() == Cast<UHoudiniInputNode>(Input->Holders[0])->GetNodeActorName()))
					{
						if (UpstreamNode == Node)  // Already a downstream node, should Never be a upstream node
							return false;

						if (!((const FHoudiniNodeAccessor*)Node.Get())->CheckDownstream(UpstreamNode, Depth + 1))
							return false;
					}
				}
			}

			return true;
		}
	};

	return ((const FHoudiniNodeAccessor*)Node)->CheckDownstream(InputNode);
}

bool UHoudiniInput::HapiImportActors(const TArray<AActor*>& SelectedActors)
{
	if (Type != EHoudiniInputType::World)
		return true;

	// Collect all old holders
	TMap<FName, UHoudiniInputActor*> OldActorHolderMap;
	TMap<FName, UHoudiniInputLandscape*> OldLandscapeHolderMap;
	for (UHoudiniInputHolder* OldHolder : Holders)
	{
		if (UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(OldHolder))
			OldActorHolderMap.Add(ActorInput->GetActorName(), ActorInput);
		else if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(OldHolder))
			OldLandscapeHolderMap.Add(LandscapeInput->GetLandscapeName(), LandscapeInput);
	}

	TArray<UHoudiniInputHolder*> NewHolders;

	// Find the actors that can reused, and collect the new actors and landscapes to import
	TArray<AActor*> Actors;
	TArray<ALandscape*> Landscapes;
	TArray<const ALandscape*> FoundLandscapes;  // Record found landscapes, for Ensure that only unique landscapes can be imported
	for (AActor* Actor : SelectedActors)
	{
		if (ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor))
		{
			if (!FoundLandscapes.Contains(Landscape))  // To avoid import same landscape twice
			{
				if (UHoudiniInputLandscape** FoundLandscapeInputPtr = OldLandscapeHolderMap.Find(Actor->GetFName()))
				{
					NewHolders.Add(*FoundLandscapeInputPtr);
					OldLandscapeHolderMap.Remove(Actor->GetFName());
				}
				else
					Landscapes.AddUnique(Landscape);

				FoundLandscapes.Add(Landscape);
			}
		}
		else  // Treat all rest as actor input
		{
			if (UHoudiniInputActor** FoundActorInputPtr = OldActorHolderMap.Find(Actor->GetFName()))
			{
				NewHolders.Add(*FoundActorInputPtr);
				OldActorHolderMap.Remove(Actor->GetFName());
			}
			else
				Actors.Add(Actor);
		}
	}

	// Construct new holders, try reuse the old holders
	for (AActor* Actor : Actors)
	{
		if (OldActorHolderMap.IsEmpty())
			NewHolders.Add(UHoudiniInputActor::Create(this, Actor));
		else
		{
			OldActorHolderMap.begin()->Value->SetActor(Actor);
			NewHolders.Add(OldActorHolderMap.begin()->Value);
			OldActorHolderMap.Remove(OldActorHolderMap.begin()->Key);
		}
	}

	for (ALandscape* Landscape : Landscapes)
	{
		if (OldLandscapeHolderMap.IsEmpty())
			NewHolders.Add(UHoudiniInputLandscape::Create(this, Landscape));
		else
		{
			OldLandscapeHolderMap.begin()->Value->SetLandscape(Landscape);
			NewHolders.Add(OldLandscapeHolderMap.begin()->Value);
			OldLandscapeHolderMap.Remove(OldLandscapeHolderMap.begin()->Key);
		}
	}

	if (NewHolders.IsEmpty())
	{
		RequestClear();
		return true;
	}

	Holders = NewHolders;

	if (!OldActorHolderMap.IsEmpty() || !OldLandscapeHolderMap.IsEmpty())
	{
		// Finally, destroy the useless holders
		for (const auto ActorInput : OldActorHolderMap)
			HOUDINI_FAIL_RETURN(ActorInput.Value->HapiDestroy());

		for (const auto LandscapeInput : OldLandscapeHolderMap)
			HOUDINI_FAIL_RETURN(LandscapeInput.Value->HapiDestroy());

		GetNode()->TriggerCookByInput(this);
	}

	return true;
}

bool UHoudiniInput::HapiReimportActorsByFilter()
{
	if (Settings.ActorFilterMethod == EHoudiniActorFilterMethod::Selection)
		return true;

	if ((Settings.ActorFilterMethod == EHoudiniActorFilterMethod::Class &&
		Settings.AllowClassNames.IsEmpty() && Settings.DisallowClassNames.IsEmpty()) ||
		(Settings.ActorFilterMethod > EHoudiniActorFilterMethod::Class && !Settings.HasAssetFilters()))  // No Filter Specified, we just clear and HapiDestroy all holders
	{
		RequestClear();
		return true;
	}

	TArray<AActor*> Actors;
	switch (Settings.ActorFilterMethod)
	{
	case EHoudiniActorFilterMethod::Class:
	{
		TArray<const UClass*> AllowClasses, DisallowClasses;
		Settings.GetFilterClasses(AllowClasses, DisallowClasses, AActor::StaticClass());
		for (AActor* Actor : GetNode()->GetLevel()->Actors)
		{
			if (IsValid(Actor) && FHoudiniEngineUtils::FilterClass(AllowClasses, DisallowClasses, Actor->GetClass()))
				Actors.Add(Actor);
		}
	}
	break;
	case EHoudiniActorFilterMethod::Label:
	{
		for (AActor* Actor : GetNode()->GetLevel()->Actors)
		{
			if (IsValid(Actor) && Settings.FilterString(Actor->GetActorLabel(false)))
				Actors.Add(Actor);
		}
	}
	break;
	case EHoudiniActorFilterMethod::Tag:
	{
		for (AActor* Actor : GetNode()->GetLevel()->Actors)
		{
			if (IsValid(Actor) && Settings.FilterActorTags(Actor))
				Actors.Add(Actor);
		}
	}
	break;
	case EHoudiniActorFilterMethod::Folder:
	{
		for (AActor* Actor : GetNode()->GetLevel()->Actors)
		{
			if (IsValid(Actor) && Settings.FilterString(Actor->GetFolderPath().ToString()))
				Actors.Add(Actor);
		}
	}
	break;
	}

	return HapiImportActors(Actors);
}


void FHoudiniEngine::RegisterActorInputDelegates()
{
	if (!OnActorChangedHandle.IsValid())
	{
		OnActorChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda(
			[](UObject* ChangedObject, FPropertyChangedEvent& ChangedEvt)
			{
				//UE_LOG(LogHoudiniEngine, Warning, TEXT("%s: %s changed, type: %d"),
				//	*ChangedObject->GetName(), *ChangedEvt.GetPropertyName().ToString(), int(ChangedEvt.ChangeType));

				if (ChangedEvt.GetPropertyName().IsNone() || (ChangedEvt.ChangeType == EPropertyChangeType::Interactive))  // Is interacting, so we should wait interaction finished
					return;

				if (AActor* Actor = Cast<AActor>(ChangedObject))
				{
					if (Actor->bIsEditorPreviewActor)
						return;

					if (ChangedEvt.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AActor, Tags))
					{
						FOREACH_HOUDINI_INPUT_CHECK_CHANGED(Input->OnActorFilterChanged(Actor, EHoudiniActorFilterMethod::Tag););
					}
					else if (ChangedEvt.GetPropertyName() == FName("ActorLabel"))
					{
						FOREACH_HOUDINI_INPUT_CHECK_CHANGED(
							if (Input->GetSettings().ActorFilterMethod == EHoudiniActorFilterMethod::Label)
								Input->OnActorFilterChanged(Actor, EHoudiniActorFilterMethod::Label);
							else
								Input->OnActorChanged(Actor););
					}
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 5)) || (ENGINE_MAJOR_VERSION > 5)
					else if ((ChangedEvt.GetPropertyName() != FName("TargetLayers")) || !UHoudiniInputLandscape::TryCast(Actor))  // Avoid notify when weight layer created by node output
#else
					else
#endif
					{
						FOREACH_HOUDINI_INPUT_CHECK_CHANGED(Input->OnActorChanged(Actor););
					}
				}
				else if (const UActorComponent* Component = Cast<UActorComponent>(ChangedObject))
				{
					if (ChangedEvt.ChangeType == EPropertyChangeType::Unspecified)
						return;

					const TArray<TSharedPtr<IHoudiniComponentInputBuilder>>& Builders = FHoudiniEngine::Get().GetComponentInputBuilders();
					TMap<TWeakPtr<IHoudiniComponentInputBuilder>, TArray<int32>> BuilderComponentIndicesMap;
					for (int32 BuilderIdx = Builders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
					{
						const TSharedPtr<IHoudiniComponentInputBuilder>& Builder = Builders[BuilderIdx];
						if (Builder->IsValidInput(Component))
						{
							Actor = Component->GetOwner();
							if (IsValid(Actor))
							{
								FOREACH_HOUDINI_INPUT_CHECK_CHANGED(Input->OnActorChanged(Actor););
							}
							else
								FHoudiniEngineUtils::NotifyAssetChanged(Component->GetOuter());  // Maybe a blueprint

							break;
						}
					}
				}
			});
	}

	if (!OnActorFolderChangedHandle.IsValid())
	{
		OnActorFolderChangedHandle = GEngine->OnLevelActorFolderChanged().AddLambda([](const AActor* Actor, FName)
			{
				FOREACH_HOUDINI_INPUT_CHECK_CHANGED(Input->OnActorFilterChanged(const_cast<AActor*>(Actor), EHoudiniActorFilterMethod::Folder););
			});
	}

	if (!OnActorDeletedHandle.IsValid())
	{
		OnActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([](AActor* Actor)
			{
				if (!Actor->bIsEditorPreviewActor)
				{
					const FName DeletedActorName = Actor->GetFName();
					FOREACH_HOUDINI_INPUT(Input->OnActorDeleted(DeletedActorName););
				}
			});
	}

	if (!OnActorAddedHandle.IsValid())
	{
		OnActorAddedHandle = GEngine->OnLevelActorAdded().AddLambda([](AActor* Actor)
			{
				if (IsValid(Actor) && !Actor->bIsEditorPreviewActor && (Actor->GetFlags() != RF_Transient))
				{
					FOREACH_HOUDINI_INPUT_CHECK_CHANGED(Input->OnActorAdded(Actor););
				}
			});
	}
}

void FHoudiniEngineUtils::NotifyAssetChanged(const UObject* Asset)
{
	FOREACH_HOUDINI_INPUT_CHECK_CHANGED(Input->OnAssetChanged(Asset););
}

void UHoudiniInput::Import(UObject* Object)
{
	if (!IsValid(Object))
		return;

	if (AActor* Actor = Cast<AActor>(Object))
	{
		switch (Type)
		{
		case EHoudiniInputType::World:
		{
			if (ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor))
			{
				for (UHoudiniInputHolder* Holder : Holders)
				{
					if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
					{
						if (LandscapeInput->GetLandscapeName() == Actor->GetFName())
						{
							LandscapeInput->RequestReimport();  // If Identical, then just reimport
							return;
						}
					}
				}
				Holders.Add(UHoudiniInputLandscape::Create(this, Landscape));
			}
			else
			{
				for (UHoudiniInputHolder* Holder : Holders)
				{
					if (UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(Holder))
					{
						if (ActorInput->GetActorName() == Actor->GetFName())
						{
							ActorInput->RequestReimport();  // If Identical, then just reimport
							return;
						}
					}
				}
				Holders.Add(UHoudiniInputActor::Create(this, Actor));
			}
		}
		break;
		case EHoudiniInputType::Mask:
			if (ALandscape * Landscape = UHoudiniInputLandscape::TryCast(Actor))
			{
				if (Holders.IsEmpty())
					Holders.Add(UHoudiniInputMask::Create(this, Landscape));
				else if (UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Holders[0]))
				{
					if (MaskInput->GetLandscapeName() == Actor->GetFName())
						MaskInput->RequestReimport();
					else
						MaskInput->SetNewLandscape(Landscape);
				}
			}
			break;
		case EHoudiniInputType::Node:
			if (AHoudiniNode* UpstreamNode = Cast<AHoudiniNode>(Actor))
			{
				if (CanNodeInput(UpstreamNode))
				{
					if (Holders.IsEmpty())
						Holders.Add(UHoudiniInputNode::Create(this, UpstreamNode));
					else if (UHoudiniInputNode* NodeInput = Cast<UHoudiniInputNode>(Holders[0]))
					{
						if (NodeInput->GetNodeActorName() == UpstreamNode->GetFName())
							NodeInput->RequestReimport();
						else
							NodeInput->SetNode(UpstreamNode);
					}
				}
			}
			break;
		}
	}
	else if (Type == EHoudiniInputType::Content)
	{
		bool bHasBeenImported = false;
		for (UHoudiniInputHolder* Holder : Holders)
		{
			if (IsValid(Holder) && (Holder->GetObject() == Object))  // If asset has been imported, then we just reimport it
			{
				Holder->RequestReimport();
				bHasBeenImported = true;
			}
		}
		if (bHasBeenImported)
			return;

		const TArray<TSharedPtr<IHoudiniContentInputBuilder>>& InputBuilders = FHoudiniEngine::Get().GetContentInputBuilders();
		for (int32 BuilderIdx = InputBuilders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
		{
			if (UHoudiniInputHolder* NewHolder = InputBuilders[BuilderIdx]->CreateOrUpdate(this, Object, nullptr))
			{
				if (Holders.IsValidIndex(0) && !IsValid(Holders[0]))
					Holders[0] = NewHolder;
				else
					Holders.Add(NewHolder);
			}
		}
	}
}

void UHoudiniInput::OnActorChanged(const AActor* Actor)
{
	if (Type == EHoudiniInputType::World)
	{
		for (UHoudiniInputHolder* Holder : Holders)
		{
			if (UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(Holder))
			{
				if (ActorInput->GetActorName() == Actor->GetFName())
					ActorInput->RequestReimport();
			}
			else if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
			{
				if (LandscapeInput->GetLandscapeName() == Actor->GetFName())
					LandscapeInput->MarkChanged(true);
			}
		}
	}
	else if ((Type == EHoudiniInputType::Mask) && Holders.IsValidIndex(0))
	{
		if (UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Holders[0]))
		{
			if (MaskInput->GetLandscapeName() == Actor->GetFName())
				MaskInput->MarkChanged(true);
		}
	}
}

void UHoudiniInput::OnActorFilterChanged(AActor* Actor, const EHoudiniActorFilterMethod& ChangeType)
{
	ALandscape* Landscape = UHoudiniInputLandscape::TryCast(Actor);
	if (Landscape && ((AActor*)Landscape != Actor))  // Is a LandscapeStreamingProxy, as it always attached to ALandscape, we need NOT to parse it
		return;

	if (Type == EHoudiniInputType::World && Settings.ActorFilterMethod == ChangeType)
	{
		const FName ActorName = Actor->GetFName();
		const int32 FoundHolderIdx = Holders.IndexOfByPredicate([ActorName](const UHoudiniInputHolder* Holder)
			{
				if (IsValid(Holder))
				{
					if (const UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(Holder))
					{
						if (ActorInput->GetActorName() == ActorName)
							return true;
					}
					else if (const UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
					{
						if (LandscapeInput->GetLandscapeName() == ActorName)
							return true;
					}
				}

				return false;
			});

		bool bShouldImport = false;
		switch (ChangeType)
		{
		case EHoudiniActorFilterMethod::Class:
		{
			TArray<const UClass*> AllowClasses, DisallowClasses;
			Settings.GetFilterClasses(AllowClasses, DisallowClasses, AActor::StaticClass());
			bShouldImport = FHoudiniEngineUtils::FilterClass(AllowClasses, DisallowClasses, Actor->GetClass());
		}
		break;
		case EHoudiniActorFilterMethod::Folder:
			bShouldImport = Settings.FilterString(Actor->GetFolderPath().ToString());
			break;
		case EHoudiniActorFilterMethod::Label:
			bShouldImport = Settings.FilterString(Actor->GetActorLabel(false));
			break;
		case EHoudiniActorFilterMethod::Tag:
			bShouldImport = Settings.FilterActorTags(Actor);
			break;
		}

		if (Holders.IsValidIndex(FoundHolderIdx) && !bShouldImport)
			HOUDINI_FAIL_INVALIDATE_RETURN(HapiRemoveHolder(FoundHolderIdx))
		else if (!Holders.IsValidIndex(FoundHolderIdx) && bShouldImport)
			Holders.Add(Landscape ? (UHoudiniInputHolder*)UHoudiniInputLandscape::Create(this, Landscape) :
				(UHoudiniInputHolder*)UHoudiniInputActor::Create(this, Actor));
		else if ((ChangeType == EHoudiniActorFilterMethod::Label) &&
			Holders.IsValidIndex(FoundHolderIdx) && bShouldImport)
			Holders[FoundHolderIdx]->RequestReimport();  // Because we will import unreal_actor_outliner_path, so label should trigger reimport.
	}
}

void UHoudiniInput::OnActorDeleted(const FName& DeletedActorName)
{
	if (Type == EHoudiniInputType::World)
	{
		const int32 NumPrevHolders = Holders.Num();
		for (int32 HolderIdx = Holders.Num() - 1; HolderIdx >= 0; --HolderIdx)
		{
			if (UHoudiniInputActor* ActorInput = Cast<UHoudiniInputActor>(Holders[HolderIdx]))
			{
				if (ActorInput->GetActorName() == DeletedActorName)
				{
					HOUDINI_FAIL_INVALIDATE(ActorInput->HapiDestroy());
					Holders.RemoveAt(HolderIdx);
				}
			}
			else if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holders[HolderIdx]))
			{
				if (LandscapeInput->GetLandscapeName() == DeletedActorName)
				{
					HOUDINI_FAIL_INVALIDATE(LandscapeInput->HapiDestroy());
					Holders.RemoveAt(HolderIdx);
				}
			}
		}
		if (NumPrevHolders != Holders.Num() && Settings.bCheckChanged)
			GetNode()->TriggerCookByInput(this);
	}
	else if (Holders.IsValidIndex(0))
	{
		if (Type == EHoudiniInputType::Mask)
		{
			if (UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Holders[0]))
			{
				if (MaskInput->GetLandscapeName() == DeletedActorName)
				{
					RequestClear();
				}
			}
		}	
	}
}

void UHoudiniInput::OnActorAdded(AActor* NewActor)
{
	if (Type == EHoudiniInputType::World)
		OnActorFilterChanged(NewActor, Settings.ActorFilterMethod);
}

void UHoudiniInput::OnAssetChanged(const UObject* Object) const
{
	if (Type != EHoudiniInputType::Content || !Settings.bCheckChanged ||
		!IsValid(Object) || !HasValidHolders())
		return;

	const FString ObjectPathName = Object->GetPathName();
	for (UHoudiniInputHolder* Holder : Holders)
	{
		if (IsValid(Holder) && Holder->GetObject().ToString() == ObjectPathName)
			Holder->MarkChanged(true);
	}
}


// -------- Modify input settings --------
void UHoudiniInput::SetImportAsReference(const bool& bImportAsReference)
{
	if (Settings.bImportAsReference != bImportAsReference)
	{
		Settings.bImportAsReference = bImportAsReference;
		for (UHoudiniInputHolder* Holder : Holders)
		{
			if (IsValid(Holder))
				Holder->RequestReimport();
		}
	}
}

void UHoudiniInput::SetCheckChanged(const bool& bCheckChanged)
{
	if (Settings.bCheckChanged != bCheckChanged)
	{
		Settings.bCheckChanged = bCheckChanged;
	}
}

void UHoudiniInput::SetFilterString(const FString& FilterStr)
{
	TArray<FString> Filters;
	TArray<FString> InvertedFilters;
	FHoudiniEngineUtils::ParseFilterPattern(FilterStr, Filters, InvertedFilters);

	const bool bUseClassFilters = (Type == EHoudiniInputType::World) && (Settings.ActorFilterMethod == EHoudiniActorFilterMethod::Class);
	TArray<FString>& SettingFilters = bUseClassFilters ? Settings.AllowClassNames : Settings.Filters;
	TArray<FString>& SettingInvertFolders = bUseClassFilters ? Settings.DisallowClassNames : Settings.InvertedFilters;
	if (SettingFilters != Filters || SettingInvertFolders != InvertedFilters)
	{
		SettingFilters = Filters;
		SettingInvertFolders = InvertedFilters;
		
		if (Type == EHoudiniInputType::World)
			HOUDINI_FAIL_INVALIDATE(HapiReimportActorsByFilter());
	}
}

void UHoudiniInput::ReimportAllMeshInputs() const
{
	if (!Settings.bImportAsReference)
	{
		for (UHoudiniInputHolder* Holder : Holders)
		{
			if (IsValid(Holder) && (Holder->IsA<UHoudiniInputStaticMesh>() ||
				Holder->IsA<UHoudiniInputFoliageType>() || Holder->IsA<UHoudiniInputComponents>() || Holder->IsA<UHoudiniInputSkeletalMesh>()))
				Holder->RequestReimport();
		}
	}
}

void UHoudiniInput::SetImportRenderData(const bool& bImportRenderData)
{
	if (Settings.bImportRenderData != bImportRenderData)
	{
		Settings.bImportRenderData = bImportRenderData;
		ReimportAllMeshInputs();
	}
}

void UHoudiniInput::SetLODImportMethod(const EHoudiniStaticMeshLODImportMethod& LODImportMethod)
{
	if (Settings.LODImportMethod != LODImportMethod)
	{
		Settings.LODImportMethod = LODImportMethod;
		ReimportAllMeshInputs();
	}
}

void UHoudiniInput::SetCollisionImportMethod(const EHoudiniMeshCollisionImportMethod& CollisionImportMethod)
{
	if (Settings.CollisionImportMethod != CollisionImportMethod)
	{
		Settings.CollisionImportMethod = CollisionImportMethod;
		ReimportAllMeshInputs();
	}
}

void UHoudiniInput::SetActorFilterMethod(const EHoudiniActorFilterMethod& ActorFilterMethod)
{
	if (Settings.ActorFilterMethod != ActorFilterMethod)
	{
		Settings.ActorFilterMethod = ActorFilterMethod;
		
		if (ActorFilterMethod >= EHoudiniActorFilterMethod::Class)
			HOUDINI_FAIL_INVALIDATE(HapiReimportActorsByFilter());
	}
}

void UHoudiniInput::SetImportRotAndScale(const bool& bImportRotAndScale)
{
	if (Settings.bImportRotAndScale != bImportRotAndScale)
	{
		Settings.bImportRotAndScale = bImportRotAndScale;
		if ((Type == EHoudiniInputType::Curves) && Holders.IsValidIndex(0))
			Holders[0]->RequestReimport();
	}
}

void UHoudiniInput::SetImportCollisionInfo(const bool& bImportCollisionInfo)
{
	if (Settings.bImportCollisionInfo != bImportCollisionInfo)
	{
		Settings.bImportCollisionInfo = bImportCollisionInfo;
		if (Type == EHoudiniInputType::Curves && Holders.IsValidIndex(0))
			Holders[0]->RequestReimport();
	}
}

void UHoudiniInput::SetImportLandscapeSplines(const bool& bImportLandscapeSplines)
{
	if (Settings.bImportLandscapeSplines != bImportLandscapeSplines)
	{
		Settings.bImportLandscapeSplines = bImportLandscapeSplines;
		for (UHoudiniInputHolder* Holder : Holders)
		{
			if (UHoudiniInputLandscape* LandscapeInput = Cast<UHoudiniInputLandscape>(Holder))
				LandscapeInput->RequestReimport();
		}
	}
}

void UHoudiniInput::SetMaskType(const EHoudiniMaskType& MaskType)
{
	if (Settings.MaskType != MaskType)
	{
		Settings.MaskType = MaskType;
		if (Holders.IsValidIndex(0))
		{
			UHoudiniInputMask* MaskInput = Cast<UHoudiniInputMask>(Holders[0]);
			MaskInput->OnChangedDelegate.Broadcast(false);
			if (MaskInput->HasData())  // Only if has data that we need to reimport this MaskInput
				MaskInput->RequestReimport();
		}
	}
}

#if WITH_EDITOR
void UHoudiniInput::PostEditUndo()
{
	Super::PostEditUndo();

	GetNode()->TriggerCookByInput(this);
}
#endif

#define HAPI_EXIST_PARAMETER_TAG_VALUE(TAG_NAME) FString TagValue;\
	{\
		HAPI_StringHandle TagValueSH;\
		HAPI_Result Result = FHoudiniApi::GetParmTagValue(FHoudiniEngine::Get().GetSession(), NodeId, ParmId, TAG_NAME, &TagValueSH);\
		if (HAPI_SESSION_INVALID_RESULT(Result))\
			return false;\
		if (Result == HAPI_RESULT_SUCCESS)\
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagValueSH, TagValue));\
	}

#define HOUDINI_SET_LINEARCOLOR_CHANNEL_VALUE switch (ChannelIdx)\
	{\
	case 0: CurveLinearColor.R = FCString::Atof(*ValueStr); break;\
	case 1: CurveLinearColor.G = FCString::Atof(*ValueStr); break;\
	case 2: CurveLinearColor.B = FCString::Atof(*ValueStr); break;\
	case 3: CurveLinearColor.A = FCString::Atof(*ValueStr); break;\
	}

const FName FHoudiniInputSettings::AllLandscapeLayerName("all_landscape_layers");

bool FHoudiniInputSettings::HapiParseFromParameterTags(UHoudiniInput* Input, const int32& NodeId, const int32 ParmId)
{
	// First, we should revert some settings that NOT shown in detail panels (Means can NOT be modified by users, can Only modify by hda parm tags)
	Tags.Empty();
	Filters.Empty();
	InvertedFilters.Empty();
	AllowClassNames.Empty();
	DisallowClassNames.Empty();
	bDefaultCurveClosed = false;
	DefaultCurveType = EHoudiniCurveType::Polygon;
	DefaultCurveColor = FColor::White;
	NumHolders = 0;


	// Iter each parm tag
	HAPI_ParmInfo ParmInfo;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmInfo(FHoudiniEngine::Get().GetSession(), NodeId, ParmId, &ParmInfo));
	TMap<FName, FString> NewLandscapeLayerFilterMap;
	for (int32 TagIdx = 0; TagIdx < ParmInfo.tagCount; ++TagIdx)
	{
		std::string TagName;
		{
			HAPI_StringHandle TagNameSH;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetParmTagName(FHoudiniEngine::Get().GetSession(), NodeId, ParmId, TagIdx, &TagNameSH));
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandle(TagNameSH, TagName));
		}

		if (TagName == HAPI_PARM_TAG_CHECK_CHANGED)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_CHECK_CHANGED);
			bCheckChanged = !TagValue.Equals(TEXT("0")) && !TagValue.Equals(TEXT("false"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_PARM_TAG_UNREAL_REF)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_UNREAL_REF);
			bImportAsReference = TagValue.Equals(TEXT("1")) || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_PARM_TAG_UNREAL_REF_FILTER)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_UNREAL_REF_FILTER);
			FHoudiniEngineUtils::ParseFilterPattern(TagValue, Filters, InvertedFilters);
		}
		else if (TagName == HAPI_PARM_TAG_UNREAL_REF_CLASS)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_UNREAL_REF_CLASS);
			FHoudiniEngineUtils::ParseFilterPattern(TagValue, AllowClassNames, DisallowClassNames);
		}
		else if (TagName == HAPI_PARM_TAG_NUM_INPUT_OBJECTS)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_NUM_INPUT_OBJECTS);
			NumHolders = FCString::Atoi(*TagValue);
		}
		else if (TagName == HAPI_PARM_TAG_IMPORT_RENDER_DATA)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_IMPORT_RENDER_DATA);
			bImportRenderData = TagValue.Equals(TEXT("1")) || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_PARM_TAG_LOD_IMPORT_METHOD)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_LOD_IMPORT_METHOD);
			if (TagValue.Equals(TEXT("0")) || TagValue.Equals(TEXT("first_lod"), ESearchCase::IgnoreCase))
				LODImportMethod = EHoudiniStaticMeshLODImportMethod::FirstLOD;
			else if (TagValue.Equals(TEXT("1")) || TagValue.StartsWith(TEXT("last_lod"), ESearchCase::IgnoreCase))
				LODImportMethod = EHoudiniStaticMeshLODImportMethod::LastLOD;
			else if (TagValue.Equals(TEXT("2")) || TagValue.Equals(TEXT("all_lods"), ESearchCase::IgnoreCase))
				LODImportMethod = EHoudiniStaticMeshLODImportMethod::AllLODs;
		}
		else if (TagName == HAPI_PARM_TAG_COLLISION_IMPORT_METHOD)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_COLLISION_IMPORT_METHOD);
			if (TagValue.Equals(TEXT("0")) || TagValue.StartsWith(TEXT("no"), ESearchCase::IgnoreCase))
				CollisionImportMethod = EHoudiniMeshCollisionImportMethod::NoImportCollision;
			else if (TagValue.Equals(TEXT("2")) || TagValue.Contains(TEXT("without"), ESearchCase::IgnoreCase))
				CollisionImportMethod = EHoudiniMeshCollisionImportMethod::ImportWithoutMesh;
			else if (TagValue.Equals(TEXT("1")) || TagValue.Contains(TEXT("with"), ESearchCase::IgnoreCase))
				CollisionImportMethod = EHoudiniMeshCollisionImportMethod::ImportWithMesh;
		}
		else if (TagName == HAPI_CURVE_CLOSED)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_CURVE_CLOSED);
			bDefaultCurveClosed = TagValue.Equals(TEXT("1")) || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_CURVE_TYPE)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_CURVE_TYPE);
			DefaultCurveType = EHoudiniCurveType::Polygon;  // Default is EHoudiniCurveType::Polygon
			if (TagValue.Equals(TEXT("-1")) || TagValue.Equals(TEXT("points"), ESearchCase::IgnoreCase))
				DefaultCurveType = EHoudiniCurveType::Points;
			else if (TagValue.Equals(TEXT("1")) || TagValue.StartsWith(TEXT("subdiv"), ESearchCase::IgnoreCase))
				DefaultCurveType = EHoudiniCurveType::Subdiv;
			else if (TagValue.Equals(TEXT("2")) || TagValue.Equals(TEXT("bezier"), ESearchCase::IgnoreCase))
				DefaultCurveType = EHoudiniCurveType::Bezier;
			else if (TagValue.Equals(TEXT("3")) || TagValue.StartsWith(TEXT("interp"), ESearchCase::IgnoreCase))
				DefaultCurveType = EHoudiniCurveType::Interpolate;
		}
		else if (TagName == HAPI_PARM_TAG_CURVE_COLOR)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_CURVE_COLOR);
			FLinearColor CurveLinearColor = FLinearColor::White;
			FString ValueStr; int32 ChannelIdx = 0;
			for (const TCHAR CH : TagValue)
			{
				if ((TCHAR('0') <= CH && CH <= TCHAR('9')) || CH == '.')
					ValueStr.AppendChar(CH);
				else if (!ValueStr.IsEmpty())
				{
					HOUDINI_SET_LINEARCOLOR_CHANNEL_VALUE;
					ValueStr.Empty();
					if (ChannelIdx == 3)
						break;
					++ChannelIdx;
				}
			}
			if (!ValueStr.IsEmpty())
			{
				HOUDINI_SET_LINEARCOLOR_CHANNEL_VALUE;
			}
			DefaultCurveColor = CurveLinearColor.ToFColor(true);
		}
		else if (TagName == HAPI_PARM_TAG_IMPORT_COLLISION_INFO)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_IMPORT_COLLISION_INFO);
			bImportCollisionInfo = TagValue.Equals(TEXT("1")) || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_PARM_TAG_IMPORT_ROT_AND_SCALE)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_IMPORT_ROT_AND_SCALE);
			bImportRotAndScale = TagValue.Equals(TEXT("1")) || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_PARM_TAG_UNREAL_ACTOR_FILTER_METHOD)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_UNREAL_ACTOR_FILTER_METHOD);
			if (TagValue == TEXT("0") || TagValue.StartsWith(TEXT("selection"), ESearchCase::IgnoreCase))
				ActorFilterMethod = EHoudiniActorFilterMethod::Selection;
			else if (TagValue == TEXT("1") || TagValue.StartsWith(TEXT("class"), ESearchCase::IgnoreCase))
				ActorFilterMethod = EHoudiniActorFilterMethod::Class;
			else if (TagValue == TEXT("2") || TagValue.StartsWith(TEXT("label"), ESearchCase::IgnoreCase))
				ActorFilterMethod = EHoudiniActorFilterMethod::Label;
			else if(TagValue == TEXT("3") || TagValue.StartsWith(TEXT("tag"), ESearchCase::IgnoreCase))
				ActorFilterMethod = EHoudiniActorFilterMethod::Tag;
			else if(TagValue == TEXT("4") || TagValue.StartsWith(TEXT("folder"), ESearchCase::IgnoreCase))
				ActorFilterMethod = EHoudiniActorFilterMethod::Folder;
		}
		else if (TagName == HAPI_PARM_TAG_IMPORT_LANDSCAPE_SPLINES)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_IMPORT_LANDSCAPE_SPLINES);
			bImportLandscapeSplines = TagValue.Equals(TEXT("1")) || TagValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
		else if (TagName == HAPI_PARM_TAG_LANDSCAPE_LAYER)  // Will combine all EditLayers to import, or use for specify layers on non-edit landscapes
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_LANDSCAPE_LAYER);
			NewLandscapeLayerFilterMap.Add(AllLandscapeLayerName, TagValue);
		}
		else if (TagName.starts_with(HAPI_PARM_TAG_PREFIX_UNREAL_LANDSCAPE_EDITLAYER))
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(TagName.c_str());
			NewLandscapeLayerFilterMap.FindOrAdd(FName(TagName.c_str() + strlen(HAPI_PARM_TAG_PREFIX_UNREAL_LANDSCAPE_EDITLAYER)), TagValue);
		}
		else if (TagName == HAPI_PARM_TAG_MASK_TYPE)
		{
			HAPI_EXIST_PARAMETER_TAG_VALUE(HAPI_PARM_TAG_MASK_TYPE);
			if (TagValue == TEXT("0") || TagValue.Equals(TEXT("bit"), ESearchCase::IgnoreCase))
				MaskType = EHoudiniMaskType::Bit;
			else if (TagValue == TEXT("1") || TagValue.Equals(TEXT("weight"), ESearchCase::IgnoreCase))
				MaskType = EHoudiniMaskType::Weight;
			else if (TagValue == TEXT("2") || TagValue.Equals(TEXT("byte"), ESearchCase::IgnoreCase))
				MaskType = EHoudiniMaskType::Byte;
		}
		else
			Tags.Add(TagName);
	}

	if (!NewLandscapeLayerFilterMap.IsEmpty())
		LandscapeLayerFilterMap = NewLandscapeLayerFilterMap;

	return true;
}

bool FHoudiniInputSettings::FilterString(const FString& TargetStr) const
{
	for (const FString& InvertedFilter : InvertedFilters)
	{
		if (TargetStr.Contains(InvertedFilter, ESearchCase::IgnoreCase))
			return false;
	}
	
	if (Filters.IsEmpty() && !InvertedFilters.IsEmpty())
		return true;

	for (const FString& Filter : Filters)
	{
		if (TargetStr.Contains(Filter, ESearchCase::IgnoreCase))
			return true;
	}

	return false;
}

void FHoudiniInputSettings::GetFilterClasses(TArray<const UClass*>& OutAllowClasses, TArray<const UClass*>& OutDisallowClasses, const UClass* ParentClass) const
{
	for (const FString& ClassName : AllowClassNames)
	{
		const UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
		if (IsValid(FoundClass) && (!ParentClass || FoundClass->IsChildOf(ParentClass)))
			OutAllowClasses.AddUnique(FoundClass);
	}
	for (const FString& ClassName : DisallowClassNames)
	{
		const UClass* FoundClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::NativeFirst);
		if (IsValid(FoundClass) && (!ParentClass || FoundClass->IsChildOf(ParentClass)))
			OutDisallowClasses.AddUnique(FoundClass);
	}
}

bool FHoudiniInputSettings::FilterActorTags(const AActor* Actor) const
{
	if (IsValid(Actor))
	{
		for (const FName& Tag : Actor->Tags)
		{
			if (InvertedFilters.Contains(Tag))
				return false;
		}
		for (const FName& Tag : Actor->Tags)
		{
			if (Filters.Contains(Tag))
				return true;
		}
	}

	return false;
}
