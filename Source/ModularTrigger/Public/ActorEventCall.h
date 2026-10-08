
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructUtils/PropertyBag.h"
#include "ActorEventCall.generated.h"

class AActor;

/**
 * Serializable, editor-authored call to a Blueprint event (or BlueprintCallable function) on a specific actor.
 */
USTRUCT(BlueprintType)
struct MODULARTRIGGER_API FActorEventCall
{
	GENERATED_BODY()

	/**
	 * Call the actor supplied at execution time (the context actor) instead of a stored TargetActor.
	 *
	 * Nothing per-instance is stored in this mode, so the call can be authored once on a Blueprint's defaults
	 * and every placed instance calls the event on itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Event Call")
	bool bUseContextActor = false;

	/** Actor whose event will be called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Event Call", meta = (EditCondition = "!bUseContextActor", EditConditionHides))
	TObjectPtr<AActor> TargetActor;

	/** Name of the UFunction on TargetActor's class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Event Call")
	FName EventName;

	/**
	 * Values for the event's parameters, keyed by parameter name.
	 */
	UPROPERTY(EditAnywhere, Category = "Actor Event Call", meta = (ChildRowFeatures = "Fixed"))
	FInstancedPropertyBag Parameters;

	/** True once an event has been picked. Does not guarantee the event still exists. */
	bool IsSet() const { return !EventName.IsNone(); }

	/**
	 * Resolves and invokes the event with the authored parameter values.
	 * @param ContextActor Target used when bUseContextActor is set; ignored otherwise.
	 * @return True if the event was resolved and dispatched.
	 */
	bool Execute(AActor* ContextActor = nullptr) const;

	/**
	 * @return The actor this call will target: ContextActor when bUseContextActor is set, TargetActor otherwise.
	 *         bUseContextActor is the only switch between the two; an unset TargetActor does not silently fall
	 *         back to ContextActor, so what the details panel offers is what the call will run against.
	 */
	AActor* ResolveTargetActor(AActor* ContextActor = nullptr) const;

	/**
	 * Class-only counterpart of ResolveTargetActor, for editor code that authors a call against a class
	 * rather than against a live actor.
	 */
	UClass* ResolveTargetClass(UClass* ContextActorClass = nullptr) const;

	/** @return The UFunction named by EventName on Actor's class, or nullptr. */
	static UFunction* ResolveFunction(const AActor* Actor, FName EventName);

	/** @return The UFunction named by EventName on ActorClass, or nullptr. */
	static UFunction* ResolveFunction(const UClass* ActorClass, FName EventName);

#if WITH_EDITOR
	
	/**
	 * @return True if Function can be driven by this struct: Blueprint-callable, non-static, no return value,
	 *         no out parameters, not latent, and every parameter is representable in a property bag.
	 */
	static bool IsCallableEvent(const UFunction* Function);
	
#endif

	/** Fills OutDescs with one descriptor per input parameter of Function, in declaration order. */
	static void BuildParameterDescs(const UFunction* Function, TArray<FPropertyBagPropertyDesc>& OutDescs);

	/**
	 * Rebuilds Parameters to match the current signature of EventName on the target class, preserving the values
	 * of parameters whose name and type are unchanged. Clears the bag when the event cannot be resolved.
	 */
	void SyncParametersToSignature(UClass* ContextActorClass = nullptr);
};

UCLASS()
class UActorEventCallBlueprintUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Actor Event Call", meta = (AdvancedDisplay = "ContextActor"))
	static bool ExecuteActorEventCall(const FActorEventCall& ActorEventCall, AActor* ContextActor = nullptr);
};
