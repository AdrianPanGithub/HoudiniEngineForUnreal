// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineCommon.h"

#if WITH_EDITOR
#include "Editor.h"
#include "AssetViewUtils.h"
#endif

#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineSettings.h"


DEFINE_LOG_CATEGORY(LogHoudiniEngine);


FHoudiniActorHolder::FHoudiniActorHolder(AActor* InActor)
{
	Actor = InActor;
	if (IsValid(InActor))
		ActorName = InActor->GetFName();

	FHoudiniEngine::Get().RegisterActor(InActor);
}

AActor* FHoudiniActorHolder::Load() const
{
	if (Actor.IsValid())
		return Actor.Get();

	AActor* FoundActor = FHoudiniEngine::Get().GetActorByName(ActorName);
	if (FoundActor)
		Actor = FoundActor;

	return FoundActor;
}

void FHoudiniActorHolder::Destroy() const
{
	FHoudiniEngine::Get().DestroyActorByName(ActorName);
}


FString IHoudiniPresetHandler::GetPresetFolder() const
{
	return GetDefault<UHoudiniEngineSettings>()->HoudiniEngineFolder + GetPresetPathFilter();
}

void IHoudiniPresetHandler::SavePreset(const FString& SavePresetName) const
{
	if (SavePresetName.IsEmpty())
		return;

	TMap<FName, FHoudiniGenericParameter> Parms;
	if (!GetGenericParameters(Parms))
		return;

	bool bFound = true;
	UDataTable* DT = FHoudiniEngineUtils::FindOrCreateAsset<UDataTable>(GetPresetFolder() / SavePresetName, &bFound);
#if WITH_EDITOR
	if (!bFound && GEditor && GEditor->GetActiveViewport())  // When first time to create preset asset, capture the viewport as asset thumbnail
		AssetViewUtils::CaptureThumbnailFromViewport(GEditor->GetActiveViewport(), TArray<FAssetData>{ DT });
#endif
	TMap<FName, const uint8*> RawData;
	for (TMap<FName, FHoudiniGenericParameter>::TIterator ParmIter(Parms); ParmIter; ++ParmIter)
		RawData.Add(ParmIter->Key, (const uint8*)&ParmIter->Value);
	DT->CreateTableFromRawData(RawData, FHoudiniGenericParameter::StaticStruct());

	DT->Modify();

	FHoudiniEngineUtils::NotifyAssetChanged(DT);  // Maybe there are some inputs fetched this preset
}

void IHoudiniPresetHandler::LoadPreset(const UDataTable* Preset)
{
	if (!IsValid(Preset) || Preset->RowStruct != FHoudiniGenericParameter::StaticStruct())
		return;

	TSharedPtr<TMap<FName, FHoudiniGenericParameter>> Parms = MakeShared<TMap<FName, FHoudiniGenericParameter>>();
	for (const auto& Row : Preset->GetRowMap())
		Parms->Add(Row.Key, *(FHoudiniGenericParameter*)Row.Value);

	SetGenericParameters(Parms);
}

TSharedPtr<FJsonObject> IHoudiniPresetHandler::ConvertParametersToJson(const TMap<FName, FHoudiniGenericParameter>& Parms)
{
	TSharedPtr<FJsonObject> JsonParms = MakeShared<FJsonObject>();
	for (const auto& Parm : Parms)
	{
		switch (Parm.Value.Type)
		{
		case EHoudiniGenericParameterType::Int:
		case EHoudiniGenericParameterType::Float:
		{
			if (Parm.Value.Size == 1)
				JsonParms->SetNumberField(Parm.Key.ToString(), Parm.Value.NumericValues.X);
			else
			{
				const int32 TupleSize = FMath::Min(4, Parm.Value.Size);
				TArray<TSharedPtr<FJsonValue>> JsonValues;
				for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
					JsonValues.Add(MakeShared<FJsonValueNumber>(Parm.Value.NumericValues[TupleIdx]));
				JsonParms->SetArrayField(Parm.Key.ToString(), JsonValues);
			}
		}
		break;
		case EHoudiniGenericParameterType::String:
			JsonParms->SetStringField(Parm.Key.ToString(), Parm.Value.StringValue);
			break;
		case EHoudiniGenericParameterType::Object:
		{
			FString StringData = FHoudiniEngineUtils::GetAssetReference(Parm.Value.ObjectValue.ToSoftObjectPath());
			if (!Parm.Value.StringValue.IsEmpty())  // Import Asset Info
				StringData += TEXT(";") + Parm.Value.StringValue;

			JsonParms->SetStringField(Parm.Key.ToString(), StringData);
		}
		break;
		case EHoudiniGenericParameterType::MultiParm:
		{
			TSharedPtr<FJsonObject> JsonMultiParm = MakeShared<FJsonObject>();
			JsonMultiParm->SetStringField(TEXT("MultiParm"), Parm.Value.StringValue);
			JsonParms->SetObjectField(Parm.Key.ToString(), JsonMultiParm);
		}
		break;
		}
	}

	return JsonParms;
}

TSharedPtr<TMap<FName, FHoudiniGenericParameter>> IHoudiniPresetHandler::ConvertJsonToParameters(const TSharedPtr<FJsonObject>& JsonParms)
{
	TSharedPtr<TMap<FName, FHoudiniGenericParameter>> Parms = MakeShared<TMap<FName, FHoudiniGenericParameter>>();

	for (const auto& JsonParm : JsonParms->Values)
	{
		FHoudiniGenericParameter GenericParm;

		GenericParm.Size = 0;
		switch (JsonParm.Value->Type)
		{
		case EJson::Number:
		{
			GenericParm.Type = EHoudiniGenericParameterType::Float;
			GenericParm.Size = 1;
			GenericParm.NumericValues.X = JsonParm.Value->AsNumber();
		}
		break;
		case EJson::Array:
		{
			GenericParm.Type = EHoudiniGenericParameterType::Float;
			const TArray<TSharedPtr<FJsonValue>>& JsonValues = JsonParm.Value->AsArray();
			GenericParm.Size = FMath::Min(4, JsonValues.Num());
			for (int32 TupleIdx = 0; TupleIdx < GenericParm.Size; ++TupleIdx)
				GenericParm.NumericValues[TupleIdx] = JsonValues[TupleIdx]->AsNumber();
		}
		break;
		case EJson::String:
		{
			GenericParm.Type = EHoudiniGenericParameterType::String;
			GenericParm.Size = 1;
			GenericParm.StringValue = JsonParm.Value->AsString();
		}
		break;
		case EJson::Object:
		{
			FString CountStr;
			if (JsonParm.Value->AsObject()->TryGetStringField(TEXT("MultiParm"), CountStr))
			{
				GenericParm.Type = EHoudiniGenericParameterType::MultiParm;
				GenericParm.Size = 1;
				GenericParm.StringValue = CountStr;
			}
		}
		break;
		}

		if (GenericParm.Size >= 1)
			Parms->Add(*JsonParm.Key, GenericParm);
	}

	return Parms;
}
