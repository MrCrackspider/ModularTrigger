
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_OverlapEvent.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Generate Overlap Event")
class MODULARTRIGGER_API UTriggerComponentModule_OverlapEvent : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()
	
public:
	
	virtual void OnBeginPlay() override;
	virtual void OnEndPlay() override;
	
	UPrimitiveComponent* GetOverlapComponent() const;
	
	UFUNCTION(BlueprintCallable, Category = "Trigger Component")
	void SetOverlapComponent(UPrimitiveComponent* NewOverlapComponent);
	
	UPROPERTY(EditAnywhere, Category = "Trigger Component", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.PrimitiveComponent"))
	FComponentReference TargetOverlapComponent;

	/**
	 * Should this module check for overlap on begin play and broadcast overlap events for each overlapped target
	 */
	UPROPERTY(EditAnywhere)
	bool bUpdateOverlapsOnBeginPlay = true;

	/**
	 * Delay in seconds after begin play before this module starts listening for overlaps.
	 * 0 activates immediately. When bUpdateOverlapsOnBeginPlay is set, the initial overlap
	 * sweep happens once the delay elapses.
	 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ActivationDelay = 0.0f;

protected:
	
	/**
	 * Should this generator propagate overlap events per each of actor's overlapped component or 
	 * by actor itself only
	 */
	UPROPERTY(EditAnywhere)
	bool bOverlapOncePerActor = true;

	/**
	 * Should this module generate event only on the very first and the very last overlap.
	 * i.e. begin overlap when first actor enters trigger, end overlap when last actor leaves the trigger
	 */
	UPROPERTY(EditAnywhere)
	bool bOverlapOncePerAnyActor = false;
	
	void SetOverlapEnabled(bool NewEnabled);

	/** Enables overlap listening and, if requested, performs the initial overlap sweep. */
	void ActivateOverlap();

	UFUNCTION()
	void ReceiveComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void ReceiveComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, 
		bool bEndOverlap, const FHitResult* SweepResult = nullptr);
	
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> OverlappedActors;

	/** Timer used to defer activation by ActivationDelay seconds. */
	FTimerHandle ActivationTimerHandle;
};
