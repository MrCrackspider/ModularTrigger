
#include "Modules/TriggerComponentModule_WorldConditionFilter.h"

#include "HierarchicalTriggerComponent.h"
#include "WorldConditionContext.h"
#include "WorldConditions/TriggerWorldConditionSchema.h"

DEFINE_LOG_CATEGORY_STATIC(LogTriggerWorldCondition, Log, All);

UTriggerComponentModule_WorldConditionFilter::UTriggerComponentModule_WorldConditionFilter()
	: SchemaClass(UTriggerWorldConditionSchema::StaticClass())
{
	// The query keeps its own copy of the class, and the condition picker reads it from there, so a freshly
	// created module has to arrive with it already set — PostLoad does not run on one.
	Condition.SetSchemaClass(SchemaClass);
}

void UTriggerComponentModule_WorldConditionFilter::PostLoad()
{
	Super::PostLoad();

	if (!SchemaClass)
	{
		SchemaClass = UTriggerWorldConditionSchema::StaticClass();
	}

	if (Condition.GetSchemaClass().Get() != SchemaClass.Get())
	{
		Condition.SetSchemaClass(SchemaClass);
	}

	// Resolves every context data reference and lays out the runtime state. In a cooked build the authored
	// rows are gone and this keeps the shared definition that was cooked with them.
	Condition.Initialize(this);
}

#if WITH_EDITOR
void UTriggerComponentModule_WorldConditionFilter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Edits to the query itself are re-linked by its own details customization; the schema it links against
	// is this module's business.
	static const FName SchemaClassName = GET_MEMBER_NAME_CHECKED(UTriggerComponentModule_WorldConditionFilter, SchemaClass);
	if (PropertyChangedEvent.GetMemberPropertyName() == SchemaClassName)
	{
		ApplySchemaClass();
	}
}
#endif

void UTriggerComponentModule_WorldConditionFilter::OnBeginPlay()
{
	Super::OnBeginPlay();

	// Initialized even when there is nothing to evaluate: an initialized but empty state is what answers
	// true, while an uninitialized one answers false.
	ConditionState.Initialize(*this, Condition);

	const UTriggerWorldConditionSchema* Schema = GetSchema();
	const UHierarchicalTriggerComponent* TriggerComponent = GetOwningComponent();
	if (!Schema || !TriggerComponent)
	{
		return;
	}

	// Only the persistent data exists this early. Conditions that read an actor supplied per event are
	// documented to receive nothing here, and register their invalidation callbacks off what is available.
	FWorldConditionContextData ContextData(*Schema);
	Schema->SetPersistentContextData(ContextData, *TriggerComponent);

	const FWorldConditionContext Context(ConditionState, ContextData);
	if (!Context.Activate())
	{
		UE_LOG(LogTriggerWorldCondition, Error,
			TEXT("%s: failed to activate its world condition query; the filter will reject every event."),
			*GetPathName());
	}
}

void UTriggerComponentModule_WorldConditionFilter::OnEndPlay()
{
	if (ConditionState.IsInitialized())
	{
		const UTriggerWorldConditionSchema* Schema = GetSchema();
		const UHierarchicalTriggerComponent* TriggerComponent = GetOwningComponent();
		if (Schema && TriggerComponent)
		{
			// Frees the state as well, so the conditions get the chance to unregister before the memory their
			// invalidation handles point at goes away.
			FWorldConditionContextData ContextData(*Schema);
			Schema->SetPersistentContextData(ContextData, *TriggerComponent);

			const FWorldConditionContext Context(ConditionState, ContextData);
			Context.Deactivate();
		}
	}

	Super::OnEndPlay();
}

bool UTriggerComponentModule_WorldConditionFilter::ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const
{
	const UTriggerWorldConditionSchema* Schema = GetSchema();
	if (!Schema)
	{
		TRIGGER_REPORT_EXECUTION(TriggerExecutionContext, ETriggerExecutionResult::Failed, TEXT("World condition schema is unavailable"));
		return false;
	}

	// Lives until the evaluation is over: the container below holds a view of it, not a copy.
	const FTriggerWorldConditionEvent Event{ TriggerExecutionContext.IsEndOverlap() };

	FWorldConditionContextData ContextData(*Schema);
	Schema->SetContextData(ContextData, TriggerExecutionContext, Event);

	const FWorldConditionContext Context(ConditionState, ContextData);

	const bool bPassed = Context.IsTrue();
	TRIGGER_REPORT_EXECUTION(TriggerExecutionContext,
		ConditionState.IsInitialized() ? ETriggerExecutionResult::Succeeded : ETriggerExecutionResult::Failed,
		TEXT("World condition query %s; initialized=%d"), bPassed ? TEXT("passed") : TEXT("rejected"), ConditionState.IsInitialized());
	return bPassed;
}

const UTriggerWorldConditionSchema* UTriggerComponentModule_WorldConditionFilter::GetSchema() const
{
	// The query's own class rather than SchemaClass: the conditions resolved their context data against that
	// one, and the two only differ while an edit is in flight.
	return Cast<UTriggerWorldConditionSchema>(Condition.GetSchemaClass().GetDefaultObject());
}

void UTriggerComponentModule_WorldConditionFilter::ApplySchemaClass()
{
	if (!SchemaClass)
	{
		SchemaClass = UTriggerWorldConditionSchema::StaticClass();
	}

	Condition.SetSchemaClass(SchemaClass);
	Condition.Initialize(this);
}
