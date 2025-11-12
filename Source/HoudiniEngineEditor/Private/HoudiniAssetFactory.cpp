// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniAssetFactory.h"

#include "HoudiniAsset.h"


UHoudiniAssetFactory::UHoudiniAssetFactory()
{
	// This factory is responsible for manufacturing HoudiniEngine assets.
	SupportedClass = UHoudiniAsset::StaticClass();

	// This factory does not manufacture new objects from scratch.
	bCreateNew = false;

	// This factory will not open the editor for each new object.
	bEditAfterNew = false;

	// This factory will import objects from files.
	bEditorImport = true;

	// Factory does not import objects from text.
	bText = false;

	// Add supported formats.
	Formats.Add(TEXT("otl;Houdini Engine Asset"));
	Formats.Add(TEXT("otllc;Houdini Engine Limited Commercial Asset"));
	Formats.Add(TEXT("otlnc;Houdini Engine Non-Commercial Asset"));
	Formats.Add(TEXT("hda;Houdini Engine Asset"));
	Formats.Add(TEXT("hdalc;Houdini Engine Limited Commercial Asset"));
	Formats.Add(TEXT("hdanc;Houdini Engine Non-Commercial Asset"));
	Formats.Add(TEXT("hdalibrary;Houdini Engine Expanded Asset"));
}

UObject* UHoudiniAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UHoudiniAsset* NewAsset = NewObject<UHoudiniAsset>(InParent, InName, Flags | RF_Public | RF_Standalone | RF_Transactional);
	NewAsset->SetFilePath(Filename);
	NewAsset->LoadBuffer();
	return NewAsset;
}

bool UHoudiniAssetFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return IsValid(Cast<UHoudiniAsset>(Obj));
}

void UHoudiniAssetFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (NewReimportPaths.IsEmpty())
		return;

	if (UHoudiniAsset* OldAsset = Cast<UHoudiniAsset>(Obj))
		OldAsset->SetFilePath(NewReimportPaths[0]);
}

EReimportResult::Type UHoudiniAssetFactory::Reimport(UObject* Obj)
{
	if (UHoudiniAsset* OldAsset = Cast<UHoudiniAsset>(Obj))
		return OldAsset->LoadBuffer() ? EReimportResult::Succeeded : EReimportResult::Failed;

	return EReimportResult::Failed;
}
