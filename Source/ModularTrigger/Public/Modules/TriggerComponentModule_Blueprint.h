
#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_Blueprint.generated.h"


/**
 * 
 */
UCLASS(Blueprintable, Abstract, DisplayName = "Trigger Component Module Blueprintable")
class MODULARTRIGGER_API UTriggerComponentModule_Blueprint : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()
	
protected:

	//~ Begin UHierarchicalTriggerComponentModule interface
	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
	virtual bool ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const override;
	//~ End UHierarchicalTriggerComponentModule interface

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "On Trigger Event Received")
	void BPOnTriggerEventReceived();

	UFUNCTION(BlueprintPure)
	FInstancedStruct GetExecutionData(const UScriptStruct* Type);

	/**
	 * Keeps the event from reaching this module's children. Affects nothing else: the modules beside this one
	 * and their children still receive the event.
	 */
	UFUNCTION(BlueprintCallable)
	void StopPropagationToChildren();

	UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage =
		"Use Stop Propagation To Children. Interruption only ever skipped this module's children, never the rest of the event."))
	void SetPendingInterruption();

	UFUNCTION(BlueprintCallable)
	void AddExecutionData(const FInstancedStruct& Data);

	//UPROPERTY()
	FInstancedStructContainer InExecutionData;
	
	//UPROPERTY()
	FInstancedStructContainer OutExecutionData;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	const UHierarchicalTriggerComponent* OwningTriggerComponent;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	const UPrimitiveComponent* OverlappedComponent;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	const UPrimitiveComponent* OtherComponent;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	FHitResult SweepResult;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	uint8 bHasSweepResult : 1 = false;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	uint8 bEndOverlap : 1 = false;

	/** Set by StopPropagationToChildren during the event, read by ShouldPropagateEvent right after it. */
	UPROPERTY(Transient, BlueprintReadOnly)
	uint8 bPropagationStopped : 1 = false;
};
