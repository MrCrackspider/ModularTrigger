#pragma once

#include "CoreMinimal.h"
#include "VisualLogger/VisualLoggerDefines.h"
#include "TriggerExecutionTrace.generated.h"

/** Describes the handler's work, independently of whether its children receive the event. */
UENUM(BlueprintType)
enum class ETriggerExecutionResult : uint8
{
	Unreported,
	Succeeded,
	Failed,
	Skipped
};

#if UE_DEBUG_RECORDING_ENABLED

enum class ETriggerPropagationResult : uint8
{
	NotEvaluated,
	Allowed,
	Rejected,
	ChildrenDisabled
};

struct FTriggerModuleExecutionTrace
{
	FGuid ModuleId;
	FString ModuleName;
	int32 ParentIndex = INDEX_NONE;
	int32 Depth = 0;
	ETriggerExecutionResult Result = ETriggerExecutionResult::Unreported;
	ETriggerPropagationResult Propagation = ETriggerPropagationResult::NotEvaluated;
	TArray<FString> Messages;
};

/** Per-dispatch history. Owns strings, never borrowed execution data or module pointers. */
struct MODULARTRIGGER_API FTriggerExecutionTrace
{
	FTriggerExecutionTrace();
	int32 BeginModule(const FGuid& ModuleId, FString ModuleName);
	void EndModule(int32 EntryIndex);
	void ReportResult(ETriggerExecutionResult Result, const FString& Message);
	void AddMessage(const FString& Message);
	void SetPropagation(bool bChildrenEnabled, bool bGateOpen);
	FString Describe() const;

	uint64 EventId = 0;
	int32 ActiveEntryIndex = INDEX_NONE;
	TArray<FTriggerModuleExecutionTrace> Entries;
};

struct FTriggerExecutionContext;
class UHierarchicalTriggerComponentModule;

/** Restores the parent's active entry after children finish, including on early return. */
struct MODULARTRIGGER_API FTriggerModuleTraceScope
{
	FTriggerModuleTraceScope(const FTriggerExecutionContext& Context, const UHierarchicalTriggerComponentModule& Module);
	~FTriggerModuleTraceScope();
	FTriggerModuleTraceScope(const FTriggerModuleTraceScope&) = delete;
	FTriggerModuleTraceScope& operator=(const FTriggerModuleTraceScope&) = delete;

private:
	FTriggerExecutionTrace* Trace = nullptr;
	int32 EntryIndex = INDEX_NONE;
};

// Keep formatting and argument evaluation out of non-recording dispatches and disabled builds.
#define TRIGGER_REPORT_EXECUTION(Context, Result, Format, ...) \
	do { if ((Context).IsExecutionTracing()) { (Context).ReportExecutionResult(Result, FString::Printf(Format, ##__VA_ARGS__)); } } while (false)
#define TRIGGER_EXECUTION_MESSAGE(Context, Format, ...) \
	do { if ((Context).IsExecutionTracing()) { (Context).ReportExecutionMessage(FString::Printf(Format, ##__VA_ARGS__)); } } while (false)

#else

#define TRIGGER_REPORT_EXECUTION(Context, Result, Format, ...) do {} while (false)
#define TRIGGER_EXECUTION_MESSAGE(Context, Format, ...) do {} while (false)

#endif
