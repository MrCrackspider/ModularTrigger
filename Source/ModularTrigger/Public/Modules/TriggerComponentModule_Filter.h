// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponentModule.h"
#include "TriggerComponentModule_Filter.generated.h"

/**
 * This whole section should better be made using WorldConditions
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
 * Stops overlap event propagation if trigger execution context do not satisfy filter requirements
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
	
	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) override;
};
