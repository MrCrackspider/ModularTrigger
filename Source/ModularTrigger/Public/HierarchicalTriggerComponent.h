
#pragma once

#include "CoreMinimal.h"
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
	
	UFUNCTION(BlueprintPure, meta=(DeterminesOutputType = Class))
	UHierarchicalTriggerComponentModule* FindModuleByClass(TSubclassOf<UHierarchicalTriggerComponentModule> Class, int32 MaxSearchDepth = -1) const;
	
	template<typename ObjType>
	ObjType* FindModuleByClass() const;
	
	UPROPERTY(EditAnywhere, Instanced, Category = "Trigger Component")
	TArray<TObjectPtr<class UHierarchicalTriggerComponentModule>> Modules;
};

template <typename ObjType>
ObjType* UHierarchicalTriggerComponent::FindModuleByClass() const
{
	return (ObjType*)FindModuleByClass(ObjType::StaticClass());
}
