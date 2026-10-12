
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ModularTriggerEditorSettings.generated.h"

/**
 * Per-user editor settings for the trigger module viewport visualization.
 *
 * Saved to EditorPerProjectUserSettings, so these are one developer's preferences and never travel with the
 * project. Reachable from Editor Preferences > Plugins > Modular Trigger, and from the level editor toolbar.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Modular Trigger"))
class UModularTriggerEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UModularTriggerEditorSettings();

	static UModularTriggerEditorSettings& Get();

	/** Master switch. With this off nothing is drawn, whatever the modules describe. */
	UPROPERTY(config, EditAnywhere, Category = "Visualization")
	bool bEnableVisualization = true;

	/** Draw the text labels modules attach to their shapes. Off keeps the geometry but drops the clutter. */
	UPROPERTY(config, EditAnywhere, Category = "Visualization", meta = (EditCondition = "bEnableVisualization"))
	bool bDrawLabels = true;

	/**
	 * Draw only the module expanded in the details panel, instead of every module on the component.
	 * When no module is expanded the whole component is drawn, so this never blanks the viewport on its own.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Visualization", meta = (EditCondition = "bEnableVisualization"))
	bool bOnlySelectedModule = false;

	/** Skip modules whose Enabled flag is off. */
	UPROPERTY(config, EditAnywhere, Category = "Visualization", meta = (EditCondition = "bEnableVisualization"))
	bool bHideDisabledModules = false;

	/** Dim everything except the module expanded in the details panel, rather than drawing it all equally. */
	UPROPERTY(config, EditAnywhere, Category = "Visualization", meta = (EditCondition = "bEnableVisualization"))
	bool bDimUnselectedModules = true;

	/** Line width in pixels, applied to every shape a module does not thicken itself. */
	UPROPERTY(config, EditAnywhere, Category = "Visualization", meta = (EditCondition = "bEnableVisualization", ClampMin = "0.0", ClampMax = "10.0"))
	float LineThickness = 1.0f;

	/** Opacity applied to modules that are not the expanded one. @see bDimUnselectedModules */
	UPROPERTY(config, EditAnywhere, Category = "Visualization", meta = (EditCondition = "bEnableVisualization && bDimUnselectedModules", ClampMin = "0.0", ClampMax = "1.0"))
	float DimOpacity = 0.35f;

	/** Flips one of the flags above, writes it to the user's config and redraws the level viewports. */
	void ToggleFlag(bool UModularTriggerEditorSettings::* Flag);

	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	//~ End UDeveloperSettings interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
