// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAssetTypeActions.h"

#include "ImageUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "DesktopPlatformModule.h"
#include "ObjectTools.h"

#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniNode.h"

#include "HoudiniEngineStyle.h"


#define LOCTEXT_NAMESPACE "HoudiniEngine"

FText FHoudiniAssetTypeActions::GetName() const
{
	return LOCTEXT("HoudiniAssetTypeActions", "Houdini Asset");
}

FColor FHoudiniAssetTypeActions::GetTypeColor() const
{
	return FColor(255, 165, 0);
}

UClass* FHoudiniAssetTypeActions::GetSupportedClass() const
{
	return UHoudiniAsset::StaticClass();
}

uint32 FHoudiniAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

void FHoudiniAssetTypeActions::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	TArray<TWeakObjectPtr<UHoudiniAsset>> HoudiniAssets;
	for (UObject* Asset : InObjects)
	{
		UHoudiniAsset* HoudiniAsset = Cast<UHoudiniAsset>(Asset);
		if (IsValid(Asset))
			HoudiniAssets.Add(HoudiniAsset);
	}

	if (HoudiniAssets.IsEmpty())
		return;


	MenuBuilder.AddMenuEntry(
		LOCTEXT("HoudiniAsset_Instantiate", "Instantiate"),
		LOCTEXT("HoudiniAsset_InstantiateTooltip", "Instantiate the selected assets in the current world."),
		FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "ClassIcon.HoudiniNode"),
		FUIAction(
			FExecuteAction::CreateLambda([HoudiniAssets]
				{
					UWorld* CurrWorld = GEditor->GetEditorWorldContext().World();
					if (!IsValid(CurrWorld))
						return;

					TArray<AHoudiniNode*> NewNodes;
					for (const TWeakObjectPtr<UHoudiniAsset>& HoudiniAsset : HoudiniAssets)
					{
						if (HoudiniAsset.IsValid())
							NewNodes.Add(AHoudiniNode::Create(CurrWorld, HoudiniAsset.Get()));
					}

					if (!NewNodes.IsEmpty())
					{
						GEditor->SelectNone(false, false, false);

						for (AHoudiniNode* NewNode : NewNodes)
							GEditor->SelectActor(NewNode, true, true, true, true);
					}
				}),
			FCanExecuteAction::CreateLambda([] { return FHoudiniEngine::Get().AllowEdit(); })
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HoudiniAsset_UsePictureAsThumbnail", "Pick a Thumbnail"),
		LOCTEXT("HoudiniAsset_UsePictureAsThumbnailTooltip", "Pick a picture as Thumbnail"),
		FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.ImageChooser"),
		FUIAction(
			FExecuteAction::CreateLambda([HoudiniAssets]
				{
					TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
					void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
						? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
						: nullptr;

					IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
					TArray<FString> FilePaths;
					if (DesktopPlatform->OpenFileDialog(ParentWindowHandle, TEXT("Select a picture as thumbnail"), TEXT(""), TEXT(""),
						TEXT("Image|*.png;*.jpg;*.jpeg;*.bmp;*.tiff;*.tif"), EFileDialogFlags::None, FilePaths))
					{
						FImage Image;
						if (FImageUtils::LoadImage(*FilePaths[0], Image))
						{
							const int64 ElemSize = ERawImageFormat::GetBytesPerPixel(Image.Format);
							

							FObjectThumbnail TempThumbnail;
							TempThumbnail.SetImageSize(Image.SizeX, Image.SizeY);
							TArray<uint8>& ThumbnailByteArray = TempThumbnail.AccessImageData();

							const int32 MemorySize = Image.SizeX * Image.SizeY * ERawImageFormat::GetBytesPerPixel(Image.Format) * Image.NumSlices;
							ThumbnailByteArray.SetNumUninitialized(MemorySize);
							FMemory::Memcpy(&(ThumbnailByteArray[0]), &(Image.RawData[0]), MemorySize);

							for (const TWeakObjectPtr<UHoudiniAsset>& HoudiniAsset : HoudiniAssets)
							{
								if (!HoudiniAsset.IsValid())
									continue;

								UPackage* HoudiniAssetPackage = HoudiniAsset->GetPackage();
								FObjectThumbnail* NewThumbnail = ThumbnailTools::CacheThumbnail(HoudiniAsset->GetFullName(), &TempThumbnail, HoudiniAssetPackage);
								if (ensure(NewThumbnail))
								{
									//we need to indicate that the package needs to be resaved
									HoudiniAssetPackage->MarkPackageDirty();

									// Let the content browser know that we've changed the thumbnail
									NewThumbnail->MarkAsDirty();

									// Signal that the asset was changed if it is loaded so thumbnail pools will update
									HoudiniAsset->PostEditChange();

									//Set that thumbnail as a valid custom thumbnail so it'll be saved out
									NewThumbnail->SetCreatedAfterCustomThumbsEnabled();
								}
							}
						}
					}
				}),
			FCanExecuteAction::CreateLambda([] { return true; })
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HoudiniAsset_Reimport", "Reimport"),
		LOCTEXT("HoudiniAsset_ReimportTooltip", "Reimport the selected assets."),
		FSlateIcon(FHoudiniEngineStyle::GetStyleSetName(), "HoudiniEngine.Rebuild"),
		FUIAction(
			FExecuteAction::CreateLambda([HoudiniAssets]
				{
					for (const TWeakObjectPtr<UHoudiniAsset>& HoudiniAsset : HoudiniAssets)
					{
						if (!HoudiniAsset.IsValid())
							continue;
						
						if (!HoudiniAsset->LoadBuffer())  // File Does NOT exists, then open a file picker to apply reimport
						{
							TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
							void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
								? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
								: nullptr;

							IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
							TArray<FString> FilePaths;
							if (DesktopPlatform->OpenFileDialog(ParentWindowHandle, TEXT("Select a Houdini Digital Asset"), TEXT(""), TEXT(""),
								TEXT("HDA|*.otl;*.otllc;*.otlnc;*.hda;*.hdalc;*.hdanc;*.hdalibrary"), EFileDialogFlags::None, FilePaths))
							{
								if (FilePaths.IsValidIndex(0))
								{
									HoudiniAsset->SetFilePath(FilePaths[0]);
									HoudiniAsset->LoadBuffer();
								}
							}
						}
					}
				}),
			FCanExecuteAction::CreateLambda([] { return FHoudiniEngine::Get().AllowEdit(); })
		)
	);
}

#undef LOCTEXT_NAMESPACE
