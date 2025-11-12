// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniOutputs.h"
#include "HoudiniOutputUtils.h"

#include "Components/SplineComponent.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniNode.h"
#include "HoudiniAttribute.h"
#include "HoudiniCurvesComponent.h"


bool FHoudiniCurveOutputBuilder::HapiIsPartValid(const int32& NodeId, const HAPI_PartInfo& PartInfo, bool& bOutIsValid, bool& bOutShouldHoldByOutput)
{
	bOutIsValid = PartInfo.type == HAPI_PARTTYPE_CURVE;
	bOutShouldHoldByOutput = true;

	return true;
}


UHoudiniCurvesComponent* FHoudiniCurvesOutput::Find(const AHoudiniNode* Node) const
{
	return Find_Internal<true>(Component, Node);
}

UHoudiniCurvesComponent* FHoudiniCurvesOutput::CreateOrUpdate(AHoudiniNode* Node, const FString& InSplitValue, const bool& bInSplitActor)
{
	return CreateOrUpdate_Internal<true>(Component, Node, InSplitValue, bInSplitActor);
}

void FHoudiniCurvesOutput::Destroy(const AHoudiniNode* Node) const
{
	DestroyComponent(Node, Component, true);
	Component.Reset();
}


static bool HapiGetColors(TArray<FColor>& OutColors, const int32& NodeId, const int32& PartId, const HAPI_AttributeOwner& Owner)
{
	HAPI_AttributeInfo AttribInfo;

	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		HAPI_ATTRIB_COLOR, Owner, &AttribInfo));

	if (AttribInfo.exists && (AttribInfo.tupleSize >= 3) &&
		((AttribInfo.storage == HAPI_STORAGETYPE_FLOAT) || (AttribInfo.storage == HAPI_STORAGETYPE_FLOAT64)))
	{
		AttribInfo.tupleSize = 3;
		TArray<float> ColorData;
		ColorData.SetNumUninitialized(AttribInfo.count * AttribInfo.tupleSize);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_COLOR, &AttribInfo, -1, ColorData.GetData(), 0, AttribInfo.count));
		
		OutColors.SetNumUninitialized(AttribInfo.count);
		for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
		{
			FColor& Color = OutColors[ElemIdx];
			Color.R = FMath::RoundToInt(ColorData[ElemIdx * 3] * 255.0f);
			Color.G = FMath::RoundToInt(ColorData[ElemIdx * 3 + 1] * 255.0f);
			Color.B = FMath::RoundToInt(ColorData[ElemIdx * 3 + 2] * 255.0f);
			Color.A = 255;
		}
	}
	
	HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
		HAPI_ALPHA, Owner, &AttribInfo));

	if (AttribInfo.exists && (AttribInfo.tupleSize >= 1) &&
		((AttribInfo.storage == HAPI_STORAGETYPE_FLOAT) || (AttribInfo.storage == HAPI_STORAGETYPE_FLOAT64)))
	{
		AttribInfo.tupleSize = 1;
		TArray<float> AlphaData;
		AlphaData.SetNumUninitialized(AttribInfo.count * AttribInfo.tupleSize);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ALPHA, &AttribInfo, -1, AlphaData.GetData(), 0, AttribInfo.count));

		if (OutColors.IsEmpty())
		{
			OutColors.SetNumUninitialized(AttribInfo.count);
			for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
			{
				FColor& Color = OutColors[ElemIdx];
				Color = FColor::White;
				Color.A = FMath::RoundToInt(AlphaData[ElemIdx] * 255.0f);
			}
		}
		else
		{
			for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
				OutColors[ElemIdx].A = FMath::RoundToInt(AlphaData[ElemIdx] * 255.0f);
		}
	}

	return true;
}


bool UHoudiniOutputCurve::HapiUpdate(const HAPI_GeoInfo& GeoInfo, const TArray<HAPI_PartInfo>& PartInfos)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HoudiniOutputCurve);

	struct FHoudiniCurveIndicesHolder
	{
		FHoudiniCurveIndicesHolder(const int8& UpdateMode, const FString& InSplitValue) :
			PartialOutputMode(UpdateMode), SplitValue(InSplitValue) {}

		int8 PartialOutputMode = 0;
		FString SplitValue;

		TMap<UObject*, TArray<int32>> ClassSplinesMap;  // <Class, PrimIndices>  For unreal SplineComponent
		TMap<int32, TArray<int32>> GroupCurvesMap;  // <GroupIdx, PrimIndices>  For HoudiniCurvesComponent
	};

	struct FHoudiniCurvesPart
	{
		FHoudiniCurvesPart(const HAPI_PartInfo& PartInfo) : Info(PartInfo) {}

		HAPI_PartInfo Info;
		TArray<std::string> AttribNames;
		TArray<int32> VertexIndices;  // Accumulate append CurveCounts
		TArray<FString> EditableGroupNames;
		TMap<int32, FHoudiniCurveIndicesHolder> SplitCurvesMap;

		FString GetGroupName(const int32& GroupIdx) const
		{
			return (GroupIdx < 0) ? TEXT(HAPI_GROUP_MAIN_GEO) : EditableGroupNames[GroupIdx];
		}
	};


	const int32& NodeId = GeoInfo.nodeId;
	AHoudiniNode* Node = GetNode();

	// -------- Retrieve all part data --------
	TArray<FHoudiniCurvesPart> Parts;
	for (const HAPI_PartInfo& PartInfo : PartInfos)
		Parts.Add(FHoudiniCurvesPart(PartInfo));

	const bool& bCanHaveEditableOutput = CanHaveEditableOutput();
	bool bPartialUpdate = false;

	bool bIsMainGeoEditable = false;  // If true, means the GroupIdx == -1 is also editable
	for (FHoudiniCurvesPart& Part : Parts)
	{
		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;


		// -------- Retrieve attrib and group names --------
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetAttributeNames(NodeId, PartId, PartInfo.attributeCounts, Part.AttribNames));
		const TArray<std::string>& AttribNames = Part.AttribNames;

		
		TArray<TArray<int32>> EditableGroupships;
		if (bCanHaveEditableOutput)  // PrimGroup is only used for find editable group, so if output cannot edit, then we need to Retrieve PrimGroupNames
		{
			TArray<std::string> PrimGroupNames;
			HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetPrimitiveGroupNames(GeoInfo, PartInfo, PrimGroupNames));

			HOUDINI_FAIL_RETURN(HapiGetEditableGroups(Part.EditableGroupNames, EditableGroupships, bIsMainGeoEditable,
				NodeId, PartInfo, PrimGroupNames));
		}


		// -------- Retrieve split values and partial output modes if exists --------
		TArray<int32> SplitKeys;  // Maybe int or HAPI_StringHandle
		HAPI_AttributeOwner SplitAttribOwner = HAPI_ATTROWNER_PRIM;
		TMap<HAPI_StringHandle, FString> SplitValueMap;
		HOUDINI_FAIL_RETURN(FHoudiniOutputUtils::HapiGetSplitValues(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			SplitKeys, SplitValueMap, SplitAttribOwner));
		const bool bHasSplitValues = !SplitKeys.IsEmpty();

		HAPI_AttributeOwner PartialOutputModeOwner = bHasSplitValues ? FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
			PartInfo.attributeCounts, HAPI_ATTRIB_PARTIAL_OUTPUT_MODE) : HAPI_ATTROWNER_INVALID;
		TArray<int8> PartialOutputModes;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
			HAPI_ATTRIB_PARTIAL_OUTPUT_MODE, PartialOutputModes, PartialOutputModeOwner));


		// -------- Retrieve vertex list --------
		TArray<int32> CurveCounts;
		CurveCounts.SetNumUninitialized(PartInfo.faceCount);
		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetCurveCounts(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			CurveCounts.GetData(), 0, PartInfo.faceCount));
		
		HAPI_AttributeOwner SplineClassOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames,
			PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_OUTPUT_SPLINE_CLASS);
		TArray<UObject*> SplineClasses;
		auto HapiRetreiveSplineClassesLambda = [&]() -> bool
			{
				if (SplineClasses.IsEmpty() && (SplineClassOwner != HAPI_ATTROWNER_INVALID))
				{
					HAPI_AttributeInfo AttribInfo;
					HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
						HAPI_ATTRIB_UNREAL_OUTPUT_SPLINE_CLASS, SplineClassOwner, &AttribInfo));
					
					if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
					{
						TArray<HAPI_StringHandle> ClassSHs;
						ClassSHs.SetNumUninitialized(AttribInfo.count);
						HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
							HAPI_ATTRIB_UNREAL_OUTPUT_SPLINE_CLASS, &AttribInfo, ClassSHs.GetData(), 0, AttribInfo.count));

						TMap<HAPI_StringHandle, UObject*> SHInstanceMap;
						const TArray<HAPI_StringHandle> UniqueInstanceRefSHs = TSet<HAPI_StringHandle>(ClassSHs).Array();
						TArray<FString> UniqueInstanceRefs;
						HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertUniqueStringHandles(UniqueInstanceRefSHs, UniqueInstanceRefs));
						for (int32 UniqueIdx = 0; UniqueIdx < UniqueInstanceRefSHs.Num(); ++UniqueIdx)
						{
							UObject* Instance = nullptr;
							FString& InstanceAssetRef = UniqueInstanceRefs[UniqueIdx];
							if (!InstanceAssetRef.IsEmpty())  // Do NOT use "IS_ASSET_PATH_INVALID", as strings like "WaterBodyLake", "CineSplineComponent" also can be instantiated
							{
								if (InstanceAssetRef.StartsWith(TEXT("HoudiniCurve")))  // Output as HoudiniCurvesComponent
									Instance = UHoudiniCurvesComponent::StaticClass();
								else
								{
									int32 SplitIdx;
									if (InstanceAssetRef.FindChar(TCHAR(';'), SplitIdx))  // UHoudiniParameterAsset support import asset ref with info, and append by ';', so we need to support it here
										InstanceAssetRef.LeftInline(SplitIdx);

									Instance = LoadObject<UObject>(nullptr, *InstanceAssetRef, nullptr, LOAD_Quiet | LOAD_NoWarn);
									if (!Instance)  // Maybe is the class name, like "WaterBodyLake", "CineSplineComponent"
										Instance = FindFirstObject<UClass>(*InstanceAssetRef, EFindFirstObjectOptions::NativeFirst);
								}
							}

							if (Instance)
							{
								if (UClass* SplineClass = Cast<UClass>(Instance))
								{
									if ((SplineClass != UHoudiniCurvesComponent::StaticClass()) &&
										SplineClass->IsChildOf(UActorComponent::StaticClass()) && !SplineClass->IsChildOf(USplineComponent::StaticClass()))
										Instance = USplineComponent::StaticClass();
								}
							}
							else
								Instance = USplineComponent::StaticClass();

							SHInstanceMap.Add(UniqueInstanceRefSHs[UniqueIdx], Instance);
						}

						SplineClasses.SetNumUninitialized(AttribInfo.count);
						for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
							SplineClasses[ElemIdx] = SHInstanceMap[ClassSHs[ElemIdx]];
					}
				}

				return true;
			};
		
		// -------- Split curves --------
		TMap<int32, FHoudiniCurveIndicesHolder>& SplitMap = Part.SplitCurvesMap;
		FHoudiniCurveIndicesHolder AllCurves(HAPI_PARTIAL_OUTPUT_MODE_REPLACE, FString());
		int32 VertexIdx = 0;  // The first vertex/point index of this curve, will accumulate each CurveCount
		for (int32 CurveIdx = 0; CurveIdx < PartInfo.faceCount; ++CurveIdx)
		{
			// Same as HoudiniOutputMesh
			// Judge PartialOutputMode, if remove && previous NOT set, then we will NOT parse the GroupIdx
			const int32 SplitKey = bHasSplitValues ?
				SplitKeys[FHoudiniOutputUtils::CurveAttributeEntryIdx(SplitAttribOwner, VertexIdx, CurveIdx)] : 0;
			FHoudiniCurveIndicesHolder* FoundHolderPtr = bHasSplitValues ? SplitMap.Find(SplitKey) : nullptr;

			const int8 PartialOutputMode = FMath::Clamp(PartialOutputModes.IsEmpty() ? HAPI_PARTIAL_OUTPUT_MODE_REPLACE :
				PartialOutputModes[FHoudiniOutputUtils::CurveAttributeEntryIdx(PartialOutputModeOwner, VertexIdx, CurveIdx)],
				HAPI_PARTIAL_OUTPUT_MODE_REPLACE, HAPI_PARTIAL_OUTPUT_MODE_REMOVE);

			if (PartialOutputMode == HAPI_PARTIAL_OUTPUT_MODE_MODIFY)
				bPartialUpdate = true;
			else if (PartialOutputMode == HAPI_PARTIAL_OUTPUT_MODE_REMOVE)  // If has PartialOutputModes, then must also HasSplitValues
			{
				bPartialUpdate = true;
				if (FoundHolderPtr)  // If previous triangles has identified PartialOutputMode, We should NOT change it
				{
					if (FoundHolderPtr->PartialOutputMode == HAPI_PARTIAL_OUTPUT_MODE_REMOVE)
						continue;
				}
				else
				{
					SplitMap.Add(SplitKey, FHoudiniCurveIndicesHolder(HAPI_PARTIAL_OUTPUT_MODE_REMOVE, GET_SPLIT_VALUE_STR));
					continue;
				}
			}

			int32 EditIdx = -1;  // -1 represents "main_geo"
			if (bCanHaveEditableOutput)
			{
				for (int32 EditableGroupIdx = 0; EditableGroupIdx < EditableGroupships.Num(); ++EditableGroupIdx)
				{
					if (EditableGroupships[EditableGroupIdx][CurveIdx])
					{
						EditIdx = EditableGroupIdx;
						break;
					}
				}
			}
			
			UObject* SplineClass = nullptr;
			if ((EditIdx < 0) && !bIsMainGeoEditable)
			{
				if ((SplineClassOwner != HAPI_ATTROWNER_INVALID) && SplineClasses.IsEmpty())
					HapiRetreiveSplineClassesLambda();
				SplineClass = SplineClasses.IsEmpty() ? USplineComponent::StaticClass() :
					SplineClasses[FHoudiniOutputUtils::CurveAttributeEntryIdx(SplineClassOwner, VertexIdx, CurveIdx)];
				if (IsValid(SplineClass))
				{
					if (SplineClass == UHoudiniCurvesComponent::StaticClass())
						SplineClass = nullptr;  // Will output as UHoudiniCurvesComponent
				}
				else
					SplineClass = USplineComponent::StaticClass();
			}

			if (bHasSplitValues)
			{
				if (!FoundHolderPtr)
					FoundHolderPtr = &SplitMap.Add(SplitKey, FHoudiniCurveIndicesHolder(PartialOutputMode, GET_SPLIT_VALUE_STR));
				
				if (SplineClass)
					FoundHolderPtr->ClassSplinesMap.FindOrAdd(SplineClass).Add(CurveIdx);
				else
					FoundHolderPtr->GroupCurvesMap.FindOrAdd(EditIdx).Add(CurveIdx);
			}
			else
			{
				if (SplineClass)
					AllCurves.ClassSplinesMap.FindOrAdd(SplineClass).Add(CurveIdx);
				else
					AllCurves.GroupCurvesMap.FindOrAdd(EditIdx).Add(CurveIdx);
			}

			VertexIdx += CurveCounts[CurveIdx];
			Part.VertexIndices.Add(VertexIdx);
		}

		if (!bHasSplitValues)
			SplitMap.Add(0, AllCurves);  // We just add AllLodTrianglesMap to SplitMeshMap
	}

	// Copy from HoudiniOutputMesh
	TDoubleLinkedList<FHoudiniInstancedComponentOutput*> OldSplineComponentOutputs;
	TArray<FHoudiniInstancedComponentOutput> NewSplineComponentOutputs;
	TDoubleLinkedList<FHoudiniInstancedActorOutput*> OldSplineActorOutputs;
	TArray<FHoudiniInstancedActorOutput> NewSplineActorOutputs;
	TDoubleLinkedList<FHoudiniCurvesOutput*> OldHoudiniCurvesOutputs;
	TArray<FHoudiniCurvesOutput> NewHoudiniCurvesOutputs;

	if (bPartialUpdate)
	{
		TSet<FString> ModifySplitValues;
		TSet<FString> RemoveSplitValues;
		for (FHoudiniCurvesPart& Part : Parts)
		{
			for (TMap<int32, FHoudiniCurveIndicesHolder>::TIterator SplitIter(Part.SplitCurvesMap); SplitIter; ++SplitIter)
			{
				if (SplitIter->Value.PartialOutputMode >= HAPI_PARTIAL_OUTPUT_MODE_REMOVE)
				{
					RemoveSplitValues.FindOrAdd(SplitIter->Value.SplitValue);
					SplitIter.RemoveCurrent();
				}
				else
					ModifySplitValues.FindOrAdd(SplitIter->Value.SplitValue);
			}
		}

		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, SplineComponentOutputs,
			[Node](FHoudiniInstancedComponentOutput& Holder) { return Holder.HasValidAndCleanup(Node); }, OldSplineComponentOutputs, NewSplineComponentOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, SplineActorOutputs,
			[](FHoudiniInstancedActorOutput& Holder) { return Holder.HasValidAndCleanup(); }, OldSplineActorOutputs, NewSplineActorOutputs);
		FHoudiniOutputUtils::UpdateSplittableOutputHolders(ModifySplitValues, RemoveSplitValues, HoudiniCurvesOutputs,
			[Node](const FHoudiniCurvesOutput& OldHCOutput) { return IsValid(OldHCOutput.Find(Node)); }, OldHoudiniCurvesOutputs, NewHoudiniCurvesOutputs);
	}
	else  // Collect valid old output holders for reuse
	{
		FHoudiniOutputUtils::UpdateOutputHolders(SplineComponentOutputs,
			[Node](FHoudiniInstancedComponentOutput& Holder) { return Holder.HasValidAndCleanup(Node); }, OldSplineComponentOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(SplineActorOutputs,
			[](FHoudiniInstancedActorOutput& Holder) { return Holder.HasValidAndCleanup(); }, OldSplineActorOutputs);
		FHoudiniOutputUtils::UpdateOutputHolders(HoudiniCurvesOutputs,
			[Node](const FHoudiniCurvesOutput& OldHCOutput) { return IsValid(OldHCOutput.Find(Node)); }, OldHoudiniCurvesOutputs);
	}


	TMap<UObject*, AActor*> InstanceActorMap;
	TMap<AActor*, TArray<FString>> ActorPropertyNamesMap;  // Use to avoid Set the same property in same SplitActor twice
	HAPI_AttributeInfo AttribInfo;
	for (const FHoudiniCurvesPart& Part : Parts)
	{
		if (Part.SplitCurvesMap.IsEmpty())
			continue;

		const HAPI_PartInfo& PartInfo = Part.Info;
		const HAPI_PartId& PartId = PartInfo.id;
		const TArray<std::string>& AttribNames = Part.AttribNames;


		// -------- curve_point_id, for fuse points --------
		TArray<int32> PointIds;
		HOUDINI_FAIL_RETURN(FHoudiniOutputUtils::HapiGetCurvePointIds(NodeId, PartId, AttribNames, PartInfo.attributeCounts, PointIds));

		// -------- Transforms --------
		TArray<float> PositionData;
		PositionData.SetNumUninitialized(PartInfo.pointCount * 3);

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_POSITION, HAPI_ATTROWNER_POINT, &AttribInfo));

		HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
			HAPI_ATTRIB_POSITION, &AttribInfo, -1, PositionData.GetData(), 0, PartInfo.pointCount));

		HAPI_AttributeOwner RotOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_ROT);
		TArray<FQuat> Rots;
		if (RotOwner != HAPI_ATTROWNER_INVALID)
		{
			HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
				HAPI_ATTRIB_ROT, RotOwner, &AttribInfo));

			if (((AttribInfo.storage == HAPI_STORAGETYPE_FLOAT) || (AttribInfo.storage == HAPI_STORAGETYPE_FLOAT64)) &&
				((AttribInfo.tupleSize == 3) || (AttribInfo.tupleSize == 4)))
			{
				TArray<float> RotData;
				RotData.SetNumUninitialized(AttribInfo.count * AttribInfo.tupleSize);

				HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
					HAPI_ATTRIB_ROT, &AttribInfo, -1, RotData.GetData(), 0, AttribInfo.count));

				Rots.SetNumUninitialized(AttribInfo.count);
				if (AttribInfo.tupleSize == 4)
				{
					for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
						Rots[ElemIdx] = FQuat(RotData[ElemIdx * 4], RotData[ElemIdx * 4 + 2], RotData[ElemIdx * 4 + 1], -RotData[ElemIdx * 4 + 3]);
				}
				else
				{
					for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
						Rots[ElemIdx] = FRotator(FMath::RadiansToDegrees(RotData[ElemIdx * 3]), FMath::RadiansToDegrees(RotData[ElemIdx * 3 + 2]), FMath::RadiansToDegrees(RotData[ElemIdx * 3 + 1])).Quaternion();
				}
			}
			else
				RotOwner = HAPI_ATTROWNER_INVALID;
		}

		HAPI_AttributeOwner ScaleOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_SCALE);
		TArray<float> ScaleData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_SCALE, 3, ScaleData, ScaleOwner));

		// -------- Curve Intrinsic --------
		HAPI_AttributeOwner CurveClosedOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_CURVE_CLOSED);
		TArray<int8> CurveClosedData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId, HAPI_CURVE_CLOSED, CurveClosedData, CurveClosedOwner));

		HAPI_AttributeOwner CurveTypeOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_CURVE_TYPE);
		TArray<int8> CurveTypeData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId, HAPI_CURVE_TYPE, [](const FUtf8StringView& AttribValue) -> int8
			{
				if (AttribValue.StartsWith("point"))
					return int8(EHoudiniCurveType::Points);
				else if (AttribValue.StartsWith("subd") || AttribValue.StartsWith("nurb"))
					return int8(EHoudiniCurveType::Subdiv);
				else if (AttribValue.StartsWith("bez"))
					return int8(EHoudiniCurveType::Bezier);
				else if (AttribValue.StartsWith("interp"))
					return int8(EHoudiniCurveType::Interpolate);

				return int8(EHoudiniCurveType::Polygon);
			}, CurveTypeData, CurveTypeOwner));

		if (CurveClosedData.IsEmpty() || CurveTypeData.IsEmpty())  // We should use intrinsic curve info if curve_closed nor curve_type not specified
		{
			HAPI_CurveInfo CurveInfo;
			FHoudiniApi::GetCurveInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId, &CurveInfo);

			if (CurveClosedData.IsEmpty())
			{
				CurveClosedOwner = HAPI_ATTROWNER_DETAIL;
				CurveClosedData.Add(int8(CurveInfo.isClosed));
			}
			
			if (CurveTypeData.IsEmpty())
			{
				CurveTypeOwner = HAPI_ATTROWNER_DETAIL;
				CurveTypeData.Add(CurveInfo.curveType);
			}
		}

		// -------- Unreal Spline Data --------
		HAPI_AttributeOwner ArriveTangentOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_SPLINE_POINT_ARRIVE_TANGENT);
		TArray<float> ArriveTangentData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_UNREAL_SPLINE_POINT_ARRIVE_TANGENT, 3, ArriveTangentData, ArriveTangentOwner));

		HAPI_AttributeOwner LeaveTangentOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_SPLINE_POINT_LEAVE_TANGENT);
		TArray<float> LeaveTangentData;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetFloatAttributeData(NodeId, PartId, HAPI_ATTRIB_UNREAL_SPLINE_POINT_LEAVE_TANGENT, 3, LeaveTangentData, LeaveTangentOwner));

		// -------- Houdini Curve Color --------
		TArray<FColor> PointColors;
		TArray<FColor> CurveColors;
		HapiGetColors(PointColors, NodeId, PartId, HAPI_ATTROWNER_POINT);
		HapiGetColors(CurveColors, NodeId, PartId, HAPI_ATTROWNER_PRIM);


		HAPI_AttributeOwner SplitActorsOwner = FHoudiniEngineUtils::QueryAttributeOwner(AttribNames, PartInfo.attributeCounts, HAPI_ATTRIB_UNREAL_SPLIT_ACTORS);
		TArray<int8> bSplitActors;
		HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiGetEnumAttributeData(NodeId, PartId,
			HAPI_ATTRIB_UNREAL_SPLIT_ACTORS, bSplitActors, SplitActorsOwner));


		TArray<TSharedPtr<FHoudiniAttribute>> PropAttribs;
		HOUDINI_FAIL_RETURN(FHoudiniAttribute::HapiRetrieveAttributes(NodeId, PartId, AttribNames, PartInfo.attributeCounts,
			HAPI_ATTRIB_PREFIX_UNREAL_UPROPERTY, PropAttribs));

		const bool bCustomFolderPath = PropAttribs.ContainsByPredicate([](const TSharedPtr<FHoudiniAttribute>& Attrib)
			{
				return Attrib->GetAttributeName().Equals(TEXT("FolderPath"), ESearchCase::IgnoreCase);
			});

		TArray<UHoudiniParameterAttribute*> EditPointParmAttribs;
		TArray<UHoudiniParameterAttribute*> EditPrimParmAttribs;

		const TArray<int32>& VertexIndices = Part.VertexIndices;
		for (const auto& SplitCurves : Part.SplitCurvesMap)
		{
			const FString& SplitValue = SplitCurves.Value.SplitValue;

			for (const auto& GroupCurves : SplitCurves.Value.GroupCurvesMap)
			{
				const int32& GroupIdx = GroupCurves.Key;

				const int32& MainCurveIdx = GroupCurves.Value[0];
				const int32 MainVertexIdx = (MainCurveIdx == 0) ? 0 : VertexIndices[MainCurveIdx - 1];

				bool bSplitActor = false;
				if (!bSplitActors.IsEmpty())
					bSplitActor = bSplitActors[FHoudiniOutputUtils::CurveAttributeEntryIdx(SplitActorsOwner, MainVertexIdx, MainCurveIdx)] >= 1;

				const FString GroupName = Part.EditableGroupNames.IsValidIndex(GroupIdx) ? Part.EditableGroupNames[GroupIdx] : TEXT(HAPI_GROUP_MAIN_GEO);
				FHoudiniCurvesOutput NewHCOutput;
				if (FHoudiniCurvesOutput* FoundHCOutput = FHoudiniOutputUtils::FindOutputHolder(OldHoudiniCurvesOutputs,
					[&](const FHoudiniCurvesOutput* OldHCOutput)
					{
						if (OldHCOutput->CanReuse(SplitValue, bSplitActor))
						{
							if (const UHoudiniCurvesComponent* HCC = OldHCOutput->Find(Node))
								return GroupName == HCC->GetGroupName();
						}

						return false;
					}))
					NewHCOutput = *FoundHCOutput;

				UHoudiniCurvesComponent* HCC = NewHCOutput.CreateOrUpdate(Node, SplitValue, bSplitActor);
				HCC->ResetCurvesData();

				TArray<int32> PointIndices;  // Record the global point index
				TMap<int32, int32> PointIdxMap;  // PointId map to local index
				for (const int32& CurveIdx : GroupCurves.Value)
				{
					FHoudiniCurve& Curve = HCC->AddCurve();
					const int32 StartVertexIdx = ((CurveIdx == 0) ? 0 : VertexIndices[CurveIdx - 1]);
					for (int32 GlobalPointIdx = StartVertexIdx; GlobalPointIdx < VertexIndices[CurveIdx]; ++GlobalPointIdx)
					{
						const int32& PointId = PointIds.IsEmpty() ? GlobalPointIdx : PointIds[GlobalPointIdx];
						if (const int32* FoundLocalPointIdxPtr = PointIds.IsEmpty() ? nullptr : PointIdxMap.Find(PointId))
							Curve.PointIndices.Add(*FoundLocalPointIdxPtr);
						else
						{
							FHoudiniCurvePoint& Point = HCC->AddPoint();
							Curve.PointIndices.Add(Point.Id);
							if (!PointIds.IsEmpty())
								PointIdxMap.Add(PointId, Point.Id);
							PointIndices.Add(GlobalPointIdx);

							Point.Transform.SetLocation(FVector(PositionData[GlobalPointIdx * 3],
								PositionData[GlobalPointIdx * 3 + 2], PositionData[GlobalPointIdx * 3 + 1]) * POSITION_SCALE_TO_UNREAL_F);
							
							if (!Rots.IsEmpty())
								Point.Transform.SetRotation(Rots[FHoudiniOutputUtils::CurveAttributeEntryIdx(RotOwner, GlobalPointIdx, CurveIdx)]);

							if (!ScaleData.IsEmpty())
							{
								const int32 ScaleDataIdx = FHoudiniOutputUtils::CurveAttributeEntryIdx(ScaleOwner, GlobalPointIdx, CurveIdx) * 3;
								Point.Transform.SetScale3D(FVector(ScaleData[ScaleDataIdx], ScaleData[ScaleDataIdx + 2], ScaleData[ScaleDataIdx + 1]));
							}
							
							if (!PointColors.IsEmpty())
								Point.Color = PointColors[GlobalPointIdx];
						}
					}

					Curve.bClosed = (bool)CurveClosedData[FHoudiniOutputUtils::CurveAttributeEntryIdx(CurveClosedOwner, StartVertexIdx, CurveIdx)];
					Curve.Type = (EHoudiniCurveType)CurveTypeData[FHoudiniOutputUtils::CurveAttributeEntryIdx(CurveTypeOwner, StartVertexIdx, CurveIdx)];

					if (!CurveColors.IsEmpty())
						Curve.Color = CurveColors[CurveIdx];
				}

				if ((GroupIdx >= 0) || bIsMainGeoEditable)  // This HoudiniCurves can be edited
				{
					HOUDINI_FAIL_RETURN(HCC->HapiUpdate(Node, Part.GetGroupName(GroupIdx), NodeId, PartId, PointIndices, GroupCurves.Value,
						AttribNames, PartInfo.attributeCounts, EditPointParmAttribs, EditPrimParmAttribs, UHoudiniCurvesComponent::GetPointIntrinsicNames(true), UHoudiniCurvesComponent::GetPrimIntrinsicNames(true)));
				}
				else
					HCC->ClearEditData();

				HCC->Modify();
				//HCC->MarkRenderStateDirty();

				// Set UProperties
				SET_OBJECT_UPROPERTIES(HCC, FHoudiniOutputUtils::CurveAttributeEntryIdx(PropAttribOwner, MainVertexIdx, MainCurveIdx));
				SET_SPLIT_ACTOR_UPROPERTIES(NewHCOutput, FHoudiniOutputUtils::CurveAttributeEntryIdx(PropAttribOwner, MainVertexIdx, MainCurveIdx), true);

				NewHoudiniCurvesOutputs.Add(NewHCOutput);
			}

			for (const auto& ClassSplines : SplitCurves.Value.ClassSplinesMap)
			{
				auto SetSplineDataLambda = [&](USplineComponent* Spline, const int32& StartVtxIdx, const int32& CurveIdx) -> void
					{
						const int32& EndVtxIdx = VertexIndices[CurveIdx];
						for (int32 PointIdx = Spline->GetNumberOfSplinePoints() - 1; PointIdx >= 0; --PointIdx)
							Spline->RemoveSplinePoint(PointIdx, false);

						for (int32 VtxIdx = StartVtxIdx; VtxIdx < EndVtxIdx; ++VtxIdx)
						{
							FSplinePoint Point;
							Point.InputKey = VtxIdx - StartVtxIdx;
							Point.Position = FVector(PositionData[VtxIdx * 3], PositionData[VtxIdx * 3 + 2], PositionData[VtxIdx * 3 + 1]) * POSITION_SCALE_TO_UNREAL;
							if (!Rots.IsEmpty())
							{
								Point.Rotation = Rots[FHoudiniOutputUtils::CurveAttributeEntryIdx(RotOwner, VtxIdx, CurveIdx)].Rotator();
							}
							if (!ScaleData.IsEmpty())
							{
								const int32 ValueIdx = FHoudiniOutputUtils::CurveAttributeEntryIdx(ScaleOwner, VtxIdx, CurveIdx) * 3;
								Point.Scale = FVector(ScaleData[ValueIdx], ScaleData[ValueIdx + 2], ScaleData[ValueIdx + 1]);
							}
							if (!ArriveTangentData.IsEmpty())
							{
								const int32 ValueIdx = FHoudiniOutputUtils::CurveAttributeEntryIdx(ArriveTangentOwner, VtxIdx, CurveIdx) * 3;
								Point.ArriveTangent = FVector(ArriveTangentData[ValueIdx], ArriveTangentData[ValueIdx + 2], ArriveTangentData[ValueIdx + 1]) * POSITION_SCALE_TO_UNREAL;
							}
							if (!LeaveTangentData.IsEmpty())
							{
								const int32 ValueIdx = FHoudiniOutputUtils::CurveAttributeEntryIdx(LeaveTangentOwner, VtxIdx, CurveIdx) * 3;
								Point.LeaveTangent = FVector(LeaveTangentData[ValueIdx], LeaveTangentData[ValueIdx + 2], LeaveTangentData[ValueIdx + 1]) * POSITION_SCALE_TO_UNREAL;
							}
							Point.Type = (ArriveTangentData.IsEmpty() || LeaveTangentData.IsEmpty()) ?
								((!CurveTypeData.IsEmpty() && (CurveTypeData[FHoudiniOutputUtils::CurveAttributeEntryIdx(CurveTypeOwner, VtxIdx, CurveIdx)] <= 0)) ?
									ESplinePointType::Linear : ESplinePointType::Curve) : ESplinePointType::CurveCustomTangent;

							Spline->AddPoint(Point, false);
						}

						if (!CurveClosedData.IsEmpty())
						{
							Spline->SetClosedLoop(bool(CurveClosedData[FHoudiniOutputUtils::CurveAttributeEntryIdx(CurveClosedOwner, StartVtxIdx, CurveIdx)]), false);
						}
						for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
							PropAttrib->SetObjectPropertyValues(Spline, FHoudiniOutputUtils::CurveAttributeEntryIdx(PropAttrib->GetOwner(), StartVtxIdx, CurveIdx));

						Spline->UpdateSpline();
						Spline->Bounds = Spline->GetLocalBounds();
					};

				const TArray<int32>& CurveIndices = ClassSplines.Value;
				if (UClass* SplineClass = Cast<UClass>(ClassSplines.Key))
				{
					if (SplineClass->IsChildOf(USplineComponent::StaticClass()))
					{
						bool bSplitActor = false;
						{
							const int32& MainCurveIdx = CurveIndices[0];
							const int32 MainVertexIdx = (MainCurveIdx == 0) ? 0 : VertexIndices[MainCurveIdx - 1];
							if (!bSplitActors.IsEmpty())
								bSplitActor = bSplitActors[FHoudiniOutputUtils::CurveAttributeEntryIdx(SplitActorsOwner, MainVertexIdx, MainCurveIdx)] >= 1;
						}

						TDoubleLinkedList<FHoudiniInstancedComponentOutput*>::TDoubleLinkedListNode* MostMatchedListNode = nullptr;
						int32 MinScore = -1;
						for (auto OldOutputIter = OldSplineComponentOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
						{
							if (OldOutputIter->GetValue()->CanReuse(SplitValue, bSplitActor))
							{
								const int32 Score = OldOutputIter->GetValue()->GetMatchScore(SplineClass, CurveIndices.Num());
								if (Score == 0)  // Perfect matched, we should stop searching here
								{
									MostMatchedListNode = OldOutputIter;
									break;
								}
								if ((Score > 0) && ((MinScore < 0) || (MinScore > Score)))
								{
									MostMatchedListNode = OldOutputIter;
									MinScore = Score;
								}
							}
						}

						FHoudiniInstancedComponentOutput NewSplineComponentOutput;
						if (MostMatchedListNode)
						{
							NewSplineComponentOutput = *MostMatchedListNode->GetValue();
							OldSplineComponentOutputs.RemoveNode(MostMatchedListNode);
						}

						TArray<USceneComponent*> Components;
						NewSplineComponentOutput.Update(SplineClass, GetNode(), SplitValue, bSplitActor, CurveIndices.Num(), Components);
						for (int32 InstIdx = 0; InstIdx < CurveIndices.Num(); ++InstIdx)
						{
							if (USplineComponent* SC = Cast<USplineComponent>(Components[InstIdx]))
							{
								const int32& CurveIdx = CurveIndices[InstIdx];
								SetSplineDataLambda(SC, (CurveIdx == 0) ? 0 : VertexIndices[CurveIdx - 1], CurveIdx);
								
								if (SC->GetClass() != USplineComponent::StaticClass())
									SC->PostEditChange();  // Notify WaterLake to generate polygon
								SC->Modify();
							}
						}

						NewSplineComponentOutputs.Add(NewSplineComponentOutput);
						continue;
					}
				}

				// May be Actors that contain SplineComponent
				// Try to find the most matched InstancedActorOutput
				UObject* SplineClass = ClassSplines.Key;

				TDoubleLinkedList<FHoudiniInstancedActorOutput*>::TDoubleLinkedListNode* MostMatchedListNode = nullptr;
				int32 MinScore = -1;
				for (auto OldOutputIter = OldSplineActorOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
				{
					const int32 Score = OldOutputIter->GetValue()->GetMatchScore(SplineClass, CurveIndices.Num());
					if (Score == 0)  // Perfect matched, we should stop searching here
					{
						MostMatchedListNode = OldOutputIter;
						break;
					}
					if (Score > 0 && (MinScore < 0 || MinScore > Score))
					{
						MostMatchedListNode = OldOutputIter;
						MinScore = Score;
					}
				}

				FHoudiniInstancedActorOutput NewSplineActorOutput;
				if (MostMatchedListNode)
				{
					NewSplineActorOutput = *MostMatchedListNode->GetValue();
					OldSplineActorOutputs.RemoveNode(MostMatchedListNode);
				}

				if (NewSplineActorOutput.Update(Node, SplitValue, SplineClass, InstanceActorMap.FindOrAdd(SplineClass),
					CurveIndices, TArray<FTransform>{}, [&](AActor* Actor, const int32& CurveIdx)
					{
						USplineComponent* SC = nullptr;
						const int32 VtxIdx = (CurveIdx == 0) ? 0 : VertexIndices[CurveIdx - 1];
						for (UActorComponent* Component : Actor->GetComponents())
						{
							if (USplineComponent* TargetSC = Cast<USplineComponent>(Component))
							{
								SetSplineDataLambda(TargetSC, VtxIdx, CurveIdx);
								SC = TargetSC;
								break;
							}
						}

						for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
							PropAttrib->SetObjectPropertyValues(Actor, FHoudiniOutputUtils::CurveAttributeEntryIdx(PropAttrib->GetOwner(), VtxIdx, CurveIdx));

						USceneComponent* RootComponent = Actor->GetRootComponent();
						if (RootComponent && RootComponent->GetClass() != USceneComponent::StaticClass())  // We should also set RootComponent properties, like PointLight. however, if is SceneComponent, then we need NOT to set
						{
							for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)
								PropAttrib->SetObjectPropertyValues(RootComponent, FHoudiniOutputUtils::CurveAttributeEntryIdx(PropAttrib->GetOwner(), VtxIdx, CurveIdx));
						}

						if (IsValid(SC) && (SC->GetClass() != USplineComponent::StaticClass()))
							SC->PostEditChange();  // Notify WaterLake to generate polygon
					}, bCustomFolderPath))
					NewSplineActorOutputs.Add(NewSplineActorOutput);
				else
					NewSplineActorOutput.Destroy();
			}
		}
	}


	// -------- Post-processing --------
	// Destroy old outputs, like this->Destroy()
	for (const FHoudiniInstancedComponentOutput* OldSplineComponentOutput : OldSplineComponentOutputs)
		OldSplineComponentOutput->Destroy(Node);
	OldSplineComponentOutputs.Empty();
	for (const FHoudiniInstancedActorOutput* OldSplineActorOutput : OldSplineActorOutputs)
		OldSplineActorOutput->Destroy();
	OldSplineActorOutputs.Empty();
	for (const FHoudiniCurvesOutput* OldHCOutput : OldHoudiniCurvesOutputs)
		OldHCOutput->Destroy(Node);
	OldHoudiniCurvesOutputs.Empty();

	// Update output holders
	SplineComponentOutputs = NewSplineComponentOutputs;
	SplineActorOutputs = NewSplineActorOutputs;
	HoudiniCurvesOutputs = NewHoudiniCurvesOutputs;

	return true;
}

void UHoudiniOutputCurve::Destroy() const
{
	for (const FHoudiniInstancedComponentOutput& OldSplineComponentOutput : SplineComponentOutputs)
		OldSplineComponentOutput.Destroy(GetNode());

	for (const FHoudiniInstancedActorOutput& OldSplineActorOutput : SplineActorOutputs)
		OldSplineActorOutput.Destroy();

	for (const FHoudiniCurvesOutput& OldHCOutput : HoudiniCurvesOutputs)
		OldHCOutput.Destroy(GetNode());
}

void UHoudiniOutputCurve::CollectActorSplitValues(TSet<FString>& InOutSplitValues, TSet<FString>& InOutEditableSplitValues) const
{
	for (const FHoudiniInstancedComponentOutput& SCOutput : SplineComponentOutputs)
	{
		if (SCOutput.IsSplitActor())
			InOutSplitValues.FindOrAdd(SCOutput.GetSplitValue());
	}

	for (const FHoudiniCurvesOutput& HCOutput : HoudiniCurvesOutputs)
	{
		if (HCOutput.IsSplitActor())
			InOutEditableSplitValues.FindOrAdd(HCOutput.GetSplitValue());
	}
}

void UHoudiniOutputCurve::OnNodeTransformed(const FMatrix& DeltaXform) const
{
	for (const FHoudiniInstancedActorOutput& SAOutput : SplineActorOutputs)
		SAOutput.TransformActors(DeltaXform);
}

void UHoudiniOutputCurve::DestroyStandaloneActors() const
{
	for (const FHoudiniInstancedActorOutput& SAOutput : SplineActorOutputs)
		SAOutput.Destroy();
}

void UHoudiniOutputCurve::CollectEditCurves(TMap<FString, TArray<UHoudiniCurvesComponent*>>& InOutGroupCurvesMap) const
{
	for (const FHoudiniCurvesOutput& HCOutput : HoudiniCurvesOutputs)
	{
		UHoudiniCurvesComponent* HCC = HCOutput.Find(GetNode());
		if (HCC && HCC->GetUpdateCycles())
			InOutGroupCurvesMap.FindOrAdd(HCC->GetGroupName()).Add(HCC);
	}
}
