// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "Materials/MaterialInstanceConstant.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Engine/Font.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#endif

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAttribute.h"


bool FHoudiniMaterialInstanceOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = false;
	bOutShouldHoldByOutput = false;
	if ((PartInfo.type == HAPI_PARTTYPE_MESH) && (PartInfo.faceCount == 0) && (PartInfo.pointCount >= 1))
	{
		// Check whether this is an instancer output
		HAPI_AttributeInfo AttribInfo;
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id,
			HAPI_ATTRIB_UNREAL_INSTANCE, HAPI_ATTROWNER_POINT, &AttribInfo));

		if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_STRING))
		{
			bOutIsValid = false;
			return true;
		}

		// Check this is material instance asset output
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartInfo.id,
			HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, HAPI_ATTROWNER_POINT, &AttribInfo));

		if (AttribInfo.exists && (AttribInfo.storage == HAPI_STORAGETYPE_STRING))
		{
			bOutIsValid = true;
			return true;
		}
	}

	return true;
}

bool FHoudiniMaterialInstanceOutputBuilder::HapiRetrieve(AHoudiniNode* Node, const FString& OutputName, const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputMaterialInstance);

	const int32& NodeId = GeoInfo.nodeId;

	HAPI_AttributeInfo AttribInfo;
	int32 CreatedMatIdx = 0;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
	{
		const int32& PartId = PartInfo.id;
		
		TArray<std::string> AttribNames;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, AttribNames));

		TArray<UMaterialInterface*> ParentMats;
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, HAPI_ATTROWNER_POINT, &AttribInfo));

			TArray<HAPI_StringHandle> SHs;
			SHs.SetNumUninitialized(AttribInfo.count);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_MATERIAL_INSTANCE, &AttribInfo, SHs.GetData(), 0, AttribInfo.count));

			TMap<HAPI_StringHandle, UMaterialInterface*> SHMatMap;
			const TArray<HAPI_StringHandle> UniqueSHs = TSet<HAPI_StringHandle>(SHs).Array();
			TArray<FString> UniqueRefs;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueSHs, UniqueRefs));
			for (int32 UniqueIdx = 0; UniqueIdx < UniqueSHs.Num(); ++UniqueIdx)
			{
				UMaterialInterface* Instance = nullptr;
				FString& AssetRef = UniqueRefs[UniqueIdx];
				if (!IS_ASSET_PATH_INVALID(AssetRef))  // Do NOT use "IS_ASSET_PATH_INVALID", as strings like "PointLight", "SplineMeshComponent" also can be instantiated
					SHMatMap.Add(UniqueSHs[UniqueIdx], LoadObject<UMaterialInterface>(nullptr, *AssetRef, nullptr, LOAD_Quiet | LOAD_NoWarn));
				else
					SHMatMap.Add(UniqueSHs[UniqueIdx], nullptr);
			}

			ParentMats.SetNumZeroed(AttribInfo.count);
			for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
				ParentMats[ElemIdx] = SHMatMap[SHs[ElemIdx]];
		}

		TArray<TSharedPtr<FHoudiniAttribute>> MatAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_MATERIAL_PARAMETER, MatAttribs));

		TArray<FString> ObjectPaths;
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, HAPI_ATTROWNER_POINT, &AttribInfo));

			TArray<HAPI_StringHandle> SHs;
			SHs.SetNumUninitialized(AttribInfo.count);
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, &AttribInfo, SHs.GetData(), 0, AttribInfo.count));

			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(SHs, ObjectPaths));
		}

		UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = nullptr;
		for (int32 PointIdx = 0; PointIdx < PartInfo.pointCount; ++PointIdx)
		{
			UMaterialInterface* Material = ParentMats[PointIdx];
			if (IsValid(Material))
			{
				FString PackagePath;
				if (ObjectPaths.IsValidIndex(PointIdx))
					PackagePath = FHoudiniEngineUtils::GetPackagePath(ObjectPaths[PointIdx]);
				FString MatName;
				if (IS_ASSET_PATH_INVALID(PackagePath))
				{
					MatName = Material->GetName();
					MatName.RemoveFromStart(TEXT("M_"));
					if (!MatName.StartsWith(TEXT("MI_")))
						MatName = TEXT("MI_") + MatName;
					MatName += FString::Printf(TEXT("_%s_%d"), *OutputName, CreatedMatIdx);
					PackagePath = FHoudiniOutputUtils::GetCookFolderPath(Node) + MatName;
					++CreatedMatIdx;
				}
				else
				{
					int32 SplitIdx = -1;
					if (PackagePath.FindLastChar(TCHAR('/'), SplitIdx))
						MatName = PackagePath.RightChop(SplitIdx + 1);
				}

				UMaterialInstanceConstant* MatInst = LoadObject<UMaterialInstanceConstant>(nullptr, *PackagePath, nullptr, LOAD_Quiet | LOAD_NoWarn);
				if (!MatInst)
				{
					if (!MaterialInstanceFactory)
					{
						MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
						MaterialInstanceFactory->AddToRoot();
					}

					UPackage* Package = CreatePackage(*PackagePath);

					// Create the new material instance
					MaterialInstanceFactory->InitialParent = Material;
					MatInst = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(
						UMaterialInstanceConstant::StaticClass(), Package, *MatName,
						RF_Public | RF_Standalone, nullptr, GWarn);
					MatInst->Modify();

					FAssetRegistryModule::AssetCreated(MatInst);  // Let Content Browser to show the NewAsset
				}
				else if (MatInst->Parent != Material)
					MatInst->SetParentEditorOnly(Material);

				for (const TSharedPtr<FHoudiniAttribute>& Attrib : MatAttribs)
				{
					const int32 ElemIdx = POINT_ATTRIB_ENTRY_IDX(Attrib->GetOwner(), PointIdx);
					for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
					{
						const EMaterialParameterType Type = (EMaterialParameterType)TypeIndex;
						FMaterialParameterMetadata Meta;
						FName MatParmName(*Attrib->GetAttributeName());
						if (!Material->GetParameterValue(Type, FHashedMaterialParameterInfo(MatParmName), Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
						{
							MatParmName = *(Attrib->GetAttributeName().Replace(TEXT("_"), TEXT(" ")));
							if (!Material->GetParameterValue(Type, FHashedMaterialParameterInfo(MatParmName), Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
								continue;
						}

						switch (Type)
						{
						case EMaterialParameterType::Scalar:
						{
							TArray<float> Values = Attrib->GetFloatData(ElemIdx);
							if (Values.IsValidIndex(0))
							{
								float OrigValue;
								MatInst->GetScalarParameterValue(MatParmName, OrigValue);
								if (OrigValue != Values[0])
									MatInst->SetScalarParameterValueEditorOnly(MatParmName, Values[0]);
							}
						}
						break;
						case EMaterialParameterType::Vector:
							//case EMaterialParameterType::DoubleVector:
						{
							TArray<float> Values = Attrib->GetFloatData(ElemIdx);
							if (Values.IsValidIndex(0))
							{
								FLinearColor Value;
								MatInst->GetVectorParameterValue(MatParmName, Value);
								bool bChanged = false;
								for (int32 ValueIdx = 0; ValueIdx < 4; ++ValueIdx)
								{
									if (Values.IsValidIndex(ValueIdx) && (Values[ValueIdx] != (&Value.R)[ValueIdx]))
									{
										(&Value.R)[ValueIdx] = Values[ValueIdx];
										bChanged = true;
									}
								}
								if (bChanged)
									MatInst->SetVectorParameterValueEditorOnly(MatParmName, Value);
							}
						}
						break;
						case EMaterialParameterType::Texture:
							if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
							{
								TArray<FString> Values = Attrib->GetStringData(ElemIdx);
								if (Values.IsValidIndex(0))
								{
									if (UTexture* Texture = LoadObject<UTexture>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
									{
										UTexture* OrigTexture = nullptr;
										MatInst->GetTextureParameterValue(MatParmName, OrigTexture);
										if (Texture != OrigTexture)
											MatInst->SetTextureParameterValueEditorOnly(MatParmName, Texture);
									}
								}
							}
							break;
						case EMaterialParameterType::Font:
							if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
							{
								TArray<FString> Values = Attrib->GetStringData(ElemIdx);
								if (Values.IsValidIndex(0))
								{
									if (UFont* Font = LoadObject<UFont>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
									{
										UFont* OrigValue;
										int32 FontPage;
										MatInst->GetFontParameterValue(MatParmName, OrigValue, FontPage);
										if (Font != OrigValue)
											MatInst->SetFontParameterValueEditorOnly(MatParmName, Font, FontPage);
									}
								}
							}
							break;
						case EMaterialParameterType::RuntimeVirtualTexture:
							if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
							{
								TArray<FString> Values = Attrib->GetStringData(ElemIdx);
								if (Values.IsValidIndex(0))
								{
									if (URuntimeVirtualTexture* Texture = LoadObject<URuntimeVirtualTexture>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
									{
										URuntimeVirtualTexture* OrigTexture = nullptr;
										MatInst->GetRuntimeVirtualTextureParameterValue(MatParmName, OrigTexture);
										if (Texture != OrigTexture)
											MatInst->SetRuntimeVirtualTextureParameterValueEditorOnly(MatParmName, Texture);
									}
								}
							}
							break;
						case EMaterialParameterType::SparseVolumeTexture:
							if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
							{
								TArray<FString> Values = Attrib->GetStringData(ElemIdx);
								if (Values.IsValidIndex(0))
								{
									if (USparseVolumeTexture* Texture = LoadObject<USparseVolumeTexture>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
									{
										USparseVolumeTexture* OrigTexture = nullptr;
										MatInst->GetSparseVolumeTextureParameterValue(MatParmName, OrigTexture);
										if (Texture != OrigTexture)
											MatInst->SetSparseVolumeTextureParameterValueEditorOnly(MatParmName, Texture);
									}
								}
							}
							break;
						case EMaterialParameterType::StaticSwitch:
						{
							TArray<int32> Values = Attrib->GetIntData(ElemIdx);
							if (Values.IsValidIndex(0))
							{
								bool bOrigValue = false;
								FGuid ExpId;
								MatInst->GetStaticSwitchParameterValue(MatParmName, bOrigValue, ExpId);
								if (bOrigValue != bool(Values[0]))
									MatInst->SetStaticSwitchParameterValueEditorOnly(MatParmName, bool(Values[0]));
							}
						}
						break;
						}
					}
				}
			}
		}

		if (MaterialInstanceFactory)
			MaterialInstanceFactory->RemoveFromRoot();
	}

	return true;
}


UMaterialInterface* FHoudiniOutputUtils::GetMaterialInstance(UMaterialInterface* Material,
	const TArray<TSharedPtr<FHoudiniAttribute>>& MatAttribs, const TFunctionRef<int32(const HAPI_AttributeOwner&)> Index,
	const FString& CookFolderPath, TMap<TPair<UMaterialInterface*, uint32>, UMaterialInstance*>& InOutMatParmMap)
{
	if (IsValid(Material))
	{
		struct FHoudiniMaterialParameterValue
		{
			FHoudiniMaterialParameterValue(const FName& InName, const float& ScalarValue) :
				Name(InName), Type(EMaterialParameterType::Scalar)
			{
				NumericValue.R = ScalarValue;
			}

			FHoudiniMaterialParameterValue(const FName& InName, const EMaterialParameterType& InType, const FVector4f& Value) :
				Name(InName), Type(InType), NumericValue(Value) {}

			FHoudiniMaterialParameterValue(const FName& InName, const EMaterialParameterType& InType, UObject* Value) :
				Name(InName), Type(InType), ObjectValue(Value) {}

			const FName Name;
			EMaterialParameterType Type;
			FLinearColor NumericValue = FLinearColor::White;
			UObject* ObjectValue = nullptr;

			void AppendBinary(TArray<uint8>& BinaryValues) const
			{
				switch (Type)
				{
				case EMaterialParameterType::Scalar: BinaryValues.Append((uint8*)&NumericValue.R, sizeof(float)); return;
				case EMaterialParameterType::Vector:
				case EMaterialParameterType::DoubleVector: BinaryValues.Append((uint8*)&NumericValue.R, sizeof(float) * 4); return;
				case EMaterialParameterType::Texture:
				case EMaterialParameterType::Font:
				case EMaterialParameterType::RuntimeVirtualTexture:
				case EMaterialParameterType::SparseVolumeTexture:
					if (ObjectValue)
					{
						FString ObjectPath = ObjectValue->GetPathName();
						BinaryValues.Append((uint8*)ObjectPath.GetCharArray().GetData(), ObjectPath.GetCharArray().Num() * ObjectPath.GetCharArray().GetTypeSize());
					}
					return;
				case EMaterialParameterType::StaticSwitch: BinaryValues.Add(NumericValue.R >= 0.5f ? 1 : 0); return;
				}
			}
		};


		TArray<FHoudiniMaterialParameterValue> ParmValues;
		for (const TSharedPtr<FHoudiniAttribute>& Attrib : MatAttribs)
		{
			const int32 ElemIdx = Index(Attrib->GetOwner());
			for (int32 TypeIndex = 0; TypeIndex < NumMaterialParameterTypes; ++TypeIndex)
			{
				const EMaterialParameterType Type = (EMaterialParameterType)TypeIndex;
				FMaterialParameterMetadata Meta;
				FName MatPropName(*Attrib->GetAttributeName());
				if (!Material->GetParameterValue(Type, FHashedMaterialParameterInfo(MatPropName), Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
				{
					MatPropName = *(Attrib->GetAttributeName().Replace(TEXT("_"), TEXT(" ")));
					if (!Material->GetParameterValue(Type, FHashedMaterialParameterInfo(MatPropName), Meta, EMaterialGetParameterValueFlags::CheckNonOverrides))
						continue;
				}

				switch (Type)
				{
				case EMaterialParameterType::Scalar:
				{
					TArray<float> Values = Attrib->GetFloatData(ElemIdx);
					if (Values.IsValidIndex(0))
						ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Values[0]));
				}
				break;
				case EMaterialParameterType::Vector:
					//case EMaterialParameterType::DoubleVector:
				{
					TArray<float> Values = Attrib->GetFloatData(ElemIdx);
					if (Values.IsValidIndex(0))
					{
						FVector4f Value;
						for (int32 ValueIdx = 0; ValueIdx < 4; ++ValueIdx)
							Value[ValueIdx] = Values.IsValidIndex(ValueIdx) ? Values[ValueIdx] : Meta.Value.Float[ValueIdx];
						ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Type, Value));
					}
				}
				break;
				case EMaterialParameterType::Texture:
					if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
					{
						TArray<FString> Values = Attrib->GetStringData(ElemIdx);
						if (Values.IsValidIndex(0))
						{
							if (UTexture* Texture = LoadObject<UTexture>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
								ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Type, Texture));
						}
					}
					break;
				case EMaterialParameterType::Font:
					if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
					{
						TArray<FString> Values = Attrib->GetStringData(ElemIdx);
						if (Values.IsValidIndex(0))
						{
							if (UFont* Font = LoadObject<UFont>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
								ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Type, Font));
						}
					}
					break;
				case EMaterialParameterType::RuntimeVirtualTexture:
					if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
					{
						TArray<FString> Values = Attrib->GetStringData(ElemIdx);
						if (Values.IsValidIndex(0))
						{
							if (URuntimeVirtualTexture* Texture = LoadObject<URuntimeVirtualTexture>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
								ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Type, Texture));
						}
					}
					break;
				case EMaterialParameterType::SparseVolumeTexture:
					if (FHoudiniEngineUtils::ConvertStorageType(Attrib->GetStorage()) == EHoudiniStorageType::String)
					{
						TArray<FString> Values = Attrib->GetStringData(ElemIdx);
						if (Values.IsValidIndex(0))
						{
							if (USparseVolumeTexture* Texture = LoadObject<USparseVolumeTexture>(nullptr, *Values[0], nullptr, LOAD_Quiet | LOAD_NoWarn))
								ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Type, Texture));
						}
					}
					break;
				case EMaterialParameterType::StaticSwitch:
				{
					TArray<int32> Values = Attrib->GetIntData(ElemIdx);
					if (Values.IsValidIndex(0))
						ParmValues.Add(FHoudiniMaterialParameterValue(MatPropName, Type, FVector4f(Values[0], 0.0f, 0.0f, 0.0f)));
				}
				break;
				}
			}
		}

		ParmValues.Sort([](const FHoudiniMaterialParameterValue& ParmA, const FHoudiniMaterialParameterValue& ParmB)
			{ return ParmA.Name.Compare(ParmB.Name) < 0; });  // Sort by name, make sure that if values are same, then string is same
		TArray<uint8> BinaryValues;
		for (const FHoudiniMaterialParameterValue& ParmValue : ParmValues)
			ParmValue.AppendBinary(BinaryValues);
		const uint32 HashValue = GetTypeHash(BinaryValues);

		const TPair<UMaterialInterface*, uint32> Identifier(Material, HashValue);

		if (UMaterialInstance** FoundMatInstPtr = InOutMatParmMap.Find(Identifier))
			return *FoundMatInstPtr;

		FString MatName = Material->GetName();
		MatName.RemoveFromStart(TEXT("M_"));
		if (!MatName.StartsWith(TEXT("MI_")))
			MatName = TEXT("MI_") + MatName;
		MatName = FString::Printf(TEXT("%s_%08X_%08X"), *MatName, GetTypeHash(Material->GetPathName()), HashValue);
		const FString PackagePath = FHoudiniEngineUtils::GetPackagePath(CookFolderPath + MatName);
		UMaterialInstanceConstant* MatInst = LoadObject<UMaterialInstanceConstant>(nullptr, *PackagePath, nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (!MatInst)
		{
			UPackage* Package = CreatePackage(*PackagePath);
			UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();

			// Create the new material instance
			MaterialInstanceFactory->AddToRoot();
			MaterialInstanceFactory->InitialParent = Material;
			MatInst = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(
				UMaterialInstanceConstant::StaticClass(), Package, *MatName,
				RF_Public | RF_Standalone, nullptr, GWarn);
			MatInst->Modify();

			FAssetRegistryModule::AssetCreated(MatInst);  // Let Content Browser to show the NewAsset

			MaterialInstanceFactory->RemoveFromRoot();
		}

		for (const FHoudiniMaterialParameterValue& ParmValue : ParmValues)
		{
			const FMaterialParameterInfo MatParmInfo(ParmValue.Name);
			switch (ParmValue.Type)
			{
			case EMaterialParameterType::Scalar:
			{
				const float& TargetValue = ParmValue.NumericValue.R;
				float OrigValue;
				MatInst->GetScalarParameterValue(MatParmInfo, OrigValue);
				if (TargetValue != OrigValue)
				{
					MatInst->SetScalarParameterValueEditorOnly(MatParmInfo, TargetValue);
					MatInst->Modify();
				}
			}
			break;
			case EMaterialParameterType::Vector:
			{
				const FLinearColor& TargetValue = ParmValue.NumericValue;
				FLinearColor OrigValue;
				MatInst->GetVectorParameterValue(MatParmInfo, OrigValue);
				if (OrigValue != TargetValue)
				{
					MatInst->SetVectorParameterValueEditorOnly(MatParmInfo, TargetValue);
					MatInst->Modify();
				}
			}
			break;
			//case EMaterialParameterType::DoubleVector:
			case EMaterialParameterType::Texture:
			{
				UTexture* TargetValue = Cast<UTexture>(ParmValue.ObjectValue);
				UTexture* OrigValue;
				MatInst->GetTextureParameterValue(MatParmInfo, OrigValue);
				if (TargetValue != OrigValue)
				{
					MatInst->SetTextureParameterValueEditorOnly(MatParmInfo, TargetValue);
					MatInst->Modify();
				}
			}
			break;
			case EMaterialParameterType::Font:
			{
				UFont* TargetValue = Cast<UFont>(ParmValue.ObjectValue);
				UFont* OrigValue;
				int32 FontPage;
				MatInst->GetFontParameterValue(MatParmInfo, OrigValue, FontPage);
				if (TargetValue != OrigValue)
				{
					MatInst->SetFontParameterValueEditorOnly(MatParmInfo, TargetValue, FontPage);
					MatInst->Modify();
				}
			}
			break;
			case EMaterialParameterType::RuntimeVirtualTexture:
			{
				URuntimeVirtualTexture* TargetValue = Cast<URuntimeVirtualTexture>(ParmValue.ObjectValue);
				URuntimeVirtualTexture* OrigValue;
				MatInst->GetRuntimeVirtualTextureParameterValue(MatParmInfo, OrigValue);
				if (TargetValue != OrigValue)
				{
					MatInst->SetRuntimeVirtualTextureParameterValueEditorOnly(MatParmInfo, TargetValue);
					MatInst->Modify();
				}
			}
			break;
			case EMaterialParameterType::SparseVolumeTexture:
			{
				USparseVolumeTexture* TargetValue = Cast<USparseVolumeTexture>(ParmValue.ObjectValue);
				USparseVolumeTexture* OrigValue;
				MatInst->GetSparseVolumeTextureParameterValue(MatParmInfo, OrigValue);
				if (TargetValue != OrigValue)
				{
					MatInst->SetSparseVolumeTextureParameterValueEditorOnly(MatParmInfo, TargetValue);
					MatInst->Modify();
				}
			}
			break;
			case EMaterialParameterType::StaticSwitch:
			{
				const bool TargetValue = ParmValue.NumericValue.R >= 0.5f;
				bool OrigValue;
				FGuid ExpGuid;
				MatInst->GetStaticSwitchParameterValue(MatParmInfo, OrigValue, ExpGuid);
				if (TargetValue != OrigValue)
				{
					MatInst->SetStaticSwitchParameterValueEditorOnly(MatParmInfo, TargetValue);
					MatInst->Modify();
				}
			}
			break;
			}
		}

		InOutMatParmMap.Add(Identifier, MatInst);
		return MatInst;
	}

	return nullptr;
}
