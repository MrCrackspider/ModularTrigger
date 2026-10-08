
#include "ActorEventCall.h"

#include "GameFramework/Actor.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogActorEventCall, Log, All);

namespace UE::ModularTrigger::ActorEventCall
{
	/**
	 * A parameter is a genuine output only when it is a non-const reference. UHT also sets CPF_OutParm on
	 * every const reference parameter (`const FVector&`), so testing CPF_OutParm alone would reject nearly
	 * every C++ BlueprintCallable function. This mirrors the engine's own OutParm|ReturnParm exact-flag test.
	 */
	static bool IsOutputParam(const FProperty* Property)
	{
		return Property->HasAllPropertyFlags(CPF_OutParm)
			&& !Property->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm);
	}

	static bool IsInputParam(const FProperty* Property)
	{
		return Property->HasAnyPropertyFlags(CPF_Parm)
			&& !Property->HasAnyPropertyFlags(CPF_ReturnParm)
			&& !IsOutputParam(Property);
	}

	/**
	 * Copies one authored value into the parameter frame.
	 *
	 * Identical types are a straight CopyCompleteValue. The representations still differ in one place:
	 * a TEnumAsByte<> parameter is an FByteProperty while the bag always builds an FEnumProperty for enums
	 * (and the same applies inside containers). Round-tripping through exported text covers those cases
	 * without ever reinterpreting memory across mismatched layouts.
	 */
	static bool CopyValueToParam(const FProperty* SourceProperty, const void* SourceAddress,
		const FProperty* ParamProperty, void* TargetAddress)
	{
		if (ParamProperty->SameType(SourceProperty))
		{
			ParamProperty->CopyCompleteValue(TargetAddress, SourceAddress);
			return true;
		}

		FString Exported;
		SourceProperty->ExportTextItem_Direct(Exported, SourceAddress, nullptr, nullptr, PPF_None);
		return ParamProperty->ImportText_Direct(*Exported, TargetAddress, nullptr, PPF_None) != nullptr;
	}
}

AActor* FActorEventCall::ResolveTargetActor(AActor* ContextActor) const
{
	if (bUseContextActor)
	{
		return ContextActor;
	}

	return IsValid(TargetActor) ? TargetActor.Get() : nullptr;
}

UClass* FActorEventCall::ResolveTargetClass(UClass* ContextActorClass) const
{
	if (bUseContextActor)
	{
		return ContextActorClass;
	}

	return IsValid(TargetActor) ? TargetActor->GetClass() : nullptr;
}

UFunction* FActorEventCall::ResolveFunction(const AActor* Actor, const FName EventName)
{
	return IsValid(Actor) ? ResolveFunction(Actor->GetClass(), EventName) : nullptr;
}

UFunction* FActorEventCall::ResolveFunction(const UClass* ActorClass, const FName EventName)
{
	if (ActorClass == nullptr || EventName.IsNone())
	{
		return nullptr;
	}

	return ActorClass->FindFunctionByName(EventName);
}

#if WITH_EDITOR

bool FActorEventCall::IsCallableEvent(const UFunction* Function)
{
	using namespace UE::ModularTrigger::ActorEventCall;

	if (Function == nullptr)
	{
		return false;
	}

	if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable)
		|| Function->HasAnyFunctionFlags(FUNC_Static | FUNC_Delegate | FUNC_EditorOnly))
	{
		return false;
	}

	// A latent node needs a live FLatentActionInfo linkage/UUID that cannot be authored statically.
	if (Function->HasMetaData(TEXT("Latent")))
	{
		return false;
	}

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		const FProperty* Property = *It;

		// We have nowhere to deliver a return value or an out parameter.
		if (Property->HasAnyPropertyFlags(CPF_ReturnParm) || IsOutputParam(Property))
		{
			return false;
		}

		// UPropertyBag silently drops descriptors it cannot represent (maps, interfaces, delegates,
		// weak/lazy object pointers, field paths). Rejecting the whole function here keeps a dropped
		// parameter from turning into a value that is silently never passed.
		const FPropertyBagPropertyDesc Desc(Property->GetFName(), Property);
		if (Desc.ValueType == EPropertyBagPropertyType::None)
		{
			return false;
		}
	}

	return true;
}

#endif


void FActorEventCall::BuildParameterDescs(const UFunction* Function, TArray<FPropertyBagPropertyDesc>& OutDescs)
{
	using namespace UE::ModularTrigger::ActorEventCall;

	OutDescs.Reset();

	if (Function == nullptr)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		const FProperty* Property = *It;
		if (!IsInputParam(Property))
		{
			continue;
		}

		// This constructor derives the value type, container types and (in editor) the source property's
		// metadata, so clamps and tooltips carry over to the details row for free.
		FPropertyBagPropertyDesc Desc(Property->GetFName(), Property);
		if (Desc.ValueType != EPropertyBagPropertyType::None)
		{
			OutDescs.Add(MoveTemp(Desc));
		}
	}
}

void FActorEventCall::SyncParametersToSignature(UClass* ContextActorClass)
{
	const UFunction* Function = ResolveFunction(ResolveTargetClass(ContextActorClass), EventName);
	if (Function == nullptr)
	{
		Parameters.Reset();
		return;
	}

	TArray<FPropertyBagPropertyDesc> Descs;
	BuildParameterDescs(Function, Descs);

	// Migrate by name rather than MigrateToNewBagStruct: descriptors built from a UFunction get fresh GUIDs
	// every time, so ID-based migration would drop every value. Name matching also converts between numeric
	// types, so widening a parameter from int32 to float in Blueprint keeps the authored value.
	FInstancedPropertyBag NewParameters;
	NewParameters.AddProperties(Descs);
	NewParameters.CopyMatchingValuesByName(Parameters);

	Parameters = MoveTemp(NewParameters);
}

bool FActorEventCall::Execute(AActor* ContextActor) const
{
	using namespace UE::ModularTrigger::ActorEventCall;

	AActor* Actor = ResolveTargetActor(ContextActor);
	UFunction* Function = ResolveFunction(Actor, EventName);
	if (Function == nullptr)
	{
		UE_LOG(LogActorEventCall, Warning, TEXT("Could not resolve event '%s' on '%s'."),
			*EventName.ToString(), *GetNameSafe(Actor));
		return false;
	}

	// The bag's generated struct and the function's parameter frame are two independent layouts, so the
	// values have to be copied property by property. Frame lifecycle mirrors
	// FMovieSceneDynamicBindingInvoker::InvokeDynamicBinding.
	uint8* Frame = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize + Function->MinAlignment));
	Frame = Align(Frame, Function->MinAlignment);
	FMemory::Memzero(Frame, Function->ParmsSize);

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (!It->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			It->InitializeValue_InContainer(Frame);
		}
	}

	const uint8* BagMemory = Parameters.GetValue().GetMemory();
	if (BagMemory != nullptr)
	{
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			const FProperty* ParamProperty = *It;
			if (!IsInputParam(ParamProperty))
			{
				continue;
			}

			const FPropertyBagPropertyDesc* Desc = Parameters.FindPropertyDescByName(ParamProperty->GetFName());
			if (Desc == nullptr || Desc->CachedProperty == nullptr)
			{
				// Signature gained a parameter since this call was authored; leave it default-initialized.
				continue;
			}

			const bool bCopied = CopyValueToParam(
				Desc->CachedProperty,
				BagMemory + Desc->CachedProperty->GetOffset_ForInternal(),
				ParamProperty,
				ParamProperty->ContainerPtrToValuePtr<void>(Frame));

			if (!bCopied)
			{
				UE_LOG(LogActorEventCall, Warning,
					TEXT("Event '%s' on '%s': stored value for parameter '%s' is incompatible with the current signature; using its default."),
					*EventName.ToString(), *GetNameSafe(Actor), *ParamProperty->GetName());
			}
		}
	}

	{
#if WITH_EDITOR
		// Permits the call from an editor (non-PIE) world; a no-op at runtime.
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		// Note: for a FUNC_Net function this follows the normal RPC path, so the call may be sent to the
		// authority rather than executed locally.
		Actor->ProcessEvent(Function, Frame);
	}

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Frame);
	}

	return true;
}

bool UActorEventCallBlueprintUtils::ExecuteActorEventCall(const FActorEventCall& ActorEventCall, AActor* ContextActor)
{
	return ActorEventCall.Execute(ContextActor);
}
