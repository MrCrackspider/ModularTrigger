
#include "Modules/TriggerComponentModule_Filter.h"

#include "Misc/EnumerateRange.h"
#include "StructUtils/InstancedStruct.h"


bool FFilterModuleEntry_Class::ActorPassesFilter(const AActor* OtherActor) const
{
	for (TSubclassOf<AActor> Class : Classes)
	{
		if (OtherActor->IsA(Class))
		{
			return true;
		}
	}
	return false;
}

bool FFilterModuleEntry_Player::ActorPassesFilter(const AActor* OtherActor) const
{
	const APawn* PlayerPawn = Cast<APawn>(OtherActor);
	
	return PlayerPawn ? PlayerPawn->IsPlayerControlled() : false;
}

bool FFilterModuleEntry_ActorTags::ActorPassesFilter(const AActor* OtherActor) const
{
	const TArray<FName>& ActorTags = OtherActor->Tags;

	for (FName Tag : Tags)
	{
		if (ActorTags.Contains(Tag))
		{
			return true;
		}
	}
	return false;
}

bool FFilterModuleEntry_ExactActors::ActorPassesFilter(const AActor* OtherActor) const
{
	return Targets.Contains(OtherActor);
}

void UTriggerComponentModule_Filter::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	auto ActorPassesFiltration = [this](const AActor* Actor) -> bool
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
				const bool bFilterPassed = FilterEntry(Actor);
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

	if (!ActorPassesFiltration(TriggerExecutionContext.GetOtherActor()))
	{
		TriggerExecutionContext.SetPendingInterruption();
	}
}
