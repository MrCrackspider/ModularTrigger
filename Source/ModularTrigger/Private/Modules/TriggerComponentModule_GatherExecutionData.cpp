#include "Modules/TriggerComponentModule_GatherExecutionData.h"

void UTriggerComponentModule_GatherExecutionData::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	for (const TInstancedStruct<FTriggerExecutionDataProvider>& Provider : DataProviders)
	{
		if (!Provider.IsValid())
		{
			TRIGGER_EXECUTION_MESSAGE(TriggerExecutionContext, TEXT("Unset provider row skipped"));
			continue;
		}

		const FInstancedStruct Data = Provider.Get().Gather(TriggerExecutionContext);
		if (Data.IsValid())
		{
			TriggerExecutionContext.AddExecutionData(FConstStructView(Data.GetScriptStruct(), Data.GetMemory()));
			TRIGGER_EXECUTION_MESSAGE(TriggerExecutionContext, TEXT("%s published %s"),
				*Provider.GetScriptStruct()->GetName(), *Data.GetScriptStruct()->GetName());
		}
		else
		{
			TRIGGER_EXECUTION_MESSAGE(TriggerExecutionContext, TEXT("%s produced no payload"), *Provider.GetScriptStruct()->GetName());
		}
	}
	TRIGGER_REPORT_EXECUTION(TriggerExecutionContext, ETriggerExecutionResult::Succeeded,
		TEXT("Finished evaluating %d provider rows"), DataProviders.Num());
}
