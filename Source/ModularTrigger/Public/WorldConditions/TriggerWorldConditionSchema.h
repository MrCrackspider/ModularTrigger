
#pragma once

#include "CoreMinimal.h"
#include "WorldConditionSchema.h"
#include "TriggerWorldConditionSchema.generated.h"

class UHierarchicalTriggerComponent;
struct FTriggerExecutionContext;
struct FWorldConditionContextData;

/**
 * The facts about an overlap event that are not objects, handed to conditions as a single context data entry.
 *
 * Context data is referenced by type and name, so everything a condition can read has to be either an object
 * or a struct; this is the struct that carries the rest of the event.
 */
USTRUCT()
struct FTriggerWorldConditionEvent
{
	GENERATED_BODY()

	/** True when the event came from leaving the trigger rather than entering it. */
	UPROPERTY()
	bool bEndOverlap = false;
};

/**
 * Describes what a world condition can read when it is evaluated against a trigger overlap event, and which
 * conditions may be authored in that context.
 *
 * Derive from this to make more data available to conditions — add the descriptors in the constructor and
 * extend SetContextData to fill them — and point a filter module's Schema Class at the subclass. Conditions
 * written against the base schema keep working, because a reference resolves by name and type.
 */
UCLASS(DisplayName = "Trigger")
class MODULARTRIGGER_API UTriggerWorldConditionSchema : public UWorldConditionSchema
{
	GENERATED_BODY()

public:

	explicit UTriggerWorldConditionSchema(const FObjectInitializer& ObjectInitializer);

	/** Component dispatching the event. Persistent: the same component for as long as the query is active. */
	FWorldConditionContextDataRef GetTriggerComponentRef() const { return TriggerComponentRef; }

	/** Actor owning the trigger component. Persistent. */
	FWorldConditionContextDataRef GetTriggerActorRef() const { return TriggerActorRef; }

	/** Actor that entered or left the trigger. Dynamic: a different actor on every event. */
	FWorldConditionContextDataRef GetOtherActorRef() const { return OtherActorRef; }

	/** The overlapping component of that actor. Dynamic. */
	FWorldConditionContextDataRef GetOtherComponentRef() const { return OtherComponentRef; }

	/** Everything about the event that is not an object. Dynamic. @see FTriggerWorldConditionEvent */
	FWorldConditionContextDataRef GetEventRef() const { return EventRef; }

	/**
	 * Fills the persistent context data, which is all that is available before an event exists — the data a
	 * condition may read in Activate and Deactivate.
	 */
	virtual void SetPersistentContextData(FWorldConditionContextData& ContextData,
		const UHierarchicalTriggerComponent& TriggerComponent) const;

	/**
	 * Fills every context data entry this schema declares, for evaluating conditions against one event.
	 *
	 * @param ContextData Container to fill. It stores views, not copies.
	 * @param ExecutionContext Event being dispatched.
	 * @param Event Storage for the non-object facts of that event. It must outlive ContextData, so it belongs
	 *              on the caller's stack rather than inside this call.
	 */
	virtual void SetContextData(FWorldConditionContextData& ContextData,
		const FTriggerExecutionContext& ExecutionContext, const FTriggerWorldConditionEvent& Event) const;

protected:

	//~ Begin UWorldConditionSchema interface
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	//~ End UWorldConditionSchema interface

private:

	FWorldConditionContextDataRef TriggerComponentRef;
	FWorldConditionContextDataRef TriggerActorRef;
	FWorldConditionContextDataRef OtherActorRef;
	FWorldConditionContextDataRef OtherComponentRef;
	FWorldConditionContextDataRef EventRef;
};
