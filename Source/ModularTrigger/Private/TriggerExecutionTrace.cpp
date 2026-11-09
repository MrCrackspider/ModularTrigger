#include "TriggerExecutionTrace.h"

#if UE_DEBUG_RECORDING_ENABLED

#include "HAL/ThreadSafeCounter64.h"
#include "Modules/HierarchicalTriggerComponentModule.h"

namespace
{
	FThreadSafeCounter64 NextTriggerEventId;

	const TCHAR* DescribeResult(ETriggerExecutionResult Result)
	{
		switch (Result)
		{
		case ETriggerExecutionResult::Succeeded: return TEXT("Succeeded");
		case ETriggerExecutionResult::Failed: return TEXT("Failed");
		case ETriggerExecutionResult::Skipped: return TEXT("Skipped");
		default: return TEXT("Unreported");
		}
	}

	const TCHAR* DescribePropagation(ETriggerPropagationResult Result)
	{
		switch (Result)
		{
		case ETriggerPropagationResult::Allowed: return TEXT("Allowed");
		case ETriggerPropagationResult::Rejected: return TEXT("Rejected");
		case ETriggerPropagationResult::ChildrenDisabled: return TEXT("Children Disabled");
		default: return TEXT("Not Evaluated");
		}
	}
}

FTriggerExecutionTrace::FTriggerExecutionTrace()
	: EventId(static_cast<uint64>(NextTriggerEventId.Increment()))
{
}

int32 FTriggerExecutionTrace::BeginModule(const FGuid& ModuleId, FString ModuleName)
{
	FTriggerModuleExecutionTrace Entry;
	Entry.ModuleId = ModuleId;
	Entry.ModuleName = MoveTemp(ModuleName);
	Entry.ParentIndex = ActiveEntryIndex;
	Entry.Depth = Entries.IsValidIndex(ActiveEntryIndex) ? Entries[ActiveEntryIndex].Depth + 1 : 0;
	ActiveEntryIndex = Entries.Add(MoveTemp(Entry));
	return ActiveEntryIndex;
}

void FTriggerExecutionTrace::EndModule(int32 EntryIndex)
{
	check(ActiveEntryIndex == EntryIndex && Entries.IsValidIndex(EntryIndex));
	ActiveEntryIndex = Entries[EntryIndex].ParentIndex;
}

void FTriggerExecutionTrace::ReportResult(ETriggerExecutionResult Result, const FString& Message)
{
	if (Entries.IsValidIndex(ActiveEntryIndex))
	{
		// A later success must not hide an earlier failure (for example, partial target acceptance).
		if (Entries[ActiveEntryIndex].Result != ETriggerExecutionResult::Failed)
		{
			Entries[ActiveEntryIndex].Result = Result;
		}
		AddMessage(Message);
	}
}

void FTriggerExecutionTrace::AddMessage(const FString& Message)
{
	if (Entries.IsValidIndex(ActiveEntryIndex) && !Message.IsEmpty())
	{
		Entries[ActiveEntryIndex].Messages.Add(Message);
	}
}

void FTriggerExecutionTrace::SetPropagation(bool bChildrenEnabled, bool bGateOpen)
{
	if (Entries.IsValidIndex(ActiveEntryIndex))
	{
		Entries[ActiveEntryIndex].Propagation = !bChildrenEnabled
			? ETriggerPropagationResult::ChildrenDisabled
			: (bGateOpen ? ETriggerPropagationResult::Allowed : ETriggerPropagationResult::Rejected);
	}
}

FString FTriggerExecutionTrace::Describe() const
{
	FString Description;
	for (const FTriggerModuleExecutionTrace& Entry : Entries)
	{
		const FString Indent = FString::ChrN((Entry.Depth + 1) * 2, TEXT(' '));
		Description += FString::Printf(TEXT("\n%s%s: Result: [%s]; Propagation: [%s]"), *Indent, *Entry.ModuleName,
			DescribeResult(Entry.Result), DescribePropagation(Entry.Propagation));
		for (const FString& Message : Entry.Messages)
		{
			Description += FString::Printf(TEXT("\n%s  %s"), *Indent, *Message);
		}
	}
	return Description;
}

FTriggerModuleTraceScope::FTriggerModuleTraceScope(
	const FTriggerExecutionContext& Context, const UHierarchicalTriggerComponentModule& Module)
	: Trace(Context.ExecutionTrace.Get())
{
	if (Trace)
	{
		FString ModuleName = Module.GetClass()->GetName();
		ModuleName.RemoveFromStart(TEXT("TriggerComponentModule_"));
		ModuleName.RemoveFromEnd(TEXT("_C"));

		int32 SiblingIndex = INDEX_NONE;
		if (const UHierarchicalTriggerComponentModule* Parent = Module.GetOuterModule())
		{
			SiblingIndex = Parent->Modules.IndexOfByKey(&Module);
		}
		else if (const UHierarchicalTriggerComponent* Component = Module.GetOwningComponent())
		{
			SiblingIndex = Component->Modules.IndexOfByKey(&Module);
		}
		EntryIndex = Trace->BeginModule(Module.GetModuleId(), FString::Printf(TEXT("%s[%d]"), *ModuleName, SiblingIndex));
	}
}

FTriggerModuleTraceScope::~FTriggerModuleTraceScope()
{
	if (Trace)
	{
		Trace->EndModule(EntryIndex);
	}
}

#endif
