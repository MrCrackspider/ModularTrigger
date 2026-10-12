
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "Components/PrimitiveComponent.h"
#include "HierarchicalTriggerComponent.h"
#include "HierarchicalTriggerModuleUtils.h"


FTriggerExecutionContext::FTriggerExecutionContext(
	TNotNull<UHierarchicalTriggerComponent*> InOwningTriggerComponent, TNotNull<UPrimitiveComponent*> OverlappedComponent, 
	TNotNull<UPrimitiveComponent*> OtherComponent, bool bEndOverlap, const FHitResult* InSweepResult) : 
	OwningTriggerComponent(InOwningTriggerComponent), OverlappedComponent(OverlappedComponent), 
	OtherComponent(OtherComponent), SweepResult(InSweepResult), bEndOverlap(bEndOverlap) 
{
}

TNotNull<const AActor*> FTriggerExecutionContext::GetOtherActor() const
{
	return GetOtherComponent()->GetOwner();
}

void FTriggerExecutionContext::AddExecutionData(FConstStructView InData)
{
	if (!InData.IsValid())
	{
		return;
	}

	// Only the current scope is searched: finding the type in an outer scope has to append a shadowing value
	// rather than write through to it, or a module would be able to change what its ancestors are reading.
	for (int32 Index = ExecutionData.Num() - 1; Index >= CurrentScopeStart; --Index)
	{
		const FStructView Data = ExecutionData[Index];
		if (Data.GetScriptStruct() == InData.GetScriptStruct())
		{
			Data.GetScriptStruct()->CopyScriptStruct(Data.GetMemory(), InData.GetMemory());
			return;
		}
	}

	ExecutionData.Append({ InData });
}

FTriggerExecutionDataScope::FTriggerExecutionDataScope(FTriggerExecutionContext& InContext)
	: Context(InContext), OuterScopeStart(InContext.CurrentScopeStart)
{
	Context.CurrentScopeStart = Context.ExecutionData.Num();
}

FTriggerExecutionDataScope::~FTriggerExecutionDataScope()
{
	// Everything this scope added sits at the end of the container, so shrinking back to where it started
	// destroys exactly its own values and leaves the inherited ones untouched.
	Context.ExecutionData.SetNum(Context.CurrentScopeStart);
	Context.CurrentScopeStart = OuterScopeStart;
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
	// These children share one data scope: what one of them writes is readable by the ones after it and by
	// their subtrees, and goes away again once this list has been walked.
	const FTriggerExecutionDataScope DataScope(TriggerExecutionContext);

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

	// A module can only ever close the gate in front of its own children: there is no shared flag for it to
	// set, so a module that rejects an event cannot affect the modules beside it.
	const bool bGateOpen = !bEnabled || ShouldPropagateEvent(TriggerExecutionContext);

	if (bChildrenEnabled && bGateOpen)
	{
		PropagateEvent(TriggerExecutionContext);
	}

	if (bTriggerOnce)
	{
		bEnabled = false;
		bChildrenEnabled = false;
	}
}
