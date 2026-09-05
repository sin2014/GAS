#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class FSpawnTabArgs;
class FUERingMcpIntegration;

class FUERingEditorModule final : public IModuleInterface
{
public:
    FUERingEditorModule();
    virtual ~FUERingEditorModule() override;
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    TSharedRef<SDockTab> SpawnStatusTab(const FSpawnTabArgs& Args);

    TUniquePtr<FUERingMcpIntegration> McpIntegration;
};

