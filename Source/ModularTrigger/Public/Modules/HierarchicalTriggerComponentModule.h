// © Volga Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HierarchicalTriggerComponent.h"
#include "Misc/Guid.h"
#include "StructUtils/InstancedStructContainer.h"
#include "UObject/Object.h"
#include "HierarchicalTriggerComponentModule.generated.h"

class UHierarchicalTriggerComponent;

struct FTriggerExecutionContext
{
	FTriggerExecutionContext(TNotNull<UHierarchicalTriggerComponent*> OwningTriggerComponent, TNotNull<UPrimitiveComponent*> OverlappedComponent, 
	TNotNull<UPrimitiveComponent*> OtherComponent, bool bEndOverlap, const FHitResult* InSweepResult = nullptr);
	
protected:
	/**
	 * Arbitrary data which can be written or read by modules. 
	 * @note Unique by type
	 */
	FInstancedStructContainer ExecutionData;
	
	TNotNull<const UHierarchicalTriggerComponent*> OwningTriggerComponent;
	
	TNotNull<const UPrimitiveComponent*> OverlappedComponent;
	
	TNotNull<const UPrimitiveComponent*> OtherComponent;

	const FHitResult* SweepResult;
	
	uint8 bEndOverlap : 1 = false;
	
	uint8 bPendingInterruption : 1 = false;
	
public:
	
	const UWorld* GetWorld() const { return OwningTriggerComponent->GetWorld(); }
	const FInstancedStructContainer& GetExecutionData() const { return ExecutionData; }
	TNotNull<const UHierarchicalTriggerComponent*> GetOwningComponent() const { return OwningTriggerComponent; }
	TNotNull<const AActor*> GetOtherActor() const { return GetOtherComponent()->GetOwner(); }
	TNotNull<const AActor*> GetOwningActor() const { return GetOwningComponent()->GetOwner(); }
	TNotNull<const UPrimitiveComponent*> GetOtherComponent() const { return OtherComponent; }
	TNotNull<const UPrimitiveComponent*> GetOverlappedComponent() const { return OverlappedComponent; }
	const FHitResult* GetSweepResult() const { return SweepResult; }
	bool HasSweepResult() const { return SweepResult != nullptr; }
	bool IsEndOverlap() const { return bEndOverlap; }
	
	bool IsPendingInterruption() const { return bPendingInterruption; }

	/**
	 * Sets this event to pending interruption. Stops further propagation of this event
	 */
	MODULARTRIGGER_API void SetPendingInterruption();

	/**
	 * Finds data in this context 
	 * @tparam T Data type to search
	 * @return Pointer to found data or nullptr
	 */
	template<typename T>
	const T* GetExecutionDataPtr() const
	{
		for (FConstStructView Data : ExecutionData)
		{
			if (const T* Item = Data.GetPtr<const T>())
			{
				return Item;
			}
		}
		return nullptr;
	}

	/**
	 * Adds or updates data to this context.
	 * @param InData Data to add to this context
	 */
	MODULARTRIGGER_API void AddExecutionData(FConstStructView InData);
	
	// TODO: Testing
	template<typename T>
	T& GetOrAddExecutionData()
	{
		T* Data = GetExecutionDataPtr<T>();
		if (Data)
		{
			return *Data;
		}
		
		T NewData{};
		ExecutionData.Append({ NewData });
		return *GetExecutionDataPtr<T>();
	}
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
	
	void PropagateEvent(FTriggerExecutionContext& TriggerExecutionContext);
	
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
