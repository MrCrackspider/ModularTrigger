
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "TriggerModuleReference.generated.h"

class AActor;
class UHierarchicalTriggerComponent;
class UHierarchicalTriggerComponentModule;

/**
 * Serializable, editor-pickable reference to a specific (sub)module inside a UHierarchicalTriggerComponent.
 */
USTRUCT(BlueprintType)
struct MODULARTRIGGER_API FTriggerModuleReference
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module Reference",
		meta = (UseComponentPicker, AllowAnyActor, AllowedClasses = "/Script/ModularTrigger.HierarchicalTriggerComponent"))
	FComponentReference TriggerComponent;

	/** Stable identity of the target module within TriggerComponent's tree. Written by the module picker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Module Reference")
	FGuid ModuleId;

	/**
	 * Resolves the referenced component.
	 * @param ContextActor Actor used to resolve a same-actor reference (i.e. when TriggerComponent.OtherActor is unset);
	 *                     typically the owner of the object that holds this reference.
	 * @return The component, or nullptr.
	 */
	UHierarchicalTriggerComponent* ResolveComponent(AActor* ContextActor) const;

	/**
	 * Resolves the referenced module, caching the result for subsequent calls.
	 * @param ContextActor See ResolveComponent.
	 * @return The module, or nullptr.
	 */
	UHierarchicalTriggerComponentModule* Resolve(AActor* ContextActor) const;
	
	UHierarchicalTriggerComponentModule* Resolve() const;

	/** True if a module has been selected. Does not guarantee the module still exists. */
	bool IsSet() const { return ModuleId.IsValid(); }

private:
	/** Transient resolve cache; not reflected/serialized. */
	mutable TWeakObjectPtr<UHierarchicalTriggerComponentModule> CachedModule;
};

UCLASS()
class UHierarchicalTriggerBlueprintUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	
	UFUNCTION(BlueprintPure, Category = "Trigger Module Reference", meta=(CompactNodeTitle = "Get", Keywords = "Get"))
	static UHierarchicalTriggerComponentModule* Resolve(const FTriggerModuleReference& TriggerModuleReference);
	
};
