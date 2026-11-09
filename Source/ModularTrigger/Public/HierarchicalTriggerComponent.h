
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Misc/Guid.h"
#include "HierarchicalTriggerComponent.generated.h"

class UHierarchicalTriggerComponentModule;

UCLASS(ClassGroup=(Trigger), meta=(BlueprintSpawnableComponent), DisplayName = "Hierarchical Trigger Component")
class MODULARTRIGGER_API UHierarchicalTriggerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHierarchicalTriggerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType = Class))
	UHierarchicalTriggerComponentModule* FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth = -1) const;

	template<typename ObjType>
	ObjType* FindModuleByClass() const;

	/** Finds all matching modules, including subclasses, in depth-first order. Returns empty for a null class.
	 * MaxSearchDepth: -1 = unlimited, 0 = root modules only, 1 = roots and their immediate children.
	 */
	UFUNCTION(BlueprintPure, Category = "Trigger Component", meta = (DeterminesOutputType = "Class"))
	TArray<UHierarchicalTriggerComponentModule*> FindModulesByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth = -1) const;

	template<typename ModuleType>
	TArray<ModuleType*> FindModulesByClass(int32 MaxSearchDepth = -1) const;

	/**
	 * Recursively finds the module carrying the given stable id anywhere in this component's tree.
	 * @param InModuleId Stable identity to look for.
	 * @return The matching module, or nullptr.
	 */
	UHierarchicalTriggerComponentModule* FindModuleByGuid(const FGuid& InModuleId) const;

#if WITH_EDITOR
	/**
	 * Walks the whole module tree and guarantees the invariant "every module has a unique, non-zero ModuleId":
	 * any module whose id is invalid or a duplicate of one already seen is reassigned a fresh id.
	 * Editor-only authoring helper; runtime trusts the ids serialized with the asset.
	 */
	void EnsureModuleIds();
#endif

	UPROPERTY(EditAnywhere, Instanced, Category = "Trigger Component")
	TArray<TObjectPtr<class UHierarchicalTriggerComponentModule>> Modules;
};

template <typename ObjType>
ObjType* UHierarchicalTriggerComponent::FindModuleByClass() const
{
	return (ObjType*)FindModuleByClass(ObjType::StaticClass());
}

template<typename ModuleType>
TArray<ModuleType*> UHierarchicalTriggerComponent::FindModulesByClass(int32 MaxSearchDepth) const
{
	const TArray<UHierarchicalTriggerComponentModule*> Matches = FindModulesByClass(ModuleType::StaticClass(), MaxSearchDepth);
	TArray<ModuleType*> Result;
	Result.Reserve(Matches.Num());
	for (UHierarchicalTriggerComponentModule* Module : Matches)
	{
		Result.Add(static_cast<ModuleType*>(Module));
	}
	return Result;
}
