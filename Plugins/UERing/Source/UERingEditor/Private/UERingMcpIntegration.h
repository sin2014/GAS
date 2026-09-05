#pragma once

#include "CoreMinimal.h"

struct IModelContextProtocolTool;

class FUERingMcpIntegration
{
public:
    void Initialize();
    void Shutdown();

private:
    void RegisterTools();

    TArray<TSharedRef<IModelContextProtocolTool>> Tools;
    FDelegateHandle RefreshToolsHandle;
};
