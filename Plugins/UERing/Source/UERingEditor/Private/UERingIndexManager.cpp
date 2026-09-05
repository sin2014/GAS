#include "UERingIndexManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PlatformCryptoContextIncludes.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingExportManager.h"
#include "UERingProjectGraphBuilder.h"
#include "UERingSettings.h"
#include "UERingSqliteIndexer.h"
#include "UERingVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogUERingIndex, Log, All);

namespace UERingIndexManager
{
    FString RelativeToProject(FString Path)
    {
        FPaths::MakePathRelativeTo(Path, *FPaths::ProjectDir());
        FPaths::NormalizeFilename(Path);
        return Path;
    }

    FString HashFile(const FString& Filename)
    {
        TArray<uint8> Bytes;
        TArray<uint8> Hash;
        FEncryptionContext Context;
        if (!FFileHelper::LoadFileToArray(Bytes, *Filename)
            || !Context.CalcSHA256(Bytes, Hash)
            || Hash.Num() != 32)
        {
            return FString();
        }
        return TEXT("sha256:") + BytesToHex(Hash.GetData(), Hash.Num()).ToLower();
    }

    bool ReadSemantic(const FString& Filename, TSharedPtr<FJsonObject>& OutRoot)
    {
        FString Json;
        return FFileHelper::LoadFileToString(Json, *Filename)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot)
            && OutRoot.IsValid();
    }

    bool WriteJson(const FString& Filename, const TSharedRef<FJsonObject>& Root, FString& OutError)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        if (!FJsonSerializer::Serialize(Root, Writer))
        {
            OutError = FString::Printf(TEXT("Could not serialize index: %s"), *Filename);
            return false;
        }
        Json += LINE_TERMINATOR;

        const FString Directory = FPaths::GetPath(Filename);
        if (!IFileManager::Get().MakeDirectory(*Directory, true))
        {
            OutError = FString::Printf(TEXT("Could not create index directory: %s"), *Directory);
            return false;
        }
        const FString Temp = Filename + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
            || !IFileManager::Get().Move(*Filename, *Temp, true, true, false, true))
        {
            IFileManager::Get().Delete(*Temp, false, true);
            OutError = FString::Printf(TEXT("Could not write index: %s"), *Filename);
            return false;
        }
        return true;
    }

    FString OwnerModuleForPackage(const FString& PackageName)
    {
        if (PackageName.StartsWith(TEXT("/Game/"))) return FApp::GetProjectName();
        FString WithoutRoot = PackageName;
        WithoutRoot.RemoveFromStart(TEXT("/"));
        FString Mount;
        WithoutRoot.Split(TEXT("/"), &Mount, nullptr);
        return Mount.IsEmpty() ? FApp::GetProjectName() : Mount;
    }

    int64 JsonValueBytes(const TSharedPtr<FJsonValue>& Value)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        if (!FJsonSerializer::Serialize(Value, TEXT(""), Writer))
        {
            return 0;
        }
        return FTCHARToUTF8(*Json).Length();
    }

    TSharedRef<FJsonObject> BuildEntry(
        const FAssetData& Asset,
        IAssetRegistry& Registry,
        TArray<TPair<FString, TSharedRef<FJsonObject>>>& OutEdges)
    {
        const bool bSupported = FUERingExportManager::Get().CanExport(Asset);
        const bool bIsMap = Asset.IsInstanceOf(UWorld::StaticClass());
        const FString PackageName = Asset.PackageName.ToString();
        const FString SourceFile = FPackageName::LongPackageNameToFilename(
            PackageName,
            bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
        const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, bIsMap);
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("packageName"), PackageName);
        Entry->SetStringField(TEXT("objectPath"), Asset.GetSoftObjectPath().ToString());
        Entry->SetStringField(TEXT("assetClass"), Asset.AssetClassPath.GetAssetName().ToString());
        Entry->SetStringField(TEXT("sourceFile"), RelativeToProject(SourceFile));
        if (bSupported) Entry->SetStringField(TEXT("semanticFile"), RelativeToProject(SemanticFile));
        else Entry->SetField(TEXT("semanticFile"), MakeShared<FJsonValueNull>());
        const FString CurrentSourceHash = IFileManager::Get().FileExists(*SourceFile) ? HashFile(SourceFile) : FString();
        Entry->SetStringField(TEXT("sourceHash"), CurrentSourceHash);
        Entry->SetStringField(
            TEXT("semanticHash"),
            IFileManager::Get().FileExists(*SemanticFile) ? HashFile(SemanticFile) : FString());

        FString Status = bSupported ? TEXT("missing") : TEXT("unsupported");
        FString ExportedAtUtc;
        FString SemanticKind;
        FString ExporterName;
        TArray<FString> Domains;
        FString Recoverability;
        double ReconstructionConfidence = 0.0;
        int64 SemanticBytes = 0;
        int32 OmissionCount = 0;
        const TSharedRef<FJsonObject> AssetSectionBytes = MakeShared<FJsonObject>();
        if (bSupported && !IFileManager::Get().FileExists(*SourceFile))
        {
            Status = TEXT("failed");
        }
        else if (bSupported && IFileManager::Get().FileExists(*SemanticFile))
        {
            SemanticBytes = FMath::Max<int64>(0, IFileManager::Get().FileSize(*SemanticFile));
            TSharedPtr<FJsonObject> SemanticRoot;
            const TSharedPtr<FJsonObject>* SemanticAsset = nullptr;
            FString SchemaVersion;
            FString StoredSourceHash;
            if (!ReadSemantic(SemanticFile, SemanticRoot)
                || !SemanticRoot->TryGetStringField(TEXT("schemaVersion"), SchemaVersion)
                || SchemaVersion != UE_RING_SCHEMA_VERSION
                || !SemanticRoot->TryGetObjectField(TEXT("asset"), SemanticAsset)
                || SemanticAsset == nullptr
                || !(*SemanticAsset)->TryGetStringField(TEXT("sourceHash"), StoredSourceHash))
            {
                Status = TEXT("failed");
            }
            else
            {
                const FDateTime SourceTimestamp = IFileManager::Get().GetTimeStamp(*SourceFile);
                ExportedAtUtc = SourceTimestamp == FDateTime::MinValue()
                    ? TEXT("1970-01-01T00:00:00Z") : SourceTimestamp.ToIso8601();
                const TSharedPtr<FJsonObject>* Semantics = nullptr;
                if (SemanticRoot->TryGetObjectField(TEXT("semantics"), Semantics) && Semantics != nullptr)
                {
                    (*Semantics)->TryGetStringField(TEXT("kind"), SemanticKind);
                    for (const auto& Pair : (*Semantics)->Values)
                    {
                        AssetSectionBytes->SetNumberField(FString(Pair.Key), JsonValueBytes(Pair.Value));
                    }
                    const TSharedPtr<FJsonObject>* Domain = nullptr;
                    const TSharedPtr<FJsonObject>* Projections = nullptr;
                    if ((*Semantics)->TryGetObjectField(TEXT("domain"), Domain) && Domain != nullptr
                        && (*Domain)->TryGetObjectField(TEXT("projections"), Projections) && Projections != nullptr)
                    {
                        for (const auto& Pair : (*Projections)->Values) Domains.Add(FString(Pair.Key));
                        Domains.Sort();
                    }
                }
                const TArray<TSharedPtr<FJsonValue>>* Omissions = nullptr;
                if (SemanticRoot->TryGetArrayField(TEXT("omissions"), Omissions) && Omissions != nullptr)
                {
                    OmissionCount = Omissions->Num();
                }
                const TSharedPtr<FJsonObject>* Reconstruction = nullptr;
                const TSharedPtr<FJsonObject>* Coverage = nullptr;
                FString Readiness;
                if (SemanticRoot->TryGetObjectField(TEXT("reconstruction"), Reconstruction)
                    && Reconstruction != nullptr
                    && (*Reconstruction)->TryGetObjectField(TEXT("coverage"), Coverage) && Coverage != nullptr)
                {
                    (*Coverage)->TryGetStringField(TEXT("readiness"), Readiness);
                    (*Coverage)->TryGetNumberField(TEXT("exactRatio"), ReconstructionConfidence);
                }
                if (Readiness == TEXT("ready")) Recoverability = TEXT("high");
                else if (Readiness == TEXT("partial")) Recoverability = TEXT("partial");
                else if (Readiness == TEXT("blocked")) Recoverability = TEXT("low");
                if (!SemanticRoot->TryGetStringField(TEXT("exporter"), ExporterName) || ExporterName.IsEmpty())
                {
                    Status = TEXT("failed");
                }
                else
                {
                    Status = CurrentSourceHash.IsEmpty() || CurrentSourceHash != StoredSourceHash
                        ? TEXT("stale") : TEXT("ok");
                }
            }
        }
        Entry->SetStringField(TEXT("exportedAtUtc"), ExportedAtUtc);
        Entry->SetStringField(TEXT("status"), Status);
        Entry->SetStringField(TEXT("semanticKind"), SemanticKind);
        Entry->SetStringField(TEXT("exporter"), ExporterName);
        TArray<TSharedPtr<FJsonValue>> JsonDomains;
        for (const FString& Domain : Domains) JsonDomains.Add(MakeShared<FJsonValueString>(Domain));
        Entry->SetArrayField(TEXT("domains"), JsonDomains);
        Entry->SetStringField(TEXT("recoverability"), Recoverability);
        Entry->SetNumberField(TEXT("reconstructionConfidence"), ReconstructionConfidence);
        Entry->SetNumberField(TEXT("semanticBytes"), SemanticBytes);
        Entry->SetNumberField(TEXT("omissionCount"), OmissionCount);
        Entry->SetObjectField(TEXT("semanticSectionBytes"), AssetSectionBytes);

        TArray<FName> HardDependencies;
        TArray<FName> SoftDependencies;
        TArray<FName> ManagementDependencies;
        TArray<FName> Referencers;
        Registry.GetDependencies(
            Asset.PackageName, HardDependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Hard);
        Registry.GetDependencies(
            Asset.PackageName, SoftDependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Soft);
        Registry.GetDependencies(
            Asset.PackageName, ManagementDependencies,
            UE::AssetRegistry::EDependencyCategory::Manage);
        Registry.GetReferencers(Asset.PackageName, Referencers);
        Entry->SetNumberField(
            TEXT("dependencyCount"),
            HardDependencies.Num() + SoftDependencies.Num() + ManagementDependencies.Num());
        Entry->SetNumberField(TEXT("referencerCount"), Referencers.Num());
        Entry->SetStringField(TEXT("ownerModule"), OwnerModuleForPackage(PackageName));
        Entry->SetStringField(TEXT("primaryAssetId"), UAssetManager::Get().GetPrimaryAssetIdForData(Asset).ToString());

        TArray<FString> TagNames;
        Asset.EnumerateTags([&TagNames](const auto& Tag) { TagNames.Add(Tag.Key.ToString()); });
        TagNames.Sort();
        TArray<TSharedPtr<FJsonValue>> JsonTags;
        for (const FString& TagName : TagNames) JsonTags.Add(MakeShared<FJsonValueString>(TagName));
        Entry->SetArrayField(TEXT("assetTags"), JsonTags);

        auto AddEdges = [&Asset, &OutEdges](const TCHAR* Type, const TArray<FName>& Dependencies)
        {
            for (const FName Dependency : Dependencies)
            {
                const TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
                Edge->SetStringField(TEXT("from"), Asset.PackageName.ToString());
                Edge->SetStringField(TEXT("to"), Dependency.ToString());
                Edge->SetStringField(TEXT("type"), Type);
                OutEdges.Emplace(
                    Asset.PackageName.ToString() + TEXT("|") + Dependency.ToString() + TEXT("|") + Type,
                    Edge);
            }
        };
        AddEdges(TEXT("hard"), HardDependencies);
        AddEdges(TEXT("soft"), SoftDependencies);
        AddEdges(TEXT("management"), ManagementDependencies);
        return Entry;
    }

    void RebuildStatistics(const TSharedRef<FJsonObject>& Root)
    {
        int64 TotalSemanticBytes = 0;
        int64 TotalOmissionCount = 0;
        TMap<FString, int64> SemanticBytesByAssetClass;
        TMap<FString, int32> SemanticAssetCountByClass;
        TMap<FString, int64> SemanticSectionBytes;
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        if (Root->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Assets)
            {
                const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(EntryPtr) || EntryPtr == nullptr) continue;
                const TSharedRef<FJsonObject> Entry = (*EntryPtr).ToSharedRef();
                double SemanticBytesValue = 0.0;
                double OmissionCountValue = 0.0;
                Entry->TryGetNumberField(TEXT("semanticBytes"), SemanticBytesValue);
                Entry->TryGetNumberField(TEXT("omissionCount"), OmissionCountValue);
                const int64 SemanticBytes = static_cast<int64>(SemanticBytesValue);
                TotalSemanticBytes += SemanticBytes;
                TotalOmissionCount += static_cast<int64>(OmissionCountValue);
                if (SemanticBytes > 0)
                {
                    const FString AssetClass = Entry->GetStringField(TEXT("assetClass"));
                    SemanticBytesByAssetClass.FindOrAdd(AssetClass) += SemanticBytes;
                    SemanticAssetCountByClass.FindOrAdd(AssetClass)++;
                }
                const TSharedPtr<FJsonObject>* Sections = nullptr;
                if (Entry->TryGetObjectField(TEXT("semanticSectionBytes"), Sections) && Sections != nullptr)
                {
                    for (const auto& Pair : (*Sections)->Values)
                    {
                        double Bytes = 0.0;
                        if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Bytes))
                        {
                            SemanticSectionBytes.FindOrAdd(FString(Pair.Key)) += static_cast<int64>(Bytes);
                        }
                    }
                }
            }
        }
        const TSharedRef<FJsonObject> Statistics = MakeShared<FJsonObject>();
        Statistics->SetNumberField(TEXT("semanticFileBytes"), TotalSemanticBytes);
        Statistics->SetNumberField(TEXT("omissionCount"), TotalOmissionCount);
        const TSharedRef<FJsonObject> Sections = MakeShared<FJsonObject>();
        TArray<FString> SectionNames;
        SemanticSectionBytes.GetKeys(SectionNames);
        SectionNames.Sort();
        for (const FString& Name : SectionNames)
        {
            Sections->SetNumberField(Name, SemanticSectionBytes.FindChecked(Name));
        }
        Statistics->SetObjectField(TEXT("semanticSectionBytes"), Sections);
        TArray<FString> AssetClasses;
        SemanticBytesByAssetClass.GetKeys(AssetClasses);
        AssetClasses.Sort([&](const FString& Left, const FString& Right)
        {
            const int64 LeftBytes = SemanticBytesByAssetClass.FindChecked(Left);
            const int64 RightBytes = SemanticBytesByAssetClass.FindChecked(Right);
            return LeftBytes == RightBytes ? Left < Right : LeftBytes > RightBytes;
        });
        TArray<TSharedPtr<FJsonValue>> ByAssetClass;
        for (const FString& AssetClass : AssetClasses)
        {
            const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("assetClass"), AssetClass);
            Item->SetNumberField(TEXT("assetCount"), SemanticAssetCountByClass.FindChecked(AssetClass));
            Item->SetNumberField(TEXT("semanticBytes"), SemanticBytesByAssetClass.FindChecked(AssetClass));
            ByAssetClass.Add(MakeShared<FJsonValueObject>(Item));
        }
        Statistics->SetArrayField(TEXT("byAssetClass"), ByAssetClass);
        Root->SetObjectField(TEXT("statistics"), Statistics);
    }
}

bool FUERingIndexManager::Rebuild(FString& OutError)
{
    using namespace UERingIndexManager;
    const double StartedAt = FPlatformTime::Seconds();

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;
    Registry.GetAllAssets(Assets, true);
    Assets.RemoveAll([](const FAssetData& Asset)
    {
        return !FUERingExportManager::Get().IsSupportedPackageName(Asset.PackageName.ToString());
    });
    FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);

    TMap<FString, int32> ExclusionCounts;
    Assets.RemoveAll([&ExclusionCounts](const FAssetData& Asset)
    {
        const FString Reason = FUERingExportManager::Get().GetExclusionReason(Asset);
        if (Reason.IsEmpty()) return false;
        ExclusionCounts.FindOrAdd(Reason)++;
        return true;
    });

    FDateTime LatestSourceTime = FDateTime::MinValue();
    TArray<TSharedPtr<FJsonValue>> JsonAssets;
    TArray<TPair<FString, TSharedRef<FJsonObject>>> SortedEdges;
    int64 TotalSemanticBytes = 0;
    int64 TotalOmissionCount = 0;
    TMap<FString, int64> SemanticBytesByAssetClass;
    TMap<FString, int32> SemanticAssetCountByClass;
    TMap<FString, int64> SemanticSectionBytes;
    for (const FAssetData& Asset : Assets)
    {
        const bool bSupported = FUERingExportManager::Get().CanExport(Asset);
        const bool bIsMap = Asset.IsInstanceOf(UWorld::StaticClass());
        const FString SourceFile = FPackageName::LongPackageNameToFilename(
            Asset.PackageName.ToString(),
            bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
        const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(
            Asset.PackageName.ToString(), bIsMap);
        LatestSourceTime = FMath::Max(LatestSourceTime, IFileManager::Get().GetTimeStamp(*SourceFile));

        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("packageName"), Asset.PackageName.ToString());
        Entry->SetStringField(TEXT("objectPath"), Asset.GetSoftObjectPath().ToString());
        Entry->SetStringField(TEXT("assetClass"), Asset.AssetClassPath.GetAssetName().ToString());
        Entry->SetStringField(TEXT("sourceFile"), RelativeToProject(SourceFile));
        if (bSupported)
        {
            Entry->SetStringField(TEXT("semanticFile"), RelativeToProject(SemanticFile));
        }
        else
        {
            Entry->SetField(TEXT("semanticFile"), MakeShared<FJsonValueNull>());
        }
        const FString CurrentSourceHash = IFileManager::Get().FileExists(*SourceFile) ? HashFile(SourceFile) : FString();
        Entry->SetStringField(TEXT("sourceHash"), CurrentSourceHash);
        Entry->SetStringField(
            TEXT("semanticHash"),
            IFileManager::Get().FileExists(*SemanticFile) ? HashFile(SemanticFile) : FString());

        FString Status = bSupported ? TEXT("missing") : TEXT("unsupported");
        FString ExportedAtUtc;
        FString SemanticKind;
        FString ExporterName;
        TArray<FString> Domains;
        FString Recoverability;
        double ReconstructionConfidence = 0.0;
        int64 SemanticBytes = 0;
        int32 OmissionCount = 0;
        const TSharedRef<FJsonObject> AssetSectionBytes = MakeShared<FJsonObject>();
        if (bSupported && !IFileManager::Get().FileExists(*SourceFile))
        {
            Status = TEXT("failed");
        }
        else if (bSupported && IFileManager::Get().FileExists(*SemanticFile))
        {
            SemanticBytes = FMath::Max<int64>(0, IFileManager::Get().FileSize(*SemanticFile));
            TSharedPtr<FJsonObject> SemanticRoot;
            const TSharedPtr<FJsonObject>* SemanticAsset = nullptr;
            FString SchemaVersion;
            FString StoredSourceHash;
            if (!ReadSemantic(SemanticFile, SemanticRoot)
                || !SemanticRoot->TryGetStringField(TEXT("schemaVersion"), SchemaVersion)
                || SchemaVersion != UE_RING_SCHEMA_VERSION
                || !SemanticRoot->TryGetObjectField(TEXT("asset"), SemanticAsset)
                || SemanticAsset == nullptr
                || !(*SemanticAsset)->TryGetStringField(TEXT("sourceHash"), StoredSourceHash))
            {
                Status = TEXT("failed");
            }
            else
            {
                const FDateTime SourceTimestamp = IFileManager::Get().GetTimeStamp(*SourceFile);
                ExportedAtUtc = SourceTimestamp == FDateTime::MinValue()
                    ? TEXT("1970-01-01T00:00:00Z")
                    : SourceTimestamp.ToIso8601();
                const TSharedPtr<FJsonObject>* SemanticObject = nullptr;
                if (SemanticRoot->TryGetObjectField(TEXT("semantics"), SemanticObject)
                    && SemanticObject != nullptr)
                {
                    (*SemanticObject)->TryGetStringField(TEXT("kind"), SemanticKind);
                    for (const auto& SemanticField : (*SemanticObject)->Values)
                    {
                        const int64 SectionBytes = JsonValueBytes(SemanticField.Value);
                        SemanticSectionBytes.FindOrAdd(FString(SemanticField.Key)) += SectionBytes;
                        AssetSectionBytes->SetNumberField(FString(SemanticField.Key), SectionBytes);
                    }
                    const TSharedPtr<FJsonObject>* DomainObject = nullptr;
                    const TSharedPtr<FJsonObject>* Projections = nullptr;
                    if ((*SemanticObject)->TryGetObjectField(TEXT("domain"), DomainObject)
                        && DomainObject != nullptr
                        && (*DomainObject)->TryGetObjectField(TEXT("projections"), Projections)
                        && Projections != nullptr)
                    {
                        for (const auto& Projection : (*Projections)->Values)
                        {
                            Domains.Emplace(Projection.Key.Len(), *Projection.Key);
                        }
                        Domains.Sort();
                    }
                }
                const TArray<TSharedPtr<FJsonValue>>* Omissions = nullptr;
                if (SemanticRoot->TryGetArrayField(TEXT("omissions"), Omissions)
                    && Omissions != nullptr)
                {
                    OmissionCount = Omissions->Num();
                }
                const TSharedPtr<FJsonObject>* Reconstruction = nullptr;
                const TSharedPtr<FJsonObject>* ReconstructionCoverage = nullptr;
                if (SemanticRoot->TryGetObjectField(TEXT("reconstruction"), Reconstruction)
                    && Reconstruction != nullptr)
                {
                    FString Readiness;
                    if ((*Reconstruction)->TryGetObjectField(TEXT("coverage"), ReconstructionCoverage)
                        && ReconstructionCoverage != nullptr)
                    {
                        (*ReconstructionCoverage)->TryGetStringField(TEXT("readiness"), Readiness);
                        (*ReconstructionCoverage)->TryGetNumberField(
                            TEXT("exactRatio"),
                            ReconstructionConfidence);
                    }
                    if (Readiness == TEXT("ready"))
                    {
                        Recoverability = TEXT("high");
                    }
                    else if (Readiness == TEXT("partial"))
                    {
                        Recoverability = TEXT("partial");
                    }
                    else if (Readiness == TEXT("blocked"))
                    {
                        Recoverability = TEXT("low");
                    }
                }
                if (!SemanticRoot->TryGetStringField(TEXT("exporter"), ExporterName)
                    || ExporterName.IsEmpty())
                {
                    Status = TEXT("failed");
                }
                else
                {
                    Status = CurrentSourceHash.IsEmpty() || CurrentSourceHash != StoredSourceHash
                        ? TEXT("stale")
                        : TEXT("ok");
                }
            }
        }
        Entry->SetStringField(TEXT("exportedAtUtc"), ExportedAtUtc);
        Entry->SetStringField(TEXT("status"), Status);
        Entry->SetStringField(TEXT("semanticKind"), SemanticKind);
        Entry->SetStringField(TEXT("exporter"), ExporterName);
        TArray<TSharedPtr<FJsonValue>> JsonDomains;
        for (const FString& Domain : Domains)
        {
            JsonDomains.Add(MakeShared<FJsonValueString>(Domain));
        }
        Entry->SetArrayField(TEXT("domains"), JsonDomains);
        Entry->SetStringField(TEXT("recoverability"), Recoverability);
        Entry->SetNumberField(TEXT("reconstructionConfidence"), ReconstructionConfidence);
        Entry->SetNumberField(TEXT("semanticBytes"), SemanticBytes);
        Entry->SetNumberField(TEXT("omissionCount"), OmissionCount);
        Entry->SetObjectField(TEXT("semanticSectionBytes"), AssetSectionBytes);
        if (SemanticBytes > 0)
        {
            TotalSemanticBytes += SemanticBytes;
            TotalOmissionCount += OmissionCount;
            const FString AssetClass = Asset.AssetClassPath.GetAssetName().ToString();
            SemanticBytesByAssetClass.FindOrAdd(AssetClass) += SemanticBytes;
            SemanticAssetCountByClass.FindOrAdd(AssetClass)++;
        }

        TArray<FName> HardDependencies;
        TArray<FName> SoftDependencies;
        TArray<FName> ManagementDependencies;
        TArray<FName> Referencers;
        Registry.GetDependencies(
            Asset.PackageName, HardDependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Hard);
        Registry.GetDependencies(
            Asset.PackageName, SoftDependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Soft);
        Registry.GetDependencies(
            Asset.PackageName, ManagementDependencies,
            UE::AssetRegistry::EDependencyCategory::Manage);
        Registry.GetReferencers(Asset.PackageName, Referencers);
        Entry->SetNumberField(
            TEXT("dependencyCount"),
            HardDependencies.Num() + SoftDependencies.Num() + ManagementDependencies.Num());
        Entry->SetNumberField(TEXT("referencerCount"), Referencers.Num());
        Entry->SetStringField(TEXT("ownerModule"), OwnerModuleForPackage(Asset.PackageName.ToString()));
        Entry->SetStringField(TEXT("primaryAssetId"), UAssetManager::Get().GetPrimaryAssetIdForData(Asset).ToString());

        TArray<FString> TagNames;
        Asset.EnumerateTags([&TagNames](const auto& Tag)
        {
            TagNames.Add(Tag.Key.ToString());
        });
        TagNames.Sort();
        TArray<TSharedPtr<FJsonValue>> JsonTags;
        for (const FString& TagName : TagNames)
        {
            JsonTags.Add(MakeShared<FJsonValueString>(TagName));
        }
        Entry->SetArrayField(TEXT("assetTags"), JsonTags);
        JsonAssets.Add(MakeShared<FJsonValueObject>(Entry));

        auto AddEdges = [&Asset, &SortedEdges](
            const TCHAR* Type,
            const TArray<FName>& Dependencies)
        {
            for (const FName Dependency : Dependencies)
            {
                const TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
                Edge->SetStringField(TEXT("from"), Asset.PackageName.ToString());
                Edge->SetStringField(TEXT("to"), Dependency.ToString());
                Edge->SetStringField(TEXT("type"), Type);
                SortedEdges.Emplace(
                    Asset.PackageName.ToString() + TEXT("|") + Dependency.ToString() + TEXT("|") + Type,
                    Edge);
            }
        };
        AddEdges(TEXT("hard"), HardDependencies);
        AddEdges(TEXT("soft"), SoftDependencies);
        AddEdges(TEXT("management"), ManagementDependencies);
    }
    SortedEdges.Sort([](const TPair<FString, TSharedRef<FJsonObject>>& Left, const TPair<FString, TSharedRef<FJsonObject>>& Right)
    {
        return Left.Key < Right.Key;
    });

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.index"));
    Root->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    const TSharedRef<FJsonObject> Generator = MakeShared<FJsonObject>();
    Generator->SetStringField(TEXT("name"), UE_RING_PLUGIN_NAME);
    Generator->SetStringField(TEXT("version"), UE_RING_PLUGIN_VERSION);
    Root->SetObjectField(TEXT("generator"), Generator);
    const TSharedRef<FJsonObject> Engine = MakeShared<FJsonObject>();
    Engine->SetStringField(TEXT("version"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
    Root->SetObjectField(TEXT("engine"), Engine);
    const TSharedRef<FJsonObject> Project = MakeShared<FJsonObject>();
    Project->SetStringField(TEXT("name"), FApp::GetProjectName());
    Project->SetStringField(TEXT("uproject"), FPaths::GetCleanFilename(FPaths::GetProjectFilePath()));
    Root->SetObjectField(TEXT("project"), Project);
    Root->SetStringField(
        TEXT("generatedAtUtc"),
        LatestSourceTime == FDateTime::MinValue() ? TEXT("1970-01-01T00:00:00Z") : LatestSourceTime.ToIso8601());
    const TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
    Coverage->SetNumberField(TEXT("indexedAssetCount"), JsonAssets.Num());
    int32 ExcludedAssetCount = 0;
    TArray<FString> ExclusionReasons;
    ExclusionCounts.GetKeys(ExclusionReasons);
    ExclusionReasons.Sort();
    TArray<TSharedPtr<FJsonValue>> Exclusions;
    for (const FString& Reason : ExclusionReasons)
    {
        const int32 Count = ExclusionCounts.FindChecked(Reason);
        ExcludedAssetCount += Count;
        const TSharedRef<FJsonObject> Exclusion = MakeShared<FJsonObject>();
        Exclusion->SetStringField(TEXT("reason"), Reason);
        Exclusion->SetNumberField(TEXT("count"), Count);
        Exclusions.Add(MakeShared<FJsonValueObject>(Exclusion));
    }
    Coverage->SetNumberField(TEXT("excludedAssetCount"), ExcludedAssetCount);
    if (!Exclusions.IsEmpty()) Coverage->SetArrayField(TEXT("exclusions"), Exclusions);
    Root->SetObjectField(TEXT("coverage"), Coverage);
    const TSharedRef<FJsonObject> Statistics = MakeShared<FJsonObject>();
    Statistics->SetNumberField(TEXT("semanticFileBytes"), TotalSemanticBytes);
    Statistics->SetNumberField(TEXT("omissionCount"), TotalOmissionCount);
    const TSharedRef<FJsonObject> Sections = MakeShared<FJsonObject>();
    TArray<FString> SectionNames;
    SemanticSectionBytes.GetKeys(SectionNames);
    SectionNames.Sort();
    for (const FString& SectionName : SectionNames)
    {
        Sections->SetNumberField(SectionName, SemanticSectionBytes.FindChecked(SectionName));
    }
    Statistics->SetObjectField(TEXT("semanticSectionBytes"), Sections);
    TArray<FString> AssetClasses;
    SemanticBytesByAssetClass.GetKeys(AssetClasses);
    AssetClasses.Sort([&SemanticBytesByAssetClass](const FString& Left, const FString& Right)
    {
        const int64 LeftBytes = SemanticBytesByAssetClass.FindChecked(Left);
        const int64 RightBytes = SemanticBytesByAssetClass.FindChecked(Right);
        return LeftBytes == RightBytes ? Left < Right : LeftBytes > RightBytes;
    });
    TArray<TSharedPtr<FJsonValue>> ByAssetClass;
    for (const FString& AssetClass : AssetClasses)
    {
        const TSharedRef<FJsonObject> ClassStatistics = MakeShared<FJsonObject>();
        ClassStatistics->SetStringField(TEXT("assetClass"), AssetClass);
        ClassStatistics->SetNumberField(
            TEXT("assetCount"), SemanticAssetCountByClass.FindChecked(AssetClass));
        ClassStatistics->SetNumberField(
            TEXT("semanticBytes"), SemanticBytesByAssetClass.FindChecked(AssetClass));
        ByAssetClass.Add(MakeShared<FJsonValueObject>(ClassStatistics));
    }
    Statistics->SetArrayField(TEXT("byAssetClass"), ByAssetClass);
    Root->SetObjectField(TEXT("statistics"), Statistics);
    Root->SetArrayField(TEXT("assets"), JsonAssets);

    const FString IndexDirectory = FPaths::Combine(FUERingExportManager::Get().GetOutputRoot(), TEXT("index"));
    if (!WriteJson(FPaths::Combine(IndexDirectory, TEXT("project.uesem.index.json")), Root, OutError))
    {
        return false;
    }

    const TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
    Graph->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.dependencies"));
    Graph->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    TArray<TSharedPtr<FJsonValue>> Edges;
    for (const TPair<FString, TSharedRef<FJsonObject>>& Pair : SortedEdges)
    {
        Edges.Add(MakeShared<FJsonValueObject>(Pair.Value));
    }
    Graph->SetArrayField(TEXT("edges"), Edges);
    if (!WriteJson(FPaths::Combine(IndexDirectory, TEXT("dependencies.uesem.json")), Graph, OutError))
    {
        return false;
    }

    TSharedPtr<FJsonObject> ProjectGraph;
    if (!FUERingProjectGraphBuilder::Rebuild(IndexDirectory, Root, Graph, ProjectGraph, OutError)
        || !ProjectGraph.IsValid())
    {
        return false;
    }

    const FString SqliteFile = FUERingSqliteIndexer::GetDatabaseFile(IndexDirectory);
    if (!GetDefault<UUERingSettings>()->bIncludeSqliteIndex)
    {
        IFileManager::Get().Delete(*SqliteFile, false, true);
        return true;
    }
    const bool bSucceeded = FUERingSqliteIndexer::Rebuild(
        IndexDirectory, Root, Graph, ProjectGraph.ToSharedRef(), OutError);
    UE_LOG(
        LogUERingIndex,
        Display,
        TEXT("Full index rebuild processed %d asset(s) in %.3f seconds (success=%s)."),
        JsonAssets.Num(),
        FPlatformTime::Seconds() - StartedAt,
        bSucceeded ? TEXT("true") : TEXT("false"));
    return bSucceeded;
}

bool FUERingIndexManager::UpdatePackages(const TArray<FName>& PackageNames, FString& OutError)
{
    using namespace UERingIndexManager;

    if (PackageNames.IsEmpty()) return true;
    const double StartedAt = FPlatformTime::Seconds();
    double StageStartedAt = StartedAt;
    const FString IndexDirectory = FPaths::Combine(FUERingExportManager::Get().GetOutputRoot(), TEXT("index"));
    const FString IndexFile = FPaths::Combine(IndexDirectory, TEXT("project.uesem.index.json"));
    const FString DependencyFile = FPaths::Combine(IndexDirectory, TEXT("dependencies.uesem.json"));
    const bool bUseSqlite = GetDefault<UUERingSettings>()->bIncludeSqliteIndex;
    TSharedPtr<FJsonObject> Root;
    TSharedPtr<FJsonObject> DependencyGraph;
    FString ExistingSchemaVersion;
    if (!ReadSemantic(IndexFile, Root) || !ReadSemantic(DependencyFile, DependencyGraph)
        || !Root->TryGetStringField(TEXT("schemaVersion"), ExistingSchemaVersion)
        || ExistingSchemaVersion != UE_RING_SCHEMA_VERSION)
    {
        UE_LOG(LogUERingIndex, Display, TEXT("Incremental index bootstrap requires a full rebuild."));
        return Rebuild(OutError);
    }
    if (bUseSqlite && !FUERingSqliteIndexer::IsIncrementalDatabaseCompatible(IndexDirectory))
    {
        UE_LOG(LogUERingIndex, Display, TEXT("Incremental SQLite bootstrap requires a full rebuild."));
        return Rebuild(OutError);
    }
    const double LoadSeconds = FPlatformTime::Seconds() - StageStartedAt;
    StageStartedAt = FPlatformTime::Seconds();

    const TArray<TSharedPtr<FJsonValue>>* ExistingAssets = nullptr;
    if (!Root->TryGetArrayField(TEXT("assets"), ExistingAssets) || ExistingAssets == nullptr)
    {
        return Rebuild(OutError);
    }
    TMap<FString, TSharedPtr<FJsonObject>> EntriesByPackage;
    for (const TSharedPtr<FJsonValue>& Value : *ExistingAssets)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        const TSharedPtr<FJsonObject>* SectionBytes = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Entry) || Entry == nullptr
            || !(*Entry)->TryGetObjectField(TEXT("semanticSectionBytes"), SectionBytes)
            || SectionBytes == nullptr)
        {
            UE_LOG(LogUERingIndex, Display, TEXT("Incremental statistics bootstrap requires a full rebuild."));
            return Rebuild(OutError);
        }
        EntriesByPackage.Add((*Entry)->GetStringField(TEXT("packageName")), *Entry);
    }

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> AllAssets;
    Registry.GetAllAssets(AllAssets, true);
    AllAssets.RemoveAll([](const FAssetData& Asset)
    {
        return !FUERingExportManager::Get().IsSupportedPackageName(Asset.PackageName.ToString());
    });
    FUERingExportManager::Get().CanonicalizeAssetsByPackage(AllAssets);

    TMap<FString, int32> ExclusionCounts;
    TMap<FString, FAssetData> CurrentAssets;
    FDateTime LatestSourceTime = FDateTime::MinValue();
    for (const FAssetData& Asset : AllAssets)
    {
        const FString Reason = FUERingExportManager::Get().GetExclusionReason(Asset);
        if (!Reason.IsEmpty())
        {
            ExclusionCounts.FindOrAdd(Reason)++;
            continue;
        }
        const FString PackageName = Asset.PackageName.ToString();
        CurrentAssets.Add(PackageName, Asset);
        const bool bIsMap = Asset.IsInstanceOf(UWorld::StaticClass());
        const FString SourceFile = FPackageName::LongPackageNameToFilename(
            PackageName,
            bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
        LatestSourceTime = FMath::Max(LatestSourceTime, IFileManager::Get().GetTimeStamp(*SourceFile));
    }

    TSet<FString> SeedPackages;
    for (const FName PackageName : PackageNames)
    {
        const FString Name = PackageName.ToString();
        if (FUERingExportManager::Get().IsSupportedPackageName(Name)) SeedPackages.Add(Name);
    }
    if (SeedPackages.IsEmpty()) return true;
    TSet<FString> DirtyPackages = SeedPackages;
    TSet<FString> GraphContributorPackages = SeedPackages;
    TSet<FString> MembershipChangedPackages;
    for (const FString& Seed : SeedPackages)
    {
        if (EntriesByPackage.Contains(Seed) != CurrentAssets.Contains(Seed))
        {
            MembershipChangedPackages.Add(Seed);
        }
    }
    auto AddTracked = [&](const FString& PackageName)
    {
        if (CurrentAssets.Contains(PackageName) || EntriesByPackage.Contains(PackageName))
        {
            DirtyPackages.Add(PackageName);
        }
    };

    const TArray<TSharedPtr<FJsonValue>>* ExistingEdges = nullptr;
    if (DependencyGraph->TryGetArrayField(TEXT("edges"), ExistingEdges) && ExistingEdges != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *ExistingEdges)
        {
            const TSharedPtr<FJsonObject>* Edge = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Edge) || Edge == nullptr) continue;
            const FString From = (*Edge)->GetStringField(TEXT("from"));
            const FString To = (*Edge)->GetStringField(TEXT("to"));
            if (SeedPackages.Contains(From)) AddTracked(To);
            if (MembershipChangedPackages.Contains(To)) GraphContributorPackages.Add(From);
        }
    }

    for (const FString& Seed : SeedPackages)
    {
        const FName SeedName(*Seed);
        TArray<FName> Related;
        if (MembershipChangedPackages.Contains(Seed))
        {
            Registry.GetReferencers(SeedName, Related);
            for (const FName Name : Related)
            {
                if (CurrentAssets.Contains(Name.ToString()) || EntriesByPackage.Contains(Name.ToString()))
                {
                    GraphContributorPackages.Add(Name.ToString());
                }
            }
        }
        Related.Reset();
        Registry.GetDependencies(
            SeedName, Related,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Hard);
        for (const FName Name : Related) AddTracked(Name.ToString());
        Related.Reset();
        Registry.GetDependencies(
            SeedName, Related,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Soft);
        for (const FName Name : Related) AddTracked(Name.ToString());
        Related.Reset();
        Registry.GetDependencies(SeedName, Related, UE::AssetRegistry::EDependencyCategory::Manage);
        for (const FName Name : Related) AddTracked(Name.ToString());
    }

    if (!MembershipChangedPackages.IsEmpty() && bUseSqlite)
    {
        if (!FUERingSqliteIndexer::FindContributorsTargetingAssets(
                IndexDirectory,
                MembershipChangedPackages,
                GraphContributorPackages))
        {
            UE_LOG(LogUERingIndex, Display, TEXT("Incremental graph membership query requires a full rebuild."));
            return Rebuild(OutError);
        }
    }
    else if (!MembershipChangedPackages.IsEmpty())
    {
        TSharedPtr<FJsonObject> ExistingProjectGraph;
        if (ReadSemantic(FUERingProjectGraphBuilder::GetGraphFile(IndexDirectory), ExistingProjectGraph))
        {
            const TArray<TSharedPtr<FJsonValue>>* GraphEdges = nullptr;
            if (ExistingProjectGraph->TryGetArrayField(TEXT("edges"), GraphEdges) && GraphEdges != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& Value : *GraphEdges)
                {
                    const TSharedPtr<FJsonObject>* Edge = nullptr;
                    if (!Value.IsValid() || !Value->TryGetObject(Edge) || Edge == nullptr) continue;
                    FString Target;
                    FString Contributor;
                    (*Edge)->TryGetStringField(TEXT("to"), Target);
                    (*Edge)->TryGetStringField(TEXT("contributorPackage"), Contributor);
                    for (const FString& Seed : MembershipChangedPackages)
                    {
                        if (Target == TEXT("asset:") + Seed && !Contributor.IsEmpty())
                        {
                            GraphContributorPackages.Add(Contributor);
                        }
                    }
                }
            }
        }
    }
    const double DiscoverSeconds = FPlatformTime::Seconds() - StageStartedAt;
    StageStartedAt = FPlatformTime::Seconds();

    TArray<TPair<FString, TSharedRef<FJsonObject>>> NewEdges;
    for (const FString& PackageName : DirtyPackages)
    {
        const TSharedPtr<FJsonObject> PreviousEntry = EntriesByPackage.FindRef(PackageName);
        EntriesByPackage.Remove(PackageName);
        if (const FAssetData* Asset = CurrentAssets.Find(PackageName))
        {
            const TSharedRef<FJsonObject> NewEntry = BuildEntry(*Asset, Registry, NewEdges);
            EntriesByPackage.Add(PackageName, NewEntry);
            if (SeedPackages.Contains(PackageName)
                && !MembershipChangedPackages.Contains(PackageName)
                && PreviousEntry.IsValid()
                && PreviousEntry->GetStringField(TEXT("semanticHash"))
                    == NewEntry->GetStringField(TEXT("semanticHash")))
            {
                GraphContributorPackages.Remove(PackageName);
            }
        }
    }

    TArray<FString> SortedEntryNames;
    EntriesByPackage.GetKeys(SortedEntryNames);
    SortedEntryNames.Sort([](const FString& Left, const FString& Right)
    {
        const FName LeftName(*Left);
        const FName RightName(*Right);
        return LeftName != RightName ? LeftName.LexicalLess(RightName) : Left < Right;
    });
    TArray<TSharedPtr<FJsonValue>> JsonAssets;
    JsonAssets.Reserve(SortedEntryNames.Num());
    for (const FString& PackageName : SortedEntryNames)
    {
        JsonAssets.Add(MakeShared<FJsonValueObject>(EntriesByPackage.FindChecked(PackageName).ToSharedRef()));
    }
    Root->SetArrayField(TEXT("assets"), JsonAssets);
    Root->SetStringField(
        TEXT("generatedAtUtc"),
        LatestSourceTime == FDateTime::MinValue() ? TEXT("1970-01-01T00:00:00Z") : LatestSourceTime.ToIso8601());

    const TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
    Coverage->SetNumberField(TEXT("indexedAssetCount"), JsonAssets.Num());
    int32 ExcludedAssetCount = 0;
    TArray<FString> ExclusionReasons;
    ExclusionCounts.GetKeys(ExclusionReasons);
    ExclusionReasons.Sort();
    TArray<TSharedPtr<FJsonValue>> Exclusions;
    for (const FString& Reason : ExclusionReasons)
    {
        const int32 Count = ExclusionCounts.FindChecked(Reason);
        ExcludedAssetCount += Count;
        const TSharedRef<FJsonObject> Exclusion = MakeShared<FJsonObject>();
        Exclusion->SetStringField(TEXT("reason"), Reason);
        Exclusion->SetNumberField(TEXT("count"), Count);
        Exclusions.Add(MakeShared<FJsonValueObject>(Exclusion));
    }
    Coverage->SetNumberField(TEXT("excludedAssetCount"), ExcludedAssetCount);
    if (!Exclusions.IsEmpty()) Coverage->SetArrayField(TEXT("exclusions"), Exclusions);
    Root->SetObjectField(TEXT("coverage"), Coverage);
    RebuildStatistics(Root.ToSharedRef());

    TArray<TPair<FString, TSharedRef<FJsonObject>>> MergedEdges;
    if (ExistingEdges != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *ExistingEdges)
        {
            const TSharedPtr<FJsonObject>* Edge = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Edge) || Edge == nullptr) continue;
            const FString From = (*Edge)->GetStringField(TEXT("from"));
            if (DirtyPackages.Contains(From)) continue;
            const FString Key = From + TEXT("|") + (*Edge)->GetStringField(TEXT("to"))
                + TEXT("|") + (*Edge)->GetStringField(TEXT("type"));
            MergedEdges.Emplace(Key, (*Edge).ToSharedRef());
        }
    }
    MergedEdges.Append(NewEdges);
    MergedEdges.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
    TArray<TSharedPtr<FJsonValue>> JsonEdges;
    JsonEdges.Reserve(MergedEdges.Num());
    for (const auto& Pair : MergedEdges) JsonEdges.Add(MakeShared<FJsonValueObject>(Pair.Value));
    DependencyGraph->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    DependencyGraph->SetArrayField(TEXT("edges"), JsonEdges);
    const double EntrySeconds = FPlatformTime::Seconds() - StageStartedAt;
    StageStartedAt = FPlatformTime::Seconds();

    if (!WriteJson(IndexFile, Root.ToSharedRef(), OutError)
        || !WriteJson(DependencyFile, DependencyGraph.ToSharedRef(), OutError))
    {
        return false;
    }
    const double JsonWriteSeconds = FPlatformTime::Seconds() - StageStartedAt;
    StageStartedAt = FPlatformTime::Seconds();
    TSharedPtr<FJsonObject> ProjectGraph;
    if (GraphContributorPackages.IsEmpty())
    {
        ProjectGraph = MakeShared<FJsonObject>();
    }
    else if (bUseSqlite)
    {
        if (!FUERingProjectGraphBuilder::BuildContributions(
                Root.ToSharedRef(),
                DependencyGraph.ToSharedRef(),
                GraphContributorPackages,
                ProjectGraph)
            || !ProjectGraph.IsValid())
        {
            OutError = TEXT("Could not build incremental unified graph contributions.");
            return false;
        }
    }
    else if (!FUERingProjectGraphBuilder::Update(
                IndexDirectory,
                Root.ToSharedRef(),
                DependencyGraph.ToSharedRef(),
                GraphContributorPackages,
                ProjectGraph,
                OutError)
            || !ProjectGraph.IsValid())
    {
        return false;
    }
    double GraphSeconds = FPlatformTime::Seconds() - StageStartedAt;
    StageStartedAt = FPlatformTime::Seconds();

    const FString SqliteFile = FUERingSqliteIndexer::GetDatabaseFile(IndexDirectory);
    if (!bUseSqlite)
    {
        IFileManager::Get().Delete(*SqliteFile, false, true);
    }
    else if (!FUERingSqliteIndexer::Update(
        IndexDirectory,
        Root.ToSharedRef(),
        DependencyGraph.ToSharedRef(),
        ProjectGraph.ToSharedRef(),
        DirtyPackages,
        GraphContributorPackages,
        OutError))
    {
        return false;
    }
    const double SqliteSeconds = FPlatformTime::Seconds() - StageStartedAt;
    if (bUseSqlite && !GraphContributorPackages.IsEmpty())
    {
        StageStartedAt = FPlatformTime::Seconds();
        if (!FUERingProjectGraphBuilder::MaterializeFromSqlite(
                IndexDirectory,
                Root.ToSharedRef(),
                ProjectGraph,
                OutError)
            || !ProjectGraph.IsValid())
        {
            return false;
        }
        GraphSeconds += FPlatformTime::Seconds() - StageStartedAt;
    }
    UE_LOG(
        LogUERingIndex,
        Display,
        TEXT("Incremental index updated %d asset row(s) and %d graph contributor(s) from %d seed(s) "
             "in %.3f seconds (load=%.3f discover=%.3f entries=%.3f json=%.3f graph=%.3f sqlite=%.3f)."),
        DirtyPackages.Num(),
        GraphContributorPackages.Num(),
        SeedPackages.Num(),
        FPlatformTime::Seconds() - StartedAt,
        LoadSeconds,
        DiscoverSeconds,
        EntrySeconds,
        JsonWriteSeconds,
        GraphSeconds,
        SqliteSeconds);
    return true;
}
