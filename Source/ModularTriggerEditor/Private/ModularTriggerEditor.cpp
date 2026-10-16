
#include "ModularTriggerEditor.h"

#include "ActorEventCallCustomization.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "Editor/UnrealEdEngine.h"
#include "HierarchicalTriggerComponent.h"
#include "HierarchicalTriggerComponentDetails.h"
#include "HierarchicalTriggerComponentVisualizer.h"
#include "ISettingsModule.h"
#include "ModularTriggerEditorSettings.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "TriggerModuleReferenceCustomization.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "FModularTriggerEditorModule"

static const FName TriggerModuleReferenceName(TEXT("TriggerModuleReference"));
static const FName ActorEventCallName(TEXT("ActorEventCall"));

namespace
{
	/** Adds a menu entry that toggles one of the visualization flags. */
	void AddFlagEntry(FToolMenuSection& Section, FName EntryName, const FText& Label, const FText& ToolTip,
		bool UModularTriggerEditorSettings::* Flag)
	{
		Section.AddMenuEntry(
			EntryName,
			Label,
			ToolTip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([Flag]() { UModularTriggerEditorSettings::Get().ToggleFlag(Flag); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Flag]() { return UModularTriggerEditorSettings::Get().*Flag; })),
			EUserInterfaceActionType::ToggleButton);
	}

	void FillVisualizationOptionsMenu(UToolMenu* Menu)
	{
		FToolMenuSection& Section = Menu->AddSection("TriggerVisualization", LOCTEXT("VisualizationSection", "Trigger Visualization"));

		AddFlagEntry(Section, "DrawLabels",
			LOCTEXT("DrawLabels", "Draw Labels"),
			LOCTEXT("DrawLabelsTip", "Draw the text labels modules attach to their shapes"),
			&UModularTriggerEditorSettings::bDrawLabels);

		AddFlagEntry(Section, "OnlySelectedModule",
			LOCTEXT("OnlySelectedModule", "Only Selected Module"),
			LOCTEXT("OnlySelectedModuleTip", "Draw only the module expanded in the details panel, instead of every module on the component"),
			&UModularTriggerEditorSettings::bOnlySelectedModule);

		AddFlagEntry(Section, "DimUnselectedModules",
			LOCTEXT("DimUnselectedModules", "Dim Other Modules"),
			LOCTEXT("DimUnselectedModulesTip", "Fade everything except the module expanded in the details panel"),
			&UModularTriggerEditorSettings::bDimUnselectedModules);

		AddFlagEntry(Section, "HideDisabledModules",
			LOCTEXT("HideDisabledModules", "Hide Disabled Modules"),
			LOCTEXT("HideDisabledModulesTip", "Skip modules whose Enabled flag is off"),
			&UModularTriggerEditorSettings::bHideDisabledModules);

		FToolMenuSection& SettingsSection = Menu->AddSection("TriggerVisualizationSettings");
		SettingsSection.AddMenuEntry(
			"OpenSettings",
			LOCTEXT("OpenSettings", "Settings..."),
			LOCTEXT("OpenSettingsTip", "Open the Modular Trigger editor preferences, for colours and line widths"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
				{
					SettingsModule->ShowViewer("Editor", "Plugins", "ModularTriggerEditorSettings");
				}
			})));
	}
}

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

	// This module loads at PreDefault, which can be ahead of GUnrealEd existing, so wait for the engine when it
	// is not there yet. A module loaded after engine init (a hot reload) never sees that delegate fire, hence
	// the immediate path as well.
	if (GUnrealEd)
	{
		RegisterComponentVisualizer();
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FModularTriggerEditorModule::RegisterComponentVisualizer);
	}

	// The level editor toolbar is not built yet at module startup, so extend it from the ToolMenus callback.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FModularTriggerEditorModule::RegisterMenus));
}

void FModularTriggerEditorModule::RegisterComponentVisualizer()
{
	if (!GUnrealEd)
	{
		return;
	}

	TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FHierarchicalTriggerComponentVisualizer>();
	GUnrealEd->RegisterComponentVisualizer(UHierarchicalTriggerComponent::StaticClass()->GetFName(), Visualizer);
	Visualizer->OnRegister();
}

void FModularTriggerEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UHierarchicalTriggerComponent::StaticClass()->GetFName());
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(TriggerModuleReferenceName);
		PropertyModule.UnregisterCustomPropertyTypeLayout(ActorEventCallName);
		PropertyModule.UnregisterCustomClassLayout(UHierarchicalTriggerComponent::StaticClass()->GetFName());
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

void FModularTriggerEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	if (!Toolbar)
	{
		return;
	}

	FToolMenuSection& Section = Toolbar->FindOrAddSection("ModularTrigger");

	// A toggle for the master switch, with the per-feature options on a dropdown beside it.
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		"ToggleTriggerVisualization",
		FUIAction(
			FExecuteAction::CreateLambda([]()
			{
				UModularTriggerEditorSettings::Get().ToggleFlag(&UModularTriggerEditorSettings::bEnableVisualization);
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]()
			{
				return UModularTriggerEditorSettings::Get().bEnableVisualization;
			})),
		LOCTEXT("TriggerDebugLabel", "Trigger Debug"),
		LOCTEXT("TriggerDebugTip", "Draw what the modules of a selected trigger component describe: references, shapes and labels"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"),
		EUserInterfaceActionType::ToggleButton));

	Section.AddEntry(FToolMenuEntry::InitComboButton(
		"TriggerVisualizationOptions",
		FUIAction(),
		FNewToolMenuChoice(FNewToolMenuDelegate::CreateStatic(&FillVisualizationOptionsMenu)),
		FText::GetEmpty(),
		LOCTEXT("TriggerDebugOptionsTip", "Trigger visualization options"),
		FSlateIcon(),
		/*bInSimpleComboBox*/ true));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FModularTriggerEditorModule, ModularTriggerEditor)
