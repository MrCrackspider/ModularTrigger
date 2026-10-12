
#include "Modules/TriggerComponentModule_Filter.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "StructUtils/InstancedStruct.h"

bool FFilterModuleEntry_Class::ContextPassesFilter(const FTriggerExecutionContext& Context) const
{
	const AActor* OtherActor = Context.GetOtherActor();
	for (TSubclassOf<AActor> Class : Classes)
	{
		if (OtherActor->IsA(Class))
		{
			return true;
		}
	}
	return false;
}

bool FFilterModuleEntry_Player::ContextPassesFilter(const FTriggerExecutionContext& Context) const
{
	const APawn* PlayerPawn = Cast<APawn>(Context.GetOtherActor());

	return PlayerPawn ? PlayerPawn->IsPlayerControlled() : false;
}

bool FFilterModuleEntry_ActorTags::ContextPassesFilter(const FTriggerExecutionContext& Context) const
{
	const TArray<FName>& ActorTags = Context.GetOtherActor()->Tags;

	for (FName Tag : Tags)
	{
		if (ActorTags.Contains(Tag))
		{
			return true;
		}
	}
	return false;
}

bool FFilterModuleEntry_ExactActors::ContextPassesFilter(const FTriggerExecutionContext& Context) const
{
	return Targets.Contains(Context.GetOtherActor());
}

bool FFilterModuleEntry_EndOverlap::ContextPassesFilter(const FTriggerExecutionContext& Context) const
{
	return Context.IsEndOverlap();
}

bool UTriggerComponentModule_Filter::ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const
{
	bool bAnyEntryEvaluated = false;

	for (const TInstancedStruct<FFilterModuleEntry>& Filter : Filters)
	{
		// An unset row is an authoring mistake rather than a rejection — the base entry answers no to
		// everything, so counting it would have the filter reject every event.
		if (!Filter.IsValid())
		{
			continue;
		}

		bAnyEntryEvaluated = true;

		const bool bEntryPassed = Filter.Get<FFilterModuleEntry>()(TriggerExecutionContext);
		if (bEntryPassed != bAllFiltersShouldBePassed)
		{
			// A failure settles an all-of filter and a pass settles an any-of one.
			return bEntryPassed;
		}
	}

	// Every entry agreed with the mode, so the mode is the answer. A filter with nothing to say passes.
	return !bAnyEntryEvaluated || bAllFiltersShouldBePassed;
}
