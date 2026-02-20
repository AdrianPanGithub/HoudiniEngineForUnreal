// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniParameterAttribute.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"
#include "HoudiniParameters.h"


// -------- UHoudiniParameterAttribute --------
UHoudiniParameterAttribute* UHoudiniParameterAttribute::Create(UObject* InOuter, UHoudiniParameter* Parm, const EHoudiniAttributeOwner& Owner)
{
	if (UHoudiniMultiParameter* MultiParm = Cast<UHoudiniMultiParameter>(Parm))
	{
		UHoudiniMultiParameterAttribute* MultiParmAttrib = NewObject<UHoudiniMultiParameterAttribute>(InOuter,
			FName(TEXT("MultiParmAttrib_") + MultiParm->GetParameterName()), RF_Public | RF_Transactional);

		MultiParmAttrib->Update(MultiParm, Owner);

		return MultiParmAttrib;
	}

	UHoudiniParameterAttribute* ParmAttrib = NewObject<UHoudiniParameterAttribute>(InOuter,
		FName(TEXT("ParmAttrib_") + Parm->GetParameterName()), RF_Public | RF_Transactional);

	ParmAttrib->Update(Parm, Owner);

	return ParmAttrib;
}

const FString& UHoudiniParameterAttribute::GetParameterName() const
{
	return Parm->GetParameterName();
}


bool UHoudiniParameterAttribute::IsParameterMatched(const UHoudiniParameter* InParm) const
{
	return Parm == InParm;
}


#define SHOULD_UNIQUE_STRINGS Parm->GetType() == EHoudiniParameterType::StringChoice
#define CASE_IDENTICAL(STR0, STR1) STR0.Equals(STR1, ESearchCase::CaseSensitive)

static int32 AddUniqueString(TArray<FString>& Strs, const FString& Str)
{
	int32 FoundIdx = Strs.IndexOfByPredicate([&](const FString& CurrStr) { return CASE_IDENTICAL(CurrStr, Str); });
	if (FoundIdx < 0)
		FoundIdx = Strs.Add(Str);
	return FoundIdx;
}


UHoudiniParameterAttribute* UHoudiniParameterAttribute::DuplicateNew() const
{
	if (Storage == EHoudiniStorageType::Invalid)
		return nullptr;
	
	UHoudiniParameterAttribute* NewAttrib = NewObject<UHoudiniParameterAttribute>(GetTransientPackage());
	NewAttrib->Owner = Owner;
	NewAttrib->Storage = Storage;
	NewAttrib->TupleSize = TupleSize;
	NewAttrib->Parm = Parm;
	if ((Storage == EHoudiniStorageType::String) && (SHOULD_UNIQUE_STRINGS))
		NewAttrib->DefaultStringValue = DefaultStringValue;
	else if (Storage == EHoudiniStorageType::Object)
		NewAttrib->DefaultObjectValue = DefaultObjectValue;
	return NewAttrib;
}

void UHoudiniParameterAttribute::ShrinkObjectValues(TArray<int32>& RemovedObjectIndices)
{
	if (RemovedObjectIndices.IsEmpty())
		return;

	RemovedObjectIndices.Sort();
	for (const int32& ObjectIdx : IntValues)
	{
		const int32 FoundIdx = FHoudiniEngineUtils::BinarySearch(RemovedObjectIndices, ObjectIdx);
		if (FoundIdx >= 0)
			RemovedObjectIndices.RemoveAt(FoundIdx);
		if (RemovedObjectIndices.IsEmpty())
			break;
	}
	if (!RemovedObjectIndices.IsEmpty())
	{
		for (int32& ObjectIdx : IntValues)
		{
			const int32 FoundRemovedIdx = FHoudiniEngineUtils::BinarySearch(RemovedObjectIndices, ObjectIdx);
			ObjectIdx += FoundRemovedIdx + 1;
		}
		for (int32 RemovedIdx = RemovedObjectIndices.Num() - 1; RemovedIdx >= 0; --RemovedIdx)
			ObjectValues.RemoveAt(RemovedObjectIndices[RemovedIdx]);
	}
}

void UHoudiniParameterAttribute::ConvertToObjectStorage()
{
	Storage = EHoudiniStorageType::Object;
	ObjectValues.Empty();
	if (IntValues.IsEmpty())
	{
		for (const FString& Str : StringValues)
		{
			int32 ObjectIdx = -1;
			const FSoftObjectPath ObjectValue = Str;
			if (ObjectValue != DefaultObjectValue)
				ObjectIdx = ObjectValues.AddUnique(ObjectValue);
			IntValues.Add(ObjectIdx);
		}
	}
	else  // Indexing strings
	{
		for (int32& StringIdx : IntValues)
		{
			int32 ObjectIdx = -1;
			const FSoftObjectPath ObjectValue = (StringIdx >= 0) ? StringValues[StringIdx] : DefaultStringValue;
			if (ObjectValue != DefaultObjectValue)
				ObjectIdx = ObjectValues.AddUnique(ObjectValue);
			StringIdx = ObjectIdx;
		}
	}
	StringValues.Empty();
	DefaultStringValue.Empty();
}

void UHoudiniParameterAttribute::ShrinkIndexingStringValues(TArray<int32>& RemovedStringIndices)  // Assume that ObjectIdx >= 0
{
	if (RemovedStringIndices.IsEmpty())
		return;

	RemovedStringIndices.Sort();
	for (const int32& StringIdx : IntValues)
	{
		const int32 FoundIdx = FHoudiniEngineUtils::BinarySearch(RemovedStringIndices, StringIdx);
		if (FoundIdx >= 0)
			RemovedStringIndices.RemoveAt(FoundIdx);
		if (RemovedStringIndices.IsEmpty())
			break;
	}
	if (!RemovedStringIndices.IsEmpty())
	{
		for (int32& StringIdx : IntValues)
		{
			const int32 FoundRemovedIdx = FHoudiniEngineUtils::BinarySearch(RemovedStringIndices, StringIdx);
			StringIdx += FoundRemovedIdx + 1;
		}
		for (int32 RemovedIdx = RemovedStringIndices.Num() - 1; RemovedIdx >= 0; --RemovedIdx)
			StringValues.RemoveAt(RemovedStringIndices[RemovedIdx]);
	}
}

void UHoudiniParameterAttribute::UniqueStringStorage()
{
	if (StringValues.IsEmpty())
		return;

	IntValues.Empty();
	IntValues.Reserve(StringValues.Num());
	TArray<FString> UniqueStrs;
	for (const FString& Str : StringValues)
	{
		int32 StringIdx = -1;
		if (Str != DefaultStringValue)
			StringIdx = AddUniqueString(UniqueStrs, Str);
		IntValues.Add(StringIdx);
	}
	StringValues = UniqueStrs;
}


FORCEINLINE static const FSoftObjectPath& GetHoudiniDefaultParmAsset(const UHoudiniParameter* Parm)
{
	class FHoudiniGetDefaultAsset : public UHoudiniParameterAsset
	{
	public:
		FORCEINLINE const FSoftObjectPath& Get() const { return DefaultValue; }
	};

	class FHoudiniGetDefaultAssetChoice : public UHoudiniParameterAssetChoice
	{
	public:
		FORCEINLINE const FSoftObjectPath& Get() const { return DefaultValue; }
	};

	return (Parm->GetType() == EHoudiniParameterType::Asset) ?
		Cast<FHoudiniGetDefaultAsset>(Parm)->Get() : Cast<FHoudiniGetDefaultAssetChoice>(Parm)->Get();
}

void UHoudiniParameterAttribute::PostLoad()
{
	Super::PostLoad();

	if (Storage == EHoudiniStorageType::String)
	{
		if ((Parm->GetType() == EHoudiniParameterType::Asset) || (Parm->GetType() == EHoudiniParameterType::AssetChoice))
		{
			DefaultObjectValue = GetHoudiniDefaultParmAsset(Parm);

			ConvertToObjectStorage();
		}
		else if ((SHOULD_UNIQUE_STRINGS) && IntValues.IsEmpty())
			UniqueStringStorage();
	}
}

static bool HapiRetrieveUniqueStrings(const FString& DefaultStringValue, TArray<FString>& OutStringValues, TArray<int32>& IntValues)
{
	const TArray<HAPI_StringHandle> UniqueSHs = TSet<HAPI_StringHandle>(IntValues).Array();
	HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueSHs, OutStringValues));
	TMap<HAPI_StringHandle, int32> SHIdxMap;
	int32 DefaultIdx = -1;
	for (int32 UniqueIdx = 0; UniqueIdx < UniqueSHs.Num(); ++UniqueIdx)
	{
		if (CASE_IDENTICAL(OutStringValues[UniqueIdx], DefaultStringValue))
		{
			DefaultIdx = UniqueIdx;
			SHIdxMap.Add(UniqueSHs[UniqueIdx], -1);
		}
		else
			SHIdxMap.Add(UniqueSHs[UniqueIdx], (DefaultIdx < 0) ? UniqueIdx : (UniqueIdx - 1));
	}
	if (DefaultIdx >= 0)
		OutStringValues.RemoveAt(DefaultIdx);
	for (int32& SH : IntValues)
		SH = SHIdxMap[SH];

	return true;
}

static bool HapiRetrieveUniqueObjectPaths(const FSoftObjectPath& DefaultObjectValue, TArray<FSoftObjectPath>& OutObjectValues, TArray<HAPI_StringHandle>& InOutSHs)
{
	if (!OutObjectValues.IsEmpty())
		OutObjectValues.Empty();

	TMap<HAPI_StringHandle, int32> SHIdxMap;
	TMap<FSoftObjectPath, int32> ObjectIdxMap;
	if (!FHoudiniEngineUtils::HapiConvertStringHandles(InOutSHs, [&](FUtf8StringView& StrView) -> int32
		{
			int32 SplitIdx;
			if (StrView.FindChar(UTF8CHAR(';'), SplitIdx))
				StrView.LeftInline(SplitIdx);

			const FSoftObjectPath ObjectValue = FString(StrView);
			if (ObjectValue == DefaultObjectValue)
				return -1;
			else if (const int32* FoundIdxPtr = ObjectIdxMap.Find(ObjectValue))  // Maybe str is different, but ref to same object
				return *FoundIdxPtr;
			
			const int32 NewObjectIdx = OutObjectValues.Add(ObjectValue);
			ObjectIdxMap.Add(ObjectValue, NewObjectIdx);
			return NewObjectIdx;
		}, SHIdxMap))
		return false;

	for (int32& SH : InOutSHs)
		SH = SHIdxMap[SH];  // Convert SH to ObjectIdx

	return true;
}

bool UHoudiniParameterAttribute::HapiDuplicateRetrieve(UHoudiniParameterAttribute*& OutNewAttrib, const int32& NodeId, const int32& PartId,
	const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS]) const
{
	OutNewAttrib = nullptr;

	if (Storage == EHoudiniStorageType::Invalid)
		return true;

	const std::string AttribName = TCHAR_TO_UTF8(*GetParameterName());
	
	// Check exists in corresponding class
	if (!FHoudiniEngineUtils::IsAttributeExists(AttribNames, AttribCounts, AttribName, HAPI_AttributeOwner(Owner)))
		return true;

	HAPI_AttributeInfo AttribInfo;
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		AttribName.c_str(), (HAPI_AttributeOwner)Owner, &AttribInfo));

	// Check whether we have the correct attribute
	if (AttribInfo.storage == HAPI_STORAGETYPE_DICTIONARY)  // ParmAttribs does NOT support dict
			return true;

	if (!AttribInfo.exists || (AttribInfo.tupleSize < TupleSize) || FHoudiniEngineUtils::IsArray(AttribInfo.storage) ||
		((int32(FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage)) / 2) != (int32(Storage) / 2)))  // int <-> float | string <-> object
		return true;

	AttribInfo.tupleSize = TupleSize;

	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		OutNewAttrib = DuplicateNew();
		OutNewAttrib->IntValues.SetNumUninitialized(AttribInfo.count * TupleSize);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName.c_str(), &AttribInfo, -1, OutNewAttrib->IntValues.GetData(), 0, AttribInfo.count));
	}
	break;
	case EHoudiniStorageType::Float:
	{
		OutNewAttrib = DuplicateNew();
		OutNewAttrib->FloatValues.SetNumUninitialized(AttribInfo.count * TupleSize);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName.c_str(), &AttribInfo, -1, OutNewAttrib->FloatValues.GetData(), 0, AttribInfo.count));
	}
	break;
	case EHoudiniStorageType::String:
	{
		OutNewAttrib = DuplicateNew();
		TArray<HAPI_StringHandle>& SHs = OutNewAttrib->IntValues;
		SHs.SetNumUninitialized(AttribInfo.count);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName.c_str(), &AttribInfo, SHs.GetData(), 0, AttribInfo.count));
		if (SHOULD_UNIQUE_STRINGS)
		{
			HOUDINI_FAIL_RETURN(HapiRetrieveUniqueStrings(OutNewAttrib->DefaultStringValue, OutNewAttrib->StringValues, SHs));
		}
		else
		{
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, OutNewAttrib->StringValues))
			SHs.Empty();
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		OutNewAttrib = DuplicateNew();
		TArray<HAPI_StringHandle>& SHs = OutNewAttrib->IntValues;
		SHs.SetNumUninitialized(AttribInfo.count);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName.c_str(), &AttribInfo, SHs.GetData(), 0, AttribInfo.count));
		HOUDINI_FAIL_RETURN(HapiRetrieveUniqueObjectPaths(OutNewAttrib->DefaultObjectValue, OutNewAttrib->ObjectValues, SHs));
	}
	break;
	}

	return true;
}

void UHoudiniParameterAttribute::Update(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner)
{
	Parm = InParm;
	TupleSize = InParm->GetSize();

	const EHoudiniStorageType OldStorage = Storage;

	switch (InParm->GetType())
	{
	case EHoudiniParameterType::Separator:
	case EHoudiniParameterType::Folder:
	case EHoudiniParameterType::Button:
		Storage = EHoudiniStorageType::Invalid;
		break;

		// Int types
	case EHoudiniParameterType::Int:
	{
		DefaultIntValues = Cast<UHoudiniParameterInt>(InParm)->GetDefaultValues();
		Storage = EHoudiniStorageType::Int;
	}
	break;
	case EHoudiniParameterType::IntChoice:
	{
		TupleSize = 1;
		DefaultIntValues.SetNumUninitialized(1);
		DefaultIntValues[0] = Cast<UHoudiniParameterIntChoice>(InParm)->GetDefaultValue();
		Storage = EHoudiniStorageType::Int;
	}
	break;
	case EHoudiniParameterType::ButtonStrip:
	{
		TupleSize = 1;
		DefaultIntValues.SetNumUninitialized(1);
		DefaultIntValues[0] = Cast<UHoudiniParameterButtonStrip>(InParm)->GetDefaultValue();
		Storage = EHoudiniStorageType::Int;
	}
	break;
	case EHoudiniParameterType::Toggle:
	{
		TupleSize = 1;
		DefaultIntValues.SetNumUninitialized(1);
		DefaultIntValues[0] = int32(Cast<UHoudiniParameterToggle>(InParm)->GetDefaultValue());
		Storage = EHoudiniStorageType::Int;
	}
	break;
	case EHoudiniParameterType::FolderList:
	{
		if (Cast<UHoudiniParameterFolderList>(InParm)->HasValue())
		{
			TupleSize = 1;
			DefaultIntValues.SetNumUninitialized(1);
			DefaultIntValues[0] = 0;
			Storage = EHoudiniStorageType::Int;
		}
		else
			Storage = EHoudiniStorageType::Invalid;
	}
	break;


	// Float types
	case EHoudiniParameterType::Float:
	{
		DefaultFloatValues = Cast<UHoudiniParameterFloat>(InParm)->GetDefaultValues();
		Storage = EHoudiniStorageType::Float;
	}
	break;
	case EHoudiniParameterType::Color:
	{
		DefaultFloatValues.SetNumUninitialized(TupleSize);
		const FLinearColor& DefaultColor = Cast<UHoudiniParameterColor>(InParm)->GetDefaultValue();
		DefaultFloatValues[0] = DefaultColor.R; DefaultFloatValues[1] = DefaultColor.G; DefaultFloatValues[2] = DefaultColor.B;
		if (TupleSize >= 4)
			DefaultFloatValues[3] = DefaultColor.A;
		Storage = EHoudiniStorageType::Float;
	}
	break;

	// String types
	case EHoudiniParameterType::String:
	{
		if ((OldStorage == EHoudiniStorageType::String) && !IntValues.IsEmpty())  // Previous is indexing strings
		{
			TArray<FString> UniqueStringValues = StringValues;
			StringValues.SetNum(IntValues.Num());
			for (int32 ElemIdx = 0; ElemIdx < IntValues.Num(); ++ElemIdx)
				StringValues[ElemIdx] = (IntValues[ElemIdx] < 0) ? DefaultStringValue : UniqueStringValues[IntValues[ElemIdx]];
			IntValues.Empty();
		}

		TupleSize = 1;
		DefaultStringValue = Cast<UHoudiniParameterString>(InParm)->GetDefaultValue();
		Storage = EHoudiniStorageType::String;
	}
	break;
	case EHoudiniParameterType::StringChoice:
	{
		const FString& NewDefaultValue = Cast<UHoudiniParameterStringChoice>(InParm)->GetDefaultValue();
		if (OldStorage == EHoudiniStorageType::String)
		{
			if (IntValues.IsEmpty())
			{
				DefaultStringValue = NewDefaultValue;
				UniqueStringStorage();
			}
			else  // Already Indexing
			{
				if (!CASE_IDENTICAL(NewDefaultValue, DefaultStringValue))
				{
					int32 FoundNewDefaultIdx = StringValues.IndexOfByKey(NewDefaultValue);
					if (FoundNewDefaultIdx >= 0)
					{
						bool bHasPreviousDefaultValues = false;
						for (int32& StringIdx : IntValues)
						{
							if (StringIdx == FoundNewDefaultIdx)
								StringIdx = -1;
							else if (StringIdx < 0)
							{
								StringIdx = FoundNewDefaultIdx;
								bHasPreviousDefaultValues = true;
							}
						}
						if (bHasPreviousDefaultValues)  // The previous default value should be reused
							StringValues[FoundNewDefaultIdx] = DefaultStringValue;
						else  // Means old default value not been used, we should shrink value indices
						{
							StringValues.RemoveAt(FoundNewDefaultIdx);
							for (int32& StringIdx : IntValues)
							{
								if (StringIdx >= FoundNewDefaultIdx)
									--StringIdx;
							}
						}
					}
					else
					{
						FoundNewDefaultIdx = StringValues.Add(DefaultStringValue);
						for (int32& StringIdx : IntValues)
						{
							if (StringIdx < 0)
								StringIdx = FoundNewDefaultIdx;
						}
					}
					DefaultStringValue = NewDefaultValue;
				}
			}
		}
		else
			DefaultStringValue = NewDefaultValue;

		TupleSize = 1;
		Storage = EHoudiniStorageType::String;
	}
	break;
	case EHoudiniParameterType::Asset:
	case EHoudiniParameterType::AssetChoice:
	{
		TupleSize = 1;
		
		const FSoftObjectPath& NewDefaultObjectValue = GetHoudiniDefaultParmAsset(InParm);
		if ((OldStorage == EHoudiniStorageType::Object) && (DefaultObjectValue != NewDefaultObjectValue) && !ObjectValues.IsEmpty())  // We should re-compress object values with new default
		{
			int32 FoundNewDefaultIdx = ObjectValues.IndexOfByKey(NewDefaultObjectValue);
			if (FoundNewDefaultIdx >= 0)
			{
				bool bHasPreviousDefaultValues = false;
				for (int32& ObjectIdx : IntValues)
				{
					if (ObjectIdx == FoundNewDefaultIdx)
						ObjectIdx = -1;
					else if (ObjectIdx < 0)
					{
						ObjectIdx = FoundNewDefaultIdx;
						bHasPreviousDefaultValues = true;
					}
				}
				if (bHasPreviousDefaultValues)  // The previous default value should be reused
					ObjectValues[FoundNewDefaultIdx] = DefaultObjectValue;
				else  // Means old default value not been used, we should shrink value indices
				{
					ObjectValues.RemoveAt(FoundNewDefaultIdx);
					for (int32& ObjectIdx : IntValues)
					{
						if (ObjectIdx >= FoundNewDefaultIdx)
							--ObjectIdx;
					}
				}
			}
			else
			{
				FoundNewDefaultIdx = ObjectValues.Add(DefaultObjectValue);
				for (int32& ObjectIdx : IntValues)
				{
					if (ObjectIdx < 0)
						ObjectIdx = FoundNewDefaultIdx;
				}
			}
		}  // Else: just reset all default value to new values
		DefaultObjectValue = NewDefaultObjectValue;
		Storage = EHoudiniStorageType::Object;
	}
	break;
	case EHoudiniParameterType::Input:  // Should never see here, converted to Asset with info when parm parsing
		Storage = EHoudiniStorageType::Invalid;
		break;
	case EHoudiniParameterType::Label:
		Storage = EHoudiniStorageType::Invalid;
		break;

		// Extras
	case EHoudiniParameterType::MultiParm: break;
	case EHoudiniParameterType::FloatRamp:
	case EHoudiniParameterType::ColorRamp:
	{
		TupleSize = 1;
		DefaultStringValue = Cast<UHoudiniParameterRamp>(InParm)->GetDefaultValueString();
		Storage = EHoudiniStorageType::String;
	}
	break;
	}

	if (OldStorage != Storage)  // We should also clear previous values, or convert values from previous storage
	{
		switch (Storage)
		{
		case EHoudiniStorageType::Invalid:
		{
			IntValues.Empty();
			DefaultIntValues.Empty();

			FloatValues.Empty();
			DefaultFloatValues.Empty();

			StringValues.Empty();
			DefaultStringValue.Empty();

			ObjectValues.Reset();
			DefaultObjectValue.Reset();
		}
		break;
		case EHoudiniStorageType::Int:
		{
			FloatValues.Empty();
			DefaultFloatValues.Empty();

			StringValues.Empty();
			DefaultStringValue.Empty();

			ObjectValues.Reset();
			DefaultObjectValue.Reset();
		}
		break;
		case EHoudiniStorageType::Float:
		{
			IntValues.Empty();
			DefaultIntValues.Empty();

			StringValues.Empty();
			DefaultStringValue.Empty();

			ObjectValues.Reset();
			DefaultObjectValue.Reset();
		}
		break;
		case EHoudiniStorageType::String:
		{
			FloatValues.Empty();
			DefaultFloatValues.Empty();

			if (OldStorage == EHoudiniStorageType::Object)
			{
				if (SHOULD_UNIQUE_STRINGS)
				{
					for (int32& ObjectIdx : IntValues)
					{
						int32 StringIdx = -1;
						const FString StringValue = FHoudiniEngineUtils::GetAssetReference(GetObjectValue(ObjectIdx));
						if (!CASE_IDENTICAL(StringValue, DefaultStringValue))
							StringIdx = AddUniqueString(StringValues, StringValue);
						ObjectIdx = StringIdx;
					}
				}
				else
				{
					StringValues.SetNum(IntValues.Num());
					for (int32 ElemIdx = 0; ElemIdx < IntValues.Num(); ++ElemIdx)
						StringValues[ElemIdx] = FHoudiniEngineUtils::GetAssetReference(GetObjectValue(ElemIdx));
				}
			}

			ObjectValues.Reset();
			DefaultObjectValue.Reset();
		}
		break;
		case EHoudiniStorageType::Object:
		{
			FloatValues.Empty();
			DefaultFloatValues.Empty();

			if (OldStorage == EHoudiniStorageType::String)
				ConvertToObjectStorage();
			else
			{
				IntValues.Empty();
				DefaultIntValues.Empty();

				StringValues.Empty();
				DefaultStringValue.Empty();
			}
		}
		break;
		}
	}

	if (Owner != InOwner)
	{
		SetNum(0);
		Owner = InOwner;
	}
}

void UHoudiniParameterAttribute::SetNum(const int32& NumElems)
{
	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		const int32 NumOldValues = IntValues.Num();
		if (NumOldValues > NumElems * TupleSize)
			IntValues.SetNum(NumElems * TupleSize);
		else if (NumOldValues < NumElems * TupleSize)
		{
			IntValues.SetNumUninitialized(NumElems * TupleSize);
			for (int32 ValueIdx = NumOldValues; ValueIdx < NumElems * TupleSize; ++ValueIdx)
				IntValues[ValueIdx] = DefaultIntValues[ValueIdx % TupleSize];
		}
	}
	break;
	case EHoudiniStorageType::Float:
	{
		const int32 NumOldValues = FloatValues.Num();
		if (NumOldValues > NumElems * TupleSize)
			FloatValues.SetNum(NumElems * TupleSize);
		else if (NumOldValues < NumElems * TupleSize)
		{
			FloatValues.SetNumUninitialized(NumElems * TupleSize);
			for (int32 ValueIdx = NumOldValues; ValueIdx < NumElems * TupleSize; ++ValueIdx)
				FloatValues[ValueIdx] = DefaultFloatValues[ValueIdx % TupleSize];
		}
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			const int32 NumOldValues = IntValues.Num();
			if (NumOldValues > NumElems)
			{
				if (NumElems <= 0)
				{
					IntValues.Empty();
					StringValues.Empty();
				}
				else
				{
					TArray<int32> RemovedStringIndices(IntValues.GetData() + NumElems, NumOldValues - NumElems);
					IntValues.SetNum(NumElems);
					RemovedStringIndices = TSet<int32>(RemovedStringIndices).Array();
					RemovedStringIndices.RemoveAll([](const int32& StringIdx) { return (StringIdx < 0); });
					ShrinkIndexingStringValues(RemovedStringIndices);
				}
			}
			else if (NumOldValues < NumElems)
			{
				IntValues.SetNumUninitialized(NumElems);
				for (int32 ValueIdx = NumOldValues; ValueIdx < NumElems; ++ValueIdx)
					IntValues[ValueIdx] = -1;
			}
		}
		else
		{
			const int32 NumOldValues = StringValues.Num();
			if (NumOldValues > NumElems)
				StringValues.SetNum(NumElems);
			else if (NumOldValues < NumElems)
			{
				StringValues.SetNum(NumElems);
				for (int32 ValueIdx = NumOldValues; ValueIdx < NumElems; ++ValueIdx)
					StringValues[ValueIdx] = DefaultStringValue;
			}
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		const int32 NumOldValues = IntValues.Num();
		if (NumOldValues > NumElems)
		{
			if (NumElems <= 0)
			{
				IntValues.Empty();
				ObjectValues.Empty();
			}
			else
			{
				TArray<int32> RemovedObjectIndices(IntValues.GetData() + NumElems, NumOldValues - NumElems);
				IntValues.SetNum(NumElems);
				RemovedObjectIndices = TSet<int32>(RemovedObjectIndices).Array();
				RemovedObjectIndices.RemoveAll([](const int32& ObjectIdx) { return (ObjectIdx < 0); });
				ShrinkObjectValues(RemovedObjectIndices);
			}
		}
		else if (NumOldValues < NumElems)
		{
			IntValues.SetNumUninitialized(NumElems);
			for (int32 ValueIdx = NumOldValues; ValueIdx < NumElems; ++ValueIdx)
				IntValues[ValueIdx] = -1;
		}
	}
	break;
	}
}

bool UHoudiniParameterAttribute::Correspond(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner)
{
	if (Parm == InParm)
	{
		Update(InParm, InOwner);
		return true;
	}

	if (Parm && Parm->GetParameterName() == InParm->GetParameterName())
	{
		Update(InParm, InOwner);
		return true;
	}

	return false;
}

void UHoudiniParameterAttribute::DuplicateAppend(const TArray<int32>& DupFromIndices)
{
	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		for (const int32& ElemIdx : DupFromIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			{
				const int32 Value = IntValues[ElemIdx * TupleSize + TupleIdx];
				IntValues.Add(Value);
			}
		}
	}
	break;
	case EHoudiniStorageType::Float:
	{
		for (const int32& ElemIdx : DupFromIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			{
				const float Value = FloatValues[ElemIdx * TupleSize + TupleIdx];
				FloatValues.Add(Value);
			}
		}
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			for (const int32& ElemIdx : DupFromIndices)
			{
				const int32 Value = IntValues[ElemIdx];
				IntValues.Add(Value);
			}
		}
		else
		{
			for (const int32& ElemIdx : DupFromIndices)
			{
				const FString Value = StringValues[ElemIdx];
				StringValues.Add(Value);
			}
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		for (const int32& ElemIdx : DupFromIndices)
		{
			const int32 Value = IntValues[ElemIdx];
			IntValues.Add(Value);
		}
	}
	break;
	}
}

void UHoudiniParameterAttribute::RemoveIndices(const TArray<int32>& RemoveSortedIndices)
{
	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		for (int32 Idx = RemoveSortedIndices.Num() - 1; Idx >= 0; --Idx)
			IntValues.RemoveAt(RemoveSortedIndices[Idx] * TupleSize, TupleSize);
	}
	break;
	case EHoudiniStorageType::Float:
	{
		for (int32 Idx = RemoveSortedIndices.Num() - 1; Idx >= 0; --Idx)
			FloatValues.RemoveAt(RemoveSortedIndices[Idx] * TupleSize, TupleSize);
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			TArray<int32> RemovedStringIndices;
			for (int32 Idx = RemoveSortedIndices.Num() - 1; Idx >= 0; --Idx)
			{
				const int32& ElemIdx = RemoveSortedIndices[Idx];
				if (IntValues[ElemIdx] >= 0)
					RemovedStringIndices.AddUnique(IntValues[ElemIdx]);
				IntValues.RemoveAt(ElemIdx);
			}
			ShrinkIndexingStringValues(RemovedStringIndices);
		}
		else
		{
			for (int32 Idx = RemoveSortedIndices.Num() - 1; Idx >= 0; --Idx)
				StringValues.RemoveAt(RemoveSortedIndices[Idx]);
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		TArray<int32> RemovedObjectIndices;
		for (int32 Idx = RemoveSortedIndices.Num() - 1; Idx >= 0; --Idx)
		{
			const int32& ElemIdx = RemoveSortedIndices[Idx];
			if (IntValues[ElemIdx] >= 0)
				RemovedObjectIndices.AddUnique(IntValues[ElemIdx]);
			IntValues.RemoveAt(ElemIdx);
		}
		ShrinkObjectValues(RemovedObjectIndices);
	}
	break;
	}
}

UHoudiniParameter* UHoudiniParameterAttribute::GetParameter(const int32& ElemIdx) const
{
	if (Storage == EHoudiniStorageType::Invalid)
		return Parm;


	class FHouParmValueIndexAccessor : public UHoudiniParameter
	{
	public:
		FORCEINLINE const int32& Get() const { return ValueIndex; }
		FORCEINLINE void Set(const int32& InValueIdx) { ValueIndex = InValueIdx; }
	};

	const int32 ParmValueIdx = ((FHouParmValueIndexAccessor*)Parm)->Get();  // Backup ValueIndex
	((FHouParmValueIndexAccessor*)Parm)->Set(ElemIdx * TupleSize);

	switch (Parm->GetType())
	{
	case EHoudiniParameterType::Separator:
	case EHoudiniParameterType::Folder:
	case EHoudiniParameterType::Button:
		break;

		// Int types
	case EHoudiniParameterType::Int:
		Cast<UHoudiniParameterInt>(Parm)->UpdateValues(IntValues);
		break;
	case EHoudiniParameterType::IntChoice:
		Cast<UHoudiniParameterIntChoice>(Parm)->UpdateValue(IntValues);
		break;
	case EHoudiniParameterType::ButtonStrip:
		Cast<UHoudiniParameterButtonStrip>(Parm)->UpdateValue(IntValues);
		break;
	case EHoudiniParameterType::Toggle:
		Cast<UHoudiniParameterToggle>(Parm)->UpdateValue(IntValues);
		break;
	case EHoudiniParameterType::FolderList:
	{
		if (Cast<UHoudiniParameterFolderList>(Parm)->HasValue())
			((UHoudiniParameterFolderList*)Parm)->UpdateValue(IntValues);
	}
	break;


	// Float types
	case EHoudiniParameterType::Float:
		Cast<UHoudiniParameterFloat>(Parm)->UpdateValues(FloatValues);
		break;
	case EHoudiniParameterType::Color:
		Cast<UHoudiniParameterColor>(Parm)->UpdateValues(FloatValues);
		break;

	// String types
	case EHoudiniParameterType::String:
		Cast<UHoudiniParameterString>(Parm)->UpdateValue(StringValues);
		break;
	case EHoudiniParameterType::StringChoice:
	{
		class FHoudiniSetParmStringChoice : public UHoudiniParameterStringChoice
		{
		public:
			FORCEINLINE void Set(const FString& InValue) { Value = InValue; }
		};

		Cast<FHoudiniSetParmStringChoice>(Parm)->Set(GetIndexingStringValue(ElemIdx));  // Do NOT call SetAssetPath, as it will trigger cook.
	}
	break;
	case EHoudiniParameterType::Asset:
	{
		class FHoudiniSetParmAsset : public UHoudiniParameterAsset
		{
		public:
			FORCEINLINE void Set(const FSoftObjectPath& InAssetPath) { Value = InAssetPath; }
		};

		Cast<FHoudiniSetParmAsset>(Parm)->Set(GetObjectValue(ElemIdx));  // Do NOT call SetAssetPath, as it will trigger cook.
	}
	break;
	case EHoudiniParameterType::AssetChoice:
	{
		class FHoudiniSetParmAssetChoice : public UHoudiniParameterAssetChoice
		{
		public:
			FORCEINLINE void Set(const FSoftObjectPath& InAssetPath) { Value = InAssetPath; }
		};

		Cast<FHoudiniSetParmAssetChoice>(Parm)->Set(GetObjectValue(ElemIdx));  // Do NOT call SetAssetPath, as it will trigger cook.
	}
	break;
	case EHoudiniParameterType::Input:  // Should never see here, converted to Asset with info when parm parsing
		break;
	case EHoudiniParameterType::Label:
		break;

		// Extras
	case EHoudiniParameterType::MultiParm: break;
	case EHoudiniParameterType::FloatRamp:
	case EHoudiniParameterType::ColorRamp:
		Cast<UHoudiniParameterRamp>(Parm)->ParseFromString(StringValues[ElemIdx]);
		break;
	}

	((FHouParmValueIndexAccessor*)Parm)->Set(ParmValueIdx);

	return Parm;
}

FString UHoudiniParameterAttribute::GetValuesString(TArray<int32> ElemIndices) const
{
	ElemIndices.Sort();  // Sort it, so that houdini could get the corresponding changed values
	FString ValuesString;

	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		for (const int32& ElemIdx : ElemIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				ValuesString += FString::FromInt(IntValues[ElemIdx * TupleSize + TupleIdx]) +
				((TupleIdx == TupleSize - 1) ? TEXT("|") : TEXT(","));
		}
	}
	break;
	case EHoudiniStorageType::Float:
	{
		for (const int32& ElemIdx : ElemIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				ValuesString += FString::SanitizeFloat(FloatValues[ElemIdx * TupleSize + TupleIdx]) +
				((TupleIdx == TupleSize - 1) ? TEXT("|") : TEXT(","));
		}
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			for (const int32& ElemIdx : ElemIndices)
				ValuesString += GetIndexingStringValue(ElemIdx) + TEXT("|");
		}
		else
		{
			for (const int32& ElemIdx : ElemIndices)
				ValuesString += StringValues[ElemIdx] + TEXT("|");
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		for (const int32& ElemIdx : ElemIndices)
			ValuesString += GetObjectValue(ElemIdx).ToString() + TEXT("|");
	}
	break;
	case EHoudiniStorageType::Invalid:
	{
		if (Parm->GetType() == EHoudiniParameterType::Button)
		{
			for (int32 DstIdx = 0; DstIdx < ElemIndices.Num(); ++DstIdx)
				ValuesString += Parm->HasChanged() ? TEXT("1|") : TEXT("0|");
		}
	}
	break;
	}

	ValuesString.RemoveFromEnd(TEXT("|"));

	return ValuesString;
}

void UHoudiniParameterAttribute::UpdateFromParameter(const TArray<int32>& ElemIndices)
{
	if (Storage == EHoudiniStorageType::Invalid)
		return;

	switch (Parm->GetType())
	{
	case EHoudiniParameterType::Separator:
	case EHoudiniParameterType::Folder:
	case EHoudiniParameterType::Button:
		break;

		// Int types
	case EHoudiniParameterType::Int:
	{
		TArray<int32> ElemValues;
		ElemValues.SetNumUninitialized(TupleSize);
		for (int32 ParmValueIdx = 0; ParmValueIdx < TupleSize; ++ParmValueIdx)
			ElemValues[ParmValueIdx] = Cast<UHoudiniParameterInt>(Parm)->GetValue(ParmValueIdx);
		for (const int32& ElemIdx : ElemIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				IntValues[ElemIdx * TupleSize + TupleIdx] = ElemValues[TupleIdx];
		}
	}
	break;
	case EHoudiniParameterType::IntChoice:
	{
		const int32& ElemValue = Cast<UHoudiniParameterIntChoice>(Parm)->GetValue();
		for (const int32& ElemIdx : ElemIndices)
			IntValues[ElemIdx] = ElemValue;
	}
	break;
	case EHoudiniParameterType::ButtonStrip:
	{
		const int32& ElemValue = Cast<UHoudiniParameterButtonStrip>(Parm)->GetValue();
		for (const int32& ElemIdx : ElemIndices)
			IntValues[ElemIdx] = ElemValue;
	}
	break;
	case EHoudiniParameterType::Toggle:
	{
		const int32 ElemValue = Cast<UHoudiniParameterToggle>(Parm)->GetValue() ? 1 : 0;
		for (const int32& ElemIdx : ElemIndices)
			IntValues[ElemIdx] = ElemValue;
	}
	break;
	case EHoudiniParameterType::FolderList:
	{
		if (Cast<UHoudiniParameterFolderList>(Parm)->HasValue())
		{
			const int32& Value = ((UHoudiniParameterFolderList*)Parm)->GetFolderStateValue();
			for (const int32& ElemIdx : ElemIndices)
				IntValues[ElemIdx] = Value;
		}
	}
	break;


	// Float types
	case EHoudiniParameterType::Float:
	{
		TArray<float> ElemValues;
		ElemValues.SetNumUninitialized(TupleSize);
		for (int32 ParmValueIdx = 0; ParmValueIdx < TupleSize; ++ParmValueIdx)
			ElemValues[ParmValueIdx] = Cast<UHoudiniParameterFloat>(Parm)->GetValue(ParmValueIdx);
		for (const int32& ElemIdx : ElemIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				FloatValues[ElemIdx * TupleSize + TupleIdx] = ElemValues[TupleIdx];
		}
	}
	break;
	case EHoudiniParameterType::Color:
	{
		TArray<float> ElemValues;
		ElemValues.SetNumUninitialized(TupleSize);
		const FLinearColor& Color = Cast<UHoudiniParameterColor>(Parm)->GetValue();
		ElemValues[0] = Color.R; ElemValues[1] = Color.G; ElemValues[2] = Color.B;
		if (TupleSize == 4) ElemValues[3] = Color.A;

		for (const int32& ElemIdx : ElemIndices)
		{
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				FloatValues[ElemIdx * TupleSize + TupleIdx] = ElemValues[TupleIdx];
		}
	}
	break;

		// String types
	case EHoudiniParameterType::String:
	{
		const FString& Value = Cast<UHoudiniParameterString>(Parm)->GetValue();
		for (const int32& ElemIdx : ElemIndices)
			StringValues[ElemIdx] = Value;
	}
	break;
	case EHoudiniParameterType::StringChoice:
	{
		const FString& NewStringValue = Cast<UHoudiniParameterStringChoice>(Parm)->GetValue();
		const int32 NewStringIdx = CASE_IDENTICAL(DefaultStringValue, NewStringValue) ? -1 : AddUniqueString(StringValues, NewStringValue);
		TArray<int32> RemovedStringIndices;
		for (const int32& ElemIdx : ElemIndices)
		{
			int32& StringIdx = IntValues[ElemIdx];
			if (StringIdx != NewStringIdx)
			{
				if (StringIdx >= 0)
					RemovedStringIndices.AddUnique(StringIdx);
				StringIdx = NewStringIdx;
			}
		}
		ShrinkIndexingStringValues(RemovedStringIndices);
	}
	break;
	case EHoudiniParameterType::Asset:
	if (const UHoudiniParameterAsset* AssetParm = Cast<UHoudiniParameterAsset>(Parm))
	{
		if (!AssetParm->NeedReimport())  // Only when value truly changed
		{
			const FSoftObjectPath& NewObjectValue = AssetParm->GetAssetPath();
			TArray<int32> RemovedObjectIndices;
			for (const int32& ElemIdx : ElemIndices)
			{
				if (GetObjectValue(ElemIdx) != NewObjectValue)
				{
					int32& ObjectIdx = IntValues[ElemIdx];
					if (ObjectIdx >= 0)
						RemovedObjectIndices.AddUnique(ObjectIdx);
					ObjectIdx = ObjectValues.AddUnique(NewObjectValue);
				}
			}
			ShrinkObjectValues(RemovedObjectIndices);
		}
	}
	break;
	case EHoudiniParameterType::AssetChoice:
	{
		const FSoftObjectPath& NewObjectValue = Cast<UHoudiniParameterAssetChoice>(Parm)->GetAssetPath();
		TArray<int32> RemovedObjectIndices;
		for (const int32& ElemIdx : ElemIndices)
		{
			if (GetObjectValue(ElemIdx) != NewObjectValue)
			{
				int32& ObjectIdx = IntValues[ElemIdx];
				if (ObjectIdx >= 0)
					RemovedObjectIndices.AddUnique(ObjectIdx);
				ObjectIdx = ObjectValues.AddUnique(NewObjectValue);
			}
		}
		ShrinkObjectValues(RemovedObjectIndices);
	}
	break;
	case EHoudiniParameterType::Input:  // Should never see here, converted to Asset with info when parm parsing
		break;
	case EHoudiniParameterType::Label:
		break;

		// Extras
	case EHoudiniParameterType::MultiParm: break;
	case EHoudiniParameterType::FloatRamp:
	case EHoudiniParameterType::ColorRamp:
	{
		const FString ElemValue = Parm->GetValueString();
		for (const int32& ElemIdx : ElemIndices)
			StringValues[ElemIdx] = ElemValue;
	}
	break;
	}
}

void UHoudiniParameterAttribute::SelectIdenticals(TArray<int32>& InOutSelectedIndices) const
{
	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		if (TupleSize == 1)  // This should be the most situation
		{
			TSet<int32> DstValues;
			for (const int32& DstSelectedIdx : InOutSelectedIndices)
				DstValues.FindOrAdd(IntValues[DstSelectedIdx]);

			InOutSelectedIndices.Empty();
			for (int32 ElemIdx = 0; ElemIdx < IntValues.Num(); ++ElemIdx)
			{
				if (DstValues.Contains(IntValues[ElemIdx]))
					InOutSelectedIndices.Add(ElemIdx);
			}
		}
		else
		{
			TSet<TArray<int32>> DstValues;
			for (const int32& DstSelectedIdx : InOutSelectedIndices)
				DstValues.FindOrAdd(TArray<int32>(IntValues.GetData() + DstSelectedIdx * TupleSize, TupleSize));

			InOutSelectedIndices.Empty();
			const int32 NumElems = IntValues.Num() / TupleSize;
			for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
			{
				if (DstValues.Contains(TArray<int32>(IntValues.GetData() + ElemIdx * TupleSize, TupleSize)))
					InOutSelectedIndices.Add(ElemIdx);
			}
		}
	}
	break;
	case EHoudiniStorageType::Float:
	{
		TSet<TArray<float>> DstValues;
		for (const int32& DstSelectedIdx : InOutSelectedIndices)
			DstValues.FindOrAdd(TArray<float>(FloatValues.GetData() + DstSelectedIdx * TupleSize, TupleSize));

		InOutSelectedIndices.Empty();
		const int32 NumElems = FloatValues.Num() / TupleSize;
		for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
		{
			if (DstValues.Contains(TArray<float>(FloatValues.GetData() + ElemIdx * TupleSize, TupleSize)))
				InOutSelectedIndices.Add(ElemIdx);
		}
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			TSet<int32> DstValues;
			for (const int32& DstSelectedIdx : InOutSelectedIndices)
				DstValues.FindOrAdd(IntValues[DstSelectedIdx]);

			InOutSelectedIndices.Empty();
			for (int32 ElemIdx = 0; ElemIdx < IntValues.Num(); ++ElemIdx)
			{
				if (DstValues.Contains(IntValues[ElemIdx]))
					InOutSelectedIndices.Add(ElemIdx);
			}
		}
		else
		{
			TSet<FString> DstValues;
			for (const int32& DstSelectedIdx : InOutSelectedIndices)
				DstValues.FindOrAdd(StringValues[DstSelectedIdx]);

			InOutSelectedIndices.Empty();
			for (int32 ElemIdx = 0; ElemIdx < StringValues.Num(); ++ElemIdx)
			{
				if (DstValues.Contains(StringValues[ElemIdx]))
					InOutSelectedIndices.Add(ElemIdx);
			}
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		TSet<int32> DstValues;
		for (const int32& DstSelectedIdx : InOutSelectedIndices)
			DstValues.FindOrAdd(IntValues[DstSelectedIdx]);

		InOutSelectedIndices.Empty();
		for (int32 ElemIdx = 0; ElemIdx < IntValues.Num(); ++ElemIdx)
		{
			if (DstValues.Contains(IntValues[ElemIdx]))
				InOutSelectedIndices.Add(ElemIdx);
		}
	}
	break;
	}
}

void UHoudiniParameterAttribute::SetDataFrom(const UHoudiniParameterAttribute* SrcAttrib, const TArray<int32>& SrcIndices)
{
	const int32 NumElems = SrcIndices.Num();

	if (!SrcAttrib || !((SrcAttrib->Storage == Storage) && (SrcAttrib->TupleSize == TupleSize)))
	{
		SetNum(NumElems);
		return;
	}

	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		IntValues.SetNumUninitialized(NumElems * TupleSize);
		for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
		{
			const int32& SrcIdx = SrcIndices[ElemIdx];
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				IntValues[ElemIdx * TupleSize + TupleIdx] = SrcAttrib->IntValues[SrcIdx * TupleSize + TupleIdx];
		}
	}
	break;
	case EHoudiniStorageType::Float:
	{
		FloatValues.SetNumUninitialized(NumElems* TupleSize);
		for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
		{
			const int32& SrcIdx = SrcIndices[ElemIdx];
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				FloatValues[ElemIdx * TupleSize + TupleIdx] = SrcAttrib->FloatValues[SrcIdx * TupleSize + TupleIdx];
		}
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			TArray<uint8> bIsStringsUsing;
			bIsStringsUsing.SetNumZeroed(SrcAttrib->StringValues.Num());
			IntValues.SetNumUninitialized(NumElems);
			for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
			{
				IntValues[ElemIdx] = SrcAttrib->IntValues[SrcIndices[ElemIdx]];
				if (IntValues[ElemIdx] >= 0)
					bIsStringsUsing[IntValues[ElemIdx]] = true;
			}
			StringValues.Empty();
			TArray<int32> RemovedStringIndices;
			for (int32 StringIdx = 0; StringIdx < bIsStringsUsing.Num(); ++StringIdx)
			{
				if (bIsStringsUsing[StringIdx])
					StringValues.Add(SrcAttrib->StringValues[StringIdx]);
				else
					RemovedStringIndices.Add(StringIdx);
			}
			if (!RemovedStringIndices.IsEmpty())  // Shrink String indices
			{
				for (int32& StringIdx : IntValues)
					StringIdx += FHoudiniEngineUtils::BinarySearch(RemovedStringIndices, StringIdx) + 1;  // Should always be minus
			}
		}
		else
		{
			StringValues.SetNum(NumElems);
			for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
				StringValues[ElemIdx] = SrcAttrib->StringValues[SrcIndices[ElemIdx]];
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		TArray<uint8> bIsObjectsUsing;
		bIsObjectsUsing.SetNumZeroed(SrcAttrib->ObjectValues.Num());
		IntValues.SetNumUninitialized(NumElems);
		for (int32 ElemIdx = 0; ElemIdx < NumElems; ++ElemIdx)
		{
			IntValues[ElemIdx] = SrcAttrib->IntValues[SrcIndices[ElemIdx]];
			if (IntValues[ElemIdx] >= 0)
				bIsObjectsUsing[IntValues[ElemIdx]] = true;
		}
		ObjectValues.Empty();
		TArray<int32> RemovedObjectIndices;
		for (int32 ObjectIdx = 0; ObjectIdx < bIsObjectsUsing.Num(); ++ObjectIdx)
		{
			if (bIsObjectsUsing[ObjectIdx])
				ObjectValues.Add(SrcAttrib->ObjectValues[ObjectIdx]);
			else
				RemovedObjectIndices.Add(ObjectIdx);
		}
		if (!RemovedObjectIndices.IsEmpty())  // Shrink Object indices
		{
			for (int32& ObjectIdx : IntValues)
				ObjectIdx += FHoudiniEngineUtils::BinarySearch(RemovedObjectIndices, ObjectIdx) + 1;  // Should always be minus
		}
	}
	break;
	}
}

bool UHoudiniParameterAttribute::Join(const UHoudiniParameterAttribute* OtherAttrib)
{
	if ((Storage == OtherAttrib->Storage) && (TupleSize == OtherAttrib->TupleSize))
	{
		switch (Storage)
		{
		case EHoudiniStorageType::Float:
			FloatValues.Append(OtherAttrib->FloatValues);
			break;
		case EHoudiniStorageType::Int:
			IntValues.Append(OtherAttrib->IntValues);
			break;
		case EHoudiniStorageType::String:
		{
			if (SHOULD_UNIQUE_STRINGS)
			{
				TArray<int32> OldNewIdxMap;
				for (const FString& StringValue : OtherAttrib->StringValues)
					OldNewIdxMap.Add(CASE_IDENTICAL(DefaultStringValue, StringValue) ? -1 : AddUniqueString(StringValues, StringValue));
				const int32 DefaultIdx = CASE_IDENTICAL(DefaultStringValue, OtherAttrib->DefaultStringValue) ? -1 : AddUniqueString(StringValues, OtherAttrib->DefaultStringValue);
				for (const int32& OldStringIdx : OtherAttrib->IntValues)
					IntValues.Add((OldStringIdx < 0) ? DefaultIdx : OldNewIdxMap[OldStringIdx]);
			}
			else
				StringValues.Append(OtherAttrib->StringValues);
		}
		break;
		case EHoudiniStorageType::Object:
		{
			TArray<int32> OldNewIdxMap;
			for (const FSoftObjectPath& ObjectValue : OtherAttrib->ObjectValues)
				OldNewIdxMap.Add((DefaultObjectValue == ObjectValue) ? -1 : ObjectValues.AddUnique(ObjectValue));
			const int32 DefaultIdx = (DefaultObjectValue == OtherAttrib->DefaultObjectValue) ? -1 : ObjectValues.AddUnique(OtherAttrib->DefaultObjectValue);
			for (const int32& OldObjectIdx : OtherAttrib->IntValues)
				IntValues.Add((OldObjectIdx < 0) ? DefaultIdx : OldNewIdxMap[OldObjectIdx]);
		}
		break;
		}
		return true;
	}

	return false;
}

void UHoudiniParameterAttribute::UploadInfo_Internal(FHoudiniSharedMemoryGeometryInput& SHMGeoInput, const int32& ArrayCount) const
{
	// See FHoudiniAttribute::UploadInputInfo
	switch (Storage)
	{
	case EHoudiniStorageType::Int:
		SHMGeoInput.AppendAttribute(TCHAR_TO_UTF8(*GetParameterName()), Owner,
			ArrayCount ? EHoudiniInputAttributeStorage::IntArray : EHoudiniInputAttributeStorage::Int, TupleSize, ArrayCount + IntValues.Num());
		break;
	case EHoudiniStorageType::Float:
		SHMGeoInput.AppendAttribute(TCHAR_TO_UTF8(*GetParameterName()), Owner,
			ArrayCount ? EHoudiniInputAttributeStorage::FloatArray : EHoudiniInputAttributeStorage::Float, TupleSize, ArrayCount + FloatValues.Num());
		break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			TArray<float>& CachedStringData = const_cast<UHoudiniParameterAttribute*>(this)->FloatValues;  // Cache string data to FloatValues
			if (!ensure(CachedStringData.IsEmpty()))
				CachedStringData.Empty();
			const EHoudiniInputAttributeCompression Compression = StringValues.IsEmpty() ?
				(ArrayCount ? (!DefaultStringValue.IsEmpty() ? EHoudiniInputAttributeCompression::Indexing : EHoudiniInputAttributeCompression::None) :  // if has value, then we should using indexing
					EHoudiniInputAttributeCompression::UniqueValue) : EHoudiniInputAttributeCompression::Indexing;

			if (Compression == EHoudiniInputAttributeCompression::None)
				CachedStringData.SetNumZeroed(IntValues.Num() / 4 + 1);
			else
			{
				size_t NumChars = 0;

				if (Compression == EHoudiniInputAttributeCompression::Indexing)
				{
					const int32 NumUniqueStrs = StringValues.Num() + 1;
					CachedStringData.Add(*((float*)&NumUniqueStrs));
					NumChars = sizeof(int32);
				}

				auto AppendStringCacheLambda = [&](const FString& Str)
					{
						const std::string StringData = TCHAR_TO_UTF8(*Str);
						const size_t StartPtr = NumChars;
						const size_t DataLength = StringData.length();
						NumChars += DataLength + 1;  // Add '\0'
						if ((NumChars / 4 + 1) > CachedStringData.Num())
							CachedStringData.SetNumZeroed(NumChars / 4 + 1);
						FMemory::Memcpy(((char*)CachedStringData.GetData()) + StartPtr, StringData.c_str(), DataLength);
					};

				
				for (const FString& Str : StringValues)
					AppendStringCacheLambda(Str);
				AppendStringCacheLambda(DefaultStringValue);

				if (Compression == EHoudiniInputAttributeCompression::Indexing)
					CachedStringData.Append((float*)IntValues.GetData(), IntValues.Num());  // append str indices
			}

			SHMGeoInput.AppendAttribute(TCHAR_TO_UTF8(*GetParameterName()), Owner,
				ArrayCount ? EHoudiniInputAttributeStorage::StringArray : EHoudiniInputAttributeStorage::String, TupleSize,
				ArrayCount + CachedStringData.Num(), Compression);  // first value is NumUniqueStrs
		}
		else
		{
			size_t NumChars = 0;
			for (const FString& StrValue : StringValues)
				NumChars += strlen(TCHAR_TO_UTF8(*StrValue)) + 1;  // Add '\0' at end

			SHMGeoInput.AppendAttribute(TCHAR_TO_UTF8(*GetParameterName()), Owner,
				ArrayCount ? EHoudiniInputAttributeStorage::StringArray : EHoudiniInputAttributeStorage::String, TupleSize, ArrayCount + NumChars / 4 + 1);
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		TArray<float>& CachedStringData = const_cast<UHoudiniParameterAttribute*>(this)->FloatValues;  // Cache string data to FloatValues
		if (!ensure(CachedStringData.IsEmpty()))
			CachedStringData.Empty();
		const EHoudiniInputAttributeCompression Compression = ObjectValues.IsEmpty() ?
			(ArrayCount ? (IsValid(DefaultObjectValue.TryLoad()) ? EHoudiniInputAttributeCompression::Indexing : EHoudiniInputAttributeCompression::None) :  // if has value, then we should using indexing
				EHoudiniInputAttributeCompression::UniqueValue) : EHoudiniInputAttributeCompression::Indexing;
		
		if (Compression == EHoudiniInputAttributeCompression::None)
			CachedStringData.SetNumZeroed(IntValues.Num() / 4 + 1);
		else
		{
			size_t NumChars = 0;
			
			if (Compression == EHoudiniInputAttributeCompression::Indexing)
			{
				const int32 NumUniqueStrs = ObjectValues.Num() + 1;
				CachedStringData.Add(*((float*)&NumUniqueStrs));
				NumChars = sizeof(int32);
			}

			auto AppendStringCacheLambda = [&](const FString& ObjectStr)
				{
					const std::string ObjectData = TCHAR_TO_UTF8(*ObjectStr);
					const size_t StartPtr = NumChars;
					const size_t DataLength = ObjectData.length();
					NumChars += DataLength + 1;  // Add '\0'
					if ((NumChars / 4 + 1) > CachedStringData.Num())
						CachedStringData.SetNumZeroed(NumChars / 4 + 1);
					FMemory::Memcpy(((char*)CachedStringData.GetData()) + StartPtr, ObjectData.c_str(), DataLength);
				};
			
			if ((Parm->GetType() == EHoudiniParameterType::Asset) && Cast<UHoudiniParameterAsset>(Parm)->ShouldImportInfo())
			{
				for (int32 ObjectIdx = 0; ObjectIdx <= ObjectValues.Num(); ++ObjectIdx)
				{
					if (const UObject* Asset = (ObjectValues.IsValidIndex(ObjectIdx) ? ObjectValues[ObjectIdx] : DefaultObjectValue).TryLoad())
					{
						FString ObjectStr;
						UHoudiniParameterInput::GetObjectInfo(Asset, ObjectStr);
						if (ObjectStr.IsEmpty())
							ObjectStr = FHoudiniEngineUtils::GetAssetReference(Asset);
						else
							ObjectStr = FHoudiniEngineUtils::GetAssetReference(Asset) + TEXT(";") + ObjectStr;
						AppendStringCacheLambda(ObjectStr);
					}
					else
					{
						++NumChars;  // Add '\0'
						if ((NumChars / 4 + 1) > CachedStringData.Num())
							CachedStringData.SetNumZeroed(NumChars / 4 + 1);
					}
				}
			}
			else
			{
				for (int32 ObjectIdx = 0; ObjectIdx <= ObjectValues.Num(); ++ObjectIdx)
					AppendStringCacheLambda((Parm->GetType() == EHoudiniParameterType::Asset) ?
						FHoudiniEngineUtils::GetAssetReference(ObjectValues.IsValidIndex(ObjectIdx) ? ObjectValues[ObjectIdx] : DefaultObjectValue) :  // See UHoudiniParameterAsset::GetValueString
						(ObjectValues.IsValidIndex(ObjectIdx) ? ObjectValues[ObjectIdx] : DefaultObjectValue).ToString());  // See UHoudiniParameterAssetChoice::GetValueString
			}
			if (Compression == EHoudiniInputAttributeCompression::Indexing)
				CachedStringData.Append((float*)IntValues.GetData(), IntValues.Num());  // append str indices
		}

		SHMGeoInput.AppendAttribute(TCHAR_TO_UTF8(*GetParameterName()), Owner,
			ArrayCount ? EHoudiniInputAttributeStorage::StringArray : EHoudiniInputAttributeStorage::String, TupleSize,
			ArrayCount + CachedStringData.Num(), Compression);  // first value is NumUniqueStrs
	}
	break;
	}
}

void UHoudiniParameterAttribute::UploadData(float*& DataPtr) const
{
	// See FHoudiniAttribute::UploadData
	switch (Storage)
	{
	case EHoudiniStorageType::Float:
	{
		FMemory::Memcpy(DataPtr, FloatValues.GetData(), FloatValues.Num() * sizeof(float));
		DataPtr += FloatValues.Num();
		return;
	}
	case EHoudiniStorageType::Int:
	{
		FMemory::Memcpy(DataPtr, IntValues.GetData(), IntValues.Num() * sizeof(float));
		DataPtr += IntValues.Num();
		return;
	}
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			FMemory::Memcpy(DataPtr, FloatValues.GetData(), FloatValues.Num() * sizeof(float));
			DataPtr += FloatValues.Num();
			const_cast<UHoudiniParameterAttribute*>(this)->FloatValues.Empty();  // Clear cached data
		}
		else
		{
			char* StrDataPtr = (char*)DataPtr;
			size_t NumChars = 0;
			for (const FString& StrValue : StringValues)
			{
				const std::string CString = TCHAR_TO_UTF8(*StrValue);
				const size_t StrLengthWithEnd = CString.length() + 1;
				NumChars += StrLengthWithEnd;
				FMemory::Memcpy(StrDataPtr, CString.c_str(), StrLengthWithEnd - 1);
				StrDataPtr[StrLengthWithEnd - 1] = '\0';
				StrDataPtr += StrLengthWithEnd;
			}
			DataPtr += (NumChars / 4 + 1);
		}
		return;
	}
	case EHoudiniStorageType::Object:
	{
		FMemory::Memcpy(DataPtr, FloatValues.GetData(), FloatValues.Num() * sizeof(float));
		DataPtr += FloatValues.Num();
		const_cast<UHoudiniParameterAttribute*>(this)->FloatValues.Empty();  // Clear cached data
		return;
	}
	}
}

TSharedPtr<FJsonObject> UHoudiniParameterAttribute::ConvertToJson() const
{
	if (IsParameterHolder())
		return nullptr;

	TArray<TSharedPtr<FJsonValue>> JsonValues;
	switch (Storage)
	{
	case EHoudiniStorageType::Int:
	{
		for (const int32& Value : IntValues)
			JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
	}
	break;
	case EHoudiniStorageType::Float:
	{
		for (const float& Value : FloatValues)
			JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
	}
	break;
	case EHoudiniStorageType::String:
	{
		if (SHOULD_UNIQUE_STRINGS)
		{
			for (const int32& StringIdx : IntValues)
				JsonValues.Add(MakeShared<FJsonValueString>((StringIdx < 0) ? DefaultStringValue : StringValues[StringIdx]));
		}
		else
		{
			for (const FString& Value : StringValues)
				JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
	}
	break;
	case EHoudiniStorageType::Object:
	{
		for (const int32& ObjectIdx : IntValues)
			JsonValues.Add(MakeShared<FJsonValueString>((ObjectIdx < 0) ? DefaultObjectValue.ToString() : ObjectValues[ObjectIdx].ToString()));
	}
	break;
	}

	TSharedPtr<FJsonObject> JsonAttrib = MakeShared<FJsonObject>();
	JsonAttrib->SetNumberField(TEXT("Storage"), int32(Storage));
	JsonAttrib->SetNumberField(TEXT("Owner"), int32(Owner));
	JsonAttrib->SetNumberField(TEXT("TupleSize"), TupleSize);
	JsonAttrib->SetArrayField(TEXT("Values"), JsonValues);
	return JsonAttrib;
}

bool UHoudiniParameterAttribute::AppendFromJson(const TSharedPtr<FJsonObject>& JsonAttrib)
{
	if (IsParameterHolder())
		return true;

	int32 Value = 0;
	if (JsonAttrib->TryGetNumberField(TEXT("Storage"), Value)) { if (FMath::Min(Value, 2) != FMath::Min(int32(Storage), 2)) return false; }  // Both String and Object is stored as strings
	else return false;
	if (JsonAttrib->TryGetNumberField(TEXT("Owner"), Value)) { if (Value != int32(Owner)) return false; } else return false;
	if (JsonAttrib->TryGetNumberField(TEXT("TupleSize"), Value)) { if (Value != TupleSize) return false; } else return false;

	const TArray<TSharedPtr<FJsonValue>>* JsonValuesPtr = nullptr;
	if (JsonAttrib->TryGetArrayField(TEXT("Values"), JsonValuesPtr))
	{
		switch (Storage)
		{
		case EHoudiniStorageType::Int:
		{
			for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValuesPtr)
				IntValues.Add(int32(JsonValue->AsNumber()));
		}
		break;
		case EHoudiniStorageType::Float:
		{
			for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValuesPtr)
				FloatValues.Add(float(JsonValue->AsNumber()));
		}
		break;
		case EHoudiniStorageType::String:
		{
			if (SHOULD_UNIQUE_STRINGS)
			{
				for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValuesPtr)
				{
					int32 StringIdx = -1;
					const FString StringValue = JsonValue->AsString();
					if (!CASE_IDENTICAL(StringValue, DefaultStringValue))
						StringIdx = AddUniqueString(StringValues, StringValue);
					IntValues.Add(StringIdx);
				}
			}
			else
			{
				for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValuesPtr)
					StringValues.Add(JsonValue->AsString());
			}
		}
		break;
		case EHoudiniStorageType::Object:
		{
			for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValuesPtr)
			{
				int32 ObjectIdx = -1;
				const FSoftObjectPath ObjectValue = JsonValue->AsString();
				if (ObjectValue != DefaultObjectValue)
					ObjectIdx = ObjectValues.AddUnique(ObjectValue);
				IntValues.Add(ObjectIdx);
			}
		}
		break;
		}

		return true;
	}

	return false;
}



// -------- UHoudiniMultiParameterAttribute --------
void UHoudiniMultiParameterAttribute::ConvertToChildAttributeIndices(const TArray<int32>& InIndices, TArray<int32>& OutChildAttribIndices) const
{
	int32 MaxDupElemIdx = InIndices[0];
	for (const int32& DupElemIdx : InIndices)
	{
		if (MaxDupElemIdx < DupElemIdx)
			MaxDupElemIdx = DupElemIdx;
	}

	// First, we need accumulate ArrayLens to gain the InstsElemIndices
	TArray<int32> InstsElemIndices;
	InstsElemIndices.SetNumUninitialized(MaxDupElemIdx + 1);
	InstsElemIndices[0] = IntValues[0];
	for (int32 ElemIdx = 1; ElemIdx <= MaxDupElemIdx; ++ElemIdx)
		InstsElemIndices[ElemIdx] = InstsElemIndices[ElemIdx - 1] + IntValues[ElemIdx];

	for (const int32& DupElemIdx : InIndices)
	{
		const int32& ArrayLen = IntValues[DupElemIdx];
		if (ArrayLen >= 1)
		{
			for (int32 LocalInstIdx = 0; LocalInstIdx < ArrayLen; ++LocalInstIdx)
				OutChildAttribIndices.Add(InstsElemIndices[DupElemIdx] - ArrayLen + LocalInstIdx);
		}
	}
}


UHoudiniParameterAttribute* UHoudiniMultiParameterAttribute::DuplicateNew() const
{
	UHoudiniMultiParameterAttribute* NewAttrib = NewObject<UHoudiniMultiParameterAttribute>(GetTransientPackage());
	NewAttrib->Owner = Owner;
	NewAttrib->Storage = Storage;
	NewAttrib->TupleSize = TupleSize;
	NewAttrib->Parm = Parm;

	class FHouCopyDefaultAttribValues : public UHoudiniParameterAttribute
	{
	public:
		FORCEINLINE void SetDefaultFloatValues(const TArray<float>& InDefaultFloatValues) { DefaultFloatValues = InDefaultFloatValues; }
		FORCEINLINE void SetDefaultIntValues(const TArray<int32>& InDefaultIntValues) { DefaultIntValues = InDefaultIntValues; }
		FORCEINLINE void SetDefaultStringValue(const FString& InDefaultStringValue) { DefaultStringValue = InDefaultStringValue; }
		FORCEINLINE void SetDefaultObjectValue(const FSoftObjectPath& InDefaultObjectValue) { DefaultObjectValue = InDefaultObjectValue; }

		void CopyTo(UHoudiniParameterAttribute* DstAttrib) const
		{
			switch (Storage)
			{
			case EHoudiniStorageType::Float: ((FHouCopyDefaultAttribValues*)DstAttrib)->SetDefaultFloatValues(DefaultFloatValues); break;
			case EHoudiniStorageType::Int: ((FHouCopyDefaultAttribValues*)DstAttrib)->SetDefaultIntValues(DefaultIntValues); break;
			case EHoudiniStorageType::String: ((FHouCopyDefaultAttribValues*)DstAttrib)->SetDefaultStringValue(DefaultStringValue); break;
			case EHoudiniStorageType::Object: ((FHouCopyDefaultAttribValues*)DstAttrib)->SetDefaultObjectValue(DefaultObjectValue); break;
			}
		}
	};

	for (const UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
	{
		if (UHoudiniParameterAttribute* NewChildAttrib = ChildParmAttrib->DuplicateNew())
		{
			((const FHouCopyDefaultAttribValues*)ChildParmAttrib)->CopyTo(NewChildAttrib);  // Maybe attrib array length are not identical, so we need default value to align
			NewAttrib->ChildParmAttribs.Add(NewChildAttrib);
		}
	}

	return NewAttrib;
}

bool UHoudiniMultiParameterAttribute::HapiDuplicateRetrieve(UHoudiniParameterAttribute*& OutNewAttrib, const int32& NodeId, const int32& PartId,
	const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS]) const
{
	OutNewAttrib = nullptr;

	UHoudiniMultiParameterAttribute* NewAttrib = (UHoudiniMultiParameterAttribute*)DuplicateNew();
	
	class FHouParmAttribAccesor : public UHoudiniParameterAttribute
	{
	public:
		bool HapiRetrieveArrayData(const int32& NodeId, const int32& PartId,
			const TArray<std::string>& AttribNames, const int AttribCounts[NUM_HOUDINI_ATTRIB_OWNERS],
			TArray<int32>& InOutCounts, int32& InOutNumElems)
		{
			if (IsParameterHolder())
				return true;

			const std::string AttribName = TCHAR_TO_UTF8(*GetParameterName());

			// Check in corresponding class
			bool bFound = false;
			if (Owner == EHoudiniAttributeOwner::Point)
			{
				for (int32 AttribIdx = AttribCounts[HAPI_ATTROWNER_VERTEX];
					AttribIdx < AttribCounts[HAPI_ATTROWNER_VERTEX] + AttribCounts[HAPI_ATTROWNER_POINT]; ++AttribIdx)
				{
					if (AttribNames[AttribIdx] == AttribName)
					{
						bFound = true;
						break;
					}
				}
			}
			else
			{
				for (int32 AttribIdx = AttribCounts[HAPI_ATTROWNER_VERTEX] + AttribCounts[HAPI_ATTROWNER_POINT];
					AttribIdx < AttribCounts[HAPI_ATTROWNER_VERTEX] + AttribCounts[HAPI_ATTROWNER_POINT] + AttribCounts[HAPI_ATTROWNER_PRIM]; ++AttribIdx)
				{
					if (AttribNames[AttribIdx] == AttribName)
					{
						bFound = true;
						break;
					}
				}
			}

			// Check storage
			HAPI_AttributeInfo AttribInfo;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				AttribName.c_str(), (HAPI_AttributeOwner)Owner, &AttribInfo));

			// Check whether we have the correct attribute
			if (AttribInfo.storage == HAPI_STORAGETYPE_DICTIONARY_ARRAY)  // ParmAttribs does NOT support dict
				return true;

			if (!AttribInfo.exists || (AttribInfo.tupleSize < TupleSize) || !FHoudiniEngineUtils::IsArray(AttribInfo.storage))
				return true;

			const HAPI_StorageType AttribStorage = HAPI_StorageType(int32(AttribInfo.storage) % HAPI_STORAGETYPE_INT_ARRAY);

			switch (Storage)
			{
			case EHoudiniStorageType::Int:
			{
				if (FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) > EHoudiniStorageType::Float)  // Can be both int and float
					return true;

				IntValues.SetNumUninitialized(AttribInfo.totalArrayElements);
				if (InOutCounts.IsEmpty())
				{
					InOutNumElems = AttribInfo.totalArrayElements / AttribInfo.tupleSize;
					if (AttribInfo.totalArrayElements == 0)
						InOutCounts.SetNumZeroed(AttribInfo.count);
					else
					{
						InOutCounts.SetNumUninitialized(AttribInfo.count);
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
							AttribName.c_str(), &AttribInfo, IntValues.GetData(), AttribInfo.totalArrayElements, InOutCounts.GetData(), 0, AttribInfo.count));
						for (int32& InOutCount : InOutCounts)
							InOutCount /= AttribInfo.tupleSize;  // ArrayCount = tupleSize * arrayLen
					}
				}
				else
				{
					TArray<int32> Counts;  // Useless
					Counts.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						AttribName.c_str(), &AttribInfo, IntValues.GetData(), AttribInfo.totalArrayElements, Counts.GetData(), 0, AttribInfo.count));
				}
			}
			break;
			case EHoudiniStorageType::Float:
			{
				if (FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) > EHoudiniStorageType::Float)  // Can be both int and float
					return true;

				FloatValues.SetNumUninitialized(AttribInfo.totalArrayElements);
				if (InOutCounts.IsEmpty())
				{
					InOutNumElems = AttribInfo.totalArrayElements / AttribInfo.tupleSize;
					if (AttribInfo.totalArrayElements == 0)
						InOutCounts.SetNumZeroed(AttribInfo.count);
					else
					{
						InOutCounts.SetNumUninitialized(AttribInfo.count);
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
							AttribName.c_str(), &AttribInfo, FloatValues.GetData(), AttribInfo.totalArrayElements, InOutCounts.GetData(), 0, AttribInfo.count));
						for (int32& InOutCount : InOutCounts)
							InOutCount /= AttribInfo.tupleSize;  // ArrayCount = tupleSize * arrayLen
					}
				}
				else
				{
					TArray<int32> Counts;  // Useless
					Counts.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						AttribName.c_str(), &AttribInfo, FloatValues.GetData(), AttribInfo.totalArrayElements, Counts.GetData(), 0, AttribInfo.count));
				}
			}
			break;
			case EHoudiniStorageType::String:
			{
				if (FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage) != EHoudiniStorageType::String)
					return true;

				TArray<HAPI_StringHandle>& SHs = IntValues;
				SHs.SetNumUninitialized(AttribInfo.totalArrayElements);
				if (InOutCounts.IsEmpty())
				{
					InOutNumElems = AttribInfo.totalArrayElements;
					if (AttribInfo.totalArrayElements == 0)
						InOutCounts.SetNumZeroed(AttribInfo.count);
					else
					{
						InOutCounts.SetNumUninitialized(AttribInfo.count);
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
							AttribName.c_str(), &AttribInfo, SHs.GetData(), AttribInfo.totalArrayElements, InOutCounts.GetData(), 0, AttribInfo.count));
						if (SHOULD_UNIQUE_STRINGS)
							HOUDINI_FAIL_RETURN(HapiRetrieveUniqueStrings(DefaultStringValue, StringValues, SHs))
						else
							HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, StringValues))
						for (int32& InOutCount : InOutCounts)
							InOutCount /= AttribInfo.tupleSize;  // ArrayCount = tupleSize * arrayLen
					}
				}
				else
				{
					TArray<int32> Counts;  // Useless
					Counts.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						AttribName.c_str(), &AttribInfo, SHs.GetData(), AttribInfo.totalArrayElements, Counts.GetData(), 0, AttribInfo.count));
					HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, StringValues));
					if (SHOULD_UNIQUE_STRINGS)
						HOUDINI_FAIL_RETURN(HapiRetrieveUniqueStrings(DefaultStringValue, StringValues, SHs))
					else
						HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, StringValues))
				}
			}
			break;
			case EHoudiniStorageType::Object:
			{
				TArray<HAPI_StringHandle>& SHs = IntValues;
				SHs.SetNumUninitialized(AttribInfo.totalArrayElements);
				if (InOutCounts.IsEmpty())
				{
					InOutNumElems = AttribInfo.totalArrayElements;
					if (AttribInfo.totalArrayElements == 0)
						InOutCounts.SetNumZeroed(AttribInfo.count);
					else
					{
						InOutCounts.SetNumUninitialized(AttribInfo.count);
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
							AttribName.c_str(), &AttribInfo, SHs.GetData(), AttribInfo.totalArrayElements, InOutCounts.GetData(), 0, AttribInfo.count));
						HOUDINI_FAIL_RETURN(HapiRetrieveUniqueObjectPaths(DefaultObjectValue, ObjectValues, SHs));
						for (int32& InOutCount : InOutCounts)
							InOutCount /= AttribInfo.tupleSize;  // ArrayCount = tupleSize * arrayLen
					}
				}
				else
				{
					TArray<int32> Counts;  // Useless
					Counts.SetNumUninitialized(AttribInfo.count);
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						AttribName.c_str(), &AttribInfo, SHs.GetData(), AttribInfo.totalArrayElements, Counts.GetData(), 0, AttribInfo.count));
					HOUDINI_FAIL_RETURN(HapiRetrieveUniqueObjectPaths(DefaultObjectValue, ObjectValues, SHs));
				}
			}
			break;
			}

			return true;
		}
	};

	int32 NumArrayElems = -1;
	for (UHoudiniParameterAttribute* ChildParmAttrib : NewAttrib->ChildParmAttribs)
		HOUDINI_FAIL_RETURN(((FHouParmAttribAccesor*)ChildParmAttrib)->HapiRetrieveArrayData(NodeId, PartId, AttribNames, AttribCounts,
			NewAttrib->IntValues, NumArrayElems));

	if (NumArrayElems >= 0)  // We need NOT record empty values 
	{
		// TODO: Align values in HapiRetrieveArrayData()
		for (UHoudiniParameterAttribute* ChildParmAttrib : NewAttrib->ChildParmAttribs)
			ChildParmAttrib->SetNum(NumArrayElems);

		OutNewAttrib = NewAttrib;
	}

	return true;
}

bool UHoudiniMultiParameterAttribute::IsParameterMatched(const UHoudiniParameter* InParm) const
{
	if (Parm == InParm)
		return true;

	return ((UHoudiniMultiParameter*)Parm)->ChildAttribParmInsts.Contains(InParm);
}

void UHoudiniMultiParameterAttribute::Update(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner)
{
	const UHoudiniMultiParameter* MultiParm = Cast<const UHoudiniMultiParameter>(InParm);

	Parm = InParm;
	TupleSize = 1;
	Storage = EHoudiniStorageType::Int;
	DefaultIntValues = TArray<int32>{ MultiParm->GetDefaultInstanceCount() };

	TArray<UHoudiniParameterAttribute*> NewParmAttribs;
	for (UHoudiniParameter* ChildAttribParm : MultiParm->GetChildAttributeParameters())
	{
		// We should try to reuse the previous corresponding ChildParmAttrib
		if (TObjectPtr<UHoudiniParameterAttribute>* FoundParmAttribPtr = ChildParmAttribs.IsEmpty() ? nullptr : ChildParmAttribs.FindByPredicate(
			[ChildAttribParm, &InOwner](UHoudiniParameterAttribute* ChildParmAttrib) { return ChildParmAttrib->Correspond(ChildAttribParm, InOwner); }))
			NewParmAttribs.Add(*FoundParmAttribPtr);
		else
			NewParmAttribs.Add(UHoudiniParameterAttribute::Create(GetOuter(), ChildAttribParm, InOwner));
	}

	ChildParmAttribs = NewParmAttribs;

	if (Owner != InOwner)
	{
		SetNum(0);
		Owner = InOwner;
	}
}

void UHoudiniMultiParameterAttribute::SetNum(const int32& NumElems)
{
	const int32 NumOldValues = IntValues.Num();
	if (NumOldValues > NumElems)
		IntValues.SetNum(NumElems);
	else if (NumOldValues < NumElems)
	{
		IntValues.SetNumUninitialized(NumElems);
		for (int32 ValueIdx = NumOldValues; ValueIdx < NumElems; ++ValueIdx)
			IntValues[ValueIdx] = DefaultIntValues[0];
	}

	int32 NumTotalElems = 0;
	for (const int32& Num : IntValues)
		NumTotalElems += Num;

	for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
		ChildParmAttrib->SetNum(NumTotalElems);
}

bool UHoudiniMultiParameterAttribute::Correspond(UHoudiniParameter* InParm, const EHoudiniAttributeOwner& InOwner)
{
	const UHoudiniMultiParameter* MultiParm = Cast<const UHoudiniMultiParameter>(InParm);
	if (!IsValid(InParm))
		return false;

	return UHoudiniParameterAttribute::Correspond(InParm, InOwner);  // will call UHoudiniMultiParameterAttribute::Update() to update ChildParmAttribs
}

void UHoudiniMultiParameterAttribute::DuplicateAppend(const TArray<int32>& DupFromIndices)
{
	if (DupFromIndices.IsEmpty())
		return;

	TArray<int32> InstsElemIndices;
	ConvertToChildAttributeIndices(DupFromIndices, InstsElemIndices);

	for (const int32& DupElemIdx : DupFromIndices)
	{
		const int32 ArrayLen = IntValues[DupElemIdx];
		IntValues.Add(ArrayLen);
	}

	if (!InstsElemIndices.IsEmpty())
	{
		for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
			ChildParmAttrib->DuplicateAppend(InstsElemIndices);
	}
}

void UHoudiniMultiParameterAttribute::RemoveIndices(const TArray<int32>& RemoveSortedIndices)
{
	if (RemoveSortedIndices.IsEmpty())
		return;

	TArray<int32> InstElemIndices;
	int32 CurrInstIdx = 0;
	int32 LastElemIdx = 0;
	for (const int32& RemoveElemIdx : RemoveSortedIndices)
	{
		for (int32 ElemIdx = LastElemIdx; ElemIdx < RemoveElemIdx; ++ElemIdx)
			CurrInstIdx += IntValues[ElemIdx];  // Calculate the curr inst idx by accumulating all prev arraylens(IntValues)

		for (int32 LocalIdx = 0; LocalIdx < IntValues[RemoveElemIdx]; ++LocalIdx)
			InstElemIndices.Add(CurrInstIdx + LocalIdx);  // Add all inst elem indices

		LastElemIdx = RemoveElemIdx;  // Update LastElemIdx to tell that we've already accumulated arraylens to this idx.
	}

	for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
		ChildParmAttrib->RemoveIndices(InstElemIndices);

	for (int32 Idx = RemoveSortedIndices.Num() - 1; Idx >= 0; --Idx)
		IntValues.RemoveAt(RemoveSortedIndices[Idx]);
}

FString UHoudiniMultiParameterAttribute::GetValuesString(TArray<int32> ElemIndices) const
{
	ElemIndices.Sort();
	FString ValuesString;

	for (const int32& ElemIdx : ElemIndices)
	{
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			ValuesString += FString::FromInt(IntValues[ElemIdx * TupleSize + TupleIdx]) +
			((TupleIdx == TupleSize - 1) ? TEXT("|") : TEXT(","));
	}

	ValuesString.RemoveFromEnd(TEXT("|"));

	return ValuesString;
}

UHoudiniParameter* UHoudiniMultiParameterAttribute::GetParameter(const int32& SrcElemIdx) const
{
	const int32& ArrayLen = IntValues[SrcElemIdx];

	UHoudiniMultiParameter* MultiParm = Cast<UHoudiniMultiParameter>(Parm);
	
	const TArray<UHoudiniParameter*>& ChildParms = MultiParm->GetChildAttributeParameters();
	TArray<TObjectPtr<UHoudiniParameter>>& ChildParmInsts = MultiParm->ChildAttribParmInsts;
	if (ChildParms.IsEmpty() || (ArrayLen <= 0))
		ChildParmInsts.Empty();
	else
	{
		int32 InstElemIdx = 0;
		for (int32 ElemIdx = 0; ElemIdx < SrcElemIdx; ++ElemIdx)
			InstElemIdx += IntValues[ElemIdx];

		class FHouParmInstanceIndexAccessor : public UHoudiniParameter
		{
		public:
			FORCEINLINE void Set(const int32& NewMultiParmIdx) { MultiParmInstanceIndex = NewMultiParmIdx; }
		};

		class FHouParmAttribParmModifier : public UHoudiniParameterAttribute
		{
		public:
			FORCEINLINE void Set(UHoudiniParameter* const InParm) { Parm = InParm; }
		};

		// We should consider the situation that child attribs and parms may NOT corresponding when on edit outputs
		TArray<UHoudiniParameterAttribute*> CorrespondingAttribs;
		for (const UHoudiniParameter* ChildParm : ChildParms)
		{
			if (TObjectPtr<UHoudiniParameterAttribute> const* FoundAttribPtr = ChildParmAttribs.FindByPredicate(
				[ChildParm](const UHoudiniParameterAttribute* ChildAttrib) { return ChildAttrib->GetParameter() == ChildParm; }))
				CorrespondingAttribs.Add(*FoundAttribPtr);
			else
				CorrespondingAttribs.Add(nullptr);
		}

		const int32 StartMultiParmIdx = ChildParms[0]->GetMultiParmInstanceIndex();
		TArray<UHoudiniParameter*> NewChildParmInsts;
		for (int32 DupIdx = 0; DupIdx < ArrayLen; ++DupIdx)
		{
			for (int32 ChildIdx = 0; ChildIdx < ChildParms.Num(); ++ChildIdx)
			{
				UHoudiniParameter* ChildParm = ChildParms[ChildIdx];

				// Try to reuse the legacy parms
				const int32 FoundChildParmInstIdx = ChildParmInsts.IndexOfByPredicate([ChildParm](UHoudiniParameter* AttribParmInst)
					{
						if (ChildParm->GetParameterName() == AttribParmInst->GetParameterName())
							return AttribParmInst->CopySettingsFrom(ChildParm);
						return false;
					});

				UHoudiniParameter* NewParmInst = nullptr;
				if (ChildParmInsts.IsValidIndex(FoundChildParmInstIdx))
				{
					NewParmInst = ChildParmInsts[FoundChildParmInstIdx];
					ChildParmInsts.RemoveAt(FoundChildParmInstIdx);
				}
				else
					NewParmInst = DuplicateObject<UHoudiniParameter>(ChildParm, ChildParm->GetOuter());

				((FHouParmInstanceIndexAccessor*)NewParmInst)->Set(DupIdx + StartMultiParmIdx);

				if (UHoudiniParameterAttribute* ChildParmAttrib = CorrespondingAttribs[ChildIdx])
				{
					((FHouParmAttribParmModifier*)ChildParmAttrib)->Set(NewParmInst);  // To let the ChildParmAttrib to update this parm's value
					NewChildParmInsts.Add(ChildParmAttrib->GetParameter(InstElemIdx + DupIdx));
					((FHouParmAttribParmModifier*)ChildParmAttrib)->Set(ChildParm);  // revert to original
				}
				else
					NewChildParmInsts.Add(NewParmInst);
			}
		}

		ChildParmInsts = NewChildParmInsts;
	}

	MultiParm->UpdateInstanceCount(ArrayLen);
	return MultiParm;
}

void UHoudiniMultiParameterAttribute::UpdateFromParameter(const TArray<int32>& DstElemIndices)
{
	class FHouParmAttribParmValueModifier : public UHoudiniParameterAttribute
	{
	public:
		void InsertZeroed(const int32& InsertElemIdx, const int32& Count)
		{
			switch (Storage)
			{
			case EHoudiniStorageType::Int:
			{
				if ((InsertElemIdx * TupleSize) >= IntValues.Num())
					IntValues.AddZeroed(Count * TupleSize);
				else
					IntValues.InsertZeroed(InsertElemIdx * TupleSize, Count * TupleSize);
			}
			break;
			case EHoudiniStorageType::Float:
			{
				if ((InsertElemIdx * TupleSize) >= FloatValues.Num())
					FloatValues.AddZeroed(Count * TupleSize);
				else
					FloatValues.InsertZeroed(InsertElemIdx * TupleSize, Count * TupleSize);
			}
			break;
			case EHoudiniStorageType::String:
			{
				if (SHOULD_UNIQUE_STRINGS)
				{
					TArray<int32> InsertedStringIndices;
					InsertedStringIndices.SetNumUninitialized(Count);
					for (int32& StringIdx : InsertedStringIndices)  // Must init to -1
						StringIdx = -1;

					if (InsertElemIdx >= IntValues.Num())
						IntValues.Append(InsertedStringIndices);
					else
						IntValues.Insert(InsertedStringIndices, InsertElemIdx);
				}
				else
				{
					if (InsertElemIdx >= StringValues.Num())
						StringValues.AddDefaulted(Count);
					else
						StringValues.InsertDefaulted(InsertElemIdx, Count);
				}
			}
			break;
			case EHoudiniStorageType::Object:
			{
				TArray<int32> InsertedObjectIndices;
				InsertedObjectIndices.SetNumUninitialized(Count);
				for (int32& ObjectIdx : InsertedObjectIndices)  // Must init to -1
					ObjectIdx = -1;

				if (InsertElemIdx >= IntValues.Num())
					IntValues.Append(InsertedObjectIndices);
				else
					IntValues.Insert(InsertedObjectIndices, InsertElemIdx);
			}
			break;
			}
		}

		void RemoveAt(const int32& RemoveElemIdx, const int32& Count)
		{
			switch (Storage)
			{
			case EHoudiniStorageType::Int:
				IntValues.RemoveAt(RemoveElemIdx * TupleSize, Count * TupleSize);
				break;
			case EHoudiniStorageType::Float:
				FloatValues.RemoveAt(RemoveElemIdx * TupleSize, Count * TupleSize);
				break;
			case EHoudiniStorageType::String:
			{
				if (SHOULD_UNIQUE_STRINGS)
				{
					FloatValues.Append((float*)IntValues.GetData() + RemoveElemIdx, Count);  // Store removed object indices to FloatValues
					IntValues.RemoveAt(RemoveElemIdx, Count);
				}
				else
					StringValues.RemoveAt(RemoveElemIdx, Count);
			}
			break;
			case EHoudiniStorageType::Object:
			{
				FloatValues.Append((float*)IntValues.GetData() + RemoveElemIdx, Count);  // Store removed object indices to FloatValues
				IntValues.RemoveAt(RemoveElemIdx, Count);
			}
			break;
			}
		}

		FORCEINLINE void SetParm(UHoudiniParameter* InParm) { Parm = InParm; }

		void ShrinkUniqueValues()
		{
			if ((Storage != EHoudiniStorageType::Object) && (Storage != EHoudiniStorageType::String))
				return;

			if (FloatValues.IsEmpty())
				return;

			TArray<int32> RemovedValueIndices;
			for (const float& FloatValue : FloatValues)
			{
				const int32& RemovedValueIdx = *((const int32*)&FloatValue);
				if (RemovedValueIdx >= 0)
					RemovedValueIndices.AddUnique(RemovedValueIdx);
			}
			FloatValues.Empty();
			if (Storage == EHoudiniStorageType::Object)
				UHoudiniParameterAttribute::ShrinkObjectValues(RemovedValueIndices);
			else
				UHoudiniParameterAttribute::ShrinkIndexingStringValues(RemovedValueIndices);
		}

		void EmptyObjectTempData()
		{
			if ((Storage == EHoudiniStorageType::Object) || ((Storage == EHoudiniStorageType::String) && (SHOULD_UNIQUE_STRINGS)))
			{
				if (!ensure(FloatValues.IsEmpty()))  // Should Never happened
					FloatValues.Empty();
			}
		}
	};

	for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)  // Empty object attrib's FloatValues first, as it maybe used to store removed object indices
		((FHouParmAttribParmValueModifier*)ChildParmAttrib)->EmptyObjectTempData();

	UHoudiniMultiParameter* MultiParm = Cast<UHoudiniMultiParameter>(Parm);
	const int32 ArrayLen = MultiParm->GetInstanceCount();

	TArray<int32> DstSortedIndices = DstElemIndices;
	DstSortedIndices.Sort();

	TArray<TPair<int32, int32>> InstElemIndices;
	TArray<int32> DstInstElemIndices;
	int32 CurrInstIdx = 0;
	int32 LastElemIdx = 0;
	for (const int32& DstElemIdx : DstSortedIndices)
	{
		for (int32 ElemIdx = LastElemIdx; ElemIdx < DstElemIdx; ++ElemIdx)
			CurrInstIdx += IntValues[ElemIdx];

		if (IntValues[DstElemIdx] < ArrayLen)
		{
			for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
				((FHouParmAttribParmValueModifier*)ChildParmAttrib)->InsertZeroed(CurrInstIdx, ArrayLen - IntValues[DstElemIdx]);
		}
		else if (IntValues[DstElemIdx] > ArrayLen)
		{
			for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
				((FHouParmAttribParmValueModifier*)ChildParmAttrib)->RemoveAt(CurrInstIdx, IntValues[DstElemIdx] - ArrayLen);
		}

		IntValues[DstElemIdx] = ArrayLen;
		if (ArrayLen)
			DstInstElemIndices.Add(CurrInstIdx);

		LastElemIdx = DstElemIdx;
	}

	for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)  // Shrink before update, as update may change unique idx
		((FHouParmAttribParmValueModifier*)ChildParmAttrib)->ShrinkUniqueValues();

	const TArray<UHoudiniParameter*>& ChildAttribParms = MultiParm->GetChildAttributeParameters();
	for (int32 ArrayIdx = 0; ArrayIdx < ArrayLen; ++ArrayIdx)
	{
		for (int32 ChildIdx = 0; ChildIdx < ChildParmAttribs.Num(); ++ChildIdx)
		{
			UHoudiniParameterAttribute* ChildParmAttrib = ChildParmAttribs[ChildIdx];
			((FHouParmAttribParmValueModifier*)ChildParmAttrib)->SetParm(MultiParm->ChildAttribParmInsts[ArrayIdx * ChildAttribParms.Num() + ChildIdx]);
			ChildParmAttrib->UpdateFromParameter(DstInstElemIndices);
			((FHouParmAttribParmValueModifier*)ChildParmAttrib)->SetParm(ChildAttribParms[ChildIdx]);
		}

		for (int32& DstInstElemIdx : DstInstElemIndices)
			++DstInstElemIdx;
	}
}

bool UHoudiniMultiParameterAttribute::Join(const UHoudiniParameterAttribute* OtherAttrib)
{
	if (const UHoudiniMultiParameterAttribute* OtherArrayAttrib = Cast<UHoudiniMultiParameterAttribute>(OtherAttrib))
	{
		UHoudiniParameterAttribute::Join(OtherAttrib);

		int32 NumTotalElems = 0;
		for (const int32& Count : IntValues)
			NumTotalElems += Count;

		for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
		{
			TObjectPtr<UHoudiniParameterAttribute> const* FoundOtherChildAttribPtr = OtherArrayAttrib->ChildParmAttribs.FindByPredicate(
				[ChildParmAttrib](const UHoudiniParameterAttribute* OtherChildAttrib)
				{ return ChildParmAttrib->GetParameter() == OtherChildAttrib->GetParameter();  });
			
			if (!FoundOtherChildAttribPtr || !ChildParmAttrib->Join(*FoundOtherChildAttribPtr))
				ChildParmAttrib->SetNum(NumTotalElems);
		}

		return true;
	}

	return false;
}

void UHoudiniMultiParameterAttribute::SetDataFrom(const UHoudiniParameterAttribute* SrcAttrib, const TArray<int32>& SrcIndices)
{
	if (const UHoudiniMultiParameterAttribute* SrcArrayAttrib = Cast<UHoudiniMultiParameterAttribute>(SrcAttrib))
	{
		UHoudiniParameterAttribute::SetDataFrom(SrcAttrib, SrcIndices);

		TArray<int32> InstsElemIndices;
		SrcArrayAttrib->ConvertToChildAttributeIndices(SrcIndices, InstsElemIndices);

		if (!InstsElemIndices.IsEmpty())
		{
			for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
			{
				if (TObjectPtr<UHoudiniParameterAttribute> const* FoundOtherChildAttribPtr = SrcArrayAttrib->ChildParmAttribs.FindByPredicate(
					[ChildParmAttrib](const UHoudiniParameterAttribute* OtherChildAttrib) { return ChildParmAttrib->GetParameter() == OtherChildAttrib->GetParameter();  }))
					ChildParmAttrib->SetDataFrom(*FoundOtherChildAttribPtr, InstsElemIndices);
			}
		}
		else
		{
			for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
				ChildParmAttrib->SetNum(0);
		}
	}
	else
	{
		SetNum(SrcIndices.Num());
	}
}

void UHoudiniMultiParameterAttribute::UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput) const
{
	for (const UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
		ChildParmAttrib->UploadInfo_Internal(SHMGeoInput, IntValues.Num());
}

void UHoudiniMultiParameterAttribute::UploadData(float*& DataPtr) const
{
	class FHouParmAttribAccessor : public UHoudiniParameterAttribute
	{
	public:
		FORCEINLINE void UploadArrayData(float*& DataPtr, const TArray<int32>& Counts) const
		{
			if (Storage == EHoudiniStorageType::Invalid)
				return;

			FMemory::Memcpy(DataPtr, Counts.GetData(), sizeof(int32) * Counts.Num());
			DataPtr += Counts.Num();

			UploadData(DataPtr);
		}
	};

	for (const UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
		((const FHouParmAttribAccessor*)ChildParmAttrib)->UploadArrayData(DataPtr, IntValues);
}

TSharedPtr<FJsonObject> UHoudiniMultiParameterAttribute::ConvertToJson() const
{
	TArray<TSharedPtr<FJsonValue>> JsonCounts;
	for (const int32& Value : IntValues)
		JsonCounts.Add(MakeShared<FJsonValueString>(FString::FromInt(Value)));

	TSharedPtr<FJsonObject> JsonChildParmAttribs = MakeShared<FJsonObject>();
	for (const UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
	{
		if (!ChildParmAttrib->IsParameterHolder())
			JsonChildParmAttribs->SetObjectField(ChildParmAttrib->GetParameterName(), ChildParmAttrib->ConvertToJson());
	}

	TSharedPtr<FJsonObject> JsonAttrib = MakeShared<FJsonObject>();
	JsonAttrib->SetArrayField(TEXT("Counts"), JsonCounts);
	JsonAttrib->SetObjectField(TEXT("Children"), JsonChildParmAttribs);

	return JsonAttrib;
}

bool UHoudiniMultiParameterAttribute::AppendFromJson(const TSharedPtr<FJsonObject>& JsonAttrib)
{
	if (IsParameterHolder())
		return true;

	const TArray<TSharedPtr<FJsonValue>>* JsonCountsPtr = nullptr;
	if (!JsonAttrib->TryGetArrayField(TEXT("Counts"), JsonCountsPtr))
		return false;
	
	for (const TSharedPtr<FJsonValue>& JsonCount : *JsonCountsPtr)
		IntValues.Add(FCString::Atoi(*JsonCount->AsString()));

	const TSharedPtr<FJsonObject>* JsonChildParmAttribsPtr = nullptr;
	if (JsonAttrib->TryGetObjectField(TEXT("Children"), JsonChildParmAttribsPtr))
	{
		for (UHoudiniParameterAttribute* ChildParmAttrib : ChildParmAttribs)
		{
			const TSharedPtr<FJsonObject>* JsonChildAttribPtr = nullptr;
			if ((*JsonChildParmAttribsPtr)->TryGetObjectField(ChildParmAttrib->GetParameterName(), JsonChildAttribPtr))
				ChildParmAttrib->AppendFromJson(*JsonChildAttribPtr);
		}
	}

	return true;
}

#if WITH_EDITOR
bool UHoudiniMultiParameterAttribute::Modify(bool bAlwaysMarkDirty)
{
	bool bModified = Super::Modify(bAlwaysMarkDirty);
	for (UHoudiniParameterAttribute* ParmAttribInst : ChildParmAttribs)
		bModified |= ParmAttribInst->Modify(bAlwaysMarkDirty);
	return bModified;
}
#endif
