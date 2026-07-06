

#include "Modules/HierarchicalTriggerComponentModule.h"
#include "HierarchicalTriggerComponent.h"
#include "Engine/Engine.h"


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

UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponentModule::FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth) const
{
	for (TObjectPtr<UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			if (Module.IsA(Class))
			{
				return Module;
			}

			if (MaxSearchDepth == 0)
			{
				return nullptr;
			}
			
			if (UHierarchicalTriggerComponentModule* OutModule = Module->FindModuleByClass(Class, MaxSearchDepth < 0 ? MaxSearchDepth : --MaxSearchDepth))
			{
				if (OutModule)
				{
					return OutModule;
				}
			}
		}
	}
	
	return nullptr;
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
	for (TObjectPtr<class UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveBeginPlay();
		}
	}
	OnBeginPlay();
}

void UHierarchicalTriggerComponentModule::ReceiveEndPlay()
{
	for (TObjectPtr<class UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveEndPlay();
		}
	}
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

void UTriggerComponentModule_DebugMessage::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	const TCHAR* EventName = TriggerExecutionContext.IsEndOverlap() ? TEXT("EndOverlap") : TEXT("BeginOverlap");
	const FString FinalMessage = FString::Printf(TEXT("[%s] %s"), EventName, *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, FinalMessage);
	}

	UE_LOG(LogTemp, Log, TEXT("%s"), *FinalMessage);
}
