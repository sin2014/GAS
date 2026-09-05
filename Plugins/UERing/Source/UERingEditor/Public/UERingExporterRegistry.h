#pragma once

#include "IUERingAssetExporter.h"

using FUERingExporterHandle = uint64;

class UERINGEDITOR_API FUERingExporterRegistry
{
public:
    static FUERingExporterHandle Register(TUniquePtr<IUERingAssetExporter> Exporter);
    static bool Unregister(FUERingExporterHandle Handle);
};
