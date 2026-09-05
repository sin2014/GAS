#include "UERingValidator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "PlatformCryptoContextIncludes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UERingExportManager.h"
#include "UERingSettings.h"
#include "UERingVersion.h"

namespace UERingValidator
{
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
}

FUERingValidationReport FUERingValidator::Validate()
{
    using namespace UERingValidator;

    FUERingValidationReport Report;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    TArray<FAssetData> Assets;
    Registry.GetAllAssets(Assets, true);
    Assets.RemoveAll([](const FAssetData& Asset)
    {
        return !FUERingExportManager::Get().CanExport(Asset);
    });
    FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);

    TSet<FString> ExpectedSemanticFiles;
    for (const FAssetData& Asset : Assets)
    {
        ++Report.Checked;
        const bool bIsMap = Asset.IsInstanceOf(UWorld::StaticClass());
        const FString SourceFile = FPackageName::LongPackageNameToFilename(
            Asset.PackageName.ToString(),
            bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
        const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(
            Asset.PackageName.ToString(), bIsMap);
        ExpectedSemanticFiles.Add(FPaths::ConvertRelativePathToFull(SemanticFile));
        if (!IFileManager::Get().FileExists(*SemanticFile))
        {
            ++Report.Missing;
            Report.Messages.Add(TEXT("missing: ") + Asset.PackageName.ToString());
            continue;
        }

        TSharedPtr<FJsonObject> Root;
        const TSharedPtr<FJsonObject>* AssetObject = nullptr;
        FString SchemaVersion;
        FString Exporter;
        FString Profile;
        FString InputFingerprint;
        FString StoredHash;
        if (!ReadSemantic(SemanticFile, Root)
            || !Root->TryGetStringField(TEXT("schemaVersion"), SchemaVersion)
            || SchemaVersion != UE_RING_SCHEMA_VERSION
            || !Root->TryGetStringField(TEXT("exporter"), Exporter)
            || Exporter.IsEmpty()
            || !Root->TryGetStringField(TEXT("profile"), Profile)
            || Profile != UERingExportProfileName(GetDefault<UUERingSettings>()->ExportProfile)
            || !Root->TryGetStringField(TEXT("inputFingerprint"), InputFingerprint)
            || !InputFingerprint.StartsWith(TEXT("sha256:"))
            || InputFingerprint.Len() != 71
            || !Root->TryGetObjectField(TEXT("asset"), AssetObject)
            || AssetObject == nullptr
            || !(*AssetObject)->TryGetStringField(TEXT("sourceHash"), StoredHash))
        {
            ++Report.Invalid;
            Report.Messages.Add(TEXT("invalid: ") + SemanticFile);
            continue;
        }
        const FString CurrentHash = HashFile(SourceFile);
        if (CurrentHash.IsEmpty() || CurrentHash != StoredHash)
        {
            ++Report.Stale;
            Report.Messages.Add(TEXT("stale: ") + Asset.PackageName.ToString());
        }
    }

    for (const TCHAR* Subdirectory : { TEXT("content"), TEXT("maps") })
    {
        TArray<FString> SemanticFiles;
        IFileManager::Get().FindFilesRecursive(
            SemanticFiles,
            *FPaths::Combine(FUERingExportManager::Get().GetOutputRoot(), Subdirectory),
            TEXT("*.uesem.json"), true, false);
        for (const FString& SemanticFile : SemanticFiles)
        {
            if (!ExpectedSemanticFiles.Contains(FPaths::ConvertRelativePathToFull(SemanticFile)))
            {
                ++Report.Orphan;
                Report.Messages.Add(TEXT("orphan: ") + SemanticFile);
            }
        }
    }
    Report.Messages.Sort();
    return Report;
}
