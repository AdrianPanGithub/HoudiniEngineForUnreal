// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAttribute.h"

#include "JsonObjectConverter.h"

#include "HoudiniEngineUtils.h"


template<typename T, typename P>
void PopulateHoudiniAttributeValue(TArray<T>& Values, const TArray<const uint8*>& Containers,
	const int32& TupleSize,
	const int32& Offset,
	const uint8& ElemSize)  // sizeof(PropertyType)
{
	for (const uint8* Data : Containers)
	{
		for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
			Values.Add((T)P::GetPropertyValue(Data + Offset + TupleIdx * ElemSize));
	}
}

template<typename T, typename P>
void PopulateHoudiniArrayAttributeValue(TArray<T>& OutValues, TArray<int32>& OutCounts, const TArray<const uint8*>& Containers,
	const FArrayProperty* ArrayProp,
	const int32& TupleSize,
	const int32& Offset,
	const uint8& ElemSize)  // sizeof(PropertyType)
{
	for (const uint8* Data : Containers)
	{
		FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, Data);
		const int32 ArrayLen = ArrayHelper.Num();
		OutCounts.Add(ArrayLen);
		for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)\
		{
			const int8* ValuePtr = (const int8*)ArrayHelper.GetElementPtr(ArrayElemIdx);
			for (int32 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
				OutValues.Add((T)P::GetPropertyValue(ValuePtr + TupleIdx * ElemSize));
		}
	}
}


#define RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(ATTRIB_INIT_FUNC, RETRIEVE_FUNC) if (ArrayProp)\
{\
	TSharedPtr<FHoudiniArrayAttribute> Attrib = MakeShared<FHoudiniArrayAttribute>(PropName);\
	ATTRIB_INIT_FUNC\
	for (const uint8* DataPtr : Containers)\
	{\
		FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, DataPtr);\
		const int32 ArrayLen = ArrayHelper.Num();\
		Attrib->Counts.Add(ArrayLen);\
		for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)\
		{\
			const uint8* ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);\
			RETRIEVE_FUNC\
		}\
	}\
	OutAttribs.Add(Attrib);\
}\
else\
{\
	TSharedPtr<FHoudiniAttribute> Attrib = MakeShared<FHoudiniAttribute>(PropName);\
	ATTRIB_INIT_FUNC\
	for (const uint8* DataPtr : Containers)\
	{\
		const uint8* ValuePtr = DataPtr + Offset;\
		RETRIEVE_FUNC\
	}\
	OutAttribs.Add(Attrib);\
}\

void FHoudiniAttribute::RetrieveAttributes(const TArray<const FProperty*>& Properties, const TArray<const uint8*>& Containers,
	const HAPI_AttributeOwner& Owner, TArray<TSharedPtr<FHoudiniAttribute>>& OutAttribs)
{
	TSet<FString> PropNameSet;
	for (const FProperty* Prop : Properties)
	{
		FString PropName = FHoudiniEngineUtils::GetValidatedString(DataTableUtils::GetPropertyExportName(Prop));
		if (PropNameSet.Contains(PropName))
			PropName = FHoudiniEngineUtils::GetValidatedString(Prop->GetName());
		PropNameSet.Add(PropName);

		const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (ArrayProp)
			Prop = ArrayProp->Inner;

		const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop);
		const FNumericProperty* NumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : CastField<FNumericProperty>(Prop);
		const int32 Offset = Prop->GetOffset_ForInternal();
		if (NumericProp)
		{
			if (ArrayProp)
			{
				TSharedPtr<FHoudiniArrayAttribute> ArrayAttrib = MakeShared<FHoudiniArrayAttribute>(PropName);
				if (NumericProp->IsInteger())
				{
					TArray<int32>& AttribValues = ArrayAttrib->InitializeInt(Owner, 1);
					for (const uint8* DataPtr : Containers)
					{
						FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, DataPtr);
						const int32 ArrayLen = ArrayHelper.Num();
						ArrayAttrib->Counts.Add(ArrayLen);
						for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)
							AttribValues.Add(int32(NumericProp->GetSignedIntPropertyValue(
								ArrayHelper.GetElementPtr(ArrayElemIdx))));
					}
				}
				else if (NumericProp->IsFloatingPoint())
				{
					TArray<float>& AttribValues = ArrayAttrib->InitializeFloat(Owner, 1);
					for (const uint8* DataPtr : Containers)
					{
						FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, DataPtr);
						const int32 ArrayLen = ArrayHelper.Num();
						ArrayAttrib->Counts.Add(ArrayLen);
						for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)
							AttribValues.Add(float(NumericProp->GetFloatingPointPropertyValue(
								ArrayHelper.GetElementPtr(ArrayElemIdx))));
					}
				}
				OutAttribs.Add(ArrayAttrib);
			}
			else
			{
				TSharedPtr<FHoudiniAttribute> Attrib = MakeShared<FHoudiniAttribute>(PropName);
				if (NumericProp->IsInteger())
				{
					TArray<int32>& AttribValues = Attrib->InitializeInt(Owner, 1);
					for (const uint8* DataPtr : Containers)
						AttribValues.Add(int32(NumericProp->GetSignedIntPropertyValue(DataPtr + Offset)));
				}
				else if (NumericProp->IsFloatingPoint())
				{
					TArray<float>& AttribValues = Attrib->InitializeFloat(Owner, 1);
					for (const uint8* DataPtr : Containers)
						AttribValues.Add(float(NumericProp->GetFloatingPointPropertyValue(DataPtr + Offset)));
				}
				OutAttribs.Add(Attrib);
			}

			continue;  // Parse finished
		}


		if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			const FName& StructName = StructProp->Struct->GetFName();
			const int32 FoundStructIdx = FHoudiniAttribute::SupportStructNames.IndexOfByKey(StructName);
			if (FHoudiniAttribute::SupportStructNames.IsValidIndex(FoundStructIdx))
			{
				const int32 TupleSize = FHoudiniAttribute::SupportStructTupleSize[FoundStructIdx];
				if (ArrayProp)
				{
					TSharedPtr<FHoudiniArrayAttribute> ArrayAttrib = MakeShared<FHoudiniArrayAttribute>(PropName);

					switch (FHoudiniAttribute::SupportStructDataType[FoundStructIdx])
					{
					case EHoudiniDataType::Float:
						PopulateHoudiniArrayAttributeValue<float, FFloatProperty>(ArrayAttrib->InitializeFloat(Owner, FMath::Abs(TupleSize)), ArrayAttrib->Counts,
							Containers, ArrayProp, TupleSize, Offset, sizeof(float));
						break;
					case EHoudiniDataType::Double:
						PopulateHoudiniArrayAttributeValue<float, FDoubleProperty>(ArrayAttrib->InitializeFloat(Owner, FMath::Abs(TupleSize)), ArrayAttrib->Counts,
							Containers, ArrayProp, TupleSize, Offset, sizeof(double));
						break;
					case EHoudiniDataType::Int:
						PopulateHoudiniArrayAttributeValue<int32, FIntProperty>(ArrayAttrib->InitializeInt(Owner, FMath::Abs(TupleSize)), ArrayAttrib->Counts,
							Containers, ArrayProp, TupleSize, Offset, sizeof(int32));
						break;
					case EHoudiniDataType::Uint:
						PopulateHoudiniArrayAttributeValue<int32, FUInt32Property>(ArrayAttrib->InitializeInt(Owner, FMath::Abs(TupleSize)), ArrayAttrib->Counts,
							Containers, ArrayProp, TupleSize, Offset, sizeof(uint32));
						break;
					case EHoudiniDataType::Int64:
						PopulateHoudiniArrayAttributeValue<int32, FInt64Property>(ArrayAttrib->InitializeInt(Owner, FMath::Abs(TupleSize)), ArrayAttrib->Counts,
							Containers, ArrayProp, TupleSize, Offset, sizeof(int64));
						break;
					case EHoudiniDataType::Uint64:
						PopulateHoudiniArrayAttributeValue<int32, FUInt64Property>(ArrayAttrib->InitializeInt(Owner, FMath::Abs(TupleSize)), ArrayAttrib->Counts,
							Containers, ArrayProp, TupleSize, Offset, sizeof(uint64));
						break;
					}

					OutAttribs.Add(ArrayAttrib);
				}
				else
				{
					TSharedPtr<FHoudiniAttribute> Attrib = MakeShared<FHoudiniAttribute>(PropName);

					switch (FHoudiniAttribute::SupportStructDataType[FoundStructIdx])
					{
					case EHoudiniDataType::Float:
						PopulateHoudiniAttributeValue<float, FFloatProperty>(Attrib->InitializeFloat(Owner, FMath::Abs(TupleSize)),
							Containers, TupleSize, Offset, sizeof(float));
						break;
					case EHoudiniDataType::Double:
						PopulateHoudiniAttributeValue<float, FDoubleProperty>(Attrib->InitializeFloat(Owner, FMath::Abs(TupleSize)),
							Containers, TupleSize, Offset, sizeof(double));
						break;
					case EHoudiniDataType::Int:
						PopulateHoudiniAttributeValue<int32, FIntProperty>(Attrib->InitializeInt(Owner, FMath::Abs(TupleSize)),
							Containers, TupleSize, Offset, sizeof(int32));
						break;
					case EHoudiniDataType::Uint:
						PopulateHoudiniAttributeValue<int32, FUInt32Property>(Attrib->InitializeInt(Owner, FMath::Abs(TupleSize)),
							Containers, TupleSize, Offset, sizeof(uint32));
						break;
					case EHoudiniDataType::Int64:
						PopulateHoudiniAttributeValue<int32, FInt64Property>(Attrib->InitializeInt(Owner, FMath::Abs(TupleSize)),
							Containers, TupleSize, Offset, sizeof(int64));
						break;
					case EHoudiniDataType::Uint64:
						PopulateHoudiniAttributeValue<int32, FUInt64Property>(Attrib->InitializeInt(Owner, FMath::Abs(TupleSize)),
							Containers, TupleSize, Offset, sizeof(uint64));
						break;
					}

					OutAttribs.Add(Attrib);
				}

				continue;  // Parse finished
			}
			else if (StructName == NAME_Color)
			{
				RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<float>&AttribValues = Attrib->InitializeFloat(Owner, 4); ,
					const FLinearColor LinearColor = *((FColor*)ValuePtr);
				AttribValues.Append((float*)(&LinearColor), 4););

				continue;  // Parse finished
			}
			else if (StructName == NAME_Transform3f || StructName == NAME_Transform3d || StructName == NAME_Transform)
			{
				const bool bIsSinglePrec = StructName == NAME_Transform3f;
				RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<float>&AttribValues = Attrib->InitializeFloat(Owner, 16);,
					FMatrix44f Matrix = bIsSinglePrec ? ((FTransform3f*)ValuePtr)->ToMatrixWithScale() : FMatrix44f(((FTransform*)ValuePtr)->ToMatrixWithScale());
				AttribValues.Append((float*)(&Matrix), 16););

				continue;  // Parse finished
			}
			else if (StructName == SoftObjectPathName)
			{
				RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<std::string>&AttribValues = Attrib->InitializeString(Owner);,
					AttribValues.Add(TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(*((FSoftObjectPath*)ValuePtr)))););

				continue;  // Parse finished
			}
			else if (StructName == SoftClassPathName)
			{
				RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<std::string>&AttribValues = Attrib->InitializeString(Owner); ,
					AttribValues.Add(TCHAR_TO_UTF8(*(((FSoftClassPath*)ValuePtr)->ToString()))););

				continue;  // Parse finished
			}
			else
			{
				auto ConvertStructToJsonStringLambda = [](const UStruct* StructDefinition, const void* Struct) -> std::string
					{
						TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 4)) || (ENGINE_MAJOR_VERSION > 5)
						if (FJsonObjectConverter::UStructToJsonObject(StructDefinition, Struct, JsonObject, 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase))
#else
						if (FJsonObjectConverter::UStructToJsonObject(StructDefinition, Struct, JsonObject))
#endif
						{
							FString JsonStr;
							TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr, 2);
							bool bSuccess = FJsonSerializer::Serialize(JsonObject, JsonWriter);
							JsonWriter->Close();
							if (bSuccess)
								return TCHAR_TO_UTF8(*JsonStr);
						}

						return std::string();
					};

				RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<std::string>&AttribValues = Attrib->InitializeDictionary(Owner); ,
					AttribValues.Add(ConvertStructToJsonStringLambda(StructProp->Struct, ValuePtr)););

				continue;  // Parse finished
			}
		}

		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<int32>&AttribValues = Attrib->InitializeInt(Owner, 1); ,
				AttribValues.Add(int32(BoolProp->GetPropertyValue(ValuePtr + Offset)));)

			continue;  // Parse finished
		}

		if (const FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Prop))  // Try NOT to load soft ptr
		{
			RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<std::string>&AttribValues = Attrib->InitializeString(Owner); ,
				AttribValues.Add(TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(SoftObjProp->GetPropertyValue(ValuePtr).ToSoftObjectPath()))););

			continue;  // Parse finished
		}

		// As for rest all property types, we just treat them as string
		const bool bIsObjectValue = (Prop->IsA<FObjectProperty>() || Prop->IsA<FWeakObjectProperty>() || Prop->IsA<FLazyObjectProperty>());
		RETRIEVE_PROPERTY_TO_HOUDINI_ATTRIBUTE(TArray<std::string>&AttribValues = Attrib->InitializeString(Owner);,
			FString StringValue = DataTableUtils::GetPropertyValueAsStringDirect(Prop, ValuePtr, EDataTableExportFlags());
		if (bIsObjectValue && StringValue.StartsWith(TEXT("/")))  // We should try to add class name to the string
			StringValue = FHoudiniEngineUtils::GetAssetReference(StringValue);
		AttribValues.Add(TCHAR_TO_UTF8(*StringValue)););
	}
}
