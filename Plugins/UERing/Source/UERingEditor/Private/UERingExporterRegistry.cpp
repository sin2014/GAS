#include "UERingExporterRegistry.h"

#include "UERingExportManager.h"

FUERingExporterHandle FUERingExporterRegistry::Register(TUniquePtr<IUERingAssetExporter> Exporter)
{
    return FUERingExportManager::Get().RegisterExporter(MoveTemp(Exporter));
}

bool FUERingExporterRegistry::Unregister(const FUERingExporterHandle Handle)
{
    return FUERingExportManager::Get().UnregisterExporter(Handle);
}
