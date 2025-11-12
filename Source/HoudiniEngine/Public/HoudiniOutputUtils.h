// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include <string>
#include "CoreMinimal.h"

#include "HAPI/HAPI_Common.h"


class ALandscape;
class AHoudiniNode;
class UHoudiniInputMask;
class FHoudiniAttribute;

#define POINT_ATTRIB_ENTRY_IDX(OWNER, POINT_INDEX) ((OWNER == HAPI_ATTROWNER_DETAIL) ? 0 : POINT_INDEX)

#define GET_SPLIT_VALUE_STR SplitValueMap.IsEmpty() ? FString::FromInt(SplitKey) : SplitValueMap[SplitKey]

#define SET_OBJECT_UPROPERTIES(OBJECT, ATTRIB_INDEX) for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)\
{\
	const HAPI_AttributeOwner& PropAttribOwner = PropAttrib->GetOwner();\
	PropAttrib->SetObjectPropertyValues(OBJECT, ATTRIB_INDEX);\
}

#define SET_SPLIT_ACTOR_UPROPERTIES(OUTPUT_HOLDER, ATTRIB_INDEX, IS_EDIT_GEO) if (OUTPUT_HOLDER.IsSplitActor())\
{\
	if (AActor* SplitActor = OUTPUT_HOLDER.FindSplitActor(GetNode(), IS_EDIT_GEO))\
	{\
		TArray<FString>& SetPropertyNames = ActorPropertyNamesMap.FindOrAdd(SplitActor);  /* Get the property names that has been set to this SplitActor*/\
		for (const TSharedPtr<FHoudiniAttribute>& PropAttrib : PropAttribs)\
		{\
			if (!SetPropertyNames.Contains(PropAttrib->GetAttributeName()))  /* We should avoid to set the property twice */\
			{\
				SetPropertyNames.Add(PropAttrib->GetAttributeName());\
				const HAPI_AttributeOwner PropAttribOwner = (HAPI_AttributeOwner)PropAttrib->GetOwner();\
				PropAttrib->SetObjectPropertyValues(SplitActor, ATTRIB_INDEX);\
			}\
		}\
	}\
}

struct HOUDINIENGINE_API FHoudiniOutputUtils
{
	// -------- Common --------
	template<typename TOutputHolder, typename Predicate>
	FORCEINLINE static void UpdateOutputHolders(TArray<TOutputHolder>& SplittableOutputs, Predicate Pred,
		TDoubleLinkedList<TOutputHolder*>& OutOldSplittableOutputs)
	{
		for (TOutputHolder& OldOutputHolder : SplittableOutputs)
		{
			if (Pred(OldOutputHolder))
				OutOldSplittableOutputs.AddTail(&OldOutputHolder);
		}
	}

	template<typename TSplittableOutput, typename Predicate>
	FORCEINLINE static void UpdateSplittableOutputHolders(const TSet<FString>& ModifySplitValues, const TSet<FString>& RemoveSplitValues,
		TArray<TSplittableOutput>& SplittableOutputs, Predicate Pred,
		TDoubleLinkedList<TSplittableOutput*>& OutOldSplittableOutputs, TArray<TSplittableOutput>& OutNewSplittableOutputs)
	{
		for (TSplittableOutput& SplittableOutput : SplittableOutputs)
		{
			if (!ModifySplitValues.Contains(SplittableOutput.GetSplitValue()) && !RemoveSplitValues.Contains(SplittableOutput.GetSplitValue()))
				OutNewSplittableOutputs.Add(SplittableOutput);  // Means these output does NOT need to modify, just add them to NewSplittableOutputs
			else if (Pred(SplittableOutput))
				OutOldSplittableOutputs.AddTail(&SplittableOutput);
		}
	}

	template<typename TOutputHolder, typename Predicate>
	FORCEINLINE static TOutputHolder* FindOutputHolder(TDoubleLinkedList<TOutputHolder*>& InOutOldSplittableOutputs, Predicate Pred)
	{
		for (auto OldOutputIter = InOutOldSplittableOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
		{
			TOutputHolder* OldOutputHolder = OldOutputIter->GetValue();
			if (Pred(OldOutputHolder))
			{
				InOutOldSplittableOutputs.RemoveNode(OldOutputIter);
				return OldOutputHolder;
			}
		}

		return nullptr;
	}

	static FString GetCookFolderPath(const AHoudiniNode* Node);

	// -------- Shared Memory --------
	static void CloseData(const float* Data, const size_t& SHMHandle);  // If SHMHandle is 0, means Data was from malloc

	// -------- Geometry --------
	static int32 CurveAttributeEntryIdx(const HAPI_AttributeOwner& AttribOwner, const int32& VtxIdx, const int32& CurveIdx);

	// Find SplitAttrib based on "unreal_split_attr"
	static bool HapiGetSplitValues(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
		TArray<int32>& OutSplitKeys, TMap<int32, FString>& OutSplitValueMap, HAPI_AttributeOwner& InOutOwner);  // SplitValueMap will be empty is split values is int

	// Get "curve_point_id" on point or vertex class
	static bool HapiGetCurvePointIds(const int32& NodeId, const int32& PartId,
		const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX], TArray<int32>& OutPointIds);

	static bool HapiGetPartialOutputModes(const int32& NodeId, const int32& PartId,
		TArray<int8>& OutData, HAPI_AttributeOwner& InOutOwner);

	// -------- Landscape --------
	static bool GatherAllLandscapes(const AHoudiniNode* Node,
		TMap<FString, TArray<ALandscape*>>& OutInputLandscapesMap, TArray<ALandscape*>& OutLandscapes);

	static ALandscape* FindTargetLandscape(const FString& LandscapeOutputTarget,
		const TMap<FString, TArray<ALandscape*>>& InputLandscapesMap, const TArray<ALandscape*>& Landscapes);  // Return nullptr if not found

	static void NotifyLandscapeChanged(const FName& LandscapeName, const FName& EditLayerName, const FName& LayerName, const FIntRect& ChangedExtent);

	static UHoudiniInputMask* FindMaskInput(const AHoudiniNode* Node, const FString& MaskInputPath, FTransform& OutLandscapeTransform, FIntRect& OutLandscapeExtent);

	static void WriteToMaskInput(UHoudiniInputMask* MaskInput, TConstArrayView<uint8> Data, const FIntRect& Extent);

	// -------- Material --------
	static UMaterialInterface* GetMaterialInstance(UMaterialInterface* Material,
		const TArray<TSharedPtr<FHoudiniAttribute>>& MatAttribs, const TFunctionRef<int32(const HAPI_AttributeOwner&)> Index,
		const FString& CookFolderPath, TMap<TPair<UMaterialInterface*, uint32>, UMaterialInstance*>& InOutMatParmMap);
};
