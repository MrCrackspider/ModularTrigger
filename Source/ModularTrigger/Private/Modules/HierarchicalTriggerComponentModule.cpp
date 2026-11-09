
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "Components/PrimitiveComponent.h"
#include "HierarchicalTriggerComponent.h"
#include "HierarchicalTriggerModuleUtils.h"
#include "GameFramework/Actor.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY_STATIC(LogTriggerExecution, Log, All);

FTriggerExecutionContext::FTriggerExecutionContext(
	TNotNull<UHierarchicalTriggerComponent*> InOwningTriggerComponent, TNotNull<UPrimitiveComponent*> OverlappedComponent, 
	TNotNull<UPrimitiveComponent*> OtherComponent, bool bEndOverlap, const FHitResult* InSweepResult) : 
	OwningTriggerComponent(InOwningTriggerComponent), OverlappedComponent(OverlappedComponent), 
	OtherComponent(OtherComponent), SweepResult(InSweepResult), bEndOverlap(bEndOverlap) 
{
#if UE_DEBUG_RECORDING_ENABLED
	if (FVisualLogger::IsRecording())
	{
		ExecutionTrace = MakeUnique<FTriggerExecutionTrace>();
	}
#endif
}

FTriggerExecutionContext::~FTriggerExecutionContext()
{
#if UE_DEBUG_RECORDING_ENABLED
	if (ExecutionTrace)
	{
		check(ExecutionTrace->ActiveEntryIndex == INDEX_NONE);
		const AActor* OwnerActor = GetOwningActor();
		const AActor* OtherActor = GetOtherActor();
		const UHierarchicalTriggerComponent* Component = OwningTriggerComponent;
		const UPrimitiveComponent* Overlapped = OverlappedComponent;
		const UPrimitiveComponent* Other = OtherComponent;
		UE_VLOG(OwnerActor, LogTriggerExecution, Log,
			TEXT("Trigger Event [%llu]: [%s]; Component: [%s]; Other: [%s]; Overlapped: [%s]; Other Component: [%s]%s"),
			ExecutionTrace->EventId, bEndOverlap ? TEXT("EndOverlap") : TEXT("BeginOverlap"),
			*GetNameSafe(Component), *GetNameSafe(OtherActor),
			*GetNameSafe(Overlapped), *GetNameSafe(Other), *ExecutionTrace->Describe());
	}
#endif
}

bool FTriggerExecutionContext::IsExecutionTracing() const
{
#if UE_DEBUG_RECORDING_ENABLED
	return ExecutionTrace.IsValid();
#else
	return false;
#endif
}

void FTriggerExecutionContext::ReportExecutionResult(ETriggerExecutionResult Result, const FString& Message) const
{
#if UE_DEBUG_RECORDING_ENABLED
	if (ExecutionTrace)
	{
		ExecutionTrace->ReportResult(Result, Message);
	}
#endif
}

void FTriggerExecutionContext::ReportExecutionMessage(const FString& Message) const
{
#if UE_DEBUG_RECORDING_ENABLED
	if (ExecutionTrace)
	{
		ExecutionTrace->AddMessage(Message);
	}
#endif
}

void FTriggerExecutionContext::ReportPropagation(bool bChildrenEnabled, bool bGateOpen) const
{
#if UE_DEBUG_RECORDING_ENABLED
	if (ExecutionTrace)
	{
		ExecutionTrace->SetPropagation(bChildrenEnabled, bGateOpen);
	}
#endif
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

TArray<UHierarchicalTriggerComponentModule*> UHierarchicalTriggerComponentModule::FindModulesByClass(
	TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth) const
{
	TArray<UHierarchicalTriggerComponentModule*> Result;
	FHierarchicalTriggerModuleUtils::FindAllByClass(Modules, Class, Result, MaxSearchDepth);
	return Result;
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
#if UE_DEBUG_RECORDING_ENABLED
	const FTriggerModuleTraceScope TraceScope(TriggerExecutionContext, *this);
#endif
	if (bEnabled)
	{
		OnTriggerEventReceived(TriggerExecutionContext);
	}
	else
	{
		TRIGGER_REPORT_EXECUTION(TriggerExecutionContext, ETriggerExecutionResult::Skipped, TEXT("Handler disabled; children may still execute"));
	}

	// A module can only ever close the gate in front of its own children: there is no shared flag for it to
	// set, so a module that rejects an event cannot affect the modules beside it.
	const bool bGateOpen = !bEnabled || ShouldPropagateEvent(TriggerExecutionContext);
	TriggerExecutionContext.ReportPropagation(bChildrenEnabled, bGateOpen);

	if (bChildrenEnabled && bGateOpen)
	{
		PropagateEvent(TriggerExecutionContext);
	}

	if (bTriggerOnce)
	{
		TRIGGER_EXECUTION_MESSAGE(TriggerExecutionContext, TEXT("Trigger once: disabling this module and its children"));
		bEnabled = false;
		bChildrenEnabled = false;
	}
}

void UHierarchicalTriggerComponentModule::DispatchGeneratedEvent(FTriggerExecutionContext& TriggerExecutionContext)
{
#if UE_DEBUG_RECORDING_ENABLED
	const FTriggerModuleTraceScope TraceScope(TriggerExecutionContext, *this);
#endif
	if (!bChildrenEnabled)
	{
		TRIGGER_REPORT_EXECUTION(TriggerExecutionContext, ETriggerExecutionResult::Skipped, TEXT("Generator children disabled"));
		TriggerExecutionContext.ReportPropagation(false, false);
		return;
	}

	TRIGGER_REPORT_EXECUTION(TriggerExecutionContext, ETriggerExecutionResult::Succeeded, TEXT("Generated overlap event"));
	const bool bGateOpen = ShouldPropagateEvent(TriggerExecutionContext);
	TriggerExecutionContext.ReportPropagation(bChildrenEnabled, bGateOpen);
	if (bGateOpen)
	{
		PropagateEvent(TriggerExecutionContext);
	}
	if (bTriggerOnce)
	{
		TRIGGER_EXECUTION_MESSAGE(TriggerExecutionContext, TEXT("Trigger once: disabling this generator and its children"));
		bEnabled = false;
		bChildrenEnabled = false;
	}
}
