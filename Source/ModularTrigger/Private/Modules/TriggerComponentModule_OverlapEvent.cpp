
#include "Modules/TriggerComponentModule_OverlapEvent.h"

#include "HierarchicalTriggerComponent.h"


void UTriggerComponentModule_OverlapEvent::OnBeginPlay()
{
	SetOverlapEnabled(true);
	if (bUpdateOverlapsOnBeginPlay)
	{
		if (UPrimitiveComponent* Comp = GetOverlapComponent())
		{
			TArray<UPrimitiveComponent*> OverlappingComponents;
			Comp->UpdateOverlaps();
			Comp->GetOverlappingComponents(OverlappingComponents);
			for (UPrimitiveComponent* OverlappedComponent : OverlappingComponents)
			{
				HandleOverlap(Comp, OverlappedComponent, false);
			}
		}
	}
}

void UTriggerComponentModule_OverlapEvent::OnEndPlay()
{
	SetOverlapEnabled(false);
}

UPrimitiveComponent* UTriggerComponentModule_OverlapEvent::GetOverlapComponent() const
{
	if (UHierarchicalTriggerComponent* Owner = GetOwningComponent())
	{
		return Cast<UPrimitiveComponent>(TargetOverlapComponent.GetComponent(Owner->GetOwner()));
	}
	return nullptr;
}

void UTriggerComponentModule_OverlapEvent::SetOverlapComponent(UPrimitiveComponent* NewOverlapComponent)
{
	SetOverlapEnabled(false);
	
	TargetOverlapComponent.OverrideComponent = NewOverlapComponent;

	if (NewOverlapComponent)
	{
		TargetOverlapComponent.OtherActor = NewOverlapComponent->GetOwner();
		TargetOverlapComponent.PathToComponent = NewOverlapComponent->GetPathName(NewOverlapComponent->GetOwner());
	}
	else
	{
		TargetOverlapComponent.OtherActor = nullptr;
		TargetOverlapComponent.PathToComponent.Reset();
	}

	SetOverlapEnabled(true);
}

void UTriggerComponentModule_OverlapEvent::SetOverlapEnabled(bool NewEnabled)
{
	if (UPrimitiveComponent* Comp = GetOverlapComponent())
	{
		if (NewEnabled)
		{
			Comp->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::ReceiveComponentBeginOverlap);
			Comp->OnComponentEndOverlap.AddDynamic(this, &ThisClass::ReceiveComponentEndOverlap);
		}
		else
		{
			Comp->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::ReceiveComponentBeginOverlap);
			Comp->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::ReceiveComponentEndOverlap);
		}
	}
}

void UTriggerComponentModule_OverlapEvent::ReceiveComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandleOverlap(OverlappedComponent, OtherComp, false, &SweepResult);
}

void UTriggerComponentModule_OverlapEvent::ReceiveComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	HandleOverlap(OverlappedComponent, OtherComp, true);
}

void UTriggerComponentModule_OverlapEvent::HandleOverlap(UPrimitiveComponent* OverlappedComponent,
	UPrimitiveComponent* OtherComp, bool bEndOverlap, const FHitResult* SweepResult)
{
	AActor* OtherActor = OtherComp->GetOwner();
	const bool bAlreadyOverlapsActor = OverlappedActors.Contains(OtherActor);
	const bool bEndOverlappingActor = !OverlappedComponent->IsOverlappingActor(OtherActor);
	const bool bOverlapsAnyActor = !OverlappedActors.IsEmpty();
	if (bEndOverlap)
	{
		if (bEndOverlappingActor)
		{
			OverlappedActors.Remove(OtherActor);
		}
		if (bOverlapOncePerAnyActor && !OverlappedActors.IsEmpty())
		{
			return;
		}
	}
	else
	{
		if (!bAlreadyOverlapsActor)
		{
			OverlappedActors.Add(OtherActor);
		}
		if (bOverlapOncePerAnyActor && bOverlapsAnyActor)
		{
			return;
		}
	}
	
	if (bOverlapOncePerActor && ((!bEndOverlap && bAlreadyOverlapsActor) || (bEndOverlap && !bEndOverlappingActor)))
	{
		return;
	}
	
	if (bChildrenEnabled)
	{
		UHierarchicalTriggerComponent* OwningComp = GetOwningComponent();
		// When is it possible?
		if (!OwningComp || !OverlappedComponent || !OtherComp)
		{
			return;
		}
		FTriggerExecutionContext TriggerExecutionContext{GetOwningComponent(), OverlappedComponent, OtherComp, bEndOverlap, SweepResult};
		PropagateEvent(TriggerExecutionContext);
		// TODO: wrap with ReceiveOverlapEvent?
		if (bTriggerOnce)
		{
			bEnabled = false;
			bChildrenEnabled = false;
		}
	}
}
