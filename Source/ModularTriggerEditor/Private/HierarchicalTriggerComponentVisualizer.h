
#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class FTriggerVisualizationBuilder;
class UHierarchicalTriggerComponent;
class UHierarchicalTriggerComponentModule;

/** Hit proxy for a shape a module marked clickable. Picking it selects the actor the shape points at. */
struct HTriggerVisualizationProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()

	HTriggerVisualizationProxy(const UActorComponent* InComponent, AActor* InTargetActor)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
		, TargetActor(InTargetActor)
	{
	}

	TWeakObjectPtr<AActor> TargetActor;
};

/**
 * Draws whatever the modules of a UHierarchicalTriggerComponent describe, for every selected actor.
 *
 * The visualizer owns no knowledge of any particular module: it walks the module tree, asks each module to
 * describe itself into a FTriggerVisualizationBuilder, and renders the result. Everything users can steer —
 * the master switch, labels, dimming, filtering to the module open in the details panel — is applied here,
 * once, rather than in each module.
 *
 * @see UHierarchicalTriggerComponentModule::DescribeVisualization
 * @see UModularTriggerEditorSettings
 */
class FHierarchicalTriggerComponentVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	/** The click selects the actor the shape points at, so leave the component selection alone. */
	virtual bool ShouldAutoSelectElementOnHandleClick() const override { return false; }
	//~ End FComponentVisualizer interface

private:
	/**
	 * Fills the builder from every module that should draw right now.
	 * @return False when there is nothing to draw, including when visualization is switched off entirely.
	 */
	static bool Collect(const UActorComponent* Component, FTriggerVisualizationBuilder& OutBuilder);

	/** Depth-first walk that asks each module in turn to describe itself. */
	static void CollectTree(const TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>& Modules, int32 Depth,
		const FGuid& HighlightedModuleId, FTriggerVisualizationBuilder& Builder);
};
