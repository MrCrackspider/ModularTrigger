
#include "TriggerModuleReference.h"

#include "GameFramework/Actor.h"
#include "HierarchicalTriggerComponent.h"
#include "Modules/HierarchicalTriggerComponentModule.h"

UHierarchicalTriggerComponent* FTriggerModuleReference::ResolveComponent(AActor* ContextActor) const
{
	return Cast<UHierarchicalTriggerComponent>(TriggerComponent.GetComponent(ContextActor));
}

UHierarchicalTriggerComponentModule* FTriggerModuleReference::Resolve(AActor* ContextActor) const
{
	if (UHierarchicalTriggerComponentModule* Cached = CachedModule.Get())
	{
		return Cached;
	}

	if (!ModuleId.IsValid())
	{
		return nullptr;
	}

	UHierarchicalTriggerComponentModule* Found = nullptr;
	if (UHierarchicalTriggerComponent* Component = ResolveComponent(ContextActor))
	{
		Found = Component->FindModuleByGuid(ModuleId);
	}

	CachedModule = Found;
	return Found;
}

UHierarchicalTriggerComponentModule* FTriggerModuleReference::Resolve() const
{
	return Resolve(TriggerComponent.OtherActor.Get());
}

UHierarchicalTriggerComponentModule* UHierarchicalTriggerBlueprintUtils::Resolve(const FTriggerModuleReference& TriggerModuleReference)
{
	return TriggerModuleReference.Resolve();
}
