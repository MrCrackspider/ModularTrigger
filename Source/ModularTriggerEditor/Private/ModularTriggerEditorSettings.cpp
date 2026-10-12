
#include "ModularTriggerEditorSettings.h"

#include "Editor.h"

UModularTriggerEditorSettings::UModularTriggerEditorSettings()
{
	CategoryName = TEXT("Plugins");
}

UModularTriggerEditorSettings& UModularTriggerEditorSettings::Get()
{
	UModularTriggerEditorSettings* Settings = GetMutableDefault<UModularTriggerEditorSettings>();
	check(Settings);
	return *Settings;
}

FName UModularTriggerEditorSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

void UModularTriggerEditorSettings::ToggleFlag(bool UModularTriggerEditorSettings::* Flag)
{
	this->*Flag = !(this->*Flag);
	SaveConfig();

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}

#if WITH_EDITOR
void UModularTriggerEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GEditor)
	{
		GEditor->RedrawLevelEditingViewports();
	}
}
#endif
