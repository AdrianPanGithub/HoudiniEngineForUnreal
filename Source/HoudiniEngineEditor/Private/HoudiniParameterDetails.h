// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class IDetailCategoryBuilder;
class FDetailWidgetRow;
class UHoudiniParameter;
class UHoudiniMultiParameter;
class UHoudiniParameterFolderList;
class UHoudiniParameterFolder;


#define HOUDINI_STANDARD_PARM_ROW_HEIGHT    27.0f
#define HOUDINI_ASSETREF_PARM_ROW_HEIGHT    52.0f
#define HOUDINI_RAMP_PARM_ROW_HEIGHT        92.0f

class HOUDINIENGINEEDITOR_API FHoudiniParameterDetails  // Just a helper to create parameter panel, do NOT save the instances of this
{
public:
	static float Parse(IDetailCategoryBuilder& CategoryBuilder, const TArray<UHoudiniParameter*>& Parms, const bool& bIsAttribPanel);

protected:
	bool bIsAttribPanel = false;

	// -------- Current Parsed-Parm Caches --------
	float CurrIndentation = 0.0;

	TArray<float> CurrIndentationPositions;

	// -------- Previous Parsed-Parm Caches --------
	TWeakObjectPtr<const UHoudiniParameter> PrevParm;  // If nullptr, means this is the first parm to parse

	TArray<float> PrevIndentationPositions;

	TArray<int32> PrevParentIdStack;

	// -------- Global Caches ---------
	mutable TSet<TPair<int32, int32>> MultiParmIdIndexSet;  // if not found in this set, means this parm need add multi-parm insert/remove buttons.

	TMap<int32, TWeakObjectPtr<const UHoudiniParameter>> IdParmParentMap;  // FolderList, Folder, MultiParm

	TSet<int32> VisibleParentIds;

	TWeakObjectPtr<UHoudiniParameterFolderList> CurrFolderList;

	TArray<TWeakObjectPtr<const UHoudiniParameterFolder>> CurrFolders;  // All current folders will be added in PreParse

	float ParmPanelHeight = 0.0f;

	// -------- For Spinning --------
	static TWeakObjectPtr<const UHoudiniParameter> SpinningParm;

	static int32 SpinningIdx;

	static int32 SpinningIntValue;

	static float SpinningFloatValue;


	// -------- Common Methods ---------
	void Parse(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm);

	bool PreParse(const TWeakObjectPtr<const UHoudiniParameter>& CurrParm);  // Return should-parse

	void ParseIndentations(IDetailCategoryBuilder& CategoryBuilder, int32 CurrParentId);

	void CreateMultiParmInstanceButtons(IDetailCategoryBuilder& CategoryBuilder, TSharedPtr<SHorizontalBox> HorizontalBox,
		const TWeakObjectPtr<UHoudiniMultiParameter>& CurrMultiParm, const int32& MultiParmInstIdx) const;

	FDetailWidgetRow& CreateNameWidget(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& CurrParm) const;

	UHoudiniMultiParameter* NeedMultiParmButton(const TWeakObjectPtr<const UHoudiniParameter> CurrParm) const;

	void EndAllFolders(IDetailCategoryBuilder& CategoryBuilder);


	// -------- Typed Methods ---------
	void ParseSeparator(IDetailCategoryBuilder& CategoryBuilder) const;
	void ParseFolder(IDetailCategoryBuilder& CategoryBuilder);
	void ParseButton(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;

	// Int types
	void ParseInt(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseIntChoice(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseButtonStrip(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseToggle(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseFolderList(const TWeakObjectPtr<UHoudiniParameter>& Parm);

	// Float types
	void ParseFloat(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseColor(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;

	// String types
	void ParseString(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseStringChoice(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseAsset(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseAssetChoice(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;

	void ParseLabel(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;

	// Extras
	void ParseMultiParm(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm);
	void ParseFloatRamp(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
	void ParseColorRamp(IDetailCategoryBuilder& CategoryBuilder, const TWeakObjectPtr<UHoudiniParameter>& Parm) const;
};
