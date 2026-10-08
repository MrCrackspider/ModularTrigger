
#include "Modules/TriggerComponentModule_Filter.h"

#include "Misc/EnumerateRange.h"
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

void UTriggerComponentModule_Filter::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	auto ContextPassesFiltration = [this](const FTriggerExecutionContext& Context) -> bool
	{
		if (Filters.IsEmpty())
		{
			return true;
		}
		for (TEnumerateRef<TInstancedStruct<FFilterModuleEntry>> Filter : EnumerateRange(Filters))
		{
			if (Filter->IsValid())
			{
				const FFilterModuleEntry& FilterEntry = Filter->Get<FFilterModuleEntry>();
				const bool bFilterPassed = FilterEntry(Context);
				if (bAllFiltersShouldBePassed ^ bFilterPassed)
				{
					return bFilterPassed;
				}
				if (Filter.GetIndex() == Filters.Num() - 1)
				{
					return bFilterPassed;
				}
			}
		}
		return false;
	};

	if (!ContextPassesFiltration(TriggerExecutionContext))
	{
		TriggerExecutionContext.SetPendingInterruption();
	}
}
