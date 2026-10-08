// © Volga Studios. All rights reserved.

#include "Modules/TriggerComponentModule_DebugMessage.h"

#include "Engine/Engine.h"

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
