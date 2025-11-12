// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "Engine/UserDefinedStruct.h"
#if WITH_EDITOR
#include "EdGraphSchema_K2.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniAttribute.h"


bool FHoudiniDataTableOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = false;
	bOutShouldHoldByOutput = false;
	if ((PartInfo.type == HAPI_PARTTYPE_MESH) && (PartInfo.faceCount == 0) && (PartInfo.pointCount >= 1))
	{
		// Check whether this is a DataTable output
		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id,
			HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWNAME, HAPI_ATTROWNER_POINT, &AttribInfo));

		if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_STRING))
		{
			bOutIsValid = true;
			return true;
		}
	}

	return true;
}

static void ConvertHoudiniAttributeStorage(FStructVariableDescription& NewVar, const HAPI_StorageType& Storage, const int32& TupleSize)
{
	switch (Storage % HAPI_STORAGETYPE_INT_ARRAY)
	{
	case HAPI_STORAGETYPE_UINT8:
	{
		if ((TupleSize == 3) || (TupleSize == 4))
		{ NewVar.SubCategory = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = TBaseStructure<FColor>::Get(); return; }
		NewVar.Category = UEdGraphSchema_K2::PC_Byte; return;
	}
	case HAPI_STORAGETYPE_INT8:
	case HAPI_STORAGETYPE_INT16:
	case HAPI_STORAGETYPE_INT:
	case HAPI_STORAGETYPE_INT64:
	{
		switch (TupleSize)
		{
		case 2: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.IntVector2"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		case 3: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.IntVector3"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		case 4: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.IntVector4"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		}
		NewVar.Category = UEdGraphSchema_K2::PC_Int; return;
	}
	case HAPI_STORAGETYPE_FLOAT:
	case HAPI_STORAGETYPE_FLOAT64:
	{
		switch (TupleSize)
		{
		case 2: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector2f"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		case 3: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector3f"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		case 4: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector4f"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		case 9: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Transform3f"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		case 16: { NewVar.Category = UEdGraphSchema_K2::PC_Struct; NewVar.SubCategoryObject = LoadObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Matrix44f"), nullptr, LOAD_Quiet | LOAD_NoWarn); } return;
		}
		NewVar.Category = UEdGraphSchema_K2::PC_Real; NewVar.SubCategory = UEdGraphSchema_K2::PC_Float; return;
	}
	case HAPI_STORAGETYPE_DICTIONARY:
	case HAPI_STORAGETYPE_STRING: NewVar.Category = UEdGraphSchema_K2::PC_String; return;
	}
}

static UUserDefinedStruct* FindOrCreateUserDefinedStructAsset(const FString& AssetPath)
{
	UUserDefinedStruct* TargetAsset = LoadObject<UUserDefinedStruct>(nullptr, *AssetPath, nullptr, LOAD_Quiet | LOAD_NoWarn);

	if (IsValid(TargetAsset))
		return TargetAsset;

	FString ObjectPath = FPackageName::ExportTextPathToObjectPath(AssetPath);
	FString PackagePath;
	FString AssetName;
	if (!ObjectPath.Split(TEXT("."), &PackagePath, &AssetName))
	{
		PackagePath = ObjectPath;
		AssetName = FPaths::GetBaseFilename(ObjectPath);
	}

	UPackage* Package = CreatePackage(*PackagePath);
	return FStructureEditorUtils::CreateUserDefinedStruct(Package, *AssetName, RF_Standalone | RF_Public);
}

bool FHoudiniDataTableOutputBuilder::HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputDataTable);

	const int32& NodeId = GeoInfo.nodeId;

	for (const HAPI_PartInfo& PartInfo : PartInfos)
	{
		const int32& PartId = PartInfo.id;

		TArray<std::string> AttribNames;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, AttribNames));

		FString RowStructRef;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
			AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWSTRUCT, RowStructRef));

		UScriptStruct* RowStruct = RowStructRef.IsEmpty() ? nullptr : LoadObject<UScriptStruct>(nullptr, *RowStructRef);

		TArray<TSharedPtr<FHoudiniAttribute>> ColumnAttribs;
		for (int32 AttribIdx = PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX];
			AttribIdx < PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX] + PartInfo.attributeCounts[HAPI_ATTROWNER_POINT]; ++AttribIdx)
		{
			const std::string& AttribName = AttribNames[AttribIdx];
			if (AttribName.starts_with(HAPI_ATTRIB_PREFIX_UNREAL_DATA_TABLE) &&
				AttribName != HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWSTRUCT && AttribName != HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWNAME)
			{
				const FString PropertyName(AttribName.c_str() + strlen(HAPI_ATTRIB_PREFIX_UNREAL_DATA_TABLE));
				if (PropertyName.IsEmpty())
					continue;

				if (IsValid(RowStruct))  // Check whether this property is exist, if not, then skip data Retrieve
				{
					TSharedPtr<FEditPropertyChain> PropertyChain;
					size_t Offset = 0;
					if (!FHoudiniAttribute::FindProperty(RowStruct, PropertyName, PropertyChain, Offset, false))
						continue;
				}

				HAPI_AttributeInfo AttribInfo;
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					AttribName.c_str(), HAPI_ATTROWNER_POINT, &AttribInfo));
				
				TSharedPtr<FHoudiniAttribute> PropAttrib = FHoudiniEngineUtils::IsArray(AttribInfo.storage) ?
					MakeShared<FHoudiniArrayAttribute>(PropertyName) : MakeShared<FHoudiniAttribute>(PropertyName);

				HOUDINI_FAIL_RETURN(PropAttrib->HapiRetrieveData(NodeId, PartId, AttribName.c_str(), AttribInfo));
				
				ColumnAttribs.Add(PropAttrib);
			}
		}

		if (!IsValid(RowStruct) && ColumnAttribs.IsEmpty())  // Means we could NOT construct a RowStruct, so skip
			continue;
		
		TArray<int32> RowElemIndices;  // The point indices, and may not be continue if RowNames has sames
		TArray<FName> RowNames;
		{
			HAPI_AttributeInfo AttribInfo;
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWNAME, HAPI_ATTROWNER_POINT, &AttribInfo));
			if (!AttribInfo.exists || AttribInfo.storage != HAPI_STORAGETYPE_STRING)  // Has been judged when UHoudiniOutput::HapiGetPartType
				continue;

			TArray<HAPI_StringHandle> RowNameSHs;
			RowNameSHs.SetNumUninitialized(AttribInfo.count);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_DATA_TABLE_ROWNAME, &AttribInfo, RowNameSHs.GetData(), 0, AttribInfo.count));

			// Ensure the row order and unique row names
			TArray<HAPI_StringHandle> UniqueRowNameSHs;
			TSet<HAPI_StringHandle> RowNameSHSet;
			for (int32 PointIdx = 0; PointIdx < AttribInfo.count; ++PointIdx)
			{
				bool bExists = false;
				RowNameSHSet.FindOrAdd(RowNameSHs[PointIdx], &bExists);
				if (!bExists)
				{
					RowElemIndices.Add(PointIdx);
					UniqueRowNameSHs.Add(RowNameSHs[PointIdx]);
				}
			}

			TArray<FString> RowNameStrs;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueRowNameSHs, RowNameStrs));
			for (const FString& RowNameStr : RowNameStrs)
				RowNames.Add(*RowNameStr);
		}
		
		FString AssetPath;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetStringAttributeValue(NodeId, PartId,
			AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OBJECT_PATH, AssetPath));

		if (IS_ASSET_PATH_INVALID(AssetPath))
			AssetPath = Node->GetCookFolderPath() + FString::Printf(TEXT("DT_%s_%d"), *FHoudiniEngineUtils::GetValidatedString(OutputName), PartId);

		UDataTable* DT = FHoudiniEngineUtils::FindOrCreateAsset<UDataTable>(AssetPath);
		
		if (IsValid(DT->RowStruct))
			DT->EmptyTable();

		if (!IsValid(RowStruct))
		{
			if (IS_ASSET_PATH_INVALID(RowStructRef))
				RowStructRef = Node->GetCookFolderPath() + FString::Printf(TEXT("UDS_%s_%d"), *FHoudiniEngineUtils::GetValidatedString(OutputName), PartId);

			UUserDefinedStruct* UDS = FindOrCreateUserDefinedStructAsset(RowStructRef);

			CastChecked<UUserDefinedStructEditorData>(UDS->EditorData)->VariablesDescriptions.Empty();

			for (const auto& ColumnAttrib : ColumnAttribs)
			{
				FEdGraphPinType PinType;
				PinType.ContainerType = FHoudiniEngineUtils::IsArray(ColumnAttrib->GetStorage()) ?
					EPinContainerType::Array : EPinContainerType::None;

				FStructVariableDescription NewVar;
				NewVar.SetPinType(PinType);
				NewVar.VarName = *ColumnAttrib->GetAttributeName();
				NewVar.FriendlyName = ColumnAttrib->GetAttributeName();
				NewVar.VarGuid = FGuid::NewGuid();
				ConvertHoudiniAttributeStorage(NewVar, ColumnAttrib->GetStorage(), ColumnAttrib->GetTupleSize());

				CastChecked<UUserDefinedStructEditorData>(UDS->EditorData)->VariablesDescriptions.Add(NewVar);
			}
			
			FStructureEditorUtils::CompileStructure(UDS);
			UDS->MarkPackageDirty();
			UDS->OnChanged();

			RowStruct = UDS;
		}
		
		
		TMap<FName, const uint8*> RowDataMap;
		for (int32 RowIdx = 0; RowIdx < RowNames.Num(); ++RowIdx)
		{
			void* RowDataPtr = FMemory::Malloc(RowStruct->GetStructureSize(), RowStruct->GetMinAlignment());
			RowStruct->InitializeStruct(RowDataPtr);  // We should init every row data, as it may contains array

			for (const TSharedPtr<FHoudiniAttribute>& ColumnAttrib : ColumnAttribs)
				ColumnAttrib->SetStructPropertyValues(RowDataPtr, RowStruct, RowElemIndices[RowIdx], false);

			RowDataMap.Add(RowNames[RowIdx], (const uint8*)RowDataPtr);
		}

		DT->CreateTableFromRawData(RowDataMap, RowStruct);

		for (const auto& Row : RowDataMap)
			RowStruct->DestroyStruct((void*)Row.Value);

		FHoudiniEngineUtils::NotifyAssetChanged(DT);
		DT->Modify();
	}

	return true;
}
