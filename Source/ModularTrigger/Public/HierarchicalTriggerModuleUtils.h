
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"

class FArchive;
class UHierarchicalTriggerComponentModule;

/**
 * Stateless helpers for the shared operations over a hierarchical module array.
 */
struct MODULARTRIGGER_API FHierarchicalTriggerModuleUtils
{
	using FModuleArray = TArray<TObjectPtr<UHierarchicalTriggerComponentModule>>;

	/**
	 * Depth-first search for the first module of the given class.
	 * @param MaxSearchDepth How many levels below the immediate children to descend. <0 = unlimited, 0 = this level only.
	 */
	static UHierarchicalTriggerComponentModule* FindByClass(const FModuleArray& Modules,
		TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth = -1);

	/** Depth-first search for the module carrying the given stable id (unlimited depth). */
	static UHierarchicalTriggerComponentModule* FindByGuid(const FModuleArray& Modules, const FGuid& ModuleId);

	/** Forwards BeginPlay to each valid module. */
	static void ReceiveBeginPlay(const FModuleArray& Modules);

	/** Forwards EndPlay to each valid module. */
	static void ReceiveEndPlay(const FModuleArray& Modules);

	/** Serializes each valid module. */
	static void SerializeSaveGame(const FModuleArray& Modules, FArchive& Ar);
};
