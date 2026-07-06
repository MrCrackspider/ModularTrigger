// © Volga Studios. All rights reserved.

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
	
	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "On Trigger Event Received")
	void BPOnTriggerEventReceived();
	
	UFUNCTION(BlueprintPure)
	FInstancedStruct GetExecutionData(const UScriptStruct* Type);
	
	UFUNCTION(BlueprintCallable)
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
	
	UPROPERTY(Transient, BlueprintReadOnly)
	uint8 bPendingInterruption : 1 = false;
};
