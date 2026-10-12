
#include "WorldConditions/TriggerWorldConditionSchema.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "HierarchicalTriggerComponent.h"
#include "Modules/HierarchicalTriggerComponentModule.h"
#include "WorldConditionContext.h"
#include "WorldConditions/TriggerWorldConditions.h"

namespace UE::ModularTrigger::ContextDataNames
{
	const FName TriggerComponent(TEXT("TriggerComponent"));
	const FName TriggerActor(TEXT("TriggerActor"));
	const FName OtherActor(TEXT("OtherActor"));
	const FName OtherComponent(TEXT("OtherComponent"));
	const FName Event(TEXT("Event"));
}

UTriggerWorldConditionSchema::UTriggerWorldConditionSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace UE::ModularTrigger;

	// The names are what an authored condition stores, so they are part of the saved data: renaming one
	// silently unresolves every reference pointing at it.
	TriggerComponentRef = AddContextDataDesc(ContextDataNames::TriggerComponent, UHierarchicalTriggerComponent::StaticClass(), EWorldConditionContextDataType::Persistent);
	TriggerActorRef = AddContextDataDesc(ContextDataNames::TriggerActor, AActor::StaticClass(), EWorldConditionContextDataType::Persistent);
	OtherActorRef = AddContextDataDesc(ContextDataNames::OtherActor, AActor::StaticClass(), EWorldConditionContextDataType::Dynamic);
	OtherComponentRef = AddContextDataDesc(ContextDataNames::OtherComponent, UPrimitiveComponent::StaticClass(), EWorldConditionContextDataType::Dynamic);
	EventRef = AddContextDataDesc(ContextDataNames::Event, FTriggerWorldConditionEvent::StaticStruct(), EWorldConditionContextDataType::Dynamic);
}

void UTriggerWorldConditionSchema::SetPersistentContextData(FWorldConditionContextData& ContextData,
	const UHierarchicalTriggerComponent& TriggerComponent) const
{
	ContextData.SetContextData(TriggerComponentRef, &TriggerComponent);
	ContextData.SetContextData(TriggerActorRef, TriggerComponent.GetOwner());
}

void UTriggerWorldConditionSchema::SetContextData(FWorldConditionContextData& ContextData,
	const FTriggerExecutionContext& ExecutionContext, const FTriggerWorldConditionEvent& Event) const
{
	SetPersistentContextData(ContextData, *ExecutionContext.GetOwningComponent());

	const AActor* OtherActor = ExecutionContext.GetOtherActor();
	const UPrimitiveComponent* OtherComponent = ExecutionContext.GetOtherComponent();

	ContextData.SetContextData(OtherActorRef, OtherActor);
	ContextData.SetContextData(OtherComponentRef, OtherComponent);
	ContextData.SetContextData(EventRef, &Event);
}

bool UTriggerWorldConditionSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	check(InScriptStruct);

	// The two common bases are the engine's extension points for conditions that need nothing beyond a
	// subsystem or a single actor, so anything written against them is usable here as well.
	return Super::IsStructAllowed(InScriptStruct)
		|| InScriptStruct->IsChildOf(TBaseStructure<FWorldConditionCommonBase>::Get())
		|| InScriptStruct->IsChildOf(TBaseStructure<FWorldConditionCommonActorBase>::Get())
		|| InScriptStruct->IsChildOf(TBaseStructure<FTriggerWorldConditionBase>::Get());
}
