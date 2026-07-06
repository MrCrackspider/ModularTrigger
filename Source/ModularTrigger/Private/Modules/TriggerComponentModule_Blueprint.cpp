// © Volga Studios. All rights reserved.


#include "Modules/TriggerComponentModule_Blueprint.h"

void UTriggerComponentModule_Blueprint::SetPendingInterruption()
{
	bPendingInterruption = true;
}

FInstancedStruct UTriggerComponentModule_Blueprint::GetExecutionData(const UScriptStruct* Type)
{
	if (!Type)
	{
		return {};
	}
	
	for (FConstStructView Data : InExecutionData)
	{
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

void UTriggerComponentModule_Blueprint::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
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
	if (bPendingInterruption)
	{
		TriggerExecutionContext.SetPendingInterruption();
	}
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
	bPendingInterruption = false;
}
