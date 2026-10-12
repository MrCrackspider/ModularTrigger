
#include "HierarchicalTriggerComponentVisualizer.h"

#include "CanvasTypes.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "HierarchicalTriggerComponent.h"
#include "HierarchicalTriggerComponentDetails.h"
#include "ModularTriggerEditorSettings.h"
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "TriggerModuleVisualization.h"

IMPLEMENT_HIT_PROXY(HTriggerVisualizationProxy, HComponentVisProxy)

namespace
{
	/** Circles and spheres are debug geometry seen at a glance; this many sides reads as round enough. */
	constexpr int32 ShapeSides = 24;

	/** Applied to a shape's alpha when its module is not the one expanded in the details panel. */
	FLinearColor ApplyDimming(const FLinearColor& Color, bool bHighlighted)
	{
		const UModularTriggerEditorSettings& Settings = UModularTriggerEditorSettings::Get();
		if (bHighlighted || !Settings.bDimUnselectedModules)
		{
			return Color;
		}

		FLinearColor Dimmed = Color;
		Dimmed.A *= Settings.DimOpacity;
		return Dimmed;
	}

	float ResolveThickness(float ModuleThickness)
	{
		// A module that says nothing about thickness gets the user's preference; one that asks for a specific
		// width means it, and keeps it.
		return ModuleThickness > 0.0f ? ModuleThickness : UModularTriggerEditorSettings::Get().LineThickness;
	}

	void DrawPrimitive(const FTriggerVisualizationPrimitive& Primitive, bool bHighlighted, FPrimitiveDrawInterface* PDI)
	{
		const FLinearColor Color = ApplyDimming(Primitive.Color, bHighlighted);
		const float Thickness = ResolveThickness(Primitive.Thickness);
		const FVector Location = Primitive.Transform.GetLocation();

		switch (Primitive.Shape)
		{
		case ETriggerVisualizationShape::Line:
			PDI->DrawLine(Location, Primitive.EndLocation, Color, SDPG_Foreground, Thickness);
			break;

		case ETriggerVisualizationShape::Arrow:
		{
			const FVector Delta = Primitive.EndLocation - Location;
			const double Length = Delta.Size();
			if (Length <= UE_KINDA_SMALL_NUMBER)
			{
				break;
			}
			const FMatrix ArrowToWorld = FRotationMatrix::MakeFromX(Delta / Length) * FTranslationMatrix(Location);
			// The head scales with the line so a long reference does not end in a speck.
			const float ArrowSize = static_cast<float>(FMath::Clamp(Length * 0.05, 8.0, 40.0));
			DrawDirectionalArrow(PDI, ArrowToWorld, Color, static_cast<float>(Length), ArrowSize, SDPG_Foreground, Thickness);
			break;
		}

		case ETriggerVisualizationShape::Sphere:
			DrawWireSphere(PDI, Primitive.Transform, Color, Primitive.Radius, ShapeSides, SDPG_Foreground, Thickness);
			break;

		case ETriggerVisualizationShape::Box:
			DrawWireBox(PDI, Primitive.Transform.ToMatrixWithScale(), FBox(-Primitive.Extent, Primitive.Extent), Color,
				SDPG_Foreground, Thickness);
			break;

		case ETriggerVisualizationShape::Circle:
		{
			const FMatrix Matrix = Primitive.Transform.ToMatrixWithScale();
			DrawCircle(PDI, Location, Matrix.GetScaledAxis(EAxis::X), Matrix.GetScaledAxis(EAxis::Y), Color,
				Primitive.Radius, ShapeSides, SDPG_Foreground, Thickness);
			break;
		}

		case ETriggerVisualizationShape::Point:
			PDI->DrawPoint(Location, Color, static_cast<float>(Primitive.Radius), SDPG_Foreground);
			break;
		}
	}

	/** Projects a world location and draws a string centred on it. No-op when the point is behind the camera. */
	void DrawLabel(const FVector& WorldLocation, const FText& Text, const FLinearColor& Color, const FSceneView* View,
		FCanvas* Canvas)
	{
		FVector2D PixelLocation;
		if (!View->ScreenToPixel(View->WorldToScreen(WorldLocation), PixelLocation))
		{
			return;
		}

		const FString String = Text.ToString();
		UFont* Font = GEngine->GetSmallFont();

		int32 Width = 0;
		int32 Height = 0;
		StringSize(Font, Width, Height, *String);

		Canvas->DrawShadowedString(
			PixelLocation.X - Width * 0.5f,
			PixelLocation.Y - Height * 0.5f,
			*String,
			Font,
			Color);
	}
}

bool FHierarchicalTriggerComponentVisualizer::Collect(const UActorComponent* Component,
	FTriggerVisualizationBuilder& OutBuilder)
{
	if (!UModularTriggerEditorSettings::Get().bEnableVisualization)
	{
		return false;
	}

	const UHierarchicalTriggerComponent* TriggerComponent = Cast<const UHierarchicalTriggerComponent>(Component);
	if (!TriggerComponent)
	{
		return false;
	}

	// Nothing to visualize about a running game: this is an authoring aid, and PIE has its own debug draw.
	const UWorld* World = TriggerComponent->GetWorld();
	if (!World || World->IsGameWorld())
	{
		return false;
	}

	const FGuid HighlightedModuleId = FHierarchicalTriggerComponentDetails::GetExpandedModuleId(TriggerComponent);
	CollectTree(TriggerComponent->Modules, 0, HighlightedModuleId, OutBuilder);

	return !OutBuilder.IsEmpty();
}

void FHierarchicalTriggerComponentVisualizer::CollectTree(
	const TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>& Modules, int32 Depth,
	const FGuid& HighlightedModuleId, FTriggerVisualizationBuilder& Builder)
{
	const UModularTriggerEditorSettings& Settings = UModularTriggerEditorSettings::Get();

	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (!IsValid(Module))
		{
			continue;
		}

		const bool bHighlighted = HighlightedModuleId.IsValid() && Module->GetModuleId() == HighlightedModuleId;

		// "Only selected" with nothing selected would blank the viewport and read as a broken tool, so it only
		// filters once the details panel actually has a module open.
		const bool bFilteredOut =
			(Settings.bOnlySelectedModule && HighlightedModuleId.IsValid() && !bHighlighted)
			|| (Settings.bHideDisabledModules && !Module->bEnabled);

		if (!bFilteredOut)
		{
			Builder.BeginModule(Module, Depth, bHighlighted);
			Module->DescribeVisualization(Builder);
			Builder.EndModule();
		}

		// Children are described regardless: a filtered-out parent hides its own shapes, not its subtree's.
		CollectTree(Module->Modules, Depth + 1, HighlightedModuleId, Builder);
	}
}

void FHierarchicalTriggerComponentVisualizer::DrawVisualization(const UActorComponent* Component,
	const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FTriggerVisualizationBuilder Builder(Cast<const UHierarchicalTriggerComponent>(Component));
	if (!Collect(Component, Builder))
	{
		return;
	}

	const FGuid HighlightedModuleId = FHierarchicalTriggerComponentDetails::GetExpandedModuleId(
		Cast<const UHierarchicalTriggerComponent>(Component));

	for (const FTriggerVisualizationPrimitive& Primitive : Builder.GetPrimitives())
	{
		const UHierarchicalTriggerComponentModule* Owner = Primitive.Owner;
		const bool bHighlighted = Owner && HighlightedModuleId.IsValid() && Owner->GetModuleId() == HighlightedModuleId;

		AActor* ClickTarget = Primitive.ClickTarget.Get();
		if (ClickTarget)
		{
			PDI->SetHitProxy(new HTriggerVisualizationProxy(Component, ClickTarget));
		}

		DrawPrimitive(Primitive, bHighlighted, PDI);

		if (ClickTarget)
		{
			PDI->SetHitProxy(nullptr);
		}
	}
}

void FHierarchicalTriggerComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component,
	const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	if (!UModularTriggerEditorSettings::Get().bDrawLabels)
	{
		return;
	}

	// Rebuilt rather than cached from the PDI pass: the two passes have no ordering guarantee, and describing
	// a handful of modules is far cheaper than keeping per-viewport state correct across undo and refreshes.
	FTriggerVisualizationBuilder Builder(Cast<const UHierarchicalTriggerComponent>(Component));
	if (!Collect(Component, Builder))
	{
		return;
	}

	const FGuid HighlightedModuleId = FHierarchicalTriggerComponentDetails::GetExpandedModuleId(
		Cast<const UHierarchicalTriggerComponent>(Component));

	auto IsHighlighted = [&HighlightedModuleId](const UHierarchicalTriggerComponentModule* Owner)
	{
		return Owner && HighlightedModuleId.IsValid() && Owner->GetModuleId() == HighlightedModuleId;
	};

	for (const FTriggerVisualizationPrimitive& Primitive : Builder.GetPrimitives())
	{
		if (Primitive.Label.IsEmpty())
		{
			continue;
		}
		DrawLabel(Primitive.GetLabelAnchor(), Primitive.Label,
			ApplyDimming(Primitive.Color, IsHighlighted(Primitive.Owner)), View, Canvas);
	}

	for (const FTriggerVisualizationText& Text : Builder.GetTexts())
	{
		DrawLabel(Text.Location, Text.Text, ApplyDimming(Text.Color, IsHighlighted(Text.Owner)), View, Canvas);
	}
}

bool FHierarchicalTriggerComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	const HTriggerVisualizationProxy* Proxy = HitProxyCast<HTriggerVisualizationProxy>(VisProxy);
	AActor* TargetActor = Proxy ? Proxy->TargetActor.Get() : nullptr;
	if (!TargetActor || !GEditor)
	{
		return false;
	}

	// Jump to whatever the shape points at. This is the whole reason the visualization is worth having: the
	// reference stops being a name in a details row and becomes something you can click.
	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(TargetActor, /*bInSelected*/ true, /*bNotify*/ true);
	return true;
}
