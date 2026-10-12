
#include "Modules/TriggerComponentModule_DebugMessage.h"

#include "Engine/Engine.h"
#include "TriggerModuleVisualization.h"

void UTriggerComponentModule_DebugMessage::OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext)
{
	const TCHAR* EventName = TriggerExecutionContext.IsEndOverlap() ? TEXT("EndOverlap") : TEXT("BeginOverlap");
	const FString FinalMessage = FString::Printf(TEXT("[%s] %s"), EventName, *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, Duration, Color, FinalMessage);
	}

	UE_LOG(LogTemp, Log, TEXT("%s"), *FinalMessage);
}

#if WITH_EDITOR
void UTriggerComponentModule_DebugMessage::DescribeVisualization(FTriggerVisualizationBuilder& Builder) const
{
	// Nothing spatial to show: this module only has something to say. Stack the text by tree depth so sibling
	// messages on the same trigger do not land on top of each other.
	const FVector Location = Builder.GetOrigin() + FVector(0.0, 0.0, 120.0 + Builder.GetModuleDepth() * 20.0);
	Builder.AddText(Location, FText::FromString(Message), FLinearColor(Color));
}
#endif
