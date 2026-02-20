// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniInputs.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/Polys.h"
#include "StaticMeshComponentLODInfo.h"
#include "Engine/SkinnedAssetCommon.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniAttribute.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniOperatorUtils.h"

#define ENABLE_INSTANCEDSKINNEDMESH_INPUT ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 6)) || (ENGINE_MAJOR_VERSION > 5)
#if ENABLE_INSTANCEDSKINNEDMESH_INPUT
#ifdef UE_EXPERIMENTAL
#pragma push_macro("UE_EXPERIMENTAL")
#undef UE_EXPERIMENTAL
#define UE_EXPERIMENTAL(VERSION, MESSAGE)
#endif

#include "Components/InstancedSkinnedMeshComponent.h"

#pragma pop_macro("UE_EXPERIMENTAL")
#endif


// FHoudiniActorComponentInputBuilder
bool FHoudiniActorComponentInputBuilder::IsValidInput(const UActorComponent* Component)
{
	return Component->GetClass() != USceneComponent::StaticClass();
}

bool FHoudiniActorComponentInputBuilder::HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
	int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints)
{
	for (const int32& CompIdx : ComponentIndices)
	{
		const UActorComponent* Component = Components[CompIdx];
		if (Components.Num() <= 3 && Component->GetClass() == UBillboardComponent::StaticClass())  // As SplineComponent will automatically create a UBillboardComponent, but we need NOT to import it
		{
			if (Components.ContainsByPredicate([](const UActorComponent* Component) { return Component->IsA<USplineComponent>(); }))
				continue;
		}

		FHoudiniComponentInputPoint Point;
		Point.bHasBeenCopied = false;
		Point.Transform = Transforms[CompIdx];
		if (const USceneComponent* SC = Cast<USceneComponent>(Component))
		{
			if (const UDecalComponent* DC = Cast<UDecalComponent>(Component))
				Point.AssetRef = IsValid(DC->GetDecalMaterial()) ? TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(DC->GetDecalMaterial())) : "";
			else if (const USpotLightComponent* SLC = Cast<USpotLightComponent>(Component))
			{
				const FLinearColor LightColor = SLC->GetLightColor();
				Point.MetaData = TCHAR_TO_UTF8(*FString::Printf(
					TEXT("{\"Intensity\":%f,\"LightColor\":[%f,%f,%f],\"InnerConeAngle\":%f,\"OuterConeAngle\":%f}"),
					SLC->Intensity, LightColor.R, LightColor.G, LightColor.B, SLC->InnerConeAngle, SLC->OuterConeAngle));
			}
			else if (const UPointLightComponent* PLC = Cast<UPointLightComponent>(Component))
			{
				const FLinearColor LightColor = PLC->GetLightColor();
				Point.MetaData = TCHAR_TO_UTF8(*FString::Printf(
					TEXT("{\"Intensity\":%f,\"LightColor\":[%f,%f,%f],\"SourceRadius\":%f,\"SoftSourceRadius\":%f}"),
					PLC->Intensity, LightColor.R, LightColor.G, LightColor.B, PLC->SourceRadius, PLC->SoftSourceRadius));
			}
			else if (const ULightComponent* LC = Cast<ULightComponent>(Component))
			{
				const FLinearColor LightColor = LC->GetLightColor();
				Point.MetaData = TCHAR_TO_UTF8(*FString::Printf(
					TEXT("{\"Intensity\":%f,\"LightColor\":[%f,%f,%f]}"), LC->Intensity, LightColor.R, LightColor.G, LightColor.B));
			}
			else if (const UBillboardComponent* BBC = Cast<UBillboardComponent>(Component))
			{
				if (IsValid(BBC->Sprite))
					Point.MetaData = TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"Sprite\":\"%s\"}"), *FHoudiniEngineUtils::GetAssetReference(BBC->Sprite.Get())));
			}
			// TODO: upload meta data
			else if (const UBrushComponent* BC = Cast<UBrushComponent>(Component))
			{
				if (AVolume* Volume = Cast<AVolume>(BC->GetOwner()))
				{
					Point.AssetRef = TCHAR_TO_UTF8(*Volume->GetClass()->GetPathName());
					const FBox Box = Volume->GetBounds().GetBox();
					Point.MetaData = TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"bounds\":[%s,%s,%s,%s,%s,%s]}"), PRINT_HOUDINI_FLOAT(Box.Min.X), PRINT_HOUDINI_FLOAT(Box.Max.X),
						PRINT_HOUDINI_FLOAT(Box.Min.Z), PRINT_HOUDINI_FLOAT(Box.Max.Z), PRINT_HOUDINI_FLOAT(Box.Min.Y), PRINT_HOUDINI_FLOAT(Box.Max.Y)));
				}
			}
		}

		if (Point.AssetRef.empty())
			Point.AssetRef = TCHAR_TO_UTF8(*Component->GetClass()->GetPathName());

		InOutPoints.Add(Point);
	}

	return true;
}

#define FLOAT_TO_HOUDINI_JSON_VALUE(VALUE) MakeShared<FJsonValueNumberString>(FString::SanitizeFloat(FMath::RoundToInt64((VALUE) * 100.0) * 0.0001, 0))
#define FLOAT_TO_SHORT_JSON_VALUE(VALUE) MakeShared<FJsonValueNumberString>(FString::SanitizeFloat(FMath::RoundToInt64((VALUE) * 10000.0) * 0.0001, 0))

static void AppendJsonTransform(const FTransform& Transform, TArray<TSharedPtr<FJsonValue>>& JsonPositions,
	TArray<TSharedPtr<FJsonValue>>& JsonRotations,
	TArray<TSharedPtr<FJsonValue>>& JsonScales)
{
	const FVector Position = Transform.GetLocation();
	const FQuat Rotation = Transform.GetRotation();
	const FVector Scale = Transform.GetScale3D();
	JsonPositions.Append(TArray<TSharedPtr<FJsonValue>>{FLOAT_TO_HOUDINI_JSON_VALUE(Position.X), FLOAT_TO_HOUDINI_JSON_VALUE(Position.Z), FLOAT_TO_HOUDINI_JSON_VALUE(Position.Y)});
	JsonRotations.Append(TArray<TSharedPtr<FJsonValue>>{FLOAT_TO_SHORT_JSON_VALUE(Rotation.X), FLOAT_TO_SHORT_JSON_VALUE(Rotation.Z), FLOAT_TO_SHORT_JSON_VALUE(Rotation.Y), FLOAT_TO_SHORT_JSON_VALUE(-Rotation.W)});
	JsonScales.Append(TArray<TSharedPtr<FJsonValue>>{FLOAT_TO_SHORT_JSON_VALUE(Scale.X), FLOAT_TO_SHORT_JSON_VALUE(Scale.Z), FLOAT_TO_SHORT_JSON_VALUE(Scale.Y)});
}

void FHoudiniActorComponentInputBuilder::AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
	const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject)
{
	TMap<FString, TArray<int32>> AssetRefIndicesMap;
	for (const int32& CompIdx : ComponentIndices)
	{
		const UActorComponent* Component = Components[CompIdx];
		if (Components.Num() <= 3 && Component->GetClass() == UBillboardComponent::StaticClass())  // As SplineComponent will automatically create a UBillboardComponent, but we need NOT to import it
		{
			if (Components.ContainsByPredicate([](const UActorComponent* Component) { return Component->IsA<USplineComponent>(); }))
				continue;
		}

		FString AssetRef;
		if (const UDecalComponent* DC = Cast<UDecalComponent>(Component))
		{
			if (IsValid(DC->GetDecalMaterial()))
				AssetRef = FHoudiniEngineUtils::GetAssetReference(DC->GetDecalMaterial());
		}

		if (AssetRef.IsEmpty())
			AssetRef = Component->GetClass()->GetPathName();

		AssetRefIndicesMap.FindOrAdd(AssetRef).Add(CompIdx);
	}

	for (const auto& AssetRefIndices : AssetRefIndicesMap)
	{
		TSharedPtr<FJsonValue>& JsonAssetInfoValue = JsonObject->Values.FindOrAdd(AssetRefIndices.Key);
		if (JsonAssetInfoValue.IsValid())  // Has been added to JsonObject
			continue;

		TSharedPtr<FJsonObject> JsonAssetInfo = MakeShared<FJsonObject>();
		JsonAssetInfoValue = MakeShared<FJsonValueObject>(JsonAssetInfo);
		TArray<TSharedPtr<FJsonValue>> JsonPositions;
		TArray<TSharedPtr<FJsonValue>> JsonRotations;
		TArray<TSharedPtr<FJsonValue>> JsonScales;

		for (const int32& CompIdx : AssetRefIndices.Value)
			AppendJsonTransform(Transforms[CompIdx], JsonPositions, JsonRotations, JsonScales);

		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_POSITION), JsonPositions);
		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_ROT), JsonRotations);
		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_SCALE), JsonScales);
	}
}

// FHoudiniMeshComponentInputBuilder
bool FHoudiniStaticMeshComponentInput::HapiDestroy(UHoudiniInput* Input) const
{
	if (MeshNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MeshNodeId));

	if (SettingsNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
	
	Input->NotifyMergedNodeDestroyed();

	Invalidate();

	return true;
}

void FHoudiniStaticMeshComponentInput::Invalidate() const
{
	if (MeshHandle)
		FHoudiniEngineUtils::CloseSharedMemoryHandle(MeshHandle);
}

bool FHoudiniStaticMeshComponentInputBuilder::IsValidInput(const UActorComponent* Component)
{
	return Component->IsA<UStaticMeshComponent>();
}

template<typename TMeshCompInput, typename ESettingsNodeType>
static TSharedPtr<TMeshCompInput> FindOrCreateMeshComponentInput(TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs,
	const ESettingsNodeType& TargetSettingsNodeType)
{
	for (int32 HolderIdx = InOutComponentInputs.Num() - 1; HolderIdx >= 0; --HolderIdx)
	{
		const TSharedPtr<TMeshCompInput>& OldComponentInput = StaticCastSharedPtr<TMeshCompInput>(InOutComponentInputs[HolderIdx]);
		if (OldComponentInput->Type == TargetSettingsNodeType)
		{
			InOutComponentInputs.RemoveAt(HolderIdx);
			return OldComponentInput;
		}
	}

	return MakeShared<TMeshCompInput>(TargetSettingsNodeType);
}

template<typename TMesh, typename TMaterialSlot, typename TComponentLODInfo>
static TPair<const TMesh*, TArray<const UMaterialInterface*>> GetMeshInputIdentifier(
	const TMesh* SM, const TArray<TMaterialSlot>& MaterialSlots, const UMeshComponent* SMC, const TArray<TComponentLODInfo>& ComponentLODInfo, bool& bOutHasVertexColor)
{
	bOutHasVertexColor = false;
	for (const TComponentLODInfo& LODInfo : ComponentLODInfo)
	{
		if (LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices())
		{
			bOutHasVertexColor = true;
			return TPair<const TMesh*, TArray<const UMaterialInterface*>>(nullptr, TArray<const UMaterialInterface*>{});
		}
	}

	TArray<const UMaterialInterface*> Materials;
	for (const TMaterialSlot& MaterialSlot : MaterialSlots)
	{
		UMaterialInterface* Material = nullptr;
		if (IsValid(SMC))
		{
			UMaterialInterface* OverrideMaterialInterface = SMC->GetMaterial(SMC->GetMaterialIndex(MaterialSlot.ImportedMaterialSlotName));
			if (IsValid(OverrideMaterialInterface))
				Material = OverrideMaterialInterface;
		}
		else
		{
			if (IsValid(MaterialSlot.MaterialInterface))
				Material = MaterialSlot.MaterialInterface;
		}

		Materials.Add(Material);
	}

	return TPair<const TMesh*, TArray<const UMaterialInterface*>>(SM, Materials);
}

static bool HapiUploadStaticMesh(const UStaticMesh* SM, const UStaticMeshComponent* SMC, const FInt32Interval& PointIdxRange, UHoudiniInput* Input, int32& InOutInstancerNodeId,
	TArray<TSharedPtr<FHoudiniComponentInput>>& InOutNewComponentInputs, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutOldComponentInputs,
	TMap<TPair<const UStaticMesh*, TArray<const UMaterialInterface*>>, TPair<int32, TArray<FInt32Interval>>>& InOutMeshPatternMap)
{
	bool bHasVertexColor = false;  // This mesh should be a unique mesh
	const TPair<const UStaticMesh*, TArray<const UMaterialInterface*>> MeshIdentifier = GetMeshInputIdentifier(SM, SM->GetStaticMaterials(), SMC, SMC->LODData, bHasVertexColor);
	if (!bHasVertexColor)
	{
		if (TPair<int32, TArray<FInt32Interval>>* FoundPatternPtr = InOutMeshPatternMap.Find(MeshIdentifier))
		{
			if (FoundPatternPtr->Value.Last().Max == PointIdxRange.Min - 1)
				FoundPatternPtr->Value.Last().Max = PointIdxRange.Max;
			else
				FoundPatternPtr->Value.Add(PointIdxRange);

			return true;  // We need NOT create mesh input again
		}
	}

	const int32& ParentGeoNodeId = Input->GetGeoNodeId();
	const FHoudiniInputSettings& Settings = Input->GetSettings();

	TSharedPtr<FHoudiniStaticMeshComponentInput> MeshComponentInput = FindOrCreateMeshComponentInput<FHoudiniStaticMeshComponentInput>(InOutOldComponentInputs,
		FHoudiniStaticMeshComponentInput::EHoudiniSettingsNodeType::copytopoints);

	HOUDINI_FAIL_RETURN(UHoudiniInputStaticMesh::HapiImport(SM, SMC, Settings,
		ParentGeoNodeId, MeshComponentInput->MeshNodeId, MeshComponentInput->MeshHandle));

	if (InOutInstancerNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(ParentGeoNodeId,
			FString::Printf(TEXT("%s_%08X"), *SMC->GetOuter()->GetName(), FPlatformTime::Cycles()), InOutInstancerNodeId));

	int32& SettingsNodeId = MeshComponentInput->SettingsNodeId;
	const bool bCreateNewSettingsNode = (SettingsNodeId < 0);
	if (bCreateNewSettingsNode)
	{
		HOUDINI_FAIL_RETURN(FHoudiniSopCopyToPoints::HapiCreateNode(ParentGeoNodeId,
			FString::Printf(TEXT("%s_%08X"), *SMC->GetName(), FPlatformTime::Cycles()), SettingsNodeId));

		HOUDINI_FAIL_RETURN(FHoudiniSopCopyToPoints::HapiInitialize(SettingsNodeId));

		HOUDINI_FAIL_RETURN(FHoudiniSopCopyToPoints::HapiConnectInputs(SettingsNodeId,
			MeshComponentInput->MeshNodeId, InOutInstancerNodeId));
	}

	if (bHasVertexColor)  // Means this is a unique mesh
		HOUDINI_FAIL_RETURN(FHoudiniSopCopyToPoints::HapiSetTargetPointGroup(SettingsNodeId,
			(PointIdxRange.Min == PointIdxRange.Max) ? FString::FromInt(PointIdxRange.Min) : FString::Printf(TEXT("%d-%d"), PointIdxRange.Min, PointIdxRange.Max)))
	else
		InOutMeshPatternMap.Add(MeshIdentifier, TPair<int32, TArray<FInt32Interval>>(
			SettingsNodeId, TArray<FInt32Interval>{ PointIdxRange }));

	if (bCreateNewSettingsNode)
		HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(SettingsNodeId));

	InOutNewComponentInputs.Add(MeshComponentInput);

	return true;
}

static FString GetActorOutlinerPath(const AActor* Actor)
{
	TArray<FString> Path;
	for (int32 Iter = 0; Iter < 100; ++Iter)
	{
		Path.Add(Actor->GetActorLabel(false));
		if (const AActor* ParentActor = Actor->GetAttachParentActor())
			Actor = ParentActor;
		else
		{
			const FName FolderPath = Actor->GetFolderPath();
			if (!FolderPath.IsNone())
				Path.Add(FolderPath.ToString());
			break;
		}
	}
	FString FullPath;
	for (int32 PathIdx = Path.Num() - 1; PathIdx >= 0; --PathIdx)
	{
		FullPath += Path[PathIdx];
		if (PathIdx)
			FullPath += TEXT("/");
	}
	return FullPath;
}

bool FHoudiniStaticMeshComponentInputBuilder::HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
	int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints)
{
	const FHoudiniInputSettings& Settings = Input->GetSettings();
	const int32& ParentGeoNodeId = Input->GetGeoNodeId();

	if (!Settings.bImportAsReference && bIsSingleComponent && !Components[ComponentIndices[0]]->IsA<UInstancedStaticMeshComponent>() &&
		Components[ComponentIndices[0]]->IsA<UStaticMeshComponent>())  // Maybe is just AStaticMeshActor
	{
		const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Components[ComponentIndices[0]]);
		const UStaticMesh* SM = SMC->GetStaticMesh();
		if (IsValid(SM))
		{
			TSharedPtr<FHoudiniStaticMeshComponentInput> SMCInput = FindOrCreateMeshComponentInput<FHoudiniStaticMeshComponentInput>(InOutComponentInputs,
				FHoudiniStaticMeshComponentInput::EHoudiniSettingsNodeType::he_setup_mesh_input);

			int32& SettingsNodeId = SMCInput->SettingsNodeId;
			const bool bCreateNewSettingsNode = (SettingsNodeId < 0);

			HOUDINI_FAIL_RETURN(UHoudiniInputStaticMesh::HapiImport(SM, SMC, Settings, ParentGeoNodeId,
				SMCInput->MeshNodeId, SMCInput->MeshHandle));

			if (bCreateNewSettingsNode)
			{
				HOUDINI_FAIL_RETURN(FHoudiniEngineSetupMeshInput::HapiCreateNode(ParentGeoNodeId,
					FString::Printf(TEXT("%s_%08X"), *SMC->GetName(), FPlatformTime::Cycles()), SettingsNodeId));

				HOUDINI_FAIL_RETURN(FHoudiniEngineSetupMeshInput::HapiConnectInput(
					SettingsNodeId, SMCInput->MeshNodeId));
			}

			const FTransform& Transform = Transforms[ComponentIndices[0]];

			const AActor* Actor = SMC->GetOwner();
			FHoudiniEngineSetupMeshInput MeshSettings;
			MeshSettings.AppendAttribute(HAPI_ATTRIB_UNREAL_INSTANCE,
				EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SM));
			MeshSettings.AppendAttribute(Actor ? HAPI_ATTRIB_UNREAL_ACTOR_PATH : HAPI_ATTRIB_UNREAL_OBJECT_PATH,
				EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SMC->GetOuter()));
			if (Actor)
				MeshSettings.AppendAttribute(HAPI_ATTRIB_UNREAL_ACTOR_OUTLINER_PATH,
					EHoudiniAttributeOwner::Point, GetActorOutlinerPath(Actor));
			HOUDINI_FAIL_RETURN(MeshSettings.HapiUpload(SettingsNodeId, Transform));

			if (bCreateNewSettingsNode)
				HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(SettingsNodeId));

			// Remove all reset settings
			for (const TSharedPtr<FHoudiniComponentInput>& OldComponentInput : InOutComponentInputs)
				HOUDINI_FAIL_RETURN(OldComponentInput->HapiDestroy(Input));

			InOutComponentInputs = TArray<TSharedPtr<FHoudiniComponentInput>>{ SMCInput };

			return true;
		}
	}
	
	TArray<TSharedPtr<FHoudiniComponentInput>> NewComponentInputs;
	TMap<TPair<const UStaticMesh*, TArray<const UMaterialInterface*>>, TPair<int32, TArray<FInt32Interval>>> MeshPatternMap;
	for (const int32& CompIdx : ComponentIndices)
	{
		const UActorComponent* Component = Components[CompIdx];
		if (const UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(Component))
		{
			if (ISMC->GetInstanceCount() <= 0)
				continue;

			const UStaticMesh* SM = ISMC->GetStaticMesh();
			std::string MetaData;
			if (IsValid(SM) && Settings.bImportAsReference)
			{
				const FBox Box = SM->GetBounds().GetBox();
				const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
				const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
				MetaData = TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y));
			}

			const std::string AssetRef = TCHAR_TO_UTF8(*(IsValid(SM) ? FHoudiniEngineUtils::GetAssetReference(SM) : ISMC->GetClass()->GetPathName()));
			const FTransform& Transform = Transforms[CompIdx];
			for (int32 InstIdx = 0; InstIdx < ISMC->GetInstanceCount(); ++InstIdx)
			{
				FHoudiniComponentInputPoint Point;
				Point.AssetRef = AssetRef;
				Point.bHasBeenCopied = IsValid(SM) && !Settings.bImportAsReference;
				ISMC->GetInstanceTransform(InstIdx, Point.Transform, false);
				Point.Transform *= Transform;
				if (Settings.bImportAsReference)
				{
					if (IsValid(SM))
						Point.MetaData = MetaData;
				}

				InOutPoints.Add(Point);
			}

			if (IsValid(SM) && !Settings.bImportAsReference)
				HOUDINI_FAIL_RETURN(HapiUploadStaticMesh(SM, ISMC,
					FInt32Interval(InOutPoints.Num() - ISMC->GetInstanceCount(), InOutPoints.Num() - 1), Input,
					InOutInstancerNodeId, NewComponentInputs, InOutComponentInputs, MeshPatternMap));
		}
		else if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component))
		{
			const USplineMeshComponent* SplineMC = Cast<USplineMeshComponent>(SMC);
			const UStaticMesh* SM = SMC->GetStaticMesh();
			FHoudiniComponentInputPoint Point;
			Point.AssetRef = TCHAR_TO_UTF8(*((IsValid(SM) && !SplineMC) ? FHoudiniEngineUtils::GetAssetReference(SM) : SMC->GetClass()->GetPathName()));
			Point.bHasBeenCopied = IsValid(SM) && !Settings.bImportAsReference;
			Point.Transform = Transforms[CompIdx];

			if (IsValid(SM) && Settings.bImportAsReference)
			{
				const FBox Box = SM->GetBounds().GetBox();
				const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
				const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
				Point.MetaData = TCHAR_TO_UTF8((SplineMC ? *FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f],"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y) :
					*FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y)));
			}
			
			if (SplineMC)
			{
				const FVector& StartPos = SplineMC->SplineParams.StartPos;
				const FVector& StartTangent = SplineMC->SplineParams.StartTangent;
				const FVector& EndPos = SplineMC->SplineParams.EndPos;
				const FVector& EndTangent = SplineMC->SplineParams.EndTangent;
				if (Point.MetaData.empty())
					Point.MetaData = "{";
				Point.MetaData += TCHAR_TO_UTF8(*FString::Printf(
					TEXT("\"StaticMesh\":\"%s\",\"StartPos\":[%f,%f,%f],\"StartTangent\":[%f,%f,%f],\"EndPos\":[%f,%f,%f],\"EndTangent\":[%f,%f,%f]}"),
					*FHoudiniEngineUtils::GetAssetReference(SplineMC->GetStaticMesh().Get()),
					StartPos.X, StartPos.Y, StartPos.Z, StartTangent.X, StartTangent.Y, StartTangent.Z,
					EndPos.X, EndPos.Y, EndPos.Z, EndTangent.X, EndTangent.Y, EndTangent.Z));
			}

			InOutPoints.Add(Point);

			if (IsValid(SM) && !Settings.bImportAsReference)
				HOUDINI_FAIL_RETURN(HapiUploadStaticMesh(SM, SMC, FInt32Interval(InOutPoints.Num() - 1, InOutPoints.Num() - 1), Input,
					InOutInstancerNodeId, NewComponentInputs, InOutComponentInputs, MeshPatternMap));
		}
	}

	for (const TSharedPtr<FHoudiniComponentInput>& OldComponentInput : InOutComponentInputs)
		HOUDINI_FAIL_RETURN(OldComponentInput->HapiDestroy(Input));

	for (const auto& MeshPattern : MeshPatternMap)
	{
		FString Pattern;
		for (const FInt32Interval& Range : MeshPattern.Value.Value)
			Pattern += (Range.Min == Range.Max) ? FString::Printf(TEXT("%d "), Range.Min) : FString::Printf(TEXT("%d-%d "), Range.Min, Range.Max);
		Pattern.RemoveFromEnd(TEXT(" "));

		HOUDINI_FAIL_RETURN(FHoudiniSopCopyToPoints::HapiSetTargetPointGroup(MeshPattern.Value.Key, Pattern));
	}

	InOutComponentInputs = NewComponentInputs;

	return true;
}

void FHoudiniStaticMeshComponentInputBuilder::AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
	const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject)
{
	TMap<FString, TArray<int32>> AssetRefIndicesMap;
	for (const int32& CompIdx : ComponentIndices)
	{
		FString AssetRef;
		const UActorComponent* Component = Components[CompIdx];
		if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component))
			AssetRef = FHoudiniEngineUtils::GetAssetReference(SMC->GetStaticMesh().Get());
			
		if (AssetRef.IsEmpty())
			AssetRef = Component->GetClass()->GetPathName();

		AssetRefIndicesMap.FindOrAdd(AssetRef).Add(CompIdx);
	}

	for (const auto& AssetRefIndices : AssetRefIndicesMap)
	{
		TSharedPtr<FJsonValue>& JsonAssetInfoValue = JsonObject->Values.FindOrAdd(AssetRefIndices.Key);
		if (JsonAssetInfoValue.IsValid())  // Has been added to JsonObject
			continue;

		TSharedPtr<FJsonObject> JsonAssetInfo = MakeShared<FJsonObject>();
		JsonAssetInfoValue = MakeShared<FJsonValueObject>(JsonAssetInfo);
		

		if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Components[AssetRefIndices.Value[0]]))
		{
			const UStaticMesh* SM = SMC->GetStaticMesh();
			if (IsValid(SM))
			{
				const FBox Box = SM->GetBounds().GetBox();
				JsonAssetInfo->SetArrayField(TEXT("bounds"), TArray<TSharedPtr<FJsonValue>>{FLOAT_TO_HOUDINI_JSON_VALUE(Box.Min.X), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Max.X),
					FLOAT_TO_HOUDINI_JSON_VALUE(Box.Min.Z), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Max.Z), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Min.Y), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Max.Y)});
			}
		}
		
		TArray<TSharedPtr<FJsonValue>> JsonPositions;
		TArray<TSharedPtr<FJsonValue>> JsonRotations;
		TArray<TSharedPtr<FJsonValue>> JsonScales;
		for (const int32& CompIdx : AssetRefIndices.Value)
		{
			const FTransform& Transform = Transforms[CompIdx];
			if (const UInstancedStaticMeshComponent* ISMC = Cast<UInstancedStaticMeshComponent>(Components[CompIdx]))
			{
				for (int32 InstIdx = 0; InstIdx < ISMC->GetInstanceCount(); ++InstIdx)
				{
					FTransform InstTransform;
					ISMC->GetInstanceTransform(InstIdx, InstTransform);
					AppendJsonTransform(InstTransform * Transform, JsonPositions, JsonRotations, JsonScales);
				}
			}
			else  // StaticMeshComponent
				AppendJsonTransform(Transform, JsonPositions, JsonRotations, JsonScales);
		}

		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_POSITION), JsonPositions);
		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_ROT), JsonRotations);
		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_SCALE), JsonScales);
	}
}

// FHoudiniSkeletalMeshComponentInput
bool FHoudiniSkeletalMeshComponentInput::HapiDestroy(UHoudiniInput* Input) const
{
	if (MeshNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), MeshNodeId));

	if (SkeletonNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SkeletonNodeId));

	if (SettingsNodeId >= 0)
	{
		Input->NotifyMergedNodeDestroyed();
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
	}

	Invalidate();

	return true;
}

void FHoudiniSkeletalMeshComponentInput::Invalidate() const
{
	if (MeshHandle)
		FHoudiniEngineUtils::CloseSharedMemoryHandle(MeshHandle);

	if (SkeletonHandle)
		FHoudiniEngineUtils::CloseSharedMemoryHandle(SkeletonHandle);
}

bool FHoudiniSkinnedMeshComponentInputBuilder::IsValidInput(const UActorComponent* Component)
{
#if ENABLE_INSTANCEDSKINNEDMESH_INPUT
	return Component->IsA<USkinnedMeshComponent>();
#else
	return Component->IsA<USkeletalMeshComponent>();
#endif
}

static bool HapiUploadSkeletalMesh(const USkeletalMesh* SM, const USkinnedMeshComponent* SMC, const FInt32Interval& PointIdxRange, UHoudiniInput* Input, int32& InOutInstancerNodeId,
	TArray<TSharedPtr<FHoudiniComponentInput>>& InOutNewComponentInputs, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutOldComponentInputs,
	TMap<TPair<const USkeletalMesh*, TArray<const UMaterialInterface*>>, TPair<int32, TArray<FInt32Interval>>>& InOutMeshPatternMap)
{
	bool bHasVertexColor = false;  // This mesh should be a unique mesh
	const TPair<const USkeletalMesh*, TArray<const UMaterialInterface*>> MeshIdentifier = GetMeshInputIdentifier(SM, SM->GetMaterials(), SMC, SMC->LODInfo, bHasVertexColor);
	if (!bHasVertexColor)
	{
		if (TPair<int32, TArray<FInt32Interval>>* FoundPatternPtr = InOutMeshPatternMap.Find(MeshIdentifier))
		{
			if (FoundPatternPtr->Value.Last().Max == PointIdxRange.Min - 1)
				FoundPatternPtr->Value.Last().Max = PointIdxRange.Max;
			else
				FoundPatternPtr->Value.Add(PointIdxRange);

			return true;  // We need NOT create mesh input again
		}
	}

	const int32& ParentGeoNodeId = Input->GetGeoNodeId();
	const FHoudiniInputSettings& Settings = Input->GetSettings();

	TSharedPtr<FHoudiniSkeletalMeshComponentInput> MeshComponentInput = FindOrCreateMeshComponentInput<FHoudiniSkeletalMeshComponentInput>(InOutOldComponentInputs,
		FHoudiniSkeletalMeshComponentInput::EHoudiniSettingsNodeType::he_setup_kinefx_inputs);

	HOUDINI_FAIL_RETURN(UHoudiniInputSkeletalMesh::HapiImport(SM, SMC, Settings,
		ParentGeoNodeId, MeshComponentInput->MeshNodeId, MeshComponentInput->MeshHandle, MeshComponentInput->SkeletonNodeId, MeshComponentInput->SkeletonHandle));

	if (InOutInstancerNodeId < 0)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(ParentGeoNodeId,
			FString::Printf(TEXT("%s_%08X"), *SMC->GetOuter()->GetName(), FPlatformTime::Cycles()), InOutInstancerNodeId));

	int32& SettingsNodeId = MeshComponentInput->SettingsNodeId;
	const bool bCreateNewSettingsNode = (SettingsNodeId < 0);
	if (bCreateNewSettingsNode)
	{
		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInputs::HapiCreateNode(ParentGeoNodeId,
			FString::Printf(TEXT("%s_%08X"), *SMC->GetName(), FPlatformTime::Cycles()), SettingsNodeId));

		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInputs::HapiConnectInputs(SettingsNodeId,
			MeshComponentInput->MeshNodeId, MeshComponentInput->SkeletonNodeId, InOutInstancerNodeId));
	}

	if (bHasVertexColor)  // Means this is a unique mesh
		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInputs::HapiSetTargetPointGroup(SettingsNodeId,
			(PointIdxRange.Min == PointIdxRange.Max) ? FString::FromInt(PointIdxRange.Min) : FString::Printf(TEXT("%d-%d"), PointIdxRange.Min, PointIdxRange.Max)))
	else
		InOutMeshPatternMap.Add(MeshIdentifier, TPair<int32, TArray<FInt32Interval>>(
			SettingsNodeId, TArray<FInt32Interval>{ PointIdxRange }));

	if (bCreateNewSettingsNode)
		HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(SettingsNodeId));

	InOutNewComponentInputs.Add(MeshComponentInput);

	return true;
}

bool FHoudiniSkinnedMeshComponentInputBuilder::HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
	int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints)
{
	const FHoudiniInputSettings& Settings = Input->GetSettings();
	const int32& ParentGeoNodeId = Input->GetGeoNodeId();

	if (bIsSingleComponent)
	{
		if (const USkeletalMeshComponent* SKMC = Cast<USkeletalMeshComponent>(Components[0]))
		{
			const USkeletalMesh* SKM = SKMC->GetSkeletalMeshAsset();
			if (!(Input->GetSettings().bImportAsReference) && IsValid(SKM))
			{
				const FHoudiniSkeletalMeshComponentInput::EHoudiniSettingsNodeType SettingNodeType =
					(!(Input->GetSettings().bImportAsReference) && IsValid(SKM)) ? FHoudiniSkeletalMeshComponentInput::EHoudiniSettingsNodeType::he_setup_kinefx_input :
					FHoudiniSkeletalMeshComponentInput::EHoudiniSettingsNodeType::null;
				TSharedPtr<FHoudiniSkeletalMeshComponentInput> SKMCInput;
				if (InOutComponentInputs.IsEmpty())
					SKMCInput = MakeShared<FHoudiniSkeletalMeshComponentInput>(SettingNodeType);
				else
				{
					int32 FoundCompInputIdx = InOutComponentInputs.IndexOfByPredicate([SettingNodeType](const TSharedPtr<FHoudiniComponentInput>& OldCompInput)
						{ return StaticCastSharedPtr<FHoudiniSkeletalMeshComponentInput>(OldCompInput)->Type == SettingNodeType; });
					if (FoundCompInputIdx >= 0)
					{
						SKMCInput = StaticCastSharedPtr<FHoudiniSkeletalMeshComponentInput>(InOutComponentInputs[FoundCompInputIdx]);
						InOutComponentInputs.RemoveAt(FoundCompInputIdx);
					}
					else
						SKMCInput = MakeShared<FHoudiniSkeletalMeshComponentInput>(SettingNodeType);
				}
				int32& SettingsNodeId = SKMCInput->SettingsNodeId;

				int32& MeshNodeId = SKMCInput->MeshNodeId;
				int32& SkeletonNodeId = SKMCInput->SkeletonNodeId;
				HOUDINI_FAIL_RETURN(UHoudiniInputSkeletalMesh::HapiImport(SKM, SKMC, Input->GetSettings(), ParentGeoNodeId,
					MeshNodeId, SKMCInput->MeshHandle, SkeletonNodeId, SKMCInput->SkeletonHandle));

				if (SettingsNodeId < 0)
				{
					HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInput::HapiCreateNode(ParentGeoNodeId,
						FString::Printf(TEXT("%s_%08X"), *SKMC->GetName(), FPlatformTime::Cycles()), SettingsNodeId));
					HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInput::HapiConnectInput(SettingsNodeId, MeshNodeId, SkeletonNodeId));
					HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(SettingsNodeId));
				}

				const AActor* Actor = SKMC->GetOwner();
				FHoudiniEngineSetupKineFXInput SettingsInput;
				SettingsInput.AppendAttribute(HAPI_ATTRIB_UNREAL_INSTANCE,
					EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SKM));
				SettingsInput.AppendAttribute(Actor ? HAPI_ATTRIB_UNREAL_ACTOR_PATH : HAPI_ATTRIB_UNREAL_OBJECT_PATH,
					EHoudiniAttributeOwner::Point, FHoudiniEngineUtils::GetAssetReference(SKMC->GetOuter()));
				if (Actor)
					SettingsInput.AppendAttribute(HAPI_ATTRIB_UNREAL_ACTOR_OUTLINER_PATH,
						EHoudiniAttributeOwner::Point, GetActorOutlinerPath(Actor));
				HOUDINI_FAIL_RETURN(SettingsInput.HapiUpload(SettingsNodeId, Transforms[0]));

				for (const TSharedPtr<FHoudiniComponentInput>& ComponentInput : InOutComponentInputs)
					HOUDINI_FAIL_RETURN(ComponentInput->HapiDestroy(Input));
				InOutComponentInputs.SetNum(1);
				InOutComponentInputs[0] = SKMCInput;

				return true;
			}
		}
	}

	TArray<TSharedPtr<FHoudiniComponentInput>> NewComponentInputs;
	TMap<TPair<const USkeletalMesh*, TArray<const UMaterialInterface*>>, TPair<int32, TArray<FInt32Interval>>> MeshPatternMap;
	for (const int32& CompIdx : ComponentIndices)
	{
		const UActorComponent* Component = Components[CompIdx];
#if ENABLE_INSTANCEDSKINNEDMESH_INPUT
		if (const UInstancedSkinnedMeshComponent* ISMC = Cast<UInstancedSkinnedMeshComponent>(Component))
		{
			if (ISMC->GetInstanceCount() <= 0)
				continue;

			const USkeletalMesh* SM = Cast<USkeletalMesh>(ISMC->GetSkinnedAsset());
			std::string MetaData;
			if (IsValid(SM) && Settings.bImportAsReference)
			{
				const FBox Box = SM->GetBounds().GetBox();
				const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
				const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
#if ((ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 7)) || (ENGINE_MAJOR_VERSION > 5)
				MetaData = IsValid(ISMC->GetTransformProvider()) ?
					TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f],\"TransformProvider\":\"%s\"}"),
						Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y, *FHoudiniEngineUtils::GetAssetReference(ISMC->GetTransformProvider()))) :
					TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y));
			}
			else if (IsValid(ISMC->GetTransformProvider()))
				MetaData = TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"TransformProvider\":\"%s\"}"), *FHoudiniEngineUtils::GetAssetReference(ISMC->GetTransformProvider())));
#else
				MetaData = TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y));
			}
#endif
			const std::string AssetRef = TCHAR_TO_UTF8(*(IsValid(SM) ? FHoudiniEngineUtils::GetAssetReference(SM) : ISMC->GetClass()->GetPathName()));
			const FTransform& Transform = Transforms[CompIdx];
			for (const FSkinnedMeshInstanceData& Instance : ISMC->GetInstanceData())
			{
				FHoudiniComponentInputPoint Point;
				Point.AssetRef = AssetRef;
				Point.bHasBeenCopied = IsValid(SM) && !Settings.bImportAsReference;
				Point.Transform = FTransform(Instance.Transform) * Transform;
				if (!MetaData.empty())
					Point.MetaData = MetaData;

				InOutPoints.Add(Point);
			}

			if (IsValid(SM) && !Settings.bImportAsReference)
				HOUDINI_FAIL_RETURN(HapiUploadSkeletalMesh(SM, ISMC,
					FInt32Interval(InOutPoints.Num() - ISMC->GetInstanceCount(), InOutPoints.Num() - 1), Input,
					InOutInstancerNodeId, NewComponentInputs, InOutComponentInputs, MeshPatternMap));
		}
		else
#endif
		if (const USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(Component))
		{
			const USkeletalMesh* SM = SMC->GetSkeletalMeshAsset();
			FHoudiniComponentInputPoint Point;
			Point.AssetRef = TCHAR_TO_UTF8(*(IsValid(SM) ? FHoudiniEngineUtils::GetAssetReference(SM) : SMC->GetClass()->GetPathName()));
			Point.bHasBeenCopied = IsValid(SM) && !Settings.bImportAsReference;
			Point.Transform = Transforms[CompIdx];

			if (IsValid(SM) && Settings.bImportAsReference)
			{
				const FBox Box = SM->GetBounds().GetBox();
				const FVector3f Min = FVector3f(Box.Min) * POSITION_SCALE_TO_HOUDINI_F;
				const FVector3f Max = FVector3f(Box.Max) * POSITION_SCALE_TO_HOUDINI_F;
				Point.MetaData = TCHAR_TO_UTF8(*FString::Printf(TEXT("{\"bounds\":[%f,%f,%f,%f,%f,%f]}"), Min.X, Max.X, Min.Z, Max.Z, Min.Y, Max.Y));
			}
			InOutPoints.Add(Point);

			if (IsValid(SM) && !Settings.bImportAsReference)
				HOUDINI_FAIL_RETURN(HapiUploadSkeletalMesh(SM, SMC, FInt32Interval(InOutPoints.Num() - 1, InOutPoints.Num() - 1), Input,
					InOutInstancerNodeId, NewComponentInputs, InOutComponentInputs, MeshPatternMap));
		}
	}

	for (const TSharedPtr<FHoudiniComponentInput>& OldComponentInput : InOutComponentInputs)
		HOUDINI_FAIL_RETURN(OldComponentInput->HapiDestroy(Input));

	for (const auto& MeshPattern : MeshPatternMap)
	{
		FString Pattern;
		for (const FInt32Interval& Range : MeshPattern.Value.Value)
			Pattern += (Range.Min == Range.Max) ? FString::Printf(TEXT("%d "), Range.Min) : FString::Printf(TEXT("%d-%d "), Range.Min, Range.Max);
		Pattern.RemoveFromEnd(TEXT(" "));

		HOUDINI_FAIL_RETURN(FHoudiniEngineSetupKineFXInputs::HapiSetTargetPointGroup(MeshPattern.Value.Key, Pattern));
	}

	InOutComponentInputs = NewComponentInputs;

	return true;
}

void FHoudiniSkinnedMeshComponentInputBuilder::AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
	const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject)
{
	TMap<FString, TArray<int32>> AssetRefIndicesMap;
	for (const int32& CompIdx : ComponentIndices)
	{
		FString AssetRef;
		const UActorComponent* Component = Components[CompIdx];
		if (const USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(Component))
			AssetRef = FHoudiniEngineUtils::GetAssetReference(SMC->GetSkeletalMeshAsset());
#if ENABLE_INSTANCEDSKINNEDMESH_INPUT
		else if (const UInstancedSkinnedMeshComponent* ISMC = Cast<UInstancedSkinnedMeshComponent>(Component))
			AssetRef = FHoudiniEngineUtils::GetAssetReference(ISMC->GetSkinnedAsset());
#endif
		if (AssetRef.IsEmpty())
			AssetRef = Component->GetClass()->GetPathName();

		AssetRefIndicesMap.FindOrAdd(AssetRef).Add(CompIdx);
	}

	for (const auto& AssetRefIndices : AssetRefIndicesMap)
	{
		TSharedPtr<FJsonValue>& JsonAssetInfoValue = JsonObject->Values.FindOrAdd(AssetRefIndices.Key);
		if (JsonAssetInfoValue.IsValid())  // Has been added to JsonObject
			continue;

		TSharedPtr<FJsonObject> JsonAssetInfo = MakeShared<FJsonObject>();
		JsonAssetInfoValue = MakeShared<FJsonValueObject>(JsonAssetInfo);

		{
			const USkeletalMesh* SM = nullptr;
			if (const USkeletalMeshComponent* SMC = Cast<USkeletalMeshComponent>(Components[AssetRefIndices.Value[0]]))
				SM = SMC->GetSkeletalMeshAsset();
#if ENABLE_INSTANCEDSKINNEDMESH_INPUT
			else if (const UInstancedSkinnedMeshComponent* ISMC = Cast<UInstancedSkinnedMeshComponent>(Components[AssetRefIndices.Value[0]]))
				SM = Cast<USkeletalMesh>(ISMC->GetSkinnedAsset());
#endif
			if (IsValid(SM))
			{
				const FBox Box = SM->GetBounds().GetBox();
				JsonAssetInfo->SetArrayField(TEXT("bounds"), TArray<TSharedPtr<FJsonValue>>{FLOAT_TO_HOUDINI_JSON_VALUE(Box.Min.X), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Max.X),
					FLOAT_TO_HOUDINI_JSON_VALUE(Box.Min.Z), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Max.Z), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Min.Y), FLOAT_TO_HOUDINI_JSON_VALUE(Box.Max.Y)});
			}
		}
		
		TArray<TSharedPtr<FJsonValue>> JsonPositions;
		TArray<TSharedPtr<FJsonValue>> JsonRotations;
		TArray<TSharedPtr<FJsonValue>> JsonScales;
		for (const int32& CompIdx : AssetRefIndices.Value)
		{
			const FTransform& Transform = Transforms[CompIdx];
#if ENABLE_INSTANCEDSKINNEDMESH_INPUT
			if (const UInstancedSkinnedMeshComponent* ISMC = Cast<UInstancedSkinnedMeshComponent>(Components[CompIdx]))
			{
				for (const FSkinnedMeshInstanceData& Instance : ISMC->GetInstanceData())
					AppendJsonTransform(FTransform(Instance.Transform) * Transform, JsonPositions, JsonRotations, JsonScales);
			}
			else  // SkeletalMeshComponent
#endif
				AppendJsonTransform(Transform, JsonPositions, JsonRotations, JsonScales);
		}

		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_POSITION), JsonPositions);
		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_ROT), JsonRotations);
		JsonAssetInfo->SetArrayField(TEXT(HAPI_ATTRIB_SCALE), JsonScales);
	}
}


// FHoudiniSplineComponentInputBuilder
bool FHoudiniGeometryComponentInput::HapiDestroy(UHoudiniInput* Input) const
{
	if (NodeId >= 0)
	{
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), NodeId));
		Input->NotifyMergedNodeDestroyed();
	}

	Invalidate();

	return true;
}

void FHoudiniGeometryComponentInput::Invalidate() const
{
	if (Handle)
		FHoudiniEngineUtils::CloseSharedMemoryHandle(Handle);
}

bool FHoudiniSplineComponentInputBuilder::IsValidInput(const UActorComponent* Component)
{
	return Component->IsA<USplineComponent>();
}

FORCEINLINE static void CopyVectorData(float*& DataPtr, const FVector3f& Vector)
{
	*DataPtr = Vector.X;
	*(DataPtr + 1) = Vector.Z;
	*(DataPtr + 2) = Vector.Y;
	DataPtr += 3;
}

FORCEINLINE static void CopyQuaternionData(float*& DataPtr, const FQuat4f& Quat)
{
	*DataPtr = Quat.X;
	*(DataPtr + 1) = Quat.Z;
	*(DataPtr + 2) = Quat.Y;
	*(DataPtr + 3) = -Quat.W;
	DataPtr += 4;
}

bool FHoudiniSplineComponentInputBuilder::HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
	int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints)
{
	int32 NumPoints = 0;
	for (const int32& CompIdx : ComponentIndices)
	{
		const USplineComponent* CC = Cast<USplineComponent>(Components[CompIdx]);
		NumPoints += CC->GetSplinePointsPosition().Points.Num();
	}


	TSharedPtr<FHoudiniGeometryComponentInput> GeoComponentInput;
	if (InOutComponentInputs.IsValidIndex(0))
		GeoComponentInput = StaticCastSharedPtr<FHoudiniGeometryComponentInput>(InOutComponentInputs[0]);
	else
	{
		GeoComponentInput = MakeShared<FHoudiniGeometryComponentInput>();
		InOutComponentInputs.Add(GeoComponentInput);
	}

	const int32& NumVertices = NumPoints;
	const int32 NumPrims = ComponentIndices.Num();

	TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
	if (const AActor* Actor = Components[ComponentIndices[0]]->GetOwner())
	{
		if (IsValid(Actor) && Actor->GetClass()->IsInBlueprint())
		{
			TArray<const FProperty*> Properties;
			for (TFieldIterator<FProperty> PropIter(Actor->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropIter; ++PropIter)
			{
				const FProperty* Property = *PropIter;
				if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
				{
					if (ObjProp->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
						continue;
				}
				Properties.Add(*PropIter);
			}

			FHoudiniAttribute::RetrieveAttributes(Properties, TArray<const uint8*>{ (const uint8*)Actor }, HAPI_ATTROWNER_PRIM, PropAttribs);
		}
	}

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumPrims, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_SPLINE_POINT_ARRIVE_TANGENT, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 3, NumPoints * 3);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_SPLINE_POINT_LEAVE_TANGENT, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 3, NumPoints * 3);
	//GeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_SPLINE_POINT_TYPE, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Int, 1, NumPoints);

	for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
		PropAttrib->UploadInfo(SHMGeoInput);

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("splines_%08X"),
		(size_t)Components[ComponentIndices[0]]->GetOuter()), GeoComponentInput->Handle);

	float* PositionDataPtr = SHM;
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	float* ArriveTangentDataPtr = (float*)(VertexDataPtr + NumVertices + NumPrims);
	float* LeaveTangentDataPtr = ArriveTangentDataPtr + NumPoints * 3;
	//int32* SplineTypeDataPtr = (int32*)(LeaveTangentDataPtr + NumPoints * 3);
	int32 GlobalPointIdx = 0;
	for (const int32& CompIdx : ComponentIndices)
	{
		const USplineComponent* CC = Cast<USplineComponent>(Components[CompIdx]);
		const FTransform& Transform = Transforms[CompIdx];
		const TArray<FInterpCurvePointVector>& Points = CC->GetSplinePointsPosition().Points;
		for (const FInterpCurvePointVector& Point : Points)
		{
			CopyVectorData(PositionDataPtr, FVector3f(Transform.TransformPosition(Point.OutVal) * POSITION_SCALE_TO_HOUDINI));
			CopyVectorData(ArriveTangentDataPtr, FVector3f(Transform.TransformVector(Point.ArriveTangent) * POSITION_SCALE_TO_HOUDINI));
			CopyVectorData(LeaveTangentDataPtr, FVector3f(Transform.TransformVector(Point.LeaveTangent) * POSITION_SCALE_TO_HOUDINI));

			//*SplineTypeDataPtr = int32(Point.Type);
			//++SplineTypeDataPtr;

			*VertexDataPtr = GlobalPointIdx;
			++VertexDataPtr;
			++GlobalPointIdx;
		}

		*VertexDataPtr = CC->IsClosedLoop() ? HOUDINI_SHM_GEO_INPUT_POLY : HOUDINI_SHM_GEO_INPUT_POLYLINE;
		++VertexDataPtr;
	}

	float* AttribDataPtr = LeaveTangentDataPtr;  // LeaveTangentDataPtr has been offset to AttribDataPtr by CopyVectorData()
	for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
		PropAttrib->UploadData(AttribDataPtr);

	int32& NodeId = GeoComponentInput->NodeId;
	const bool bCreateNewNode = (NodeId < 0);
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(Input->GetGeoNodeId(),
			FString::Printf(TEXT("%s_splines_%08X"), *(Components[ComponentIndices[0]]->GetOuter()->GetName()), FPlatformTime::Cycles()), NodeId));

	HOUDINI_FAIL_RETURN(SHMGeoInput.HapiUpload(NodeId, SHM));
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(NodeId));

	return true;
}

void FHoudiniSplineComponentInputBuilder::AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
	const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject)
{
	// TODO:
}


bool FHoudiniBrushComponentInputBuilder::IsValidInput(const UActorComponent* Component)
{
	if (const UBrushComponent* BC = Cast<UBrushComponent>(Component))
	{
		if (!IsValid(BC->Brush))
			return false;

		if (!IsValid(BC->Brush->Polys))
			return false;

		if (IsValid(Cast<AVolume>(Component->GetOwner())))  // Only when owner is ABrush, then we should treat it as a BSP, otherwise maybe a AVolume or something else
			return false;

		return true;
	}
	
	return false;
}

bool FHoudiniBrushComponentInputBuilder::HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
	int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints)
{
	class FHoudiniPolyAccessor : public UObject  // See class UPolys
	{
	public:
		TArray<FPoly> Element;
	};

	TArray<float> Positions;
	TMap<FVector3f, int32> PositionIndexMap;
	int32 NumPrims = 0;
	TArray<int32> Vertices;
	for (const int32& CompIdx : ComponentIndices)
	{
		const UBrushComponent* BC = Cast<UBrushComponent>(Components[CompIdx]);
		const TArray<FPoly>& Polys = ((FHoudiniPolyAccessor*)BC->Brush->Polys.Get())->Element;
		const FTransform& Transform = Transforms[CompIdx];
		for (const FPoly& Poly : Polys)
		{
			for (const FVector3f& Position : Poly.Vertices)
			{
				if (const int32* PointIdxPtr = PositionIndexMap.Find(Position))
					Vertices.Add(*PointIdxPtr);
				else
				{
					const int32 PointIdx = PositionIndexMap.Num();
					const FVector3f FinalPosition = FVector3f(Transform.TransformPosition((FVector)Position)) * POSITION_SCALE_TO_HOUDINI_F;
					Positions.Add(FinalPosition.X);
					Positions.Add(FinalPosition.Z);
					Positions.Add(FinalPosition.Y);

					Vertices.Add(PointIdx);
					PositionIndexMap.Add(Position, PointIdx);
				}
			}
			Vertices.Add(-1);
		}
		NumPrims += Polys.Num();
	}
	const int32 NumPoints = Positions.Num() / 3;
	const int32 NumVertices = Vertices.Num() - NumPrims;

	TSharedPtr<FHoudiniGeometryComponentInput> GeoComponentInput;
	if (InOutComponentInputs.IsValidIndex(0))
		GeoComponentInput = StaticCastSharedPtr<FHoudiniGeometryComponentInput>(InOutComponentInputs[0]);
	else
	{
		GeoComponentInput = MakeShared<FHoudiniGeometryComponentInput>();
		InOutComponentInputs.Add(GeoComponentInput);
	}


	const UActorComponent* MainComp = Components[ComponentIndices[0]];
	EBrushType BrushType = Brush_Default;
	if (ABrush* Brush = Cast<ABrush>(MainComp->GetOwner()))
		BrushType = Brush->BrushType;


	FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumPrims, NumVertices);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_BRUSH_TYPE, EHoudiniAttributeOwner::Prim, EHoudiniInputAttributeStorage::Int, 1, 1, EHoudiniInputAttributeCompression::UniqueValue);

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"),
		(size_t)MainComp->GetOuter()), GeoComponentInput->Handle);

	float* PositionDataPtr = SHM;
	FMemory::Memcpy(PositionDataPtr, Positions.GetData(), Positions.Num() * sizeof(float));
	int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);
	FMemory::Memcpy(VertexDataPtr, Vertices.GetData(), Vertices.Num() * sizeof(int32));
	*(VertexDataPtr + Vertices.Num()) = (int32)BrushType;

	int32& NodeId = GeoComponentInput->NodeId;
	const bool bCreateNewNode = (NodeId < 0);
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(Input->GetGeoNodeId(),
			FString::Printf(TEXT("%s_BSP_%08X"), *(MainComp->GetOuter()->GetName()), FPlatformTime::Cycles()), NodeId));

	HOUDINI_FAIL_RETURN(SHMGeoInput.HapiUpload(NodeId, SHM));
	if (bCreateNewNode)
		HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(NodeId));

	return true;
}

void FHoudiniBrushComponentInputBuilder::AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
	const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject)
{
	// TODO:
}


bool FHoudiniDynamicMeshComponentInputBuilder::IsValidInput(const UActorComponent* Component)
{
	return Component->IsA<UDynamicMeshComponent>();
}

bool FHoudiniDynamicMeshComponentInputBuilder::HapiUpload(UHoudiniInput* Input, const bool& bIsSingleComponent,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, const TArray<int32>& ComponentIndices,
	int32& InOutInstancerNodeId, TArray<TSharedPtr<FHoudiniComponentInput>>& InOutComponentInputs, TArray<FHoudiniComponentInputPoint>& InOutPoints)
{
	TArray<TSharedPtr<FHoudiniComponentInput>> ComponentInputs;
	
	for (const int32& CompIdx : ComponentIndices)
	{
		const FDynamicMesh3* DM = Cast<UDynamicMeshComponent>(Components[CompIdx])->GetMesh();
		if (!DM)
			continue;

		const FTransform& Transform = Transforms[CompIdx];
		TSharedPtr<FHoudiniGeometryComponentInput> GeoComponentInput;
		if (InOutComponentInputs.IsValidIndex(0))
		{
			GeoComponentInput = StaticCastSharedPtr<FHoudiniGeometryComponentInput>(InOutComponentInputs[0]);
			InOutComponentInputs.RemoveAt(0);
		}
		else
			GeoComponentInput = MakeShared<FHoudiniGeometryComponentInput>();

		const int NumPoints = DM->VertexCount();
		const int NumPrims = DM->TriangleCount();
		const int NumVertices = NumPrims * 3;
		FHoudiniSharedMemoryGeometryInput SHMGeoInput(NumPoints, NumPrims, NumVertices);

		float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"),
			(size_t)Components[CompIdx]), GeoComponentInput->Handle);

		float* PositionDataPtr = SHM;
		int32* VertexDataPtr = (int32*)(PositionDataPtr + NumPoints * 3);

		for (int PointId = 0; PointId < NumPoints; ++PointId)
		{
			const FVector3f Position = FVector3f(Transform.TransformPosition(DM->GetVertexRef(PointId)) * POSITION_SCALE_TO_HOUDINI);
			*PositionDataPtr = Position.X;
			*(PositionDataPtr + 1) = Position.Z;
			*(PositionDataPtr + 2) = Position.Y;
			PositionDataPtr += 3;
		}

		for (int TriId = 0; TriId < NumPrims; ++TriId)
		{
			const UE::Geometry::FIndex3i Triangle = DM->GetTriangle(TriId);
			*VertexDataPtr = Triangle.A;
			*(VertexDataPtr + 1) = Triangle.B;
			*(VertexDataPtr + 2) = Triangle.C;
			*(VertexDataPtr + 3) = HOUDINI_SHM_GEO_INPUT_POLY;
			VertexDataPtr += 4;
		}

		int32& NodeId = GeoComponentInput->NodeId;
		const bool bCreateNewNode = (NodeId < 0);
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(Input->GetGeoNodeId(),
				FString::Printf(TEXT("%s_%08X"), *(Components[CompIdx]->GetName()), FPlatformTime::Cycles()), NodeId));

		HOUDINI_FAIL_RETURN(SHMGeoInput.HapiUpload(NodeId, SHM));
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(Input->HapiConnectToMergeNode(NodeId));

		ComponentInputs.Add(GeoComponentInput);
	}

	for (const TSharedPtr<FHoudiniComponentInput>& OldComponentInput : InOutComponentInputs)
		HOUDINI_FAIL_RETURN(OldComponentInput->HapiDestroy(Input));

	InOutComponentInputs = ComponentInputs;

	return true;
}

void FHoudiniDynamicMeshComponentInputBuilder::AppendInfo(const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms,
	const TArray<int32>& ComponentIndices, const TSharedPtr<FJsonObject>& JsonObject)
{
	// TODO:
}


// UHoudiniInputComponents
bool UHoudiniInputComponents::HapiDestroy()
{
	HOUDINI_FAIL_RETURN(HapiDestroyNodes());
	for (const auto& BuilderComponents : ComponentInputs)
	{
		for (const TSharedPtr<FHoudiniComponentInput>& ComponentInput : BuilderComponents.Value)
			HOUDINI_FAIL_RETURN(ComponentInput->HapiDestroy(GetInput()));
	}
	ComponentInputs.Empty();

	return true;
}

void UHoudiniInputComponents::Invalidate()
{
	if (InstanceHandle)
		FHoudiniEngineUtils::CloseSharedMemoryHandle(InstanceHandle);
	InstanceHandle = 0;

	InstancerNodeId = -1;
	SettingsNodeId = -1;
	bHasStandaloneInstancers = false;

	for (const auto& BuilderComponents : ComponentInputs)
	{
		for (const TSharedPtr<FHoudiniComponentInput>& ComponentInput : BuilderComponents.Value)
			ComponentInput->Invalidate();
	}
	ComponentInputs.Empty();
}

bool UHoudiniInputComponents::HapiDestroyNodes()
{
	bHasStandaloneInstancers = false;

	if (InstanceHandle)
		FHoudiniEngineUtils::CloseSharedMemoryHandle(InstanceHandle);
	InstanceHandle = 0;

	if ((InstancerNodeId >= 0) || (SettingsNodeId >= 0))
		GetInput()->NotifyMergedNodeDestroyed();

	if (InstancerNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), InstancerNodeId));
	InstancerNodeId = -1;

	if (SettingsNodeId >= 0)
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
	SettingsNodeId = -1;
	
	return true;
}

bool UHoudiniInputComponents::HapiUploadComponents(const UObject* Asset, const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms)
{
	const TArray<TSharedPtr<IHoudiniComponentInputBuilder>>& Builders = FHoudiniEngine::Get().GetComponentInputBuilders();
	TMap<TWeakPtr<IHoudiniComponentInputBuilder>, TArray<int32>> BuilderComponentIndicesMap;
	for (int32 CompIdx = 0; CompIdx < Components.Num(); ++CompIdx)
	{
		for (int32 BuilderIdx = Builders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
		{
			const TSharedPtr<IHoudiniComponentInputBuilder>& Builder = Builders[BuilderIdx];
			if (Builder->IsValidInput(Components[CompIdx]))
			{
				BuilderComponentIndicesMap.FindOrAdd(Builder).Add(CompIdx);
				break;
			}
		}
	}
	
	const bool bIsSingleValidComponent = BuilderComponentIndicesMap.Num() == 1 && BuilderComponentIndicesMap.begin()->Value.Num() == 1;  // We could reduce shm and node count when there is only one single component input

	const AActor* Actor = Cast<AActor>(Asset);

	if (BuilderComponentIndicesMap.IsEmpty())  // Just create a "null" reference node
	{
		if (InstancerNodeId >= 0 || !ComponentInputs.IsEmpty())  // Means previous has import something else, we need destroy them first
			HOUDINI_FAIL_RETURN(HapiDestroy());

		const bool bCreateNewNode = (SettingsNodeId < 0);
		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiCreateNode(GetGeoNodeId(),
				FString::Printf(TEXT("reference_%s_%08X"), *Asset->GetName(), FPlatformTime::Cycles()), SettingsNodeId));

		const bool bHasTransform = IsValid(Actor) && IsValid(Actor->GetRootComponent());
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiSetupBaseInfos(SettingsNodeId, bHasTransform ? FVector3f(Actor->GetActorLocation()) : FVector3f::ZeroVector));
		HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId, HAPI_ATTRIB_UNREAL_INSTANCE, FHoudiniEngineUtils::GetAssetReference(Asset)));
		if (bHasTransform)
		{
			const FTransform& ActorTransform = Actor->GetActorTransform();
			const FQuat Rotation = ActorTransform.GetRotation();
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddFloatAttribute(SettingsNodeId,
				HAPI_ATTRIB_ROT, 4, FVector4f(Rotation.X, Rotation.Z, Rotation.Y, -Rotation.W)));

			const FVector Scale = ActorTransform.GetScale3D();
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddFloatAttribute(SettingsNodeId,
				HAPI_ATTRIB_SCALE, 4, FVector4f(Scale.X, Scale.Z, Scale.Y, 0.0f)));
		}
		if (Actor)
		{
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId,
				HAPI_ATTRIB_UNREAL_ACTOR_PATH, FHoudiniEngineUtils::GetAssetReference(Asset)));
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId,
				HAPI_ATTRIB_UNREAL_ACTOR_OUTLINER_PATH, GetActorOutlinerPath(Actor)));
		}
		else
			HOUDINI_FAIL_RETURN(FHoudiniSopNull::HapiAddStringAttribute(SettingsNodeId,
				HAPI_ATTRIB_UNREAL_OBJECT_PATH, FHoudiniEngineUtils::GetAssetReference(Asset)));

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::CommitGeo(FHoudiniEngine::Get().GetSession(), SettingsNodeId));

		if (bCreateNewNode)
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));

		return true;
	}

	const bool bPreviousHasInstancerNode = InstancerNodeId >= 0;  // As FHoudiniComponentInputBuilder::HapiUpload may create a new instancer node

	// For each type to upload component data
	TSet<TWeakPtr<IHoudiniComponentInputBuilder>> FoundBuilders;
	TArray<FHoudiniComponentInputPoint> Points;
	for (const auto& BuilderComponents : BuilderComponentIndicesMap)
	{
		if (TArray<TSharedPtr<FHoudiniComponentInput>>* FoundTypedComponentInputsPtr = ComponentInputs.Find(BuilderComponents.Key))
		{
			TArray<TSharedPtr<FHoudiniComponentInput>>& TypedComponentInputs = *FoundTypedComponentInputsPtr;
			HOUDINI_FAIL_RETURN(BuilderComponents.Key.Pin()->HapiUpload(GetInput(), bIsSingleValidComponent,
				Components, Transforms, BuilderComponents.Value, InstancerNodeId, TypedComponentInputs, Points));
			if (!TypedComponentInputs.IsEmpty())
				FoundBuilders.Add(BuilderComponents.Key);
		}
		else
		{
			TArray<TSharedPtr<FHoudiniComponentInput>> TypedComponentInputs;
			HOUDINI_FAIL_RETURN(BuilderComponents.Key.Pin()->HapiUpload(GetInput(), bIsSingleValidComponent,
				Components, Transforms, BuilderComponents.Value, InstancerNodeId, TypedComponentInputs, Points));
			if (!TypedComponentInputs.IsEmpty())
			{
				FoundBuilders.Add(BuilderComponents.Key);
				ComponentInputs.Add(BuilderComponents.Key, TypedComponentInputs);
			}
		}
	}
	
	// Destroy and clear old holders
	for (TMap<TWeakPtr<IHoudiniComponentInputBuilder>, TArray<TSharedPtr<FHoudiniComponentInput>>>::TIterator
		BuilderIter(ComponentInputs); BuilderIter; ++BuilderIter)
	{
		if (!BuilderIter->Key.IsValid() || !FoundBuilders.Contains(BuilderIter->Key))
		{
			for (const TSharedPtr<FHoudiniComponentInput>& ComponentInput : BuilderIter->Value)
				HOUDINI_FAIL_RETURN(ComponentInput->HapiDestroy(GetInput()));
			BuilderIter.RemoveCurrent();
		}
	}

	if (Points.IsEmpty())  // If there is no points, then we should NOT have a instancer node and blast node
		return HapiDestroyNodes();  // Finished

	const size_t NumPoints = Points.Num();
	TArray<FInt32Interval> InstRanges;  // Should never be empty, but we still need consider empty situation
	size_t NumInstRefChars = 0;
	size_t NumMetaDataChars = 0;
	{
		bool bPrevHasBeenCopied = true;
		for (int32 PointIdx = 0; PointIdx < Points.Num(); ++PointIdx)
		{
			const FHoudiniComponentInputPoint& Point = Points[PointIdx];
			NumInstRefChars += Point.AssetRef.length() + 1;
			NumMetaDataChars += Point.MetaData.length() + 1;
			if (!Point.bHasBeenCopied && bPrevHasBeenCopied)
				InstRanges.Add(FInt32Interval(PointIdx, PointIdx));
			else if (Point.bHasBeenCopied && !bPrevHasBeenCopied)
				InstRanges.Last().Max = PointIdx - 1;
			else if (!Point.bHasBeenCopied && PointIdx == Points.Num() - 1)  // the last one
				InstRanges.Last().Max = PointIdx;
			bPrevHasBeenCopied = Point.bHasBeenCopied;
		}
	}
	
	if (InstRanges.IsEmpty())  // Means InstancerNode should NOT connect to merge directly or indirectly
	{
		if (SettingsNodeId >= 0)
		{
			if (bPreviousHasInstancerNode)
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DisconnectNodeInput(FHoudiniEngine::Get().GetSession(), SettingsNodeId, 0));
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
			GetInput()->NotifyMergedNodeDestroyed();
		}
		else if (bHasStandaloneInstancers)  // If previous has standalone instancers, then means InstancerNode is directly connect to merge node, then we should disconnect it
			HOUDINI_FAIL_RETURN(GetInput()->HapiDisconnectFromMergeNode(InstancerNodeId));

		bHasStandaloneInstancers = false;
	}
	else
	{
		// Create Instancer Node
		const bool bHasCopiedPoint = (InstRanges.Num() != 1) || (InstRanges[0].Min != 0) || (InstRanges[0].Max != (Points.Num() - 1));
		bool bShouldCreateAndConnectBlastNode = false;
		if (InstancerNodeId < 0)
		{
			if (SettingsNodeId >= 0)  // Means this is a "null" node, but we may need "blast" node here
			{
				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
				SettingsNodeId = -1;
				GetInput()->NotifyMergedNodeDestroyed();  // "null" node was connected to "merge" node
			}

			HOUDINI_FAIL_RETURN(FHoudiniSharedMemoryGeometryInput::HapiCreateNode(GetInput()->GetGeoNodeId(),
				FString::Printf(TEXT("references_%s_%08X"), *Asset->GetName(), FPlatformTime::Cycles()), InstancerNodeId));

			if (bHasCopiedPoint)
				bShouldCreateAndConnectBlastNode = true;
			else
				HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(InstancerNodeId));  // Directly connect to InstancerNode "merge" node
		}
		else
		{
			if (bHasCopiedPoint && SettingsNodeId < 0)  // We need create a "blast" node to split uncopied points
			{
				if (bPreviousHasInstancerNode)  // Means previously InstancerNode was connected to merge node directly
					HOUDINI_FAIL_RETURN(GetInput()->HapiDisconnectFromMergeNode(InstancerNodeId));
				bShouldCreateAndConnectBlastNode = true;
			}
			else if (!bHasCopiedPoint)
			{
				if (SettingsNodeId >= 0)  // Just delete "blast" node, As SettingsNode existing, means previously InstancerNode is connected
				{
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::DeleteNode(FHoudiniEngine::Get().GetSession(), SettingsNodeId));
					SettingsNodeId = -1;
				}
				else if (!bHasStandaloneInstancers)
					HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(InstancerNodeId));
			}
		}

		if (bShouldCreateAndConnectBlastNode)
		{
			HOUDINI_FAIL_RETURN(FHoudiniSopBlast::HapiCreateNode(GetInput()->GetGeoNodeId(), FString(), SettingsNodeId));
			HOUDINI_FAIL_RETURN(FHoudiniSopBlast::HapiConnectInput(SettingsNodeId, InstancerNodeId));
			HOUDINI_FAIL_RETURN(GetInput()->HapiConnectToMergeNode(SettingsNodeId));
		}

		if (SettingsNodeId >= 0)  // We should set which points have NOT been copied
		{
			FString GroupRangesStr;
			for (const FInt32Interval Range : InstRanges)
				GroupRangesStr += (Range.Min == Range.Max) ?
					(FString::FromInt(Range.Min) + TEXT(" ")) : FString::Printf(TEXT("%d-%d "), Range.Min, Range.Max);
			GroupRangesStr.RemoveFromEnd(TEXT(" "));
			HOUDINI_FAIL_RETURN(FHoudiniSopBlast::HapiSetGroup(SettingsNodeId, GroupRangesStr, EHoudiniBlastGroupType::Points, true));
		}

		bHasStandaloneInstancers = true;
	}
	// Set SHM data
	const size_t InstRefsLength32 = NumInstRefChars / 4 + 1;
	const size_t MetaDataLength32 = NumMetaDataChars / 4 + 1;

	const std::string AssetRef = TCHAR_TO_UTF8(*FHoudiniEngineUtils::GetAssetReference(Asset));
	const size_t AssetRefLength32 = (AssetRef.length() + 1) / 4 + 1;  // just upload this unique value

	FHoudiniSharedMemoryGeometryInput SHMGeoInput(Points.Num(), 0, 0);
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_ROT, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 4, NumPoints * 4);  // p@rot
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_SCALE, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Float, 3, NumPoints * 3);  // v@scale
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_INSTANCE, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, NumInstRefChars / 4 + 1);  // s@unreal_instance
	SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_OBJECT_METADATA,  // s@unreal_object_metadata
		EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::Dict, 1, MetaDataLength32);
	SHMGeoInput.AppendAttribute(IsValid(Actor) ? HAPI_ATTRIB_UNREAL_ACTOR_PATH : HAPI_ATTRIB_UNREAL_OBJECT_PATH,  // s@unreal_object_path/s@unreal_actor_path
		EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, AssetRefLength32, EHoudiniInputAttributeCompression::UniqueValue);
	
	std::string ActorOutlinerPath;
	if (Actor)
	{
		ActorOutlinerPath = TCHAR_TO_UTF8(*GetActorOutlinerPath(Actor));
		SHMGeoInput.AppendAttribute(HAPI_ATTRIB_UNREAL_ACTOR_OUTLINER_PATH, EHoudiniAttributeOwner::Point, EHoudiniInputAttributeStorage::String, 1, (ActorOutlinerPath.length() + 1) / 4 + 1, EHoudiniInputAttributeCompression::UniqueValue);
	}

	float* const SHM = SHMGeoInput.GetSharedMemory(FString::Printf(TEXT("%08X"), (size_t)Asset), InstanceHandle);
	
	float* PositionDataPtr = SHM;
	float* RotDataPtr = PositionDataPtr + NumPoints * 3;
	float* ScaleDataPtr = RotDataPtr + NumPoints * 4;
	char* InstRefDataPtr = (char*)(ScaleDataPtr + NumPoints * 3);
	char* MetaDataPtr = (char*)(((float*)InstRefDataPtr) + InstRefsLength32);
	char* AssetRefDataPtr = (char*)(((float*)MetaDataPtr) + MetaDataLength32);

	for (const FHoudiniComponentInputPoint& Point : Points)
	{
		CopyVectorData(PositionDataPtr, FVector3f(Point.Transform.GetLocation()) * POSITION_SCALE_TO_HOUDINI_F);
		CopyQuaternionData(RotDataPtr, FQuat4f(Point.Transform.GetRotation()));
		CopyVectorData(ScaleDataPtr, FVector3f(Point.Transform.GetScale3D()));

		FMemory::Memcpy(InstRefDataPtr, Point.AssetRef.c_str(), Point.AssetRef.length());
		InstRefDataPtr += Point.AssetRef.length() + 1;
		FMemory::Memcpy(MetaDataPtr, Point.MetaData.c_str(), Point.MetaData.length());
		MetaDataPtr += Point.MetaData.length() + 1;
	}

	// s@unreal_object_path/s@unreal_actor_path
	FMemory::Memcpy(AssetRefDataPtr, AssetRef.c_str(), AssetRef.length());
	if (Actor)
	{
		AssetRefDataPtr += AssetRefLength32 * sizeof(int32);
		FMemory::Memcpy(AssetRefDataPtr, ActorOutlinerPath.c_str(), ActorOutlinerPath.length());
	}

	return SHMGeoInput.HapiUpload(InstancerNodeId, SHM);
}

void UHoudiniInputComponents::GetComponentsInfo(const AActor* Actor,
	const TArray<const UActorComponent*>& Components, const TArray<FTransform>& Transforms, FString& OutInfoStr)
{
	const TArray<TSharedPtr<IHoudiniComponentInputBuilder>>& Builders = FHoudiniEngine::Get().GetComponentInputBuilders();
	TMap<TWeakPtr<IHoudiniComponentInputBuilder>, TArray<int32>> BuilderComponentIndicesMap;
	for (int32 CompIdx = 0; CompIdx < Components.Num(); ++CompIdx)
	{
		for (int32 BuilderIdx = Builders.Num() - 1; BuilderIdx >= 0; --BuilderIdx)
		{
			const TSharedPtr<IHoudiniComponentInputBuilder>& Builder = Builders[BuilderIdx];
			if (Builder->IsValidInput(Components[CompIdx]))
			{
				BuilderComponentIndicesMap.FindOrAdd(Builder).Add(CompIdx);
				break;
			}
		}
	}

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	for (const auto& BuilderComponents : BuilderComponentIndicesMap)
		BuilderComponents.Key.Pin()->AppendInfo(Components, Transforms, BuilderComponents.Value, JsonObject);
	
	FJsonDataBag DataBag;
	DataBag.JsonObject = JsonObject;

	OutInfoStr = DataBag.ToJson(false);
}
