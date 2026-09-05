#include "UERingLifecycleManager.h"

#include "Algo/Unique.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingExportManager.h"
#include "UERingCppIndexer.h"
#include "UERingDerivedArtifactWriter.h"
#include "UERingIndexManager.h"
#include "UERingSettings.h"
#include "UERingSummaryWriter.h"
#include "UERingVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogUERingLifecycle, Log, All);

FUERingLifecycleManager& FUERingLifecycleManager::Get()
{
    static FUERingLifecycleManager Instance;
    return Instance;
}

void FUERingLifecycleManager::Initialize()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(
        this, &FUERingLifecycleManager::HandlePackageSaved);
    AssetRemovedHandle = Registry.OnAssetRemoved().AddRaw(this, &FUERingLifecycleManager::HandleAssetRemoved);
    AssetRenamedHandle = Registry.OnAssetRenamed().AddRaw(this, &FUERingLifecycleManager::HandleAssetRenamed);
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FUERingLifecycleManager::Tick),
        0.25f);
}

void FUERingLifecycleManager::Shutdown()
{
    UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
    {
        IAssetRegistry& Registry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.OnAssetRemoved().Remove(AssetRemovedHandle);
        Registry.OnAssetRenamed().Remove(AssetRenamedHandle);
    }
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    PendingPackages.Reset();
    PendingIndexOnlyPackages.Reset();
}

void FUERingLifecycleManager::HandlePackageSaved(
    const FString& Filename,
    UPackage* Package,
    const FObjectPostSaveContext SaveContext)
{
    const UUERingSettings* Settings = GetDefault<UUERingSettings>();
    if (!Settings->bEnableAutoExport
        || !Settings->bExportOnAssetSave
        || Package == nullptr
        || SaveContext.IsProceduralSave()
        || SaveContext.IsFromAutoSave())
    {
        return;
    }
    QueuePackage(Package->GetFName());
}

void FUERingLifecycleManager::HandleAssetRemoved(const FAssetData& AssetData)
{
    if (!FUERingExportManager::Get().CanExport(AssetData))
    {
        return;
    }

    PendingPackages.Remove(AssetData.PackageName);
    PendingIndexOnlyPackages.Remove(AssetData.PackageName);

    const UUERingSettings* Settings = GetDefault<UUERingSettings>();
    if (Settings->bKeepTombstone)
    {
        WriteTombstone(
            AssetData.PackageName.ToString(),
            AssetData.GetSoftObjectPath().ToString());
    }
    if (Settings->bDeleteSemanticOnAssetDelete)
    {
        const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(
            AssetData.PackageName.ToString(), AssetData.IsInstanceOf(UWorld::StaticClass()));
        IFileManager::Get().Delete(*SemanticFile, false, true);
        FUERingSummaryWriter::Remove(SemanticFile);
        FUERingDerivedArtifactWriter::RemoveArtifacts(SemanticFile);
    }

    FString IndexError;
    if (!FUERingIndexManager::UpdatePackages({ AssetData.PackageName }, IndexError))
    {
        UE_LOG(LogUERingLifecycle, Error, TEXT("Index rebuild after delete failed: %s"), *IndexError);
    }
    if (Settings->bIncludeCppIndex
        && !FUERingCppIndexer::UpdatePackages({ AssetData.PackageName }, IndexError))
    {
        UE_LOG(LogUERingLifecycle, Error, TEXT("C++ index rebuild after delete failed: %s"), *IndexError);
    }
}

void FUERingLifecycleManager::HandleAssetRenamed(
    const FAssetData& AssetData,
    const FString& OldObjectPath)
{
    if (!FUERingExportManager::Get().CanExport(AssetData))
    {
        return;
    }

    const bool bIsMap = AssetData.IsInstanceOf(UWorld::StaticClass());
    const FString OldPackageName = FPackageName::ObjectPathToPackageName(OldObjectPath);
    const FString OldSemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(OldPackageName, bIsMap);
    const FString NewSemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(
        AssetData.PackageName.ToString(), bIsMap);
    if (IFileManager::Get().FileExists(*OldSemanticFile))
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(NewSemanticFile), true);
        if (!IFileManager::Get().Move(*NewSemanticFile, *OldSemanticFile, true, true, false, true))
        {
            UE_LOG(LogUERingLifecycle, Warning, TEXT("Could not move renamed semantic file %s to %s"),
                *OldSemanticFile, *NewSemanticFile);
        }
    }
    FString SummaryMoveError;
    if (!FUERingSummaryWriter::Move(OldSemanticFile, NewSemanticFile, SummaryMoveError))
    {
        UE_LOG(LogUERingLifecycle, Warning, TEXT("%s"), *SummaryMoveError);
    }
    FString DerivedMoveError;
    if (!FUERingDerivedArtifactWriter::MoveArtifacts(
        OldSemanticFile,
        NewSemanticFile,
        DerivedMoveError))
    {
        UE_LOG(LogUERingLifecycle, Warning, TEXT("%s"), *DerivedMoveError);
    }
    if (GetDefault<UUERingSettings>()->bKeepTombstone)
    {
        WriteTombstone(
            OldPackageName,
            OldObjectPath,
            AssetData.PackageName.ToString(),
            AssetData.GetSoftObjectPath().ToString());
    }
    PendingPackages.Remove(*OldPackageName);
    PendingIndexOnlyPackages.Add(*OldPackageName);
    QueuePackage(AssetData.PackageName);
}

bool FUERingLifecycleManager::Tick(const float DeltaTime)
{
    if (PendingPackages.IsEmpty() && PendingIndexOnlyPackages.IsEmpty())
    {
        return true;
    }

    const double Now = FPlatformTime::Seconds();
    TArray<FName> ReadyPackages;
    for (const TPair<FName, double>& Pending : PendingPackages)
    {
        if (Now - Pending.Value >= 0.5)
        {
            ReadyPackages.Add(Pending.Key);
        }
    }
    ReadyPackages.Sort(FNameLexicalLess());
    if (ReadyPackages.IsEmpty() && PendingIndexOnlyPackages.IsEmpty())
    {
        return true;
    }


    TArray<FName> IndexOnlyPackages = PendingIndexOnlyPackages.Array();
    PendingIndexOnlyPackages.Reset();
    IndexOnlyPackages.Sort(FNameLexicalLess());

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;
    for (const FName PackageName : ReadyPackages)
    {
        PendingPackages.Remove(PackageName);
        if (!FPackageName::DoesPackageExist(PackageName.ToString()))
        {
            // A delete can race the debounced save callback before Asset Registry removal
            // is delivered. Treat it as an index invalidation, not an export failure.
            IndexOnlyPackages.Add(PackageName);
            continue;
        }
        TArray<FAssetData> PackageAssets;
        Registry.GetAssetsByPackageName(PackageName, PackageAssets, true);
        for (const FAssetData& Asset : PackageAssets)
        {
            if (FUERingExportManager::Get().CanExport(Asset))
            {
                Assets.Add(Asset);
            }
        }
    }
    FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);

    bool bAttemptedExport = false;
    bool bSemanticChanged = false;
    for (const FUERingExportResult& Result : FUERingExportManager::Get().ExportAssets(Assets))
    {
        bAttemptedExport = true;
        bSemanticChanged |= Result.Status == EUERingExportStatus::Exported;
        if (!Result.IsSuccess())
        {
            UE_LOG(LogUERingLifecycle, Error, TEXT("Automatic export failed: %s"), *Result.Error);
        }
    }
    if ((bAttemptedExport && bSemanticChanged) || !IndexOnlyPackages.IsEmpty())
    {
        TArray<FName> IndexPackages = IndexOnlyPackages;
        if (bSemanticChanged)
        {
            IndexPackages.Append(ReadyPackages);
        }
        IndexPackages.Sort(FNameLexicalLess());
        IndexPackages.SetNum(Algo::Unique(IndexPackages));
        FString IndexError;
        if (!FUERingIndexManager::UpdatePackages(IndexPackages, IndexError))
        {
            UE_LOG(LogUERingLifecycle, Error, TEXT("Index rebuild after automatic export failed: %s"), *IndexError);
        }
        if (GetDefault<UUERingSettings>()->bIncludeCppIndex
            && !FUERingCppIndexer::UpdatePackages(IndexPackages, IndexError))
        {
            UE_LOG(LogUERingLifecycle, Error, TEXT("C++ index rebuild after automatic export failed: %s"), *IndexError);
        }
    }
    return true;
}

void FUERingLifecycleManager::QueuePackage(const FName PackageName)
{
    if (FUERingExportManager::Get().IsSupportedPackageName(PackageName.ToString()))
    {
        PendingPackages.FindOrAdd(PackageName) = FPlatformTime::Seconds();
    }
}

void FUERingLifecycleManager::WriteTombstone(
    const FString& OldPackageName,
    const FString& OldObjectPath,
    const FString& NewPackageName,
    const FString& NewObjectPath) const
{
    FString RelativePackage = OldPackageName;
    RelativePackage.RemoveFromStart(TEXT("/"));
    const FString TombstoneFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("tombstones"),
        RelativePackage + TEXT(".deleted.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TombstoneFile), true);

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.tombstone"));
    Root->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    Root->SetStringField(TEXT("packageName"), OldPackageName);
    Root->SetStringField(TEXT("objectPath"), OldObjectPath);
    Root->SetStringField(TEXT("event"), NewPackageName.IsEmpty() ? TEXT("deleted") : TEXT("renamed"));
    if (!NewPackageName.IsEmpty())
    {
        Root->SetStringField(TEXT("newPackageName"), NewPackageName);
        Root->SetStringField(TEXT("newObjectPath"), NewObjectPath);
    }
    Root->SetStringField(TEXT("deletedAtUtc"), FDateTime::UtcNow().ToIso8601());
    FString Json;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
    if (FJsonSerializer::Serialize(Root, Writer))
    {
        Json += LINE_TERMINATOR;
        FFileHelper::SaveStringToFile(Json, *TombstoneFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}
