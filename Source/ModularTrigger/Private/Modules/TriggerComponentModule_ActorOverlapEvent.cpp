
#include "Modules/TriggerComponentModule_ActorOverlapEvent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HierarchicalTriggerComponent.h"
#include "TimerManager.h"

void UTriggerComponentModule_ActorOverlapEvent::OnBeginPlay()
{
	OverlappedActors.Reset();

	if (ActivationDelay > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ActivationTimerHandle, this, &ThisClass::ActivateOverlap, ActivationDelay, false);
			return;
		}
	}

	ActivateOverlap();
}

void UTriggerComponentModule_ActorOverlapEvent::OnEndPlay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivationTimerHandle);
	}

	SetOverlapEnabled(false);
	OverlappedActors.Reset();
}

void UTriggerComponentModule_ActorOverlapEvent::ActivateOverlap()
{
	SetOverlapEnabled(true);

	if (bUpdateOverlapsOnBeginPlay)
	{
		for (UPrimitiveComponent* PrimitiveComponent : BoundComponents)
		{
			if (IsValid(PrimitiveComponent) && PrimitiveComponent->IsRegistered())
			{
				PrimitiveComponent->UpdateOverlaps();
			}
		}
	}

	CacheOrHandleExistingOverlaps(bUpdateOverlapsOnBeginPlay);
}

void UTriggerComponentModule_ActorOverlapEvent::SetOverlapEnabled(bool bNewEnabled)
{
	for (UPrimitiveComponent* PrimitiveComponent : BoundComponents)
	{
		if (IsValid(PrimitiveComponent))
		{
			PrimitiveComponent->OnComponentBeginOverlap.RemoveDynamic(
				this, &ThisClass::ReceiveComponentBeginOverlap);
			PrimitiveComponent->OnComponentEndOverlap.RemoveDynamic(
				this, &ThisClass::ReceiveComponentEndOverlap);
		}
	}
	BoundComponents.Reset();

	if (!bNewEnabled)
	{
		return;
	}

	UHierarchicalTriggerComponent* OwningComponent = GetOwningComponent();
	AActor* OwnerActor = OwningComponent ? OwningComponent->GetOwner() : nullptr;
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->OnComponentBeginOverlap.AddUniqueDynamic(
			this, &ThisClass::ReceiveComponentBeginOverlap);
		PrimitiveComponent->OnComponentEndOverlap.AddUniqueDynamic(
			this, &ThisClass::ReceiveComponentEndOverlap);
		BoundComponents.Add(PrimitiveComponent);
	}
}

void UTriggerComponentModule_ActorOverlapEvent::CacheOrHandleExistingOverlaps(bool bPropagateEvents)
{
	for (UPrimitiveComponent* PrimitiveComponent : BoundComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		TArray<UPrimitiveComponent*> OtherComponents;
		PrimitiveComponent->GetOverlappingComponents(OtherComponents);
		for (UPrimitiveComponent* OtherComponent : OtherComponents)
		{
			if (!IsValid(OtherComponent))
			{
				continue;
			}

			if (bPropagateEvents)
			{
				HandleOverlap(PrimitiveComponent, OtherComponent, false);
			}
			else if (AActor* OtherActor = OtherComponent->GetOwner())
			{
				OverlappedActors.Add(OtherActor);
			}
		}
	}
}

void UTriggerComponentModule_ActorOverlapEvent::ReceiveComponentBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* /*OtherActor*/, UPrimitiveComponent* OtherComponent,
	int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& SweepResult)
{
	HandleOverlap(OverlappedComponent, OtherComponent, false, &SweepResult);
}

void UTriggerComponentModule_ActorOverlapEvent::ReceiveComponentEndOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* /*OtherActor*/, UPrimitiveComponent* OtherComponent,
	int32 /*OtherBodyIndex*/)
{
	HandleOverlap(OverlappedComponent, OtherComponent, true);
}

void UTriggerComponentModule_ActorOverlapEvent::HandleOverlap(
	UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComponent,
	bool bEndOverlap, const FHitResult* SweepResult)
{
	if (!IsValid(OverlappedComponent) || !IsValid(OtherComponent))
	{
		return;
	}

	AActor* OtherActor = OtherComponent->GetOwner();
	AActor* OwnerActor = OverlappedComponent->GetOwner();
	if (!IsValid(OtherActor) || !IsValid(OwnerActor) || OtherActor == OwnerActor)
	{
		return;
	}

	const bool bAlreadyOverlapsActor = OverlappedActors.Contains(OtherActor);
	const bool bOverlapsAnyActor = !OverlappedActors.IsEmpty();

	if (bEndOverlap)
	{
		if (OwnerActor->IsOverlappingActor(OtherActor))
		{
			return;
		}

		OverlappedActors.Remove(OtherActor);
		if (bOverlapOncePerAnyActor && !OverlappedActors.IsEmpty())
		{
			return;
		}
	}
	else
	{
		if (bAlreadyOverlapsActor)
		{
			return;
		}

		OverlappedActors.Add(OtherActor);
		if (bOverlapOncePerAnyActor && bOverlapsAnyActor)
		{
			return;
		}
	}

	UHierarchicalTriggerComponent* OwningComponent = GetOwningComponent();
	if (!OwningComponent)
	{
		return;
	}

	FTriggerExecutionContext TriggerExecutionContext{
		OwningComponent, OverlappedComponent, OtherComponent, bEndOverlap, SweepResult};
	DispatchGeneratedEvent(TriggerExecutionContext);
}
