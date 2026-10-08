
#include "Modules/TriggerComponentModule_CallActorEvent.h"

void UTriggerComponentModule_CallActorEvent::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	EventCall.Execute(TriggerExecutionContext.GetOwningComponent()->GetOwner());
}
