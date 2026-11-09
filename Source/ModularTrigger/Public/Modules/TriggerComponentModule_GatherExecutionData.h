#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "StructUtils/InstancedStruct.h"
#include "TriggerExecutionDataProvider.h"
#include "TriggerComponentModule_GatherExecutionData.generated.h"

/** Gathers data into the current scope before its children and following siblings execute. */
UCLASS(DisplayName = "Gather Execution Data")
class MODULARTRIGGER_API UTriggerComponentModule_GatherExecutionData : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()

public:

	/** Evaluated in order. Later payloads of the same type replace earlier ones in this scope. */
	UPROPERTY(EditAnywhere, Category = "Execution Data", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FTriggerExecutionDataProvider>> DataProviders;

protected:

	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
};
