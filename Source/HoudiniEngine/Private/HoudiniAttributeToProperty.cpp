// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAttribute.h"

#include "JsonObjectConverter.h"

#include "HoudiniEngineCommon.h"


const TArray<FName> FHoudiniAttribute::SupportStructNames = { NAME_Vector2f, NAME_Vector2D,
	NAME_Vector3f, NAME_Vector3d, NAME_Vector,
	NAME_Vector4f, NAME_Vector4d, NAME_Vector4,
	NAME_Matrix44f, NAME_Matrix44d, NAME_Matrix,
	NAME_Quat4f, NAME_Quat4d, NAME_Quat,
	NAME_Rotator3f, NAME_Rotator3d, NAME_Rotator,
	NAME_LinearColor,
	NAME_IntPoint, NAME_Int32Point, NAME_UintPoint, NAME_Uint32Point, NAME_Int64Point, NAME_Uint64Point,
	NAME_IntVector2, NAME_Int32Vector2, NAME_UintVector2, NAME_Uint32Vector2, NAME_Int64Vector2, NAME_Uint64Vector2,
	NAME_IntVector, NAME_Int32Vector, NAME_UintVector, NAME_Uint32Vector, NAME_Int64Vector, NAME_Uint64Vector,
	NAME_IntVector4, NAME_Int32Vector4, NAME_UintVector4, NAME_Uint32Vector4, NAME_Int64Vector4, NAME_Uint64Vector4 };

const EHoudiniDataType FHoudiniAttribute::SupportStructDataType[] = { EHoudiniDataType::Float, EHoudiniDataType::Double,
	EHoudiniDataType::Float, EHoudiniDataType::Double, EHoudiniDataType::Double,
	EHoudiniDataType::Float, EHoudiniDataType::Double, EHoudiniDataType::Double,
	EHoudiniDataType::Float, EHoudiniDataType::Double, EHoudiniDataType::Double,
	EHoudiniDataType::Float, EHoudiniDataType::Double, EHoudiniDataType::Double,
	EHoudiniDataType::Float, EHoudiniDataType::Double, EHoudiniDataType::Double,
	EHoudiniDataType::Float,
	EHoudiniDataType::Int, EHoudiniDataType::Int, EHoudiniDataType::Uint, EHoudiniDataType::Uint, EHoudiniDataType::Int64, EHoudiniDataType::Uint64,
	EHoudiniDataType::Int, EHoudiniDataType::Int, EHoudiniDataType::Uint, EHoudiniDataType::Uint, EHoudiniDataType::Int64, EHoudiniDataType::Uint64,
	EHoudiniDataType::Int, EHoudiniDataType::Int, EHoudiniDataType::Uint, EHoudiniDataType::Uint, EHoudiniDataType::Int64, EHoudiniDataType::Uint64,
	EHoudiniDataType::Int, EHoudiniDataType::Int, EHoudiniDataType::Uint, EHoudiniDataType::Uint, EHoudiniDataType::Int64, EHoudiniDataType::Uint64 };

const uint8 FHoudiniAttribute::SupportStructTupleSize[] = {
	2, 2,    3, 3, 3,    4, 4, 4,    16, 16, 16,
	4, 4, 4,    3, 3, 3,    4,
	2, 2, 2, 2, 2, 2,    2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3,    4, 4, 4, 4, 4, 4 };


TMap<const UStruct*, TMap<FString, TPair<TSharedPtr<FEditPropertyChain>, size_t>>> FHoudiniAttribute::StructPropertiesMap;

void FHoudiniAttribute::ResetPropertiesMaps()
{
	StructPropertiesMap.Empty();
}

void FHoudiniAttribute::ClearBlueprintProperties()
{
	for (TMap<const UStruct*, TMap<FString, TPair<TSharedPtr<FEditPropertyChain>, size_t>>>::TIterator PropIter(StructPropertiesMap); PropIter; ++PropIter)
	{
		if (PropIter->Key->IsAsset() || PropIter->Key->IsInBlueprint())
			PropIter.RemoveCurrent();
	}
}

bool FHoudiniAttribute::SetStructPropertyValues(void* StructPtr, const UStruct* Struct, const int32& Index, const bool& bVerbose) const
{
	TSharedPtr<FEditPropertyChain> PropertyChain;
	size_t Offset = 0;
	if (FindProperty(Struct, Name, PropertyChain, Offset, bVerbose))
	{
		SetPropertyValues(StructPtr, Struct, PropertyChain, ((uint8*)StructPtr + Offset), Index);
		return true;
	}

	return false;
}

bool FHoudiniAttribute::SetObjectPropertyValues(UObject* Object, const int32& Index, const bool& bVerbose) const
{
	// -------- Set some special properties ---------
	if (UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(Object))
	{
		if (Name.Equals(HOUDINI_PROPERTY_COLLISION_PROFILE_NAME, ESearchCase::IgnoreCase) ||
			Name.Equals(HOUDINI_PROPERTY_COLLISION_PRESETS, ESearchCase::IgnoreCase))
		{
			if (IsRestrictString())
			{
				const TArray<FString> StringData = GetStringData(Index);
				if (StringData.IsValidIndex(0))
				{
					UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(PC);
					if (StringData[0].Equals(TEXT("Default"), ESearchCase::IgnoreCase))
					{
						if (SMC)
							SMC->bUseDefaultCollision = true;
					}
					else
					{
						if (SMC)
							SMC->bUseDefaultCollision = false;

						PC->SetCollisionProfileName(*StringData[0]);
					}
				}
			}

			return true;
		}
		else if (Name.Equals(HOUDINI_PROPERTY_MATERIALS, ESearchCase::IgnoreCase))
		{
			if (IsRestrictString())
			{
				const TArray<FString> StringData = GetStringData(Index);
				const int32 NumMats = FMath::Min(PC->GetNumMaterials(), StringData.Num());
				for (int32 MatSlotIdx = 0; MatSlotIdx < NumMats; ++MatSlotIdx)
				{
					const FString& MatStr = StringData[MatSlotIdx];
					if (!MatStr.IsEmpty())
						PC->SetMaterial(MatSlotIdx, LoadObject<UMaterialInterface>(nullptr, *MatStr, nullptr, LOAD_Quiet | LOAD_NoWarn));
				}
			}

			return true;
		}
		else if (Name.Equals(TEXT("CollisionEnabled"), ESearchCase::IgnoreCase))  // Set CollisionEnabled property directly will cause Crash
		{
			if (IsRestrictString())
			{
				const TArray<FString> StringData = GetStringData(Index);
				if (StringData.IsValidIndex(0))
				{
					const FString& StringValue = StringData[0];
					if (StringValue.Equals(TEXT("NoCollision"), ESearchCase::IgnoreCase))
						PC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					else if (StringValue.Equals(TEXT("QueryOnly"), ESearchCase::IgnoreCase))
						PC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					else if (StringValue.Equals(TEXT("PhysicsOnly"), ESearchCase::IgnoreCase))
						PC->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
					else if (StringValue.Equals(TEXT("QueryAndPhysics"), ESearchCase::IgnoreCase))
						PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				}
			}
			else if (Storage != HAPI_STORAGETYPE_DICTIONARY && Storage != HAPI_STORAGETYPE_DICTIONARY_ARRAY)
			{
				const TArray<int32> IntData = GetIntData(Index);
				if (IntData.IsValidIndex(0))
					PC->SetCollisionEnabled(ECollisionEnabled::Type(IntData[0]));
			}

			return true;
		}
	}
	else if (AActor* Actor = Cast<AActor>(Object))
	{
		if (Name.Equals(HOUDINI_PROPERTY_ACTOR_LOCATION, ESearchCase::IgnoreCase))
		{
			const TArray<float> FloatData = GetFloatData(Index);
			if (FloatData.Num() >= 3)
			{
				const FVector NewLocation(double(FloatData[0]) * POSITION_SCALE_TO_UNREAL, double(FloatData[2]) * POSITION_SCALE_TO_UNREAL, double(FloatData[1]) * POSITION_SCALE_TO_UNREAL);
				if (NewLocation != Actor->GetActorLocation())
					Actor->SetActorLocation(NewLocation);
			}
			return true;
		}
		else if (Name.Equals(TEXT("RuntimeGrid"), ESearchCase::IgnoreCase))
		{
			if (!IsRestrictString())
				return true;

			const TArray<FString> StringData = GetStringData(Index);
			if (StringData.IsValidIndex(0))
			{
				const FName RuntimeGridName = *StringData[0];
				if (Actor->GetRuntimeGrid() != RuntimeGridName)
					Actor->SetRuntimeGrid(RuntimeGridName);
			}

			return true;
		}
		else if (Name.Equals(TEXT("FolderPath"), ESearchCase::IgnoreCase) && IsRestrictString())  // We should call SetFolderPath, rather than modify it directly
		{
			const TArray<FString> StringData = GetStringData(Index);
			if (StringData.IsValidIndex(0))
				Actor->SetFolderPath(*(StringData[0]));
		}
		else if (Name.Equals(TEXT("ActorLabel"), ESearchCase::IgnoreCase) && IsRestrictString())  // We should call SetFolderPath, rather than modify it directly
		{
			const TArray<FString> StringData = GetStringData(Index);
			if (StringData.IsValidIndex(0))
				Actor->SetActorLabel(StringData[0]);
		}
		else if (Name.Equals(TEXT("Tags"), ESearchCase::IgnoreCase))
		{
			if (!IsRestrictString())
				return true;

			const TArray<FString> StringData = GetStringData(Index);
			Actor->Tags.Empty();
			for (const FString& TagStr : StringData)
				Actor->Tags.Add(*TagStr);

			return true;
		}
	}

	// -------- Set common properties --------
	return SetStructPropertyValues(Object, Object->GetClass(), Index, bVerbose);
}

const FName FHoudiniAttribute::SoftObjectPathName = FName("SoftObjectPath");
const FName FHoudiniAttribute::SoftClassPathName = FName("SoftClassPath");

bool FHoudiniAttribute::FindPropertyRecursive(const UStruct* Struct, const FString& PropertyName, const TSharedPtr<FEditPropertyChain>& PropertyChain, size_t& OutOffset)
{
	// Iterate manually on the properties, in order to handle StructProperties correctly
	for (TFieldIterator<FProperty> PropIter(Struct, EFieldIteratorFlags::IncludeSuper); PropIter; ++PropIter)
	{
		const FProperty* Prop = *PropIter;
		if (!Prop)
			continue;

		FString DisplayName = Prop->GetDisplayNameText().ToString();
		DisplayName.RemoveSpacesInline();
		const FString Name = Prop->GetName();
		FString Name_WithoutBoolPreFix = Name; Name_WithoutBoolPreFix.RemoveFromStart(TEXT("b"));

		const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
		if (ArrayProp)
			Prop = ArrayProp->Inner;

		const FStructProperty* StructProperty = CastField<FStructProperty>(Prop);
		if (Name.Equals(PropertyName, ESearchCase::IgnoreCase) || Name_WithoutBoolPreFix.Equals(PropertyName, ESearchCase::IgnoreCase) ||
			DisplayName.Equals(PropertyName, ESearchCase::IgnoreCase))
		{
			PropertyChain->AddTail(*PropIter);
			PropertyChain->SetActivePropertyNode(*PropIter);
			return true;
		}
		else if (StructProperty && !ArrayProp)
		{
			const FName StructName = StructProperty->Struct->GetFName();
			// If this struct is intrinsic support, then we should NOT parse recursive
			if (SupportStructNames.Contains(StructName) ||
				StructName == NAME_Color || StructName == NAME_Transform3f || StructName == NAME_Transform3d || StructName == NAME_Transform ||
				StructName == SoftObjectPathName || StructName == SoftClassPathName)
				continue;

			// Do a recursive parsing for StructProperties
			const int32 PropOffset = StructProperty->GetOffset_ForInternal();
			OutOffset += PropOffset;
			PropertyChain->AddTail(*PropIter);
			if (FindPropertyRecursive(StructProperty->Struct, PropertyName, PropertyChain, OutOffset))
				return true;
			else
			{
				PropertyChain->RemoveNode(*PropIter);
				OutOffset -= PropOffset;
			}
		}
	}

	return false;
}

bool FHoudiniAttribute::FindProperty(const UStruct* Struct, const FString& PropertyName, TSharedPtr<FEditPropertyChain>& OutFoundPropertyChain, size_t& OutOffset, const bool& bVerbose)
{
	if (const TMap<FString, TPair<TSharedPtr<FEditPropertyChain>, size_t>>* FoundPropertyMapPtr = StructPropertiesMap.Find(Struct))
	{
		if (const TPair<TSharedPtr<FEditPropertyChain>, size_t>* FoundPropertyOffsetPtr = FoundPropertyMapPtr->Find(PropertyName))
		{
			if (FoundPropertyOffsetPtr->Key.IsValid())
			{
				OutFoundPropertyChain = FoundPropertyOffsetPtr->Key;
				OutOffset = FoundPropertyOffsetPtr->Value;
				
				if (bVerbose)
					UE_LOG(LogHoudiniEngine, Log, TEXT("Get Property: %s in %s: %s"), *PropertyName,
						(Struct->IsA<UClass>() ? TEXT("Class") : TEXT("Struct")), *(Struct->GetName()));

				return true;
			}
			else
				return false;
		}
	}

	OutFoundPropertyChain = MakeShared<FEditPropertyChain>();
	OutOffset = 0;
	if (FindPropertyRecursive(Struct, PropertyName, OutFoundPropertyChain, OutOffset))
	{
		StructPropertiesMap.FindOrAdd(Struct).FindOrAdd(PropertyName) = TPair<TSharedPtr<FEditPropertyChain>, size_t>(OutFoundPropertyChain, OutOffset);
		
		UE_LOG(LogHoudiniEngine, Log, TEXT("Find Property: %s in %s: %s"), *PropertyName,
			(Struct->IsA<UClass>() ? TEXT("Class") : TEXT("Struct")), *(Struct->GetName()));
		
		return true;
	}
	else  // Not found, add a empty property chain to mark it not found
		StructPropertiesMap.FindOrAdd(Struct).FindOrAdd(PropertyName) = TPair<TSharedPtr<FEditPropertyChain>, size_t>();

	return false;
}



#define NOTIFY_PROPERTY_CHANGE(SET_VALUE_FUNC) \
	if (PropertyChain->Num() <= 1)\
		((UObject*)TargetPtr)->PreEditChange(CurrProp);\
	else\
		((UObject*)TargetPtr)->PreEditChange(*PropertyChain);\
	SET_VALUE_FUNC\
	FPropertyChangedEvent ChangedEvent(CurrProp, EPropertyChangeType::ValueSet, TArray<const UObject*>{ ((UObject*)TargetPtr) });\
	if (PropertyChain->Num() <= 1)\
		((UObject*)TargetPtr)->PostEditChangeProperty(ChangedEvent);\
	else\
	{\
		FPropertyChangedChainEvent ChangedChainEvent(*PropertyChain, ChangedEvent);\
		((UObject*)TargetPtr)->PostEditChangeChainProperty(ChangedChainEvent);\
	}\

#define SET_PROPERTY_BY_ATTRIB_VALUE(GET_VALUE, GET_ARRAY_VALUE, IF_VALUE_CHANGED, SET_PROPERTY_VALUE_FUNC) if (ArrayProp)\
{\
	FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, ContainerPtr);\
	const int32 ArrayLen = GetDataCount(Index) / (StructProperty ? TupleSize : 1);  /* single values NO need for TupleSize */\
	if (!bShouldMarkPropertyEdit)\
	{\
		if (ArrayHelper.Num() != ArrayLen) ArrayHelper.Resize(ArrayLen);\
		for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)\
		{\
			ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx); \
			GET_ARRAY_VALUE\
			SET_PROPERTY_VALUE_FUNC\
		}\
	}\
	else\
	{\
		/* First we should check array len changed, then check individual value changed */  bool bArrayChanged = ArrayHelper.Num() != ArrayLen;\
		if (!bArrayChanged)\
		{\
			for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)\
			{\
				ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);\
				GET_ARRAY_VALUE\
				IF_VALUE_CHANGED\
				{\
					bArrayChanged = true;\
					break;\
				}\
			}\
		}\
		if (bArrayChanged)\
		{\
			NOTIFY_PROPERTY_CHANGE(if (ArrayHelper.Num() != ArrayLen) ArrayHelper.Resize(ArrayLen);\
				for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)\
				{\
					ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);\
					GET_ARRAY_VALUE\
					SET_PROPERTY_VALUE_FUNC\
				})\
		}\
	}\
}\
else if (AttribData.IsValidIndex(0))\
{\
	GET_VALUE\
	if (!bShouldMarkPropertyEdit)\
	{\
		SET_PROPERTY_VALUE_FUNC\
	}\
	else IF_VALUE_CHANGED\
	{\
		NOTIFY_PROPERTY_CHANGE(SET_PROPERTY_VALUE_FUNC)\
	}\
}


#define SET_OBJECT_PROPERTY_BY_ATTRIB_VALUE(OBJECT_PROPERTY_TYPE, PROPERTY, OBJECT_PTR_TYPE) if (const OBJECT_PROPERTY_TYPE* PROPERTY = CastField<OBJECT_PROPERTY_TYPE>(Prop))\
{\
	const TArray<FString> AttribData = GetStringData(Index);\
	SET_PROPERTY_BY_ATTRIB_VALUE(UObject * FoundObject = AttribData[0].IsEmpty() ?\
			nullptr : LoadObject<UObject>(nullptr, *AttribData[0], nullptr, LOAD_Quiet | LOAD_NoWarn);,\
		UObject * FoundObject = AttribData[ArrayElemIdx].IsEmpty() ?\
			nullptr : LoadObject<UObject>(nullptr, *AttribData[ArrayElemIdx], nullptr, LOAD_Quiet | LOAD_NoWarn);,\
		if (PROPERTY->GetPropertyValue(ValuePtr) != OBJECT_PTR_TYPE(FoundObject)),\
			PROPERTY->SetObjectPropertyValue(ValuePtr, FoundObject););\
}


template<typename DataType, typename AttribStorage>
static void SetStructPropertValues(DataType* ValuePtr, const uint8& TupleSize, const TArray<AttribStorage>& AttribValues)
{
	for (uint8 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
	{
		if (!AttribValues.IsValidIndex(TupleIdx))
			break;

		ValuePtr[TupleIdx] = AttribValues[TupleIdx];
	}
}

template<typename DataType, typename AttribStorage>
static bool IsStructPropertValueChanged(const DataType* ValuePtr, const uint8& TupleSize, const TArray<AttribStorage>& AttribValues)
{
	bool bValueChanged = false;
	for (uint8 TupleIdx = 0; TupleIdx < TupleSize; ++TupleIdx)
	{
		if (!AttribValues.IsValidIndex(TupleIdx))
			break;

		if (ValuePtr[TupleIdx] != AttribValues[TupleIdx])
		{
			bValueChanged = true;
			break;
		}
	}

	return bValueChanged;
}

#define SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(DARA_TYPE, ATTRIB_DATA_TYPE, GET_ATTRIB_DATA) const TArray<ATTRIB_DATA_TYPE> AttribData = GET_ATTRIB_DATA(Index);\
	SET_PROPERTY_BY_ATTRIB_VALUE(const TArray<ATTRIB_DATA_TYPE>& ElemData = AttribData;,\
		TArray<ATTRIB_DATA_TYPE> ElemData(AttribData.GetData() + ArrayElemIdx * TupleSize, TupleSize);,\
		if (IsStructPropertValueChanged((const DARA_TYPE*)ValuePtr, SupportStructTupleSize[FoundStructIdx], ElemData)),\
			SetStructPropertValues((DARA_TYPE*)ValuePtr, SupportStructTupleSize[FoundStructIdx], ElemData);)


void FHoudiniAttribute::SetPropertyValues(void* TargetPtr, const UStruct* Struct,
	const TSharedPtr<FEditPropertyChain>& PropertyChain, void* ContainerPtr, const int32& Index) const
{
	FProperty* CurrProp = PropertyChain->GetActiveNode()->GetValue();

	FProperty* Prop = CurrProp;
	const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop);
	if (ArrayProp)
		Prop = ArrayProp->Inner;

	const bool bShouldMarkPropertyEdit = Struct->IsA<UClass>();  // Will check values changed and notify object property changed. Otherwise, just set value directly

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Prop))
	{
		void* ValuePtr = StructProperty->ContainerPtrToValuePtr<void>(ContainerPtr, 0);

		const FName StructName = StructProperty->Struct->GetFName();
		const int32 FoundStructIdx = SupportStructNames.IndexOfByKey(StructName);
		if (SupportStructNames.IsValidIndex(FoundStructIdx))
		{
			switch (SupportStructDataType[FoundStructIdx])
			{
			case EHoudiniDataType::Float:
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(float, float, GetFloatData)
			}
			break;
			case EHoudiniDataType::Double:
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(double, float, GetFloatData)
			}
			break;
			case EHoudiniDataType::Int:
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(int32, int32, GetIntData)
			}
			break;
			case EHoudiniDataType::Uint:
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(uint32, int32, GetIntData)
			}
			break;
			case EHoudiniDataType::Int64:
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(int64, int32, GetIntData)
			}
			break;
			case EHoudiniDataType::Uint64:
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(uint64, int32, GetIntData)
			}
			break;
			}
		}
		else if (StructName == NAME_Color)
		{
			if (Storage == HAPI_STORAGETYPE_UINT8 || Storage == HAPI_STORAGETYPE_UINT8_ARRAY)
			{
				SET_STRUCT_PROPERTY_BY_ATTRIB_VALUES(uint8, int32, GetIntData);
			}
			else
			{
				auto ConvertFloatToByteArrayLambda = [](const TArray<float>& InAttribData) -> TArray<uint8>
					{
						TArray<uint8> ElemData;
						for (const float& AttribValue : InAttribData)
							ElemData.Add(FMath::Clamp(FMath::RoundToInt(AttribValue * 255.0f), 0, 255));
						return ElemData;
					};

				const TArray<float> AttribData = GetFloatData(Index);
				SET_PROPERTY_BY_ATTRIB_VALUE(const TArray<uint8>&ElemData = ConvertFloatToByteArrayLambda(AttribData);,
					TArray<uint8> ElemData = ConvertFloatToByteArrayLambda(TArray<float>(AttribData.GetData() + ArrayElemIdx * TupleSize, TupleSize));,
					if (IsStructPropertValueChanged((const uint8*)ValuePtr, SupportStructTupleSize[FoundStructIdx], ElemData)),
						SetStructPropertValues((uint8*)ValuePtr, SupportStructTupleSize[FoundStructIdx], ElemData);)
			}
		}
		else if ((StructName == NAME_Transform3f || StructName == NAME_Transform3d || StructName == NAME_Transform)
			&& ((TupleSize == 9) || (TupleSize == 16)))  // 9: T(3)R(3)S(3), 16: Matrix4x4
		{
			auto ConvertFloatToMatrixLambda = [&](const float* AttribDataPtr) -> FTransform3f
				{
					return TupleSize == 9 ? 
						FTransform3f(*((FRotator3f*)(AttribDataPtr + 3)), *((FVector3f*)AttribDataPtr), *((FVector3f*)(AttribDataPtr + 6))) :
						FTransform3f(*((FMatrix44f*)AttribDataPtr));  // 16 (4x4)
				};

			const TArray<float> AttribData = GetFloatData(Index);
			if (StructName == NAME_Transform3f)
			{
				SET_PROPERTY_BY_ATTRIB_VALUE(const FTransform3f Value = ConvertFloatToMatrixLambda(AttribData.GetData());,
					const FTransform3f Value = ConvertFloatToMatrixLambda(AttribData.GetData() + ArrayElemIdx * TupleSize);,
					if (!((FTransform3f*)ValuePtr)->Equals(Value)),
						*(FTransform3f*)ValuePtr = Value;)
			}
			else
			{
				SET_PROPERTY_BY_ATTRIB_VALUE(const FTransform Value = FTransform(ConvertFloatToMatrixLambda(AttribData.GetData()));,
					const FTransform Value = FTransform(ConvertFloatToMatrixLambda(AttribData.GetData() + ArrayElemIdx * TupleSize));,
					if (!((FTransform*)ValuePtr)->Equals(Value)),
						*(FTransform*)ValuePtr = Value;)
			}
		}
		else if (IsRestrictString())
		{
			if (StructName == SoftObjectPathName)
			{
				const TArray<FString> AttribData = GetStringData(Index);
				SET_PROPERTY_BY_ATTRIB_VALUE(const FSoftObjectPath Value = AttribData[0];, const FSoftObjectPath Value = AttribData[ArrayElemIdx];,
					if (*((FSoftObjectPath*)ValuePtr) != Value),
						*((FSoftObjectPath*)ValuePtr) = Value;);
			}
			else if (StructName == SoftClassPathName)
			{
				const TArray<FString> AttribData = GetStringData(Index);
				SET_PROPERTY_BY_ATTRIB_VALUE(const FSoftClassPath Value = AttribData[0];, const FSoftClassPath Value = AttribData[ArrayElemIdx];,
					if (*((FSoftClassPath*)ValuePtr) != Value),
						*((FSoftClassPath*)ValuePtr) = Value;);
			}
		}

		// Also provide functionalities to convert dictionary to struct
		else if (((Storage == HAPI_STORAGETYPE_DICTIONARY) || (Storage == HAPI_STORAGETYPE_STRING)) && !ArrayProp)
		{
			TArray<FString> AttribData = GetStringData(Index);
			if (AttribData.IsValidIndex(0) && !AttribData[0].IsEmpty())
			{
				TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(AttribData[0]);
				TSharedPtr<FJsonObject> JsonStruct;
				if (FJsonSerializer::Deserialize(JsonReader, JsonStruct))
				{
					if (bShouldMarkPropertyEdit)
					{
						if (FJsonObjectConverter::JsonObjectToUStruct(JsonStruct.ToSharedRef(), StructProperty->Struct, ValuePtr))
							((UObject*)TargetPtr)->MarkPackageDirty();
					}
					else
						FJsonObjectConverter::JsonObjectToUStruct(JsonStruct.ToSharedRef(), StructProperty->Struct, ValuePtr);
				}
			}
		}
		else if (((Storage == HAPI_STORAGETYPE_DICTIONARY_ARRAY) || (Storage == HAPI_STORAGETYPE_STRING_ARRAY)) && ArrayProp)
		{
			FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, ContainerPtr);
			TArray<FString> AttribData = GetStringData(Index);
			const int32 ArrayLen = AttribData.Num();
			bool bHasValueChanged = false;
			if (ArrayHelper.Num() != ArrayLen)
			{
				ArrayHelper.Resize(ArrayLen);
				bHasValueChanged = true;
			}
			for (int32 ArrayElemIdx = 0; ArrayElemIdx < ArrayLen; ++ArrayElemIdx)
			{
				ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);
				if (!AttribData[ArrayElemIdx].IsEmpty())
				{
					TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(AttribData[ArrayElemIdx]);
					TSharedPtr<FJsonObject> JsonStruct;
					if (FJsonSerializer::Deserialize(JsonReader, JsonStruct))
					{
						if (FJsonObjectConverter::JsonObjectToUStruct(JsonStruct.ToSharedRef(), StructProperty->Struct, ValuePtr))
							bHasValueChanged = true;
					}
				}
			}

			if (bShouldMarkPropertyEdit && bHasValueChanged)
				((UObject*)TargetPtr)->MarkPackageDirty();
		}
	}
	else  // Common property type
	{
		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerPtr, 0);
		const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop);
		const FNumericProperty* NumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : CastField<FNumericProperty>(Prop);
		if (NumericProp)
		{
			if (NumericProp->IsInteger())
			{
				const TArray<int32> AttribData = GetIntData(Index);
				SET_PROPERTY_BY_ATTRIB_VALUE(const int64 Value = AttribData[0];, const int64 Value = AttribData[ArrayElemIdx];,
					if (NumericProp->GetSignedIntPropertyValue(ValuePtr) != Value),
						NumericProp->SetIntPropertyValue(ValuePtr, Value);)
				/*
				if (ArrayProp)
				{
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, ContainerPtr);
					const int32 ArrayLen = GetDataCount(Index);

					if (!bShouldMarkPropertyEdit)
					{
						if (ArrayHelper.Num() != ArrayLen)
							ArrayHelper.Resize(ArrayLen);
						for (int32 ArrayElemIdx = 0; ArrayElemIdx < AttribData.Num(); ++ArrayElemIdx)\
						{
							ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);
							const int64 Value = AttribData[ArrayElemIdx];
							NumericProp->SetIntPropertyValue(ValuePtr, Value);
						}
					}
					else
					{
						bool bArrayChanged = ArrayHelper.Num() != ArrayLen;

						if (!bArrayChanged)
						{
							for (int32 ArrayElemIdx = 0; ArrayElemIdx < AttribData.Num(); ++ArrayElemIdx)
							{
								ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);
								const int64 Value = AttribData[ArrayElemIdx];
								if (NumericProp->GetSignedIntPropertyValue(ValuePtr) != Value)
								{
									bArrayChanged = true;
									break;
								}
							}
						}

						if (bArrayChanged)
						{
							NOTIFY_PROPERTY_CHANGE(if (ArrayHelper.Num() != ArrayLen) ArrayHelper.Resize(ArrayLen);
								for (int32 ArrayElemIdx = 0; ArrayElemIdx < AttribData.Num(); ++ArrayElemIdx)
								{
									ValuePtr = ArrayHelper.GetElementPtr(ArrayElemIdx);
									const int32& AttribValue = AttribData[ArrayElemIdx];
									const int64 Value = AttribValue;
									NumericProp->SetIntPropertyValue(ValuePtr, Value);
								})
						}
					}
				}
				else if (AttribData.IsValidIndex(0))
				{
					const int64 Value = AttribData[0];
					if (!bShouldMarkPropertyEdit)
					{
						NumericProp->SetIntPropertyValue(ValuePtr, Value);
					}
					else if (NumericProp->GetSignedIntPropertyValue(ValuePtr) != Value)
					{
						NOTIFY_PROPERTY_CHANGE(NumericProp->SetIntPropertyValue(ValuePtr, Value);)
					}
				}
				*/
			}
			else if (NumericProp->IsFloatingPoint())
			{
				const TArray<float> AttribData = GetFloatData(Index);
				SET_PROPERTY_BY_ATTRIB_VALUE(const double Value = AttribData[0];, const double Value = AttribData[ArrayElemIdx];,
					if (NumericProp->GetFloatingPointPropertyValue(ValuePtr) != Value),
						NumericProp->SetFloatingPointPropertyValue(ValuePtr, Value););
			}
		}
		else if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			const TArray<int32> AttribData = GetIntData(Index);
			SET_PROPERTY_BY_ATTRIB_VALUE(const bool Value = bool(AttribData[0]);, const bool Value = bool(AttribData[ArrayElemIdx]);,
				if (BoolProp->GetPropertyValue(ValuePtr) != Value),
					BoolProp->SetPropertyValue(ValuePtr, Value););
		}
		else if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			const TArray<FString> AttribData = GetStringData(Index);
			SET_PROPERTY_BY_ATTRIB_VALUE(const FString& Value = AttribData[0];, const FString& Value = AttribData[ArrayElemIdx];,
				if (StrProp->GetPropertyValue(ValuePtr) != Value),
					StrProp->SetPropertyValue(ValuePtr, Value););
		}
		else if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			const TArray<FString> AttribData = GetStringData(Index);
			SET_PROPERTY_BY_ATTRIB_VALUE(const FName Value = *AttribData[0];, const FName Value = *AttribData[ArrayElemIdx];,
				if (NameProp->GetPropertyValue(ValuePtr) != Value),
					NameProp->SetPropertyValue(ValuePtr, Value););
		}
		else if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			const TArray<FString> AttribData = GetStringData(Index);
			SET_PROPERTY_BY_ATTRIB_VALUE(const FString& Value = AttribData[0];, const FString& Value = AttribData[ArrayElemIdx];,
				if (TextProp->GetPropertyValue(ValuePtr).ToString() != Value),
					TextProp->SetPropertyValue(ValuePtr, FText::FromString(Value)););
		}
		else if (IsRestrictString())
		{
			SET_OBJECT_PROPERTY_BY_ATTRIB_VALUE(FObjectProperty, ObjectProp, FObjectPtr)
			else SET_OBJECT_PROPERTY_BY_ATTRIB_VALUE(FWeakObjectProperty, WeakObjectProp, FWeakObjectPtr)
			else SET_OBJECT_PROPERTY_BY_ATTRIB_VALUE(FLazyObjectProperty, LazyObjectProp, FLazyObjectPtr)
			else SET_OBJECT_PROPERTY_BY_ATTRIB_VALUE(FSoftObjectProperty, SoftObjectProp, FSoftObjectPtr)
		}
	}
}
