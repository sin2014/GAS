#pragma once

#include "UERingExportTypes.h"

class UERINGEDITOR_API IUERingAssetExporter
{
public:
    virtual ~IUERingAssetExporter() = default;

    virtual FName GetName() const = 0;
    virtual int32 GetPriority() const { return 0; }
    virtual bool SupportsUnloadedAssets() const { return false; }
    virtual bool CanExport(const FAssetData& AssetData) const = 0;
    virtual bool BuildPayload(
        const FUERingExportContext& Context,
        FUERingSemanticPayload& OutPayload,
        FString& OutError) const = 0;
};
