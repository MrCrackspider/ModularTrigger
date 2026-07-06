
#include "HierarchicalTriggerComponent.h"

#include "Modules/HierarchicalTriggerComponentModule.h"

UHierarchicalTriggerComponent::UHierarchicalTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHierarchicalTriggerComponent::BeginPlay()
{
	for (TObjectPtr<class UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveBeginPlay();
		}
	}
	Super::BeginPlay();
}

void UHierarchicalTriggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (TObjectPtr<class UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveEndPlay();
		}
	}
	Super::EndPlay(EndPlayReason);
}

// TODO: Remove copy from UHierarchicalTriggerComponentModule
UHierarchicalTriggerComponentModule* UHierarchicalTriggerComponent::FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth) const
{
	for (TObjectPtr<UHierarchicalTriggerComponentModule> Module : Modules)
	{
		if (IsValid(Module))
		{
			if (Module.IsA(Class))
			{
				return Module;
			}
			if (MaxSearchDepth == -1 || MaxSearchDepth > 0)
			{
				if (UHierarchicalTriggerComponentModule* OutModule = Module->FindModuleByClass(Class, MaxSearchDepth == -1 ? MaxSearchDepth : --MaxSearchDepth))
				{
					if (OutModule)
					{
						return OutModule;
					}
				}
			}
		}
	}
	
	return nullptr;
}


