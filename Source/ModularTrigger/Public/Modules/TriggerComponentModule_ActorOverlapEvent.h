// Copyright Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_ActorOverlapEvent.generated.h"

/** Generates actor-level trigger events by aggregating overlaps from all primitive components. */
UCLASS(DisplayName = "Generate Actor Overlap Event")
class MODULARTRIGGER_API UTriggerComponentModule_ActorOverlapEvent : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()

public:
	virtual void OnBeginPlay() override;
	virtual void OnEndPlay() override;

	/** Whether to refresh overlaps and emit Begin events for actors already overlapping at activation time. */
	UPROPERTY(EditAnywhere, Category = "Trigger Component")
	bool bUpdateOverlapsOnBeginPlay = true;

	/** Delay before subscribing to actor overlap events. */
	UPROPERTY(EditAnywhere, Category = "Trigger Component", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ActivationDelay = 0.0f;

protected:
	/** Emit Begin only for the first overlapping actor and End only after the last actor leaves. */
	UPROPERTY(EditAnywhere, Category = "Trigger Component")
	bool bOverlapOncePerAnyActor = false;

private:
	void ActivateOverlap();
	void SetOverlapEnabled(bool bNewEnabled);
	void CacheOrHandleExistingOverlaps(bool bPropagateEvents);
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComponent,
		bool bEndOverlap, const FHitResult* SweepResult = nullptr);

	UFUNCTION()
	void ReceiveComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void ReceiveComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> BoundComponents;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> OverlappedActors;

	FTimerHandle ActivationTimerHandle;
};
