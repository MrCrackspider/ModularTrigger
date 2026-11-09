
#include "Modules/TriggerComponentModule_Blueprint.h"
#include "Templates/UnrealTemplate.h"

void UTriggerComponentModule_Blueprint::ReportExecutionResult(ETriggerExecutionResult Result, const FString& Message)
{
	if (ActiveExecutionContext)
	{
		ActiveExecutionContext->ReportExecutionResult(Result, Message);
	}
}

bool UTriggerComponentModule_Blueprint::IsExecutionTracing() const
{
	return ActiveExecutionContext && ActiveExecutionContext->IsExecutionTracing();
}

bool UTriggerComponentModule_Blueprint::HasExecutionContext() const
{
	return ActiveExecutionContext != nullptr;
}

const UHierarchicalTriggerComponent* UTriggerComponentModule_Blueprint::GetOwningTriggerComponent() const
{
	if (ActiveExecutionContext)
	{
		return ActiveExecutionContext->GetOwningComponent();
	}
	return nullptr;
}

const UPrimitiveComponent* UTriggerComponentModule_Blueprint::GetOverlappedComponent() const
{
	if (ActiveExecutionContext)
	{
		return ActiveExecutionContext->GetOverlappedComponent();
	}
	return nullptr;
}

const UPrimitiveComponent* UTriggerComponentModule_Blueprint::GetOtherComponent() const
{
	if (ActiveExecutionContext)
	{
		return ActiveExecutionContext->GetOtherComponent();
	}
	return nullptr;
}

FHitResult UTriggerComponentModule_Blueprint::GetSweepResult() const
{
	return HasSweepResult() ? *ActiveExecutionContext->GetSweepResult() : FHitResult{};
}

bool UTriggerComponentModule_Blueprint::HasSweepResult() const
{
	return ActiveExecutionContext && ActiveExecutionContext->HasSweepResult();
}

bool UTriggerComponentModule_Blueprint::IsEndOverlap() const
{
	return ActiveExecutionContext && ActiveExecutionContext->IsEndOverlap();
}

void UTriggerComponentModule_Blueprint::StopPropagationToChildren()
{
	bPropagationStopped = true;
}

void UTriggerComponentModule_Blueprint::SetPendingInterruption()
{
	StopPropagationToChildren();
}

FInstancedStruct UTriggerComponentModule_Blueprint::GetExecutionData(const UScriptStruct* Type) const
{
	if (!ActiveExecutionContext || !Type)
	{
		return {};
	}

	// Back to front, so the nearest scope's value wins over one an outer scope holds of the same type.
	const FInstancedStructContainer& ExecutionData = ActiveExecutionContext->GetExecutionData();
	for (int32 Index = ExecutionData.Num() - 1; Index >= 0; --Index)
	{
		const FConstStructView Data = ExecutionData[Index];
		if (Data.GetScriptStruct() == Type)
		{
			FInstancedStruct OutData;
			OutData.InitializeAs(Type, Data.GetMemory());
			return OutData;
		}
	}
	return {};
}

void UTriggerComponentModule_Blueprint::AddExecutionData(const FInstancedStruct& Data)
{
	if (!ActiveExecutionContext || !Data.IsValid())
	{
		return;
	}
	ActiveExecutionContext->AddExecutionData(FConstStructView(Data));
}

bool UTriggerComponentModule_Blueprint::ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const
{
	// Set while the graph ran, and cleared when the next event starts rather than at the end of the last one,
	// because this is asked after OnTriggerEventReceived has returned.
	return !bPropagationStopped;
}

void UTriggerComponentModule_Blueprint::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	bPropagationStopped = false;
	TGuardValue<FTriggerExecutionContext*> ContextGuard(ActiveExecutionContext, &TriggerExecutionContext);
	BPOnTriggerEventReceived();
}
