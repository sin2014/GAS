#pragma once

#include "IUERingAssetExporter.h"

class FUERingWidgetBlueprintExporter final : public IUERingAssetExporter
{
public:
    virtual FName GetName() const override;
    virtual int32 GetPriority() const override { return 100; }
    virtual bool CanExport(const FAssetData& AssetData) const override;
    virtual bool BuildPayload(
        const FUERingExportContext& Context,
        FUERingSemanticPayload& OutPayload,
        FString& OutError) const override;
};
