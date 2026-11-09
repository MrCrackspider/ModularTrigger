#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "TriggerExecutionDataProvider.generated.h"

struct FTriggerExecutionContext;

/** Authored configuration that produces execution data for each trigger event. */
USTRUCT(BlueprintType, meta = (HideDropdown))
struct MODULARTRIGGER_API FTriggerExecutionDataProvider
{
	GENERATED_BODY()

	virtual ~FTriggerExecutionDataProvider() = default;

	/**
	 * Return an owned payload to copy into the context, or an invalid struct to publish nothing.
	 * Providers can read earlier providers' results through Context. Keep per-event results out of this
	 * authored configuration; a provider's reflected type need not match the payload's reflected type.
	 * An empty payload (for example, an empty actor array) is valid and shadows inherited data normally.
	 */
	virtual FInstancedStruct Gather(const FTriggerExecutionContext& Context) const { return {}; }
};
