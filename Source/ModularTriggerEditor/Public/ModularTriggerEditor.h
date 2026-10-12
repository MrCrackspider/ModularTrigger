
#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleInterface.h"

class FModularTriggerEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Adds the trigger visualization toggle and its options dropdown to the level editor toolbar. */
	void RegisterMenus();

	/** Hands FHierarchicalTriggerComponentVisualizer to the editor engine. Needs GUnrealEd to exist. */
	void RegisterComponentVisualizer();

	/** Bound only when this module loads before the editor engine is up. */
	FDelegateHandle PostEngineInitHandle;
};
