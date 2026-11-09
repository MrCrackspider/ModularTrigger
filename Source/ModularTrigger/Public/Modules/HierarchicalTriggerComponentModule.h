
#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "HierarchicalTriggerComponent.h"
#include "Misc/Guid.h"
#include "StructUtils/InstancedStructContainer.h"
#include "UObject/Object.h"
#include "TriggerExecutionTrace.h"
#include "HierarchicalTriggerComponentModule.generated.h"

class FTriggerVisualizationBuilder;
class UHierarchicalTriggerComponent;
struct FTriggerExecutionDataScope;

struct FTriggerExecutionContext
{
	MODULARTRIGGER_API FTriggerExecutionContext(TNotNull<UHierarchicalTriggerComponent*> OwningTriggerComponent, TNotNull<UPrimitiveComponent*> OverlappedComponent,
	TNotNull<UPrimitiveComponent*> OtherComponent, bool bEndOverlap, const FHitResult* InSweepResult = nullptr);
	MODULARTRIGGER_API ~FTriggerExecutionContext();

	FTriggerExecutionContext(const FTriggerExecutionContext&) = delete;
	FTriggerExecutionContext& operator=(const FTriggerExecutionContext&) = delete;

protected:
	/**
	 * Arbitrary data which can be written or read by modules, scoped to the module tree the way a block scopes
	 * local variables: a module writes into the list of siblings it belongs to, which its following siblings and
	 * every descendant of theirs can read, and which is discarded once that list has been walked.
	 * @note Unique by type within a scope; an outer scope's value of the same type is shadowed, not replaced.
	 * @see FTriggerExecutionDataScope
	 */
	FInstancedStructContainer ExecutionData;

	/** First index in ExecutionData owned by the innermost open scope. Everything before it is inherited. */
	int32 CurrentScopeStart = 0;

	TNotNull<const UHierarchicalTriggerComponent*> OwningTriggerComponent;

	TNotNull<const UPrimitiveComponent*> OverlappedComponent;

	TNotNull<const UPrimitiveComponent*> OtherComponent;

	const FHitResult* SweepResult;

	uint8 bEndOverlap : 1 = false;

public:

	MODULARTRIGGER_API bool IsExecutionTracing() const;
	/** Diagnostics only: reporting never changes event propagation or gameplay results. */
	MODULARTRIGGER_API void ReportExecutionResult(ETriggerExecutionResult Result, const FString& Message) const;
	MODULARTRIGGER_API void ReportExecutionMessage(const FString& Message) const;
	MODULARTRIGGER_API void ReportPropagation(bool bChildrenEnabled, bool bGateOpen) const;

	const UWorld* GetWorld() const { return OwningTriggerComponent->GetWorld(); }
	/** Every value visible from the module currently executing: its own scope and those of its ancestors. */
	const FInstancedStructContainer& GetExecutionData() const { return ExecutionData; }
	TNotNull<const UHierarchicalTriggerComponent*> GetOwningComponent() const { return OwningTriggerComponent; }
	/** Out of line: UPrimitiveComponent is only forward declared here. */
	MODULARTRIGGER_API TNotNull<const AActor*> GetOtherActor() const;
	TNotNull<const AActor*> GetOwningActor() const { return GetOwningComponent()->GetOwner(); }
	TNotNull<const UPrimitiveComponent*> GetOtherComponent() const { return OtherComponent; }
	TNotNull<const UPrimitiveComponent*> GetOverlappedComponent() const { return OverlappedComponent; }
	const FHitResult* GetSweepResult() const { return SweepResult; }
	bool HasSweepResult() const { return SweepResult != nullptr; }
	bool IsEndOverlap() const { return bEndOverlap; }

	/**
	 * Finds the nearest value of the given type visible from the module currently executing: its own scope
	 * first, then outwards through the scopes of its ancestors.
	 * @tparam T Data type to search
	 * @return Pointer to found data or nullptr
	 */
	template<typename T>
	const T* GetExecutionDataPtr() const
	{
		for (int32 Index = ExecutionData.Num() - 1; Index >= 0; --Index)
		{
			if (const T* Item = ExecutionData[Index].GetPtr<const T>())
			{
				return Item;
			}
		}
		return nullptr;
	}

	/**
	 * Writes data into the scope of the module currently executing, replacing that scope's value of the same
	 * type if it already has one. A value of the same type held by an outer scope is shadowed rather than
	 * overwritten, so a module can never corrupt the data an ancestor is still reading.
	 * @param InData Data to add to this context
	 */
	MODULARTRIGGER_API void AddExecutionData(FConstStructView InData);

	/**
	 * Returns a writable value of the given type owned by the scope of the module currently executing, creating
	 * one if that scope does not have it yet. A created value starts as a copy of the nearest value visible from
	 * an outer scope, or default constructed when there is none — so a module reads what its ancestors provided
	 * while its own writes stay inside its scope.
	 *
	 * @warning The reference is invalidated by any further AddExecutionData or GetOrAddExecutionData call on this
	 *          context, including one made by a module further down the tree: the values share one contiguous
	 *          block of memory that is relocated as it grows. Finish with it before adding anything else.
	 */
	template<typename T>
	T& GetOrAddExecutionData()
	{
		for (int32 Index = ExecutionData.Num() - 1; Index >= CurrentScopeStart; --Index)
		{
			if (T* OwnData = ExecutionData[Index].GetPtr<T>())
			{
				return *OwnData;
			}
		}

		T NewData{};
		for (int32 Index = CurrentScopeStart - 1; Index >= 0; --Index)
		{
			if (const T* InheritedData = ExecutionData[Index].GetPtr<T>())
			{
				NewData = *InheritedData;
				break;
			}
		}

		ExecutionData.Append({ FConstStructView::Make(NewData) });

		T* AddedData = ExecutionData[ExecutionData.Num() - 1].GetPtr<T>();
		check(AddedData);
		return *AddedData;
	}

private:

	friend struct FTriggerExecutionDataScope;
#if UE_DEBUG_RECORDING_ENABLED
	friend struct FTriggerModuleTraceScope;
	TUniquePtr<FTriggerExecutionTrace> ExecutionTrace;
#endif
};

/**
 * Opens a nested execution-data scope on a context for as long as it is alive.
 *
 * Data written while it is open belongs to it, is visible to it and to everything nested inside it, and is
 * discarded when it closes — which is what keeps one branch of the module tree from reading or clobbering
 * the data of a branch that has already finished.
 *
 * Scopes close in the order they opened, which the module tree walk guarantees.
 */
struct FTriggerExecutionDataScope
{
	MODULARTRIGGER_API explicit FTriggerExecutionDataScope(FTriggerExecutionContext& InContext);
	MODULARTRIGGER_API ~FTriggerExecutionDataScope();

	FTriggerExecutionDataScope(const FTriggerExecutionDataScope&) = delete;
	FTriggerExecutionDataScope& operator=(const FTriggerExecutionDataScope&) = delete;

private:

	FTriggerExecutionContext& Context;

	/** Scope start to restore on close; the data added past it is dropped. */
	int32 OuterScopeStart = 0;
};

/**
 * 
 */
UCLASS(DefaultToInstanced, EditInlineNew, CollapseCategories, Abstract, HideDropdown)
class MODULARTRIGGER_API UHierarchicalTriggerComponentModule : public UObject
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintPure)
	UHierarchicalTriggerComponent* GetOwningComponent() const;
	
	UFUNCTION(BlueprintPure)
	UHierarchicalTriggerComponentModule* GetOuterModule() const;
	
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType = Class))
	UHierarchicalTriggerComponentModule* FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth = -1) const;

	template<typename ObjType>
	ObjType* FindModuleByClass(int32 MaxSearchDepth = -1) const;

	/** Finds all matching descendants, including subclasses, in depth-first order; excludes this module.
	 * MaxSearchDepth: -1 = unlimited, 0 = immediate children only. Returns empty for a null class.
	 */
	UFUNCTION(BlueprintPure, Category = "Trigger Component", meta = (DeterminesOutputType = "Class"))
	TArray<UHierarchicalTriggerComponentModule*> FindModulesByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth = -1) const;

	template<typename ModuleType>
	TArray<ModuleType*> FindModulesByClass(int32 MaxSearchDepth = -1) const;

	/**
	 * Recursively searches this module's subtree for a module carrying the given stable id.
	 * @param InModuleId Stable identity to look for. Checks this module first, then descends into children.
	 * @return The matching module, or nullptr.
	 */
	UHierarchicalTriggerComponentModule* FindModuleByGuid(const FGuid& InModuleId) const;

	/** Stable identity of this module within its owning component's tree. @see FTriggerModuleReference. */
	const FGuid& GetModuleId() const { return ModuleId; }
	
	virtual UWorld* GetWorld() const override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	/**
	 * Describes what this module should draw in the level viewport while its actor is selected.
	 *
	 * Purely declarative: fill the builder with shapes and text and the component visualizer renders them,
	 * so every module is free to describe itself however it likes — a module pointing at actors draws lines
	 * to them, one with a radius draws a sphere, one with nothing spatial to say draws only text, and the
	 * default is to draw nothing at all.
	 *
	 * Called once per viewport per frame for every module of a selected trigger component, so keep it cheap
	 * and free of side effects.
	 */
	virtual void DescribeVisualization(FTriggerVisualizationBuilder& Builder) const {}
#endif

	/**
	 * Should this module react to incoming overlap event
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	uint8 bEnabled : 1 = true;

	/**
	 * Should children of this module receive incoming overlap event
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	uint8 bChildrenEnabled : 1 = true;

	/**
	 * Should this module and its children be deactivated after first overlap
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	uint8 bTriggerOnce : 1 = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TArray<TObjectPtr<UHierarchicalTriggerComponentModule>> Modules;

	/**
	 * Stable identity assigned by the owning component and kept unique across its module tree.
	 * Used by FTriggerModuleReference to point at this module regardless of sibling reordering.
	 * Read-only in the editor; assigned/repaired by UHierarchicalTriggerComponent::EnsureModuleIds.
	 */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Trigger Component")
	FGuid ModuleId;

protected:
	
	virtual void OnBeginPlay() {}
	virtual void OnEndPlay() {}
	virtual void OnTriggerEventReceived(FTriggerExecutionContext& TriggerExecutionContext) {}

	/**
	 * Gate for this module's own subtree: returning false means the event is not propagated to this module's
	 * children. Nothing outside the subtree is affected — the modules beside this one and their children still
	 * receive the event — so a module that guards a set of actions has to be their parent, not their neighbour.
	 *
	 * Asked once per event, after OnTriggerEventReceived and only when this module is enabled: a disabled module
	 * is a pass-through. A module that has to do work before it can decide does that work in the handler and
	 * answers from what it recorded there.
	 */
	virtual bool ShouldPropagateEvent(const FTriggerExecutionContext& TriggerExecutionContext) const { return true; }

	void PropagateEvent(FTriggerExecutionContext& TriggerExecutionContext);
	/** Entry point for generators, which do not receive their own event through ReceiveTriggerEvent. */
	void DispatchGeneratedEvent(FTriggerExecutionContext& TriggerExecutionContext);
	
private:

	friend UHierarchicalTriggerComponent;
	friend struct FHierarchicalTriggerModuleUtils;
	void ReceiveBeginPlay();
	void ReceiveEndPlay();
	void ReceiveTriggerEvent(FTriggerExecutionContext& TriggerExecutionContext);
};

template <typename ObjType>
ObjType* UHierarchicalTriggerComponentModule::FindModuleByClass(int32 MaxSearchDepth) const
{
	return (ObjType*)FindModuleByClass(ObjType::StaticClass(), MaxSearchDepth);
}

template<typename ModuleType>
TArray<ModuleType*> UHierarchicalTriggerComponentModule::FindModulesByClass(int32 MaxSearchDepth) const
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
