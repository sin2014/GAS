#pragma once

#include "Modules/ModuleManager.h"

class FBlueprintReaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};
