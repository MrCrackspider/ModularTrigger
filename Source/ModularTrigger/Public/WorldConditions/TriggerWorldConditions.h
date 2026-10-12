
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "WorldConditionBase.h"
#include "TriggerWorldConditions.generated.h"

class AActor;

/**
 * Base struct for conditions that only make sense against a trigger event.
 *
 * It is what keeps them out of the condition pickers of unrelated systems: a schema has to opt in to this
 * base to offer them. A condition that needs nothing but an actor or a subsystem is better written against
 * FWorldConditionCommonActorBase / FWorldConditionCommonBase, which this plugin's schema also accepts.
 */
USTRUCT(meta = (Hidden))
struct FTriggerWorldConditionBase : public FWorldConditionBase
{
	GENERATED_BODY()
};

/**
 * Base struct for the conditions below, which all test one actor drawn from the context.
 *
 * Which actor that is comes from the context data reference, so the same condition can ask about the actor
 * that entered the trigger or about the trigger's own owner.
 */
USTRUCT(meta = (Hidden))
struct FTriggerWorldConditionActorBase : public FTriggerWorldConditionBase
{
	GENERATED_BODY()

	/** Actor to test. Defaults to the actor that entered or left the trigger. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (BaseClass = "/Script/Engine.Actor"))
	FWorldConditionContextDataRef ActorRef = FWorldConditionContextDataRef(TEXT("OtherActor"));

protected:

	//~ Begin FWorldConditionBase interface
	virtual bool Initialize(const UWorldConditionSchema& Schema) override;
	//~ End FWorldConditionBase interface

	/** @return The referenced actor, or nullptr when the reference is unset or holds nothing this event. */
	const AActor* GetActor(const FWorldConditionContext& Context) const;

	/** Result of this condition, tagged cacheable only when the actor it reads cannot change under it. */
	FWorldConditionResult MakeResult(bool bIsTrue) const;
};

/** Passes when the actor is an instance of any of the listed classes. */
USTRUCT(DisplayName = "Actor Is Any Of Classes")
struct FTriggerWorldCondition_ActorClass : public FTriggerWorldConditionActorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Default")
	TArray<TSubclassOf<AActor>> Classes;

protected:

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;
};

/** Passes when the actor is a pawn under the control of a player. */
USTRUCT(DisplayName = "Actor Is Player Controlled")
struct FTriggerWorldCondition_PlayerControlled : public FTriggerWorldConditionActorBase
{
	GENERATED_BODY()

protected:

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;
};

/** Passes when the actor carries any of the listed actor tags. */
USTRUCT(DisplayName = "Actor Has Any Of Tags")
struct FTriggerWorldCondition_ActorTags : public FTriggerWorldConditionActorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Default")
	TArray<FName> Tags;

protected:

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;
};

/**
 * Passes when the actor is one of the actors pointed at directly.
 *
 * Hard references, so every actor listed here is loaded with the trigger and has to live in a level that is
 * loaded alongside it.
 */
USTRUCT(DisplayName = "Actor Is Any Of References")
struct FTriggerWorldCondition_ActorReference : public FTriggerWorldConditionActorBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Default")
	TArray<TObjectPtr<AActor>> Actors;

protected:

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;
};

/**
 * Passes when the event is an actor leaving the trigger rather than entering it.
 *
 * Inverted — "NOT" in the expression row — it is how a filter lets begin overlaps through only.
 */
USTRUCT(DisplayName = "Is End Overlap")
struct FTriggerWorldCondition_EndOverlap : public FTriggerWorldConditionBase
{
	GENERATED_BODY()

protected:

#if WITH_EDITOR
	virtual FText GetDescription() const override;
#endif
	virtual bool Initialize(const UWorldConditionSchema& Schema) override;
	virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;

	/** Resolved from the schema rather than authored: there is only one event to read. */
	FWorldConditionContextDataRef EventRef;
};
