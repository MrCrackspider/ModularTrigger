
#include "Modules/TriggerComponentModule_Blueprint.h"

void UTriggerComponentModule_Blueprint::StopPropagationToChildren()
{
	bPropagationStopped = true;
}

void UTriggerComponentModule_Blueprint::SetPendingInterruption()
{
	StopPropagationToChildren();
}

FInstancedStruct UTriggerComponentModule_Blueprint::GetExecutionData(const UScriptStruct* Type)
{
	if (!Type)
	{
		return {};
	}

	// Back to front, so the nearest scope's value wins over one an outer scope holds of the same type.
	for (int32 Index = InExecutionData.Num() - 1; Index >= 0; --Index)
	{
		const FConstStructView Data = InExecutionData[Index];
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
	if (!Data.IsValid())
	{
		return;
	}
	TArray<FInstancedStruct> Structs{ Data };
	OutExecutionData.Append(Structs);
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

	OwningTriggerComponent = TriggerExecutionContext.GetOwningComponent();
	OverlappedComponent = TriggerExecutionContext.GetOverlappedComponent();
	OtherComponent = TriggerExecutionContext.GetOtherComponent();
	bHasSweepResult = TriggerExecutionContext.HasSweepResult();
	InExecutionData = TriggerExecutionContext.GetExecutionData();
	if (bHasSweepResult)
	{
		SweepResult = *TriggerExecutionContext.GetSweepResult();
	}
	bEndOverlap = TriggerExecutionContext.IsEndOverlap();
	BPOnTriggerEventReceived();
	for (FConstStructView Data : OutExecutionData)
	{
		TriggerExecutionContext.AddExecutionData(Data);
	}
	OwningTriggerComponent = nullptr;
	OverlappedComponent = nullptr;
	OtherComponent = nullptr;
	bHasSweepResult = false;
	InExecutionData.Reset();
	OutExecutionData.Reset();
}
