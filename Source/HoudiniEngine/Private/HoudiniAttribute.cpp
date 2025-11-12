// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAttribute.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"


// -------- FHoudiniAttribute --------
bool FHoudiniAttribute::HapiRetrieveAttributes(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames,
	const int AttribCounts[HAPI_ATTROWNER_MAX], const char* AttributePrefix, TArray<TSharedPtr<FHoudiniAttribute>>& OutAttribs)
{
	int32 AttribIdx = 0;
	for (const std::string& AttribName : AttribNames)
	{
		if (AttribName.starts_with(AttributePrefix))
		{
			const FString PropertyName(AttribName.c_str() + strlen(AttributePrefix));
			if (PropertyName.IsEmpty())
				continue;

			// We have already had AttribIdx, so we just use it to figure out Owner, NO need for FHoudiniEngineUtils::QueryAttributeOwner
			HAPI_AttributeOwner Owner = HAPI_ATTROWNER_VERTEX;
			int32 NumAttribs = 0;
			for (; Owner < HAPI_ATTROWNER_MAX; Owner = HAPI_AttributeOwner(int(Owner) + 1))
			{
				NumAttribs += AttribCounts[Owner];
				if (AttribIdx < NumAttribs)
					break;
			}

			HAPI_AttributeInfo AttribInfo;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				AttribName.c_str(), Owner, &AttribInfo));

			TSharedPtr<FHoudiniAttribute> PropAttrib = FHoudiniEngineUtils::IsArray(AttribInfo.storage) ?
				MakeShared<FHoudiniArrayAttribute>(PropertyName) : MakeShared<FHoudiniAttribute>(PropertyName);

			HOUDINI_FAIL_RETURN(PropAttrib->HapiRetrieveData(NodeId, PartId, AttribName.c_str(), AttribInfo));

			OutAttribs.Add(PropAttrib);
		}
		++AttribIdx;
	}

	return true;
}

static bool HapiRetrieveUniqueStrings(TArray<int32>& IntValues, TArray<std::string>& OutStringValues)
{
	if (IntValues.Num() == 1)
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(IntValues, OutStringValues));
		IntValues.Empty();
	}
	else
	{
		const TArray<HAPI_StringHandle> UniqueSHs = TSet<HAPI_StringHandle>(IntValues).Array();
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueSHs, OutStringValues));
		TMap<HAPI_StringHandle, int32> SHIdxMap;
		for (int32 StrIdx = 0; StrIdx < UniqueSHs.Num(); ++StrIdx)
			SHIdxMap.Add(UniqueSHs[StrIdx], StrIdx);
		for (int32& StrIdx : IntValues)  // Record unique string idx on IntValues
			StrIdx = SHIdxMap[StrIdx];
	}

	return true;
}

bool FHoudiniAttribute::HapiRetrieveData(const int32& NodeId, const int32& PartId, const char* AttribName,
	HAPI_AttributeInfo& AttribInfo)
{
	Owner = AttribInfo.owner;
	TupleSize = AttribInfo.tupleSize;
	Storage = AttribInfo.storage;

	switch (FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage))
	{
	case EHoudiniStorageType::Int:
	{
		IntValues.SetNumUninitialized(AttribInfo.count * AttribInfo.tupleSize);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName, &AttribInfo, -1, IntValues.GetData(), 0, AttribInfo.count));
	}
	break;
	case EHoudiniStorageType::Float:
	{
		FloatValues.SetNumUninitialized(AttribInfo.count * AttribInfo.tupleSize);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName, &AttribInfo, -1, FloatValues.GetData(), 0, AttribInfo.count));
	}
	break;
	case EHoudiniStorageType::String:
	{
		TupleSize = 1;
		IntValues.SetNumUninitialized(AttribInfo.count);
		if (AttribInfo.storage == HAPI_STORAGETYPE_DICTIONARY)
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeDictionaryData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				AttribName, &AttribInfo, IntValues.GetData(), 0, AttribInfo.count))
		else
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				AttribName, &AttribInfo, IntValues.GetData(), 0, AttribInfo.count));
		HOUDINI_FAIL_RETURN(HapiRetrieveUniqueStrings(IntValues, StringValues));
	}
	break;
	}

	return true;
}

TArray<float> FHoudiniAttribute::GetFloatData(const int32& Index) const
{
	TArray<float> ElemData;

	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Int:
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			ElemData.Add(float(IntValues[Index * TupleSize + TupleIdx]));
		break;
	case EHoudiniStorageType::Float:
		ElemData.Append(FloatValues.GetData() + Index * TupleSize, TupleSize);
		break;
	case EHoudiniStorageType::String:
		ElemData.Add(FCStringAnsi::Atof(StringValues[IntValues.IsEmpty() ? Index : IntValues[Index]].c_str()));
		break;
	}

	return ElemData;
}

TArray<int32> FHoudiniAttribute::GetIntData(const int32& Index) const
{
	TArray<int32> ElemData;

	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Int:
		ElemData.Append(IntValues.GetData() + Index * TupleSize, TupleSize);
		break;
	case EHoudiniStorageType::Float:
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			ElemData.Add(int32(FloatValues[Index * TupleSize + TupleIdx]));
		break;
	case EHoudiniStorageType::String:
		ElemData.Add(FCStringAnsi::Atoi(StringValues[IntValues.IsEmpty() ? Index : IntValues[Index]].c_str()));
		break;
	}

	return ElemData;
}

TArray<FString> FHoudiniAttribute::GetStringData(const int32& Index) const
{
	TArray<FString> ElemData;

	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Int:
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			ElemData.Add(FString::FromInt(IntValues[Index * TupleSize + TupleIdx]));
		break;
	case EHoudiniStorageType::Float:
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			ElemData.Add(FString::SanitizeFloat(FloatValues[Index * TupleSize + TupleIdx]));
		break;
	case EHoudiniStorageType::String:
		ElemData.Add(UTF8_TO_TCHAR(StringValues[IntValues.IsEmpty() ? Index : IntValues[Index]].c_str()));
	break;
	}

	return ElemData;
}


void FHoudiniAttribute::UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput, const char* AttributePrefix) const
{
	std::string AttribName;
	if (AttributePrefix)
		AttribName = AttributePrefix;
	AttribName += TCHAR_TO_UTF8(*Name);

	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Int:
		SHMGeoInput.AppendAttribute(AttribName.c_str(), (EHoudiniAttributeOwner)Owner, EHoudiniInputAttributeStorage::Int, TupleSize, IntValues.Num(),
			(TupleSize == IntValues.Num()) ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None);
		break;
	case EHoudiniStorageType::Float:
		SHMGeoInput.AppendAttribute(AttribName.c_str(), (EHoudiniAttributeOwner)Owner, EHoudiniInputAttributeStorage::Float, TupleSize, FloatValues.Num(),
			(TupleSize == FloatValues.Num()) ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None);
		break;
	case EHoudiniStorageType::String:
	{
		size_t NumChars = 0;
		for (const std::string& StrValue : StringValues)
			NumChars += StrValue.length() + 1;  // Add '\0' at end

		SHMGeoInput.AppendAttribute(AttribName.c_str(), (EHoudiniAttributeOwner)Owner,
			(Storage == HAPI_STORAGETYPE_DICTIONARY) ? EHoudiniInputAttributeStorage::Dict : EHoudiniInputAttributeStorage::String, TupleSize, NumChars / 4 + 1,
			(StringValues.Num() == 1) ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None);
	}
	break;
	}
}

void FHoudiniAttribute::UploadData(float*& DataPtr) const
{
	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
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
		char* StrDataPtr = (char*)DataPtr;
		size_t NumChars = 0;
		for (const std::string& StrValue : StringValues)
		{
			const size_t StrLengthWithEnd = StrValue.length() + 1;
			NumChars += StrLengthWithEnd;
			FMemory::Memcpy(StrDataPtr, StrValue.c_str(), StrLengthWithEnd - 1);
			StrDataPtr[StrLengthWithEnd - 1] = '\0';
			StrDataPtr += StrLengthWithEnd;
		}
		DataPtr += (NumChars / 4 + 1);
		return;
	}
	}
}


// -------- FHoudiniArrayAttribute --------
bool FHoudiniArrayAttribute::HapiRetrieveData(const int32& NodeId, const int32& PartId, const char* AttribName,
	HAPI_AttributeInfo& AttribInfo)
{
	Owner = AttribInfo.owner;
	TupleSize = AttribInfo.tupleSize;
	Storage = AttribInfo.storage;

	TArray<int32> ArrayCounts;
	ArrayCounts.SetNumZeroed(AttribInfo.count);
	
	switch (FHoudiniEngineUtils::ConvertStorageType(AttribInfo.storage))
	{
	case EHoudiniStorageType::Int: if (AttribInfo.totalArrayElements >= 1)
	{
		IntValues.SetNumUninitialized(AttribInfo.totalArrayElements);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeIntArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName, &AttribInfo, IntValues.GetData(), AttribInfo.totalArrayElements, ArrayCounts.GetData(), 0, AttribInfo.count));
	}
	break;
	case EHoudiniStorageType::Float: if (AttribInfo.totalArrayElements >= 1)
	{
		FloatValues.SetNumUninitialized(AttribInfo.totalArrayElements);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			AttribName, &AttribInfo, FloatValues.GetData(), AttribInfo.totalArrayElements, ArrayCounts.GetData(), 0, AttribInfo.count));
	}
	break;
	case EHoudiniStorageType::String:
	{
		TupleSize = 1;
		if (AttribInfo.totalArrayElements >= 1)
		{
			IntValues.SetNumUninitialized(AttribInfo.totalArrayElements);
			if (Storage == HAPI_STORAGETYPE_DICTIONARY_ARRAY)
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeDictionaryArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					AttribName, &AttribInfo, IntValues.GetData(), AttribInfo.totalArrayElements, ArrayCounts.GetData(), 0, AttribInfo.count))
			else
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringArrayData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					AttribName, &AttribInfo, IntValues.GetData(), AttribInfo.totalArrayElements, ArrayCounts.GetData(), 0, AttribInfo.count))
			HOUDINI_FAIL_RETURN(HapiRetrieveUniqueStrings(IntValues, StringValues));  // TODO: check Houdini 21.0 still make all strings unique?
		}
	}
	break;
	}

	Counts.SetNumUninitialized(AttribInfo.count);
	int32 AccumulatedCount = 0;
	for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
	{
		AccumulatedCount += ArrayCounts[ElemIdx];  // ArrayCount = tupleSize * arrayLen
		Counts[ElemIdx] = AccumulatedCount;
	}

	return true;
}

int32 FHoudiniArrayAttribute::GetDataCount(const int32& Index) const
{
	const int32 PrevCount = (Index <= 0) ? 0 : Counts[Index - 1];
	const int32 CurrCount = Counts[Index];
	return (CurrCount - PrevCount);
}

TArray<float> FHoudiniArrayAttribute::GetFloatData(const int32& Index) const
{
	TArray<float> ElemData;

	// ArrayCount = tupleSize * arrayLen
	const int32 StartDataIdx = (Index <= 0) ? 0 : Counts[Index - 1];
	const int32 EndDataIdx = Counts[Index];
	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Float:
		ElemData.Append(FloatValues.GetData() + StartDataIdx, EndDataIdx - StartDataIdx);
		break;
	case EHoudiniStorageType::Int:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(float(IntValues[DataIdx]));
		break;
	case EHoudiniStorageType::String:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(FCStringAnsi::Atof(StringValues[IntValues.IsEmpty() ? DataIdx : IntValues[DataIdx]].c_str()));
		break;
	}

	return ElemData;
}

TArray<int32> FHoudiniArrayAttribute::GetIntData(const int32& Index) const
{
	TArray<int32> ElemData;

	// ArrayCount = tupleSize * arrayLen
	const int32 StartDataIdx = (Index <= 0) ? 0 : Counts[Index - 1];
	const int32 EndDataIdx = Counts[Index];
	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Float:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(int32(FloatValues[DataIdx]));
		break;
	case EHoudiniStorageType::Int:
		ElemData.Append(IntValues.GetData() + StartDataIdx, EndDataIdx - StartDataIdx);
		break;
	case EHoudiniStorageType::String:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(FCStringAnsi::Atoi(StringValues[IntValues.IsEmpty() ? DataIdx : IntValues[DataIdx]].c_str()));
		break;
	}

	return ElemData;
}

TArray<FString> FHoudiniArrayAttribute::GetStringData(const int32& Index) const
{
	TArray<FString> ElemData;

	// ArrayCount = tupleSize * arrayLen
	const int32 StartDataIdx = (Index <= 0) ? 0 : Counts[Index - 1];
	const int32 EndDataIdx = Counts[Index];
	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Float:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(FString::SanitizeFloat(FloatValues[DataIdx]));
		break;
	case EHoudiniStorageType::Int:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(FString::FromInt(IntValues[DataIdx]));
		break;
	case EHoudiniStorageType::String:
		for (int32 DataIdx = StartDataIdx; DataIdx < EndDataIdx; ++DataIdx)
			ElemData.Add(UTF8_TO_TCHAR(StringValues[IntValues.IsEmpty() ? DataIdx : IntValues[DataIdx]].c_str()));
		break;
	}

	return ElemData;
}

void FHoudiniArrayAttribute::UploadInfo(FHoudiniSharedMemoryGeometryInput& SHMGeoInput, const char* AttributePrefix) const
{
	std::string AttribName;
	if (AttributePrefix)
		AttribName = AttributePrefix;
	AttribName += TCHAR_TO_UTF8(*Name);

	switch (FHoudiniEngineUtils::ConvertStorageType(Storage))
	{
	case EHoudiniStorageType::Int:
		SHMGeoInput.AppendAttribute(AttribName.c_str(), (EHoudiniAttributeOwner)Owner, EHoudiniInputAttributeStorage::IntArray, TupleSize, Counts.Num() + IntValues.Num(),
			(Counts.Num() == 1) ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None);
		break;
	case EHoudiniStorageType::Float:
		SHMGeoInput.AppendAttribute(AttribName.c_str(), (EHoudiniAttributeOwner)Owner, EHoudiniInputAttributeStorage::FloatArray, TupleSize, Counts.Num() + FloatValues.Num(),
			(Counts.Num() == 1) ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None);
		break;
	case EHoudiniStorageType::String:
	{
		size_t NumChars = 0;
		for (const std::string& StrValue : StringValues)
			NumChars += StrValue.length() + 1;  // Add '\0' at end

		SHMGeoInput.AppendAttribute(AttribName.c_str(), (EHoudiniAttributeOwner)Owner,
			(Storage == HAPI_STORAGETYPE_DICTIONARY || Storage == HAPI_STORAGETYPE_DICTIONARY_ARRAY) ? EHoudiniInputAttributeStorage::DictArray : EHoudiniInputAttributeStorage::StringArray,
			TupleSize, Counts.Num() + NumChars / 4 + 1, (Counts.Num() == 1) ? EHoudiniInputAttributeCompression::UniqueValue : EHoudiniInputAttributeCompression::None);
	}
	break;
	}
}

void FHoudiniArrayAttribute::UploadData(float*& DataPtr) const
{
	FMemory::Memcpy(DataPtr, Counts.GetData(), sizeof(int32) * Counts.Num());
	DataPtr += Counts.Num();

	FHoudiniAttribute::UploadData(DataPtr);
}
