
#pragma once

#include "Modules/ModuleInterface.h"

class FModularTriggerEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
