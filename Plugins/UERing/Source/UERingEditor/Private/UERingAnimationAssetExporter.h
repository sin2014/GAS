#pragma once

#include "IUERingAssetExporter.h"

class FUERingAnimationAssetExporter final : public IUERingAssetExporter
{
public:
    virtual FName GetName() const override;
    virtual int32 GetPriority() const override { return 200; }
    virtual bool CanExport(const FAssetData& AssetData) const override;
    virtual bool BuildPayload(
        const FUERingExportContext& Context,
        FUERingSemanticPayload& OutPayload,
        FString& OutError) const override;
};
