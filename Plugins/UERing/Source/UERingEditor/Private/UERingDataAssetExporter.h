#pragma once

#include "IUERingAssetExporter.h"

class FUERingDataAssetExporter final : public IUERingAssetExporter
{
public:
    virtual FName GetName() const override;
    virtual bool CanExport(const FAssetData& AssetData) const override;
    virtual bool BuildPayload(
        const FUERingExportContext& Context,
        FUERingSemanticPayload& OutPayload,
        FString& OutError) const override;
};
