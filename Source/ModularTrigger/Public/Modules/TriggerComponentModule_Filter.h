
#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_Filter.generated.h"

/**
 * Superseded by UTriggerComponentModule_WorldConditionFilter, which does the same job through the engine's
 * World Conditions framework and gets AND / OR, parentheses, per-row inversion and result caching for free.
 *
 * Kept because levels are authored against it: the two store entirely different data, so nothing migrates on
 * its own. Prefer the new module for new work, and port an existing filter by re-authoring its entries as
 * conditions — the plugin's own entries all have an equivalent in WorldConditions/TriggerWorldConditions.h.
 */

USTRUCT(meta=(HideDropdown))
struct MODULARTRIGGER_API FFilterModuleEntry
{
	GENERATED_BODY()
	
	bool operator()(const FTriggerExecutionContext& Context) const
	{
		return ContextPassesFilter(Context) ^ bInvert;
	}

	UPROPERTY(EditAnywhere)
	uint8 bInvert : 1 = false;

	virtual ~FFilterModuleEntry() = default;

protected:

	virtual bool ContextPassesFilter(const FTriggerExecutionContext& Context) const { return false; }
};

USTRUCT(DisplayName = "Actor is any of Classes")
struct FFilterModuleEntry_Class : public FFilterModuleEntry
{
	GENERATED_BODY()

	virtual bool ContextPassesFilter(const FTriggerExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<AActor>> Classes;
};

USTRUCT(DisplayName = "Actor is Player controlled")
struct FFilterModuleEntry_Player : public FFilterModuleEntry
{
	GENERATED_BODY()

	virtual bool ContextPassesFilter(const FTriggerExecutionContext& Context) const override;
};

USTRUCT(DisplayName = "Actor Has Tag")
struct FFilterModuleEntry_ActorTags : public FFilterModuleEntry
{
	GENERATED_BODY()

	virtual bool ContextPassesFilter(const FTriggerExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere)
	TArray<FName> Tags;
};

USTRUCT(DisplayName = "Filter By Reference")
struct FFilterModuleEntry_ExactActors : public FFilterModuleEntry
{
	GENERATED_BODY()

	virtual bool ContextPassesFilter(const FTriggerExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> Targets;
};

USTRUCT(DisplayName = "Is End Overlap")
struct FFilterModuleEntry_EndOverlap : public FFilterModuleEntry
{
	GENERATED_BODY()

	virtual bool ContextPassesFilter(const FTriggerExecutionContext& Context) const override;
};

/**
 * Keeps the event from reaching its children unless the trigger execution context satisfies the filter
 * requirements. Guards this module's subtree only, so the modules it is meant to gate belong underneath it.
 *
 * @see UTriggerComponentModule_WorldConditionFilter, which supersedes this.
 */
UCLASS(DisplayName = "Filter")
class MODULARTRIGGER_API UTriggerComponentModule_Filter : public UHierarchicalTriggerComponentModule
{
	GENERATED_BODY()
	
public:

	/**
	 * Should actor pass all filters or any filter
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bAllFiltersShouldBePassed : 1 = false;

	UPROPERTY(EditAnywhere, meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FFilterModuleEntry>> Filters;

protected:

	//~ Begin UHierarchicalTriggerComponentModule interface
	virtual bool ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const override;
	//~ End UHierarchicalTriggerComponentModule interface
};
