#include "UERingCommandlets.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UERingBundleBuilder.h"
#include "UERingCppIndexer.h"
#include "UERingExportManager.h"
#include "UERingIndexManager.h"
#include "UERingSettings.h"
#include "UERingValidator.h"

UUERingExportCommandlet::UUERingExportCommandlet()
{
    IsServer = true;
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UUERingExportCommandlet::Main(const FString& Params)
{
    FUERingExportManager::Get().Initialize();
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);

    const bool bExportAll = FParse::Param(*Params, TEXT("All"));
    FString PackageFilter;
    const bool bHasPackageFilter = FParse::Value(*Params, TEXT("Package="), PackageFilter);
    if (bExportAll == bHasPackageFilter)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Exactly one export scope is required: use -All or -Package=/Mount/Path/Asset."));
        return 1;
    }
    if (bHasPackageFilter
        && !FUERingExportManager::Get().IsSupportedPackageName(PackageFilter))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid -Package value: %s"), *PackageFilter);
        return 1;
    }

    FString BundlePackage;
    FString BundleScopeName;
    FString BundleValue;
    const bool bLegacyBundle = FParse::Value(*Params, TEXT("Bundle="), BundlePackage);
    const bool bHasBundleScope = FParse::Value(*Params, TEXT("BundleScope="), BundleScopeName);
    const bool bHasBundleValue = FParse::Value(*Params, TEXT("BundleValue="), BundleValue);
    if (bLegacyBundle && (bHasBundleScope || bHasBundleValue))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Use either legacy -Bundle=/Game/Asset or -BundleScope plus -BundleValue, not both."));
        return 1;
    }
    if (bHasBundleScope != bHasBundleValue)
    {
        UE_LOG(LogTemp, Error, TEXT("-BundleScope and -BundleValue must be provided together."));
        return 1;
    }
    if (bLegacyBundle
        && !FUERingExportManager::Get().IsSupportedPackageName(BundlePackage))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid -Bundle value: %s"), *BundlePackage);
        return 1;
    }

    FUERingBundleRequest BundleRequest;
    if (bHasBundleScope)
    {
        BundleRequest.Value = BundleValue;
        if (BundleScopeName.Equals(TEXT("Asset"), ESearchCase::IgnoreCase))
        {
            BundleRequest.Scope = EUERingBundleScope::Asset;
        }
        else if (BundleScopeName.Equals(TEXT("Folder"), ESearchCase::IgnoreCase))
        {
            BundleRequest.Scope = EUERingBundleScope::Folder;
        }
        else if (BundleScopeName.Equals(TEXT("Module"), ESearchCase::IgnoreCase))
        {
            BundleRequest.Scope = EUERingBundleScope::Module;
        }
        else if (BundleScopeName.Equals(TEXT("PrimaryAssetType"), ESearchCase::IgnoreCase))
        {
            BundleRequest.Scope = EUERingBundleScope::PrimaryAssetType;
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("Invalid -BundleScope '%s'; expected Asset, Folder, Module, or PrimaryAssetType."),
                *BundleScopeName);
            return 1;
        }
        if (BundleValue.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("-BundleValue must not be empty."));
            return 1;
        }
        const bool bAssetScope = BundleRequest.Scope == EUERingBundleScope::Asset;
        const bool bFolderScope = BundleRequest.Scope == EUERingBundleScope::Folder;
        const bool bValidGamePath = FUERingExportManager::Get().IsSupportedPackageName(BundleValue);
        if ((bAssetScope || bFolderScope)
            && (!bValidGamePath || !FPackageName::IsValidLongPackageName(BundleValue)))
        {
            UE_LOG(LogTemp, Error, TEXT("Invalid -BundleValue package path: %s"), *BundleValue);
            return 1;
        }
    }

    TArray<FAssetData> Assets;
    Registry.GetAllAssets(Assets, true);
    Assets.RemoveAll([&PackageFilter, bHasPackageFilter](const FAssetData& Asset)
    {
        return !FUERingExportManager::Get().CanExport(Asset)
            || (bHasPackageFilter && Asset.PackageName.ToString() != PackageFilter);
    });
    FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);

    if (bHasPackageFilter && Assets.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("No supported asset matched -Package=%s."), *PackageFilter);
        return 1;
    }

    FString Error;
    if (bExportAll && !FUERingExportManager::Get().PrepareFullExport(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
        return 1;
    }

    int32 Failed = 0;
    int32 Exported = 0;
    int32 Unchanged = 0;
    bool bSemanticChanged = false;
    for (const FUERingExportResult& Result : FUERingExportManager::Get().ExportAssets(Assets))
    {
        if (Result.Status == EUERingExportStatus::Exported)
        {
            ++Exported;
            bSemanticChanged = true;
        }
        else if (Result.Status == EUERingExportStatus::Unchanged) ++Unchanged;
        else if (Result.Status == EUERingExportStatus::Failed) ++Failed;
    }

    if (bExportAll && !FUERingExportManager::Get().FinalizeFullExport(Assets, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
        ++Failed;
    }
    TArray<FName> ExportedPackages;
    for (const FAssetData& Asset : Assets) ExportedPackages.Add(Asset.PackageName);
    if (!(bExportAll
            ? FUERingIndexManager::Rebuild(Error)
            : FUERingIndexManager::UpdatePackages(ExportedPackages, Error)))
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
        ++Failed;
    }
    if (GetDefault<UUERingSettings>()->bIncludeCppIndex
        && (bExportAll || bSemanticChanged)
        && !(bExportAll
            ? FUERingCppIndexer::Rebuild(Error)
            : FUERingCppIndexer::UpdatePackages(ExportedPackages, Error)))
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
        ++Failed;
    }

    if (bLegacyBundle)
    {
        FString BundleDirectory;
        if (!FUERingBundleBuilder::Build(BundlePackage, BundleDirectory, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
            ++Failed;
        }
    }
    else if (bHasBundleScope)
    {
        FString BundleDirectory;
        if (!FUERingBundleBuilder::Build(BundleRequest, BundleDirectory, Error))
        {
            UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
            ++Failed;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("UE Ring export complete: exported=%d unchanged=%d failed=%d"),
        Exported, Unchanged, Failed);
    return Failed == 0 ? 0 : 1;
}

UUERingValidateCommandlet::UUERingValidateCommandlet()
{
    IsServer = true;
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UUERingValidateCommandlet::Main(const FString& Params)
{
    FUERingExportManager::Get().Initialize();
    const FUERingValidationReport Report = FUERingValidator::Validate();
    for (const FString& Message : Report.Messages)
    {
        UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
    }
    UE_LOG(LogTemp, Display,
        TEXT("UE Ring validation: checked=%d missing=%d stale=%d orphan=%d invalid=%d"),
        Report.Checked, Report.Missing, Report.Stale, Report.Orphan, Report.Invalid);
    return Report.IsValid() ? 0 : 1;
}
