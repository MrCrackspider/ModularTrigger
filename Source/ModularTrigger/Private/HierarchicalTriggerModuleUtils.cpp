// © Volga Studios. All rights reserved.

#include "HierarchicalTriggerModuleUtils.h"

#include "Modules/HierarchicalTriggerComponentModule.h"

UHierarchicalTriggerComponentModule* FHierarchicalTriggerModuleUtils::FindByClass(const FModuleArray& Modules,
	TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth)
{
	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (!IsValid(Module))
		{
			continue;
		}

		if (Module->IsA(Class))
		{
			return Module;
		}

		if (MaxSearchDepth != 0)
		{
			const int32 ChildDepth = (MaxSearchDepth < 0) ? MaxSearchDepth : MaxSearchDepth - 1;
			if (UHierarchicalTriggerComponentModule* Found = FindByClass(Module->Modules, Class, ChildDepth))
			{
				return Found;
			}
		}
	}

	return nullptr;
}

UHierarchicalTriggerComponentModule* FHierarchicalTriggerModuleUtils::FindByGuid(const FModuleArray& Modules, const FGuid& ModuleId)
{
	if (!ModuleId.IsValid())
	{
		return nullptr;
	}

	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (!IsValid(Module))
		{
			continue;
		}

		if (Module->GetModuleId() == ModuleId)
		{
			return Module;
		}

		if (UHierarchicalTriggerComponentModule* Found = FindByGuid(Module->Modules, ModuleId))
		{
			return Found;
		}
	}

	return nullptr;
}

void FHierarchicalTriggerModuleUtils::ReceiveBeginPlay(const FModuleArray& Modules)
{
	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveBeginPlay();
		}
	}
}

void FHierarchicalTriggerModuleUtils::ReceiveEndPlay(const FModuleArray& Modules)
{
	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (IsValid(Module))
		{
			Module->ReceiveEndPlay();
		}
	}
}

void FHierarchicalTriggerModuleUtils::SerializeSaveGame(const FModuleArray& Modules, FArchive& Ar)
{
	for (const TObjectPtr<UHierarchicalTriggerComponentModule>& Module : Modules)
	{
		if (Module)
		{
			Module->Serialize(Ar);
		}
	}
}
