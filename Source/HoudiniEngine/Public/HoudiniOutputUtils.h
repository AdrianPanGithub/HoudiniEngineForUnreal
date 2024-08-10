// Copyright (c) <2024> Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include <string>
#include "CoreMinimal.h"

#include "HAPI/HAPI_Common.h"


class AHoudiniNode;
class ALandscape;
class FHoudiniAttribute;

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
	FORCEINLINE static void UpdateOutputHolders(TArray<TOutputHolder>& SplitableOutputs, Predicate Pred,
		TDoubleLinkedList<TOutputHolder*>& OutOldSplitableOutputs)
	{
		for (TOutputHolder& OldOutputHolder : SplitableOutputs)
		{
			if (Pred(OldOutputHolder))
				OutOldSplitableOutputs.AddTail(&OldOutputHolder);
		}
	}

	template<typename TSplitableOutput, typename Predicate>
	FORCEINLINE static void UpdateSplitableOutputHolders(const TSet<FString>& ModifySplitValues, const TSet<FString>& RemoveSplitValues,
		TArray<TSplitableOutput>& SplitableOutputs, Predicate Pred,
		TDoubleLinkedList<TSplitableOutput*>& OutOldSplitableOutputs, TArray<TSplitableOutput>& OutNewSplitableOutputs)
	{
		for (TSplitableOutput& SplitableOutput : SplitableOutputs)
		{
			if (!ModifySplitValues.Contains(SplitableOutput.GetSplitValue()) && !RemoveSplitValues.Contains(SplitableOutput.GetSplitValue()))
				OutNewSplitableOutputs.Add(SplitableOutput);  // Means these output does NOT need to modify, just add them to NewMeshOutputs
			else if (Pred(SplitableOutput))
				OutOldSplitableOutputs.AddTail(&SplitableOutput);
		}
	}

	template<typename TOutputHolder, typename Predicate>
	FORCEINLINE static TOutputHolder* FindOutputHolder(TDoubleLinkedList<TOutputHolder*>& InOutOldSplitableOutputs, Predicate Pred)
	{
		for (auto OldOutputIter = InOutOldSplitableOutputs.GetHead(); OldOutputIter; OldOutputIter = OldOutputIter->GetNextNode())
		{
			TOutputHolder* OldOutputHolder = OldOutputIter->GetValue();
			if (Pred(OldOutputHolder))
			{
				InOutOldSplitableOutputs.RemoveNode(OldOutputIter);
				return OldOutputHolder;
			}
		}

		return nullptr;
	}

	// -------- Shared Memory --------
	static void CloseData(const float* Data, const size_t& SHMHandle);  // If SHMHandle is 0, means Data was from malloc

	// -------- Geoemetry --------
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


	// -------- Material --------
	static UMaterialInterface* GetMaterialInstance(UMaterialInterface* Material,
		const TArray<TSharedPtr<FHoudiniAttribute>>& MatAttribs, const TFunctionRef<int32(const HAPI_AttributeOwner& AttribOwner)> Index,
		const FString& CookFolderPath, TMap<TPair<UMaterialInterface*, FString>, UMaterialInstance*>& InOutMatParmMap);
};
