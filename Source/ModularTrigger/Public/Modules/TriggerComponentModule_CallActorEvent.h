
#pragma once

#include "ActorEventCall.h"
#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_CallActorEvent.generated.h"

/**
 * Calls a Blueprint event on a level actor.
 */
UCLASS(DisplayName = "Call Actor Event")
class MODULARTRIGGER_API UTriggerComponentModule_CallActorEvent : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()

public:

	/** "ContextActorName" labels the context-actor toggle and the events it lists with what supplies it here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Component", meta = (ContextActorName = "Trigger Owner"))
	FActorEventCall EventCall;

protected:

	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
};
