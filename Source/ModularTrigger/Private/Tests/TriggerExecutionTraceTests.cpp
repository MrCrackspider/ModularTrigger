#include "TriggerExecutionTrace.h"

#if WITH_DEV_AUTOMATION_TESTS && UE_DEBUG_RECORDING_ENABLED

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggerExecutionTraceHierarchyTest,
	"ModularTrigger.ExecutionTrace.HierarchyAndResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTriggerExecutionTraceHierarchyTest::RunTest(const FString& Parameters)
{
	FTriggerExecutionTrace Trace;
	const int32 Generator = Trace.BeginModule(FGuid::NewGuid(), TEXT("Generator"));
	Trace.ReportResult(ETriggerExecutionResult::Succeeded, TEXT("Event generated"));
	const int32 Branch = Trace.BeginModule(FGuid::NewGuid(), TEXT("Branch"));
	Trace.ReportResult(ETriggerExecutionResult::Succeeded, TEXT("Filter rejected normally"));
	Trace.SetPropagation(true, false);
	Trace.EndModule(Branch);
	TestEqual(TEXT("Branch exit restores the generator"), Trace.ActiveEntryIndex, Generator);

	// Force reallocation: active scopes must use indices, not references into Entries.
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const int32 Action = Trace.BeginModule(FGuid::NewGuid(), TEXT("Action"));
		Trace.ReportResult(ETriggerExecutionResult::Failed, TEXT("One target rejected the request"));
		Trace.ReportResult(ETriggerExecutionResult::Succeeded, TEXT("Another target accepted it"));
		Trace.SetPropagation(false, true);
		Trace.EndModule(Action);
	}
	Trace.AddMessage(TEXT("Generator finished"));
	Trace.EndModule(Generator);

	TestEqual(TEXT("Every scope closed"), Trace.ActiveEntryIndex, INDEX_NONE);
	TestEqual(TEXT("Sibling parent is the generator"), Trace.Entries.Last().ParentIndex, Generator);
	TestEqual(TEXT("Sibling depth is restored"), Trace.Entries.Last().Depth, 1);
	TestTrue(TEXT("Success does not conceal partial failure"), Trace.Entries.Last().Result == ETriggerExecutionResult::Failed);
	TestTrue(TEXT("A rejected filter is not an execution failure"), Trace.Entries[Branch].Result == ETriggerExecutionResult::Succeeded);
	TestTrue(TEXT("Filter rejection is recorded independently"), Trace.Entries[Branch].Propagation == ETriggerPropagationResult::Rejected);
	TestTrue(TEXT("Disabled children take precedence over an open gate"), Trace.Entries.Last().Propagation == ETriggerPropagationResult::ChildrenDisabled);
	TestEqual(TEXT("Messages return to the parent"), Trace.Entries[Generator].Messages.Last(), FString(TEXT("Generator finished")));
	TestTrue(TEXT("Description retains failure reason"), Trace.Describe().Contains(TEXT("One target rejected the request")));

	FTriggerExecutionTrace AnotherEvent;
	TestTrue(TEXT("Same-frame events have distinct identities"), Trace.EventId != AnotherEvent.EventId);
	return true;
}

#endif
