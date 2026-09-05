#include "Modules/ModuleManager.h"
#include "UERingVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogUERingCore, Log, All);

class FUERingCoreModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogUERingCore, Log, TEXT("%s core module started. Schema=%s Version=%s"),
            UE_RING_PLUGIN_NAME,
            UE_RING_SCHEMA_VERSION,
            UE_RING_PLUGIN_VERSION);
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogUERingCore, Log, TEXT("%s core module stopped."), UE_RING_PLUGIN_NAME);
    }
};

IMPLEMENT_MODULE(FUERingCoreModule, UERingCore)

