
#include "Modules/TriggerComponentModule_CallActorEvent.h"

void UTriggerComponentModule_CallActorEvent::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	const bool bDispatched = EventCall.Execute(TriggerExecutionContext.GetOwningComponent()->GetOwner());
	TRIGGER_REPORT_EXECUTION(TriggerExecutionContext,
		bDispatched ? ETriggerExecutionResult::Succeeded : ETriggerExecutionResult::Failed,
		TEXT("Actor event %s: %s"), *EventCall.EventName.ToString(), bDispatched ? TEXT("dispatched") : TEXT("could not be dispatched"));
}
