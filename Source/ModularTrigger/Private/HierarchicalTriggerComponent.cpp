
#include "HierarchicalTriggerComponent.h"

#include "HierarchicalTriggerModuleUtils.h"
#include "Modules/HierarchicalTriggerComponentModule.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

UHierarchicalTriggerComponent::UHierarchicalTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHierarchicalTriggerComponent::BeginPlay()
{
	FHierarchicalTriggerModuleUtils::ReceiveBeginPlay(Modules);
	Super::BeginPlay();
}

void UHierarchicalTriggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FHierarchicalTriggerModuleUtils::ReceiveEndPlay(Modules);
	Super::EndPlay(EndPlayReason);
}

void UHierarchicalTriggerComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsSaveGame())
	{
		FHierarchicalTriggerModuleUtils::SerializeSaveGame(Modules, Ar);
	}
}

void UHierarchicalTriggerComponent::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	// Backfill/dedupe stable module ids for assets saved before they existed. Runtime trusts serialized ids.
	EnsureModuleIds();
#endif
}

#if WITH_EDITOR
void UHierarchicalTriggerComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	// Covers adding/pasting modules at any depth (nested instanced edits route here on the owning component).
	EnsureModuleIds();
}

void UHierarchicalTriggerComponent::EnsureModuleIds()
{
	TSet<FGuid> SeenIds;

	TArray<UHierarchicalTriggerComponentModule*> Stack;
	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (IsValid(Module))
		{
			Stack.Add(Module);
		}
	}

	while (Stack.Num() > 0)
	{
		UHierarchicalTriggerComponentModule* Module = Stack.Pop();

		if (!Module->ModuleId.IsValid() || SeenIds.Contains(Module->ModuleId))
		{
			Module->ModuleId = FGuid::NewGuid();
		}
		SeenIds.Add(Module->ModuleId);

		for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Child : Module->Modules)
		{
			if (IsValid(Child))
			{
				Stack.Add(Child);
			}
		}
	}
}
#endif

UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponent::FindModuleByGuid(const FGuid& InModuleId) const
{
	return FHierarchicalTriggerModuleUtils::FindByGuid(Modules, InModuleId);
}

UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponent::FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth) const
{
	return FHierarchicalTriggerModuleUtils::FindByClass(Modules, Class, MaxSearchDepth);
}

TArray<UHierarchicalTriggerComponentModule*> UHierarchicalTriggerComponent::FindModulesByClass(
	TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth) const
{
	TArray<UHierarchicalTriggerComponentModule*> Result;
	FHierarchicalTriggerModuleUtils::FindAllByClass(Modules, Class, Result, MaxSearchDepth);
	return Result;
}


