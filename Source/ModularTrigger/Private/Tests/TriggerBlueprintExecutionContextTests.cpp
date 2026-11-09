#include "Modules/TriggerComponentModule_Blueprint.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Templates/UnrealTemplate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriggerBlueprintExecutionContextTest,
	"ModularTrigger.Blueprint.LiveExecutionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTriggerBlueprintExecutionContextTest::RunTest(const FString& Parameters)
{
	// No Blueprint asset is needed: these accessors depend only on the borrowed context.
	UTriggerComponentModule_Blueprint* Module = GetMutableDefault<UTriggerComponentModule_Blueprint>();
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world created"), World))
	{
		return false;
	}
	AActor* Owner = World->SpawnActor<AActor>();
	AActor* OtherActor = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Owner spawned"), Owner) || !TestNotNull(TEXT("Other actor spawned"), OtherActor))
	{
		World->DestroyWorld(false);
		return false;
	}
	UHierarchicalTriggerComponent* Component = NewObject<UHierarchicalTriggerComponent>(Owner);
	UBoxComponent* Overlapped = NewObject<UBoxComponent>(Owner);
	UBoxComponent* Other = NewObject<UBoxComponent>(OtherActor);

	TestFalse(TEXT("No context outside event"), Module->HasExecutionContext());
	TestNull(TEXT("No stale other component"), Module->GetOtherComponent());
	TestFalse(TEXT("No stale sweep result"), Module->HasSweepResult());
	FHitResult Payload;
	Payload.Time = 0.25f;
	const FInstancedStruct Data = FInstancedStruct::Make<FHitResult>(Payload);

	{
		FTriggerExecutionContext OuterContext(Component, Overlapped, Other, false, &Payload);
		FTriggerExecutionContext InnerContext(Component, Overlapped, Other, true);
		{
			TGuardValue<FTriggerExecutionContext*> OuterGuard(Module->ActiveExecutionContext, &OuterContext);
			TestTrue(TEXT("Context available during event"), Module->HasExecutionContext());
			TestTrue(TEXT("Owning component read directly"), Module->GetOwningTriggerComponent() == Component);
			TestTrue(TEXT("Overlapped component read directly"), Module->GetOverlappedComponent() == Overlapped);
			TestTrue(TEXT("Other component read directly"), Module->GetOtherComponent() == Other);
			TestTrue(TEXT("Outer sweep available"), Module->HasSweepResult());
			TestEqual(TEXT("Sweep returned by value"), Module->GetSweepResult().Time, 0.25f);

			Module->AddExecutionData(Data);
			TestEqual(TEXT("Data published immediately"), OuterContext.GetExecutionData().Num(), 1);
			FInstancedStruct ReadData = Module->GetExecutionData(FHitResult::StaticStruct());
			if (TestTrue(TEXT("Read after write succeeds"), ReadData.IsValid()))
			{
				ReadData.GetMutable<FHitResult>().Time = 0.75f;
				TestEqual(TEXT("Blueprint receives a copy"), OuterContext.GetExecutionDataPtr<FHitResult>()->Time, 0.25f);
			}

			{
				const FTriggerExecutionDataScope Scope(OuterContext);
				Payload.Time = 0.5f;
				Module->AddExecutionData(FInstancedStruct::Make<FHitResult>(Payload));
				TestEqual(TEXT("Nearest scope is read"), Module->GetExecutionData(FHitResult::StaticStruct()).Get<FHitResult>().Time, 0.5f);
			}
			TestEqual(TEXT("Outer data revealed after scope exit"), Module->GetExecutionData(FHitResult::StaticStruct()).Get<FHitResult>().Time, 0.25f);

			{
				TGuardValue<FTriggerExecutionContext*> InnerGuard(Module->ActiveExecutionContext, &InnerContext);
				TestTrue(TEXT("Nested event uses its own overlap flag"), Module->IsEndOverlap());
				TestFalse(TEXT("Nested event does not inherit sweep"), Module->HasSweepResult());
				TestFalse(TEXT("Nested event does not inherit data"), Module->GetExecutionData(FHitResult::StaticStruct()).IsValid());
			}
			TestFalse(TEXT("Outer overlap restored"), Module->IsEndOverlap());
			TestTrue(TEXT("Outer sweep restored"), Module->HasSweepResult());
		}
		TestFalse(TEXT("Context released after event"), Module->HasExecutionContext());
		TestNull(TEXT("No retained owner"), Module->GetOwningTriggerComponent());
		TestNull(TEXT("No retained overlapped component"), Module->GetOverlappedComponent());
		TestFalse(TEXT("No data outside event"), Module->GetExecutionData(FHitResult::StaticStruct()).IsValid());
		Module->AddExecutionData(Data);
		TestEqual(TEXT("Write outside event does nothing"), OuterContext.GetExecutionData().Num(), 1);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
