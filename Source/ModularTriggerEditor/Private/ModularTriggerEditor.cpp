
#include "ModularTriggerEditor.h"

#include "ActorEventCallCustomization.h"
#include "HierarchicalTriggerComponent.h"
#include "HierarchicalTriggerComponentDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "TriggerModuleReferenceCustomization.h"

#define LOCTEXT_NAMESPACE "FModularTriggerEditorModule"

static const FName TriggerModuleReferenceName(TEXT("TriggerModuleReference"));
static const FName ActorEventCallName(TEXT("ActorEventCall"));

void FModularTriggerEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(TriggerModuleReferenceName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTriggerModuleReferenceCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(ActorEventCallName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FActorEventCallCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UHierarchicalTriggerComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHierarchicalTriggerComponentDetails::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FModularTriggerEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(TriggerModuleReferenceName);
		PropertyModule.UnregisterCustomPropertyTypeLayout(ActorEventCallName);
		PropertyModule.UnregisterCustomClassLayout(UHierarchicalTriggerComponent::StaticClass()->GetFName());
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FModularTriggerEditorModule, ModularTriggerEditor)
