#pragma once

#include "IUERingAssetExporter.h"

class FUERingExportManager
{
public:
    static FUERingExportManager& Get();

    void Initialize();
    void Shutdown();

    uint64 RegisterExporter(TUniquePtr<IUERingAssetExporter> Exporter);
    bool UnregisterExporter(uint64 Handle);
    bool CanExport(const FAssetData& AssetData) const;
    FUERingExportResult ExportAsset(const FAssetData& AssetData) const;
    TArray<FUERingExportResult> ExportAssets(const TArray<FAssetData>& Assets) const;
    bool PrepareFullExport(FString& OutError) const;
    bool FinalizeFullExport(const TArray<FAssetData>& Assets, FString& OutError) const;
    bool IsSupportedPackageName(const FString& PackageName) const;
    FString GetExclusionReason(const FAssetData& AssetData) const;
    void CanonicalizeAssetsByPackage(TArray<FAssetData>& Assets) const;

    FString GetOutputRoot() const;
    FString GetSemanticFileForPackage(const FString& PackageName, bool bIsMap) const;

private:
    const IUERingAssetExporter* FindExporter(const FAssetData& AssetData) const;
    bool IsIgnored(const FAssetData& AssetData) const;

    TArray<TUniquePtr<IUERingAssetExporter>> Exporters;
    TMap<uint64, IUERingAssetExporter*> ExporterHandles;
    uint64 NextExporterHandle = 1;
    bool bInitialized = false;
};
