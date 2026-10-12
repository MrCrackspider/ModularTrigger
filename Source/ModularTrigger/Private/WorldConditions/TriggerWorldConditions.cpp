
#include "WorldConditions/TriggerWorldConditions.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "WorldConditionContext.h"
#include "WorldConditions/TriggerWorldConditionSchema.h"

#define LOCTEXT_NAMESPACE "TriggerWorldConditions"

//
// FTriggerWorldConditionActorBase
//

bool FTriggerWorldConditionActorBase::Initialize(const UWorldConditionSchema& Schema)
{
	if (!Schema.ResolveContextDataRef<AActor>(ActorRef))
	{
		// Returning false fails the whole query to link, which is what should happen: a condition that cannot
		// tell what it is testing must not be quietly skipped.
		return false;
	}

	// An actor supplied per event may be a different one on every call, so nothing read from it can be reused.
	bCanCacheResult = Schema.GetContextDataTypeByRef(ActorRef) == EWorldConditionContextDataType::Persistent;

	return true;
}

const AActor* FTriggerWorldConditionActorBase::GetActor(const FWorldConditionContext& Context) const
{
	if (!ActorRef.IsValid())
	{
		return nullptr;
	}

	return Context.GetContextDataPtr<AActor>(ActorRef);
}

FWorldConditionResult FTriggerWorldConditionActorBase::MakeResult(const bool bIsTrue) const
{
	return FWorldConditionResult(
		bIsTrue ? EWorldConditionResultValue::IsTrue : EWorldConditionResultValue::IsFalse,
		bCanCacheResult);
}

//
// FTriggerWorldCondition_ActorClass
//

#if WITH_EDITOR
FText FTriggerWorldCondition_ActorClass::GetDescription() const
{
	return FText::Format(LOCTEXT("ActorClassDescription", "{0} is any of {1} classes"),
		FText::FromName(ActorRef.GetName()), Classes.Num());
}
#endif

FWorldConditionResult FTriggerWorldCondition_ActorClass::IsTrue(const FWorldConditionContext& Context) const
{
	const AActor* Actor = GetActor(Context);
	if (!Actor)
	{
		return MakeResult(false);
	}

	for (const TSubclassOf<AActor>& Class : Classes)
	{
		if (Actor->IsA(Class))
		{
			return MakeResult(true);
		}
	}

	return MakeResult(false);
}

//
// FTriggerWorldCondition_PlayerControlled
//

#if WITH_EDITOR
FText FTriggerWorldCondition_PlayerControlled::GetDescription() const
{
	return FText::Format(LOCTEXT("PlayerControlledDescription", "{0} is player controlled"),
		FText::FromName(ActorRef.GetName()));
}
#endif

FWorldConditionResult FTriggerWorldCondition_PlayerControlled::IsTrue(const FWorldConditionContext& Context) const
{
	const APawn* Pawn = Cast<APawn>(GetActor(Context));

	return MakeResult(Pawn && Pawn->IsPlayerControlled());
}

//
// FTriggerWorldCondition_ActorTags
//

#if WITH_EDITOR
FText FTriggerWorldCondition_ActorTags::GetDescription() const
{
	return FText::Format(LOCTEXT("ActorTagsDescription", "{0} has any of {1} tags"),
		FText::FromName(ActorRef.GetName()), Tags.Num());
}
#endif

FWorldConditionResult FTriggerWorldCondition_ActorTags::IsTrue(const FWorldConditionContext& Context) const
{
	const AActor* Actor = GetActor(Context);
	if (!Actor)
	{
		return MakeResult(false);
	}

	for (const FName Tag : Tags)
	{
		if (Actor->Tags.Contains(Tag))
		{
			return MakeResult(true);
		}
	}

	return MakeResult(false);
}

//
// FTriggerWorldCondition_ActorReference
//

#if WITH_EDITOR
FText FTriggerWorldCondition_ActorReference::GetDescription() const
{
	return FText::Format(LOCTEXT("ActorReferenceDescription", "{0} is any of {1} actors"),
		FText::FromName(ActorRef.GetName()), Actors.Num());
}
#endif

FWorldConditionResult FTriggerWorldCondition_ActorReference::IsTrue(const FWorldConditionContext& Context) const
{
	const AActor* Actor = GetActor(Context);

	return MakeResult(Actor != nullptr && Actors.Contains(Actor));
}

//
// FTriggerWorldCondition_EndOverlap
//

#if WITH_EDITOR
FText FTriggerWorldCondition_EndOverlap::GetDescription() const
{
	return LOCTEXT("EndOverlapDescription", "Is end overlap");
}
#endif

bool FTriggerWorldCondition_EndOverlap::Initialize(const UWorldConditionSchema& Schema)
{
	const UTriggerWorldConditionSchema* TriggerSchema = Cast<UTriggerWorldConditionSchema>(&Schema);
	if (!TriggerSchema)
	{
		return false;
	}

	EventRef = TriggerSchema->GetEventRef();

	// The event is what changes between two dispatches, so this is never the same answer twice by luck.
	bCanCacheResult = false;

	return EventRef.IsValid();
}

FWorldConditionResult FTriggerWorldCondition_EndOverlap::IsTrue(const FWorldConditionContext& Context) const
{
	const FTriggerWorldConditionEvent* Event = EventRef.IsValid()
		? Context.GetContextDataPtr<FTriggerWorldConditionEvent>(EventRef)
		: nullptr;

	// No event means this is not being evaluated against a dispatch at all, which cannot be an end overlap.
	return FWorldConditionResult(
		Event && Event->bEndOverlap ? EWorldConditionResultValue::IsTrue : EWorldConditionResultValue::IsFalse,
		/*bCanBeCached*/ false);
}

#undef LOCTEXT_NAMESPACE
