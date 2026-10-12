
#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_DebugMessage.generated.h"

UCLASS(DisplayName = "Debug Message")
class MODULARTRIGGER_API UTriggerComponentModule_DebugMessage : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Message = TEXT("Trigger fired");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor Color = FColor::Green;

#if WITH_EDITOR
	//~ Begin UHierarchicalTriggerComponentModule interface
	virtual void DescribeVisualization(FTriggerVisualizationBuilder& Builder) const override;
	//~ End UHierarchicalTriggerComponentModule interface
#endif

protected:

	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
};
