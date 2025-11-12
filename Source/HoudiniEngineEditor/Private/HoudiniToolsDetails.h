// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"


class AHoudiniNode;
class UHoudiniInputMask;


class HOUDINIENGINEEDITOR_API FHoudiniManageToolDetails : public IDetailCustomization
{
protected:
	FDelegateHandle HoudiniNodeEventsHandle;

	FDelegateHandle OnNodeRegisteredHandle;

	FDelegateHandle OnNodeFolderChangedHandle;

	FDelegateHandle OnNodeFolderRenamedHandle;

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	virtual void PendingDelete() override;
};


class HOUDINIENGINEEDITOR_API FHoudiniMaskToolDetails : public IDetailCustomization
{
protected:
	TWeakObjectPtr<UHoudiniInputMask> SelectedMaskInput;

	FDelegateHandle OnMaskChangedHandle;

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	virtual void PendingDelete() override;
};


class HOUDINIENGINEEDITOR_API FHoudiniEditToolDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
