// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEditableGeometryEditorUtils.h"

#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "EditorViewportClient.h"
#include "DragAndDrop/AssetDragDropOp.h"

#include "HoudiniEditableGeometry.h"

#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniCurvesComponentVisualizer.h"
#include "HoudiniMeshComponentVisualizer.h"
#include "HoudiniAttributeParameterHolder.h"


// -------- FHoudiniEditableGeometryVisualSettings --------
FHoudiniEditableGeometryVisualSettings* FHoudiniEditableGeometryVisualSettings::Settings = nullptr;

#define HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS TEXT("HoudiniEditableGeomemtryVisualSettings")

FHoudiniEditableGeometryVisualSettings* FHoudiniEditableGeometryVisualSettings::Load()
{
	if (!Settings)
	{
		Settings = new FHoudiniEditableGeometryVisualSettings;

		GConfig->GetFloat(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("PointSize"), Settings->PointSize, GEditorPerProjectIni);
		GConfig->GetFloat(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("EdgeThickness"), Settings->EdgeThickness, GEditorPerProjectIni);
		GConfig->GetBool(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("DistanceCulling"), Settings->bDistanceCulling, GEditorPerProjectIni);
		GConfig->GetFloat(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("CullDistance"), Settings->CullDistance, GEditorPerProjectIni);
	}

	return Settings;
}

void FHoudiniEditableGeometryVisualSettings::Save()
{
	if (Settings)
	{
		if (Settings->PointSize != DEFAULT_CULL_DISTANCE)
			GConfig->SetFloat(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("PointSize"), Settings->PointSize, GEditorPerProjectIni);
		else
			GConfig->RemoveKey(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("PointSize"), GEditorPerProjectIni);
		
		if (Settings->EdgeThickness != DEFAULT_EDGE_THICKNESS)
			GConfig->SetFloat(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("EdgeThickness"), Settings->EdgeThickness, GEditorPerProjectIni);
		else
			GConfig->RemoveKey(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("EdgeThickness"), GEditorPerProjectIni);

		if (Settings->bDistanceCulling != DEFAULT_DISTANCE_CULLING)
			GConfig->SetBool(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("DistanceCulling"), Settings->bDistanceCulling, GEditorPerProjectIni);
		else
			GConfig->RemoveKey(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("DistanceCulling"), GEditorPerProjectIni);

		if (Settings->CullDistance != DEFAULT_CULL_DISTANCE)
			GConfig->SetFloat(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("CullDistance"), Settings->CullDistance, GEditorPerProjectIni);
		else
			GConfig->RemoveKey(HOUDINI_EDITABLE_GEOMETRY_VISUAL_SETTINGS, TEXT("CullDistance"), GEditorPerProjectIni);

		delete Settings;
		Settings = nullptr;
	}
}


// -------- FHoudiniEditableGeometryAssetDrop --------
TWeakPtr<IAssetViewport> FHoudiniEditableGeometryAssetDrop::AssetDropViewport;
TWeakPtr<SOverlay> FHoudiniEditableGeometryAssetDrop::AssetDropOverlay;

void FHoudiniEditableGeometryAssetDrop::Register()
{
	if (!AssetDropViewport.IsValid())
	{
		if (TSharedPtr<IAssetViewport> ActiveViewport = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstActiveViewport())
		{
			class SAssetDropAreaOverlay : public SCompoundWidget
			{
			public:
				bool bCanDropAssets = false;

				SLATE_BEGIN_ARGS(SAssetDropAreaOverlay) {}
				SLATE_END_ARGS()

				void Construct(const FArguments& InArgs) {}

				virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
				{
					bCanDropAssets = false;

					UHoudiniEditableGeometry* EditGeo = FHoudiniEngineEditor::Get().GetEditingGeometry();
					if (EditGeo && EditGeo->CanDropAssets())
					{
						if (ULevel* Level = EditGeo->GetWorld()->GetCurrentLevel())
						{
							if (!Level->bLocked)
							{
								Level->bLocked = true;  // Skip SLevelViewport::OnDragEnter
								AsyncTask(ENamedThreads::GameThread, [Level] { if (IsValid(Level)) Level->bLocked = false; });
							}
						}
						bCanDropAssets = true;
					}
				}

				virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
				{
					return bCanDropAssets ? FReply::Handled() : FReply::Unhandled();
				}

				virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
				{
					if (!bCanDropAssets)
						return FReply::Unhandled();

					if (TSharedPtr<FAssetDragDropOp> AssetDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>())
					{
						UHoudiniEditableGeometry* EditGeo = FHoudiniEngineEditor::Get().GetEditingGeometry();
						if (EditGeo && AssetDropViewport.IsValid())
						{
							const EHoudiniAttributeOwner SelectedClass = EditGeo->GetSelectedClass();  // Backup original SelectedClass
							const bool bPointCanDrop = bool(EditGeo->GetPointEditOptions() & EHoudiniEditOptions::AllowAssetDrop);
							const bool bPrimCanDrop = bool(EditGeo->GetPrimEditOptions() & EHoudiniEditOptions::AllowAssetDrop);
							if ((SelectedClass == EHoudiniAttributeOwner::Point) && bPointCanDrop) {}
							else if ((SelectedClass == EHoudiniAttributeOwner::Prim) && bPrimCanDrop) {}
							else if (bPrimCanDrop)  // Switch SelectedClass to prim, so that asset can be dropped to prim
							{
								EditGeo->ResetSelection();
								*((EHoudiniAttributeOwner*)&EditGeo->GetSelectedClass()) = EHoudiniAttributeOwner::Prim;
							}
							else if (bPointCanDrop)  // Switch SelectedClass to point, so that asset can be dropped to point
							{
								EditGeo->ResetSelection();
								*((EHoudiniAttributeOwner*)&EditGeo->GetSelectedClass()) = EHoudiniAttributeOwner::Point;
							}
							else
								return FReply::Unhandled();

							// Copy from "Editor/Experimental/EditorInteractiveToolsFramework/Private/EdModeInteractiveToolsContext.cpp" Line 680: UEditorInteractiveToolsContext::GetRayFromMousePos
							auto GetMouseRayFromViewportLambda = [](FEditorViewportClient* ViewportClient, FViewport* Viewport, int MouseX, int MouseY, FSceneView*& View) -> FRay
								{
									FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
										ViewportClient->Viewport,
										ViewportClient->GetScene(),
										ViewportClient->EngineShowFlags).SetRealtimeUpdate(ViewportClient->IsRealtime()));		// why SetRealtimeUpdate here??
									// this View is deleted by the FSceneViewFamilyContext destructor
									View = ViewportClient->CalcSceneView(&ViewFamily);
									FViewportCursorLocation MouseViewportRay(View, ViewportClient, MouseX, MouseY);

									FVector RayOrigin = MouseViewportRay.GetOrigin();
									FVector RayDirection = MouseViewportRay.GetDirection();

									// in Ortho views, the RayOrigin appears to be completely arbitrary, in some views it is on the view plane,
									// others it moves back/forth with the OrthoZoom. Translate by a large amount here in hopes of getting
									// ray origin "outside" the scene (which is a disaster for numerical precision !! ... )
									if (ViewportClient->IsOrtho())
									{
										RayOrigin -= 0.1 * HALF_WORLD_MAX * RayDirection;
									}

									return FRay(RayOrigin, RayDirection, true);
								};

							const FVector2D ViewportMousePos = DragDropEvent.GetScreenSpacePosition() - MyGeometry.AbsolutePosition;

							FSceneView* View = nullptr;
							FVector RayCastPos;
							const int32 ElemIdx = EditGeo->RayCast(GetMouseRayFromViewportLambda(
								&(AssetDropViewport.Pin()->GetAssetViewportClient()), AssetDropViewport.Pin()->GetActiveViewport(),
								FMath::RoundToInt32(ViewportMousePos.X), FMath::RoundToInt32(ViewportMousePos.Y), View), RayCastPos);

							if (ElemIdx >= 0)
							{
								FVector2D PixelPos;
								View->WorldToPixel(RayCastPos, PixelPos);
								if (FVector2D::Distance(PixelPos, ViewportMousePos) < (FHoudiniEditableGeometryVisualSettings::Load()->PointSize * 1.5f))
								{
									TArray<UObject*> Assets;
									for (const FAssetData& AssetData : AssetDragDropOp->GetAssets())
									{
										UObject* Asset = AssetData.GetAsset();
										if (IsValid(Asset))
											Assets.Add(Asset);
									}
									if (!Assets.IsEmpty())
									{
										bool bShouldRefreshAttribPanel = EditGeo->OnAssetsDropped(Assets, ElemIdx, RayCastPos, DragDropEvent.GetModifierKeys());
										if (!bShouldRefreshAttribPanel)
											bShouldRefreshAttribPanel = (EditGeo->GetSelectedClass() != SelectedClass);
										
										if (UHoudiniAttributeParameterHolder* AttribParmHolder = UHoudiniAttributeParameterHolder::Get())
										{
											if (AttribParmHolder->DetailsView.IsValid() && (bShouldRefreshAttribPanel || (AttribParmHolder->GetEditableGeometry() != EditGeo)))
											{
												AttribParmHolder = UHoudiniAttributeParameterHolder::Get(EditGeo);
												if (TSharedPtr<IDetailsView> DetailsView = AttribParmHolder->DetailsView.Pin())
													DetailsView->ForceRefresh();
											}
										}

										return FReply::Handled();
									}
								}
							}
							
							*((EHoudiniAttributeOwner*)&EditGeo->GetSelectedClass()) = SelectedClass;  // Recover SelectedClass if assets failed to drop
						}
					}

					return FReply::Unhandled();
				}
			};

			TSharedPtr<SAssetDropAreaOverlay> DropArea;

			AssetDropViewport = ActiveViewport;
			ActiveViewport->AddOverlayWidget(SAssignNew(AssetDropOverlay, SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Padding(0.0)
				[
					SAssignNew(DropArea, SAssetDropAreaOverlay)
				]);
		}
	}
}

void FHoudiniEditableGeometryAssetDrop::Unregister()
{
	if (AssetDropViewport.IsValid() && AssetDropOverlay.IsValid())
		AssetDropViewport.Pin()->RemoveOverlayWidget(AssetDropOverlay.Pin().ToSharedRef());

	AssetDropViewport.Reset();
	AssetDropOverlay.Reset();
}


// -------- FHoudiniEditableGeometryEditingScope --------
bool FHoudiniEditableGeometryEditingScope::bAllowResetSelection = true;

FHoudiniEditableGeometryEditingScope::FHoudiniEditableGeometryEditingScope()
{
	bAllowResetSelection = false;
}

FHoudiniEditableGeometryEditingScope::~FHoudiniEditableGeometryEditingScope()
{
	if (UWorld* CurvesWorld = FHoudiniEngineEditor::Get().GetCurvesComponentVisualizer()->GetEditingWorld())
	{
		TSharedPtr<FComponentVisualizer> Visualizer = FHoudiniEngineEditor::Get().GetCurvesComponentVisualizer();
		GUnrealEd->ComponentVisManager.SetActiveComponentVis(
			FHoudiniEngineEditorUtils::FindEditorViewportClient(CurvesWorld), Visualizer);
	}
	else if (UWorld* MeshWorld = FHoudiniEngineEditor::Get().GetMeshComponentVisualizer()->GetEditingWorld())
	{
		TSharedPtr<FComponentVisualizer> Visualizer = FHoudiniEngineEditor::Get().GetMeshComponentVisualizer();
		GUnrealEd->ComponentVisManager.SetActiveComponentVis(
			FHoudiniEngineEditorUtils::FindEditorViewportClient(MeshWorld), Visualizer);
	}

	bAllowResetSelection = true;
}
