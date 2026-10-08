
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "HierarchicalTriggerComponent.h"
#include "HierarchicalTriggerModuleUtils.h"


FTriggerExecutionContext::FTriggerExecutionContext(
	TNotNull<UHierarchicalTriggerComponent*> InOwningTriggerComponent, TNotNull<UPrimitiveComponent*> OverlappedComponent, 
	TNotNull<UPrimitiveComponent*> OtherComponent, bool bEndOverlap, const FHitResult* InSweepResult) : 
	OwningTriggerComponent(InOwningTriggerComponent), OverlappedComponent(OverlappedComponent), 
	OtherComponent(OtherComponent), SweepResult(InSweepResult), bEndOverlap(bEndOverlap) 
{
}

void FTriggerExecutionContext::SetPendingInterruption()
{
	bPendingInterruption = true;
}

void FTriggerExecutionContext::AddExecutionData(FConstStructView InData)
{
	bool bFound = false;
	for (FStructView Data : ExecutionData)
	{
		if (Data.GetScriptStruct() == InData.GetScriptStruct())
		{
			Data.GetScriptStruct()->CopyScriptStruct(Data.GetMemory(), InData.GetMemory());
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		ExecutionData.Append({ InData });
	}
}

UHierarchicalTriggerComponent* UHierarchicalTriggerComponentModule::GetOwningComponent() const
{
	return GetTypedOuter<UHierarchicalTriggerComponent>();
}

UWorld* UHierarchicalTriggerComponentModule::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponentModule::GetOuterModule() const
{
	return GetTypedOuter<UHierarchicalTriggerComponentModule>();
}

void UHierarchicalTriggerComponentModule::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsSaveGame())
	{
		FHierarchicalTriggerModuleUtils::SerializeSaveGame(Modules, Ar);
	}
}

UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponentModule::FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth) const
{
	return FHierarchicalTriggerModuleUtils::FindByClass(Modules, Class, MaxSearchDepth);
}

UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponentModule::FindModuleByGuid(const FGuid& InModuleId) const
{
	return FHierarchicalTriggerModuleUtils::FindByGuid(Modules, InModuleId);
}

void UHierarchicalTriggerComponentModule::PropagateEvent(FTriggerExecutionContext& TriggerExecutionContext)
{
	for (TObjectPtr<UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveTriggerEvent(TriggerExecutionContext);
		}
	}
}

void UHierarchicalTriggerComponentModule::ReceiveBeginPlay()
{
	FHierarchicalTriggerModuleUtils::ReceiveBeginPlay(Modules);
	OnBeginPlay();
}

void UHierarchicalTriggerComponentModule::ReceiveEndPlay()
{
	FHierarchicalTriggerModuleUtils::ReceiveEndPlay(Modules);
	OnEndPlay();
}

void UHierarchicalTriggerComponentModule::ReceiveTriggerEvent(FTriggerExecutionContext& TriggerExecutionContext)
{
	if (bEnabled)
	{
		OnTriggerEventReceived(TriggerExecutionContext);
	}
	
	if (bChildrenEnabled && !TriggerExecutionContext.IsPendingInterruption())
	{
		PropagateEvent(TriggerExecutionContext);
	}
	
	if (bTriggerOnce)
	{
		bEnabled = false;
		bChildrenEnabled = false;
	}
}
