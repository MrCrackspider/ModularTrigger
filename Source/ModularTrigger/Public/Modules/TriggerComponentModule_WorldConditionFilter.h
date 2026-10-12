
#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "WorldConditionQuery.h"
#include "TriggerComponentModule_WorldConditionFilter.generated.h"

class UTriggerWorldConditionSchema;

/**
 * Keeps the event from reaching its children unless a world condition query is satisfied. Guards this
 * module's subtree only, so the modules it is meant to gate belong underneath it.
 *
 * World Conditions is the engine's own framework for authoring this kind of expression: the rows combine
 * with AND / OR, nest with parentheses, can be individually inverted, and conditions whose answer cannot
 * change between events are evaluated once and cached. What can be authored, and what a condition is able
 * to read, comes from the schema.
 *
 * @see UTriggerWorldConditionSchema for the data conditions can read, and for extending it.
 * @see UTriggerComponentModule_Filter for the hand-rolled predecessor this replaces.
 */
UCLASS(DisplayName = "World Condition Filter")
class MODULARTRIGGER_API UTriggerComponentModule_WorldConditionFilter : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()

public:

	UTriggerComponentModule_WorldConditionFilter();

	/** Expression the event has to satisfy. An empty query passes everything. */
	UPROPERTY(EditAnywhere, Category = "Trigger Module")
	FWorldConditionQueryDefinition Condition;

	/**
	 * Schema deciding which conditions can be authored above and what they can read.
	 *
	 * Changing it re-links the query, which unresolves any condition that referenced context data the new
	 * schema does not declare — so change it before authoring, not after.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Trigger Module")
	TSubclassOf<UTriggerWorldConditionSchema> SchemaClass;

	//~ Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

protected:

	//~ Begin UHierarchicalTriggerComponentModule interface
	virtual void OnBeginPlay() override;
	virtual void OnEndPlay() override;
	virtual bool ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const override;
	//~ End UHierarchicalTriggerComponentModule interface

private:

	/** @return The schema the query was linked against, which is the one its conditions resolved their data from. */
	const UTriggerWorldConditionSchema* GetSchema() const;

	/** Pushes SchemaClass into the query and re-links it. */
	void ApplySchemaClass();

	/**
	 * Per-instance state of the query: cached results, and whatever the conditions allocated when they were
	 * activated. Mutable because evaluating a query updates its cache, and the gate it answers is const.
	 *
	 * An uninitialized state answers false to everything, which is deliberate: it is the state left behind by
	 * a query that failed to link or to activate, and a filter that cannot evaluate must not let events past.
	 */
	UPROPERTY(Transient)
	mutable FWorldConditionQueryState ConditionState;
};
