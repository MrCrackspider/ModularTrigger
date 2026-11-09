
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
	FInstancedStruct GetExecutionData(const UScriptStruct* Type) const;

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

	/** Reports diagnostics for the current synchronous event; does not control propagation. */
	UFUNCTION(BlueprintCallable, Category = "Trigger Debug")
	void ReportExecutionResult(ETriggerExecutionResult Result, const FString& Message);

	UFUNCTION(BlueprintPure, Category = "Trigger Debug")
	bool IsExecutionTracing() const;

	/** Context access is available only during the synchronous On Trigger Event Received event. */
	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	bool HasExecutionContext() const;

	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	const UHierarchicalTriggerComponent* GetOwningTriggerComponent() const;

	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	const UPrimitiveComponent* GetOverlappedComponent() const;

	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	const UPrimitiveComponent* GetOtherComponent() const;

	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	FHitResult GetSweepResult() const;

	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	bool HasSweepResult() const;

	UFUNCTION(BlueprintPure, Category = "Trigger Context")
	bool IsEndOverlap() const;

	/** Set by StopPropagationToChildren during the event, read by ShouldPropagateEvent right after it. */
	UPROPERTY(Transient, BlueprintReadOnly)
	uint8 bPropagationStopped : 1 = false;

private:
	friend class FTriggerBlueprintExecutionContextTest;

	// Borrowed only while BPOnTriggerEventReceived is executing. Never retained by latent callbacks.
	// TGuardValue restores the outer event's context after a nested invocation on this same module.
	FTriggerExecutionContext* ActiveExecutionContext = nullptr;
};
