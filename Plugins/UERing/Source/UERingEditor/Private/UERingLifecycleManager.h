#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"

class FObjectPostSaveContext;
class UPackage;

class FUERingLifecycleManager
{
public:
    static FUERingLifecycleManager& Get();

    void Initialize();
    void Shutdown();

private:
    void HandlePackageSaved(const FString& Filename, UPackage* Package, FObjectPostSaveContext SaveContext);
    void HandleAssetRemoved(const FAssetData& AssetData);
    void HandleAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
    bool Tick(float DeltaTime);
    void QueuePackage(FName PackageName);
    void WriteTombstone(
        const FString& OldPackageName,
        const FString& OldObjectPath,
        const FString& NewPackageName = FString(),
        const FString& NewObjectPath = FString()) const;

    FDelegateHandle PackageSavedHandle;
    FDelegateHandle AssetRemovedHandle;
    FDelegateHandle AssetRenamedHandle;
    FTSTicker::FDelegateHandle TickerHandle;
    TMap<FName, double> PendingPackages;
    TSet<FName> PendingIndexOnlyPackages;
};
