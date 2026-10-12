
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR

class AActor;
class FTriggerVisualizationBuilder;
class UHierarchicalTriggerComponent;
class UHierarchicalTriggerComponentModule;

/** The shapes a module can ask the editor visualizer to draw for it. */
enum class ETriggerVisualizationShape : uint8
{
	Line,
	Arrow,
	Sphere,
	Box,
	Circle,
	Point,
};

/**
 * One world-space shape queued by a module.
 *
 * Deliberately plain data: a module says what it wants drawn, and the editor-side visualizer decides how,
 * where and whether to draw it. That keeps modules free of UnrealEd, of hit proxies and of view state, and
 * it lets the visualizer apply user settings uniformly instead of every module reimplementing them.
 * Fields that do not apply to a shape kind are ignored.
 */
struct FTriggerVisualizationPrimitive
{
	ETriggerVisualizationShape Shape = ETriggerVisualizationShape::Line;

	/** Placement. Line and Arrow take their start point from the translation. */
	FTransform Transform = FTransform::Identity;

	/** End point of a Line or an Arrow. */
	FVector EndLocation = FVector::ZeroVector;

	/** Radius of a Sphere or a Circle; screen-space size of a Point. */
	double Radius = 0.0;

	/** Half-extent of a Box. */
	FVector Extent = FVector::ZeroVector;

	FLinearColor Color = FLinearColor::White;

	/** 0 draws the renderer's default single-pixel line. */
	float Thickness = 0.0f;

	/** Optional text drawn at this shape's anchor during the HUD pass. */
	FText Label;

	/** When set, the shape becomes clickable in the viewport and picking it selects this actor. */
	TWeakObjectPtr<AActor> ClickTarget;

	/** The module that queued this shape. Filled in by the builder. */
	const UHierarchicalTriggerComponentModule* Owner = nullptr;

	/** Where this shape's label is drawn: the midpoint of a line, otherwise the shape's origin. */
	MODULARTRIGGER_API FVector GetLabelAnchor() const;
};

/** A world-anchored string with no shape attached, for modules that only want to say something. */
struct FTriggerVisualizationText
{
	FVector Location = FVector::ZeroVector;
	FText Text;
	FLinearColor Color = FLinearColor::White;
	const UHierarchicalTriggerComponentModule* Owner = nullptr;
};

/**
 * Fluent handle to a shape already queued in a builder.
 *
 * Holds an index rather than a reference, so it stays valid across further Add calls even when the
 * builder's array reallocates.
 */
class MODULARTRIGGER_API FTriggerVisualizationHandle
{
public:
	FTriggerVisualizationHandle(FTriggerVisualizationBuilder& InBuilder, int32 InIndex);

	FTriggerVisualizationHandle& SetColor(const FLinearColor& InColor);
	FTriggerVisualizationHandle& SetThickness(float InThickness);
	FTriggerVisualizationHandle& SetLabel(const FText& InLabel);
	/** Makes this shape pickable: clicking it in the viewport selects InActor. */
	FTriggerVisualizationHandle& SetClickTarget(AActor* InActor);

private:
	FTriggerVisualizationPrimitive& Get() const;

	FTriggerVisualizationBuilder& Builder;
	int32 Index = INDEX_NONE;
};

/**
 * Collects what the modules of one trigger component want drawn in the level viewport.
 *
 * The visualizer walks the module tree, scopes the builder to each module in turn with BeginModule, and
 * calls UHierarchicalTriggerComponentModule::DescribeVisualization. Modules only ever see the Add* side.
 *
 * Per-frame scratch: built, filled and consumed inside one draw call, which is why it can hold the component
 * and modules as raw pointers. Do not keep one alive across frames.
 */
class MODULARTRIGGER_API FTriggerVisualizationBuilder
{
public:
	explicit FTriggerVisualizationBuilder(const UHierarchicalTriggerComponent* InComponent);

	/** World location of the component's owning actor: the natural start point for a line to a reference. */
	FVector GetOrigin() const { return Origin; }
	const UHierarchicalTriggerComponent* GetComponent() const { return Component; }

	/** The module currently being asked to describe itself. */
	const UHierarchicalTriggerComponentModule* GetModule() const { return CurrentModule; }

	/** True when the module being asked is the one expanded in the details panel. Draw it louder. */
	bool IsModuleHighlighted() const { return bCurrentModuleHighlighted; }

	/** Depth of the current module in the tree; 0 for a root module. Useful for staggering labels. */
	int32 GetModuleDepth() const { return CurrentDepth; }

	FTriggerVisualizationHandle AddLine(const FVector& Start, const FVector& End);
	FTriggerVisualizationHandle AddArrow(const FVector& Start, const FVector& End);
	FTriggerVisualizationHandle AddSphere(const FVector& Center, double InRadius);
	FTriggerVisualizationHandle AddBox(const FTransform& InTransform, const FVector& InExtent);
	FTriggerVisualizationHandle AddCircle(const FTransform& InTransform, double InRadius);
	FTriggerVisualizationHandle AddPoint(const FVector& Location, double ScreenSize = 6.0);
	void AddText(const FVector& Location, const FText& Text, const FLinearColor& Color = FLinearColor::White);

	const TArray<FTriggerVisualizationPrimitive>& GetPrimitives() const { return Primitives; }
	const TArray<FTriggerVisualizationText>& GetTexts() const { return Texts; }
	bool IsEmpty() const { return Primitives.Num() == 0 && Texts.Num() == 0; }

	/** Scopes everything added after it to one module. Called by the visualizer, not by modules. */
	void BeginModule(const UHierarchicalTriggerComponentModule* InModule, int32 InDepth, bool bInHighlighted);
	void EndModule();

private:
	friend class FTriggerVisualizationHandle;

	FTriggerVisualizationHandle AddPrimitive(FTriggerVisualizationPrimitive&& Primitive);

	const UHierarchicalTriggerComponent* Component = nullptr;
	const UHierarchicalTriggerComponentModule* CurrentModule = nullptr;
	FVector Origin = FVector::ZeroVector;
	int32 CurrentDepth = 0;
	bool bCurrentModuleHighlighted = false;
	TArray<FTriggerVisualizationPrimitive> Primitives;
	TArray<FTriggerVisualizationText> Texts;
};

#endif // WITH_EDITOR
