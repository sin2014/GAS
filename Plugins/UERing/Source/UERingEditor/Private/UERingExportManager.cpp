#include "UERingExportManager.h"

#include "Algo/Unique.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PlatformCryptoContextIncludes.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingBlueprintExporter.h"
#include "UERingAnimBlueprintExporter.h"
#include "UERingAnimationAssetExporter.h"
#include "UERingAudioAssetExporter.h"
#include "UERingControlRigExporter.h"
#include "UERingDataAssetExporter.h"
#include "UERingDefinitionExporter.h"
#include "UERingDomainSemanticBuilder.h"
#include "UERingDomainGraphExporter.h"
#include "UERingDerivedArtifactWriter.h"
#include "UERingPaperTileSetExporter.h"
#include "UERingPaperTileMapExporter.h"
#include "UERingLevelSequenceExporter.h"
#include "UERingMaterialExporter.h"
#include "UERingMetaSoundExporter.h"
#include "UERingNiagaraExporter.h"
#include "UERingReflectionExporter.h"
#include "UERingSettings.h"
#include "UERingSummaryWriter.h"
#include "UERingTableExporter.h"
#include "UERingVersion.h"
#include "UERingWidgetBlueprintExporter.h"
#include "UERingWorldExporter.h"
#include "UObject/Package.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogUERingExport, Log, All);

namespace UERingExportManager
{
    const TArray<FString>& ProjectContentRoots()
    {
        static const TArray<FString> Roots = []
        {
            TArray<FString> Result = { TEXT("/Game/") };
            for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
            {
                if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project
                    && Plugin->CanContainContent()
                    && Plugin->IsMounted())
                {
                    FString Root = Plugin->GetMountedAssetPath();
                    if (!Root.EndsWith(TEXT("/"))) Root += TEXT("/");
                    Result.AddUnique(MoveTemp(Root));
                }
            }
            Result.Sort();
            return Result;
        }();
        return Roots;
    }

    bool IsProjectContentPath(const FString& PackageName, const bool bAllowMountRoot)
    {
        if (!FPackageName::IsValidLongPackageName(PackageName)) return false;
        for (const FString& Root : ProjectContentRoots())
        {
            if (PackageName.StartsWith(Root)
                || (bAllowMountRoot && PackageName == Root.LeftChop(1)))
            {
                return true;
            }
        }
        return false;
    }

    FString NormalizeRelativePath(FString Path)
    {
        FPaths::NormalizeFilename(Path);
        return Path;
    }

    TArray<TSharedPtr<FJsonValue>> StringArray(TArray<FString> Values)
    {
        Values.Sort();
        Values.SetNum(Algo::Unique(Values));

        TArray<TSharedPtr<FJsonValue>> JsonValues;
        JsonValues.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            JsonValues.Add(MakeShared<FJsonValueString>(Value));
        }
        return JsonValues;
    }

    UObject* ResolveAssetObject(const FAssetData& AssetData, FString& OutFailureDetail)
    {
        if (UObject* RedirectedAsset = AssetData.GetSoftObjectPath().TryLoad())
        {
            return RedirectedAsset;
        }
        if (UObject* Asset = AssetData.GetAsset())
        {
            return Asset;
        }

        UObject* Candidate = StaticLoadObject(
            UObject::StaticClass(),
            nullptr,
            *AssetData.PackageName.ToString());
        if (UPackage* Package = Cast<UPackage>(Candidate))
        {
            Candidate = StaticLoadObject(
                UObject::StaticClass(),
                Package,
                *FPackageName::GetShortName(Package));
        }
        while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Candidate))
        {
            Candidate = Redirector->DestinationObject;
        }
        if (Candidate != nullptr && Candidate->IsAsset())
        {
            return Candidate;
        }

        OutFailureDetail = TEXT("the soft path, registry entry, and package-relative object load all failed");
        return nullptr;
    }

    TArray<FString> NamesToStrings(const TArray<FName>& Names)
    {
        TArray<FString> Result;
        Result.Reserve(Names.Num());
        for (const FName Name : Names)
        {
            Result.Add(Name.ToString());
        }
        return Result;
    }

    FString CalculateSourceHash(const FString& SourceFile)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *SourceFile))
        {
            return FString();
        }

        FEncryptionContext EncryptionContext;
        TArray<uint8> Hash;
        if (!EncryptionContext.CalcSHA256(Bytes, Hash) || Hash.Num() != 32)
        {
            return FString();
        }
        return TEXT("sha256:") + BytesToHex(Hash.GetData(), Hash.Num()).ToLower();
    }

    FString CalculateStringHash(const FString& Value)
    {
        const FTCHARToUTF8 Utf8(*Value);
        TArray<uint8> Bytes;
        Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
        FEncryptionContext EncryptionContext;
        TArray<uint8> Hash;
        if (!EncryptionContext.CalcSHA256(Bytes, Hash) || Hash.Num() != 32)
        {
            return FString();
        }
        return TEXT("sha256:") + BytesToHex(Hash.GetData(), Hash.Num()).ToLower();
    }

    FString CalculateInputFingerprint(
        const UObject* Asset,
        const FString& SourceHash,
        const FName ExporterName,
        const FString& ProfileName)
    {
        FString Evidence = SourceHash + TEXT("\n") + ExporterName.ToString()
            + TEXT("\n") + ProfileName + TEXT("\n") + FString::FromInt(UE_RING_SEMANTIC_REVISION);
        for (const UClass* Class = Asset != nullptr ? Asset->GetClass() : nullptr;
            Class != nullptr;
            Class = Class->GetSuperClass())
        {
            Evidence += TEXT("\nclass:") + Class->GetPathName();
            for (TFieldIterator<FProperty> It(Class, EFieldIterationFlags::None); It; ++It)
            {
                Evidence += TEXT("\nproperty:") + It->GetName() + TEXT(":")
                    + It->GetCPPType() + TEXT(":")
                    + LexToString(static_cast<uint64>(It->GetPropertyFlags()));
            }
        }
        if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
        {
            for (const UClass* Parent = Blueprint->ParentClass;
                Parent != nullptr;
                Parent = Parent->GetSuperClass())
            {
                const FString ParentPackage = Parent->GetOutermost()->GetName();
                if (!IsProjectContentPath(ParentPackage, false))
                {
                    continue;
                }
                const FString ParentFile = FPackageName::LongPackageNameToFilename(
                    ParentPackage,
                    FPackageName::GetAssetPackageExtension());
                Evidence += TEXT("\nparent:") + ParentPackage + TEXT(":")
                    + CalculateSourceHash(ParentFile);
            }
        }
        return CalculateStringHash(Evidence);
    }

    bool HasSameExportState(
        const FString& OutputFile,
        const FString& SourceHash,
        const FName ExporterName,
        const FString& ProfileName,
        const FString& InputFingerprint,
        const FString& PackageName,
        const FString& ObjectPath)
    {
        if (SourceHash.IsEmpty())
        {
            return false;
        }

        FString ExistingJson;
        if (!FFileHelper::LoadFileToString(ExistingJson, *OutputFile))
        {
            return false;
        }

        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingJson);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            return false;
        }

        FString ExistingSchemaVersion;
        double ExistingSemanticRevision = 0.0;
        FString ExistingHash;
        FString ExistingExporterName;
        FString ExistingProfileName;
        FString ExistingInputFingerprint;
        FString ExistingPackageName;
        FString ExistingObjectPath;
        const TSharedPtr<FJsonObject>* AssetObject = nullptr;
        return Root->TryGetStringField(TEXT("schemaVersion"), ExistingSchemaVersion)
            && ExistingSchemaVersion == UE_RING_SCHEMA_VERSION
            && Root->TryGetNumberField(TEXT("semanticRevision"), ExistingSemanticRevision)
            && ExistingSemanticRevision == UE_RING_SEMANTIC_REVISION
            && Root->TryGetObjectField(TEXT("asset"), AssetObject)
            && AssetObject != nullptr
            && (*AssetObject)->TryGetStringField(TEXT("sourceHash"), ExistingHash)
            && ExistingHash == SourceHash
            && (*AssetObject)->TryGetStringField(TEXT("packageName"), ExistingPackageName)
            && ExistingPackageName == PackageName
            && (*AssetObject)->TryGetStringField(TEXT("objectPath"), ExistingObjectPath)
            && ExistingObjectPath == ObjectPath
            && Root->TryGetStringField(TEXT("exporter"), ExistingExporterName)
            && ExistingExporterName == ExporterName.ToString()
            && Root->TryGetStringField(TEXT("profile"), ExistingProfileName)
            && ExistingProfileName == ProfileName
            && Root->TryGetStringField(TEXT("inputFingerprint"), ExistingInputFingerprint)
            && ExistingInputFingerprint == InputFingerprint;
    }

    bool WriteAtomically(const FString& OutputFile, const FString& Contents, FString& OutError)
    {
        const FString Directory = FPaths::GetPath(OutputFile);
        if (!IFileManager::Get().MakeDirectory(*Directory, true))
        {
            OutError = FString::Printf(TEXT("Could not create output directory: %s"), *Directory);
            return false;
        }

        const FString TempFile = OutputFile + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(
                Contents,
                *TempFile,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = FString::Printf(TEXT("Could not write temporary file: %s"), *TempFile);
            return false;
        }

        if (!IFileManager::Get().Move(*OutputFile, *TempFile, true, true, false, true))
        {
            IFileManager::Get().Delete(*TempFile, false, true);
            OutError = FString::Printf(TEXT("Could not replace output file: %s"), *OutputFile);
            return false;
        }
        return true;
    }

    template<typename PrintPolicy>
    bool SerializeJson(const TSharedRef<FJsonObject>& Root, FString& OutJson)
    {
        const TSharedRef<TJsonWriter<TCHAR, PrintPolicy>> Writer =
            TJsonWriterFactory<TCHAR, PrintPolicy>::Create(&OutJson);
        return FJsonSerializer::Serialize(Root, Writer);
    }

    void AppendErrorLog(const FAssetData& Asset, const FUERingExportResult& Result, const FString& OutputRoot)
    {
        if (Result.Status != EUERingExportStatus::Failed)
        {
            return;
        }
        const FString LogFile = FPaths::Combine(OutputRoot, TEXT("logs/export-errors.jsonl"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(LogFile), true);
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("atUtc"), FDateTime::UtcNow().ToIso8601());
        Entry->SetStringField(TEXT("packageName"), Asset.PackageName.ToString());
        Entry->SetStringField(TEXT("objectPath"), Asset.GetSoftObjectPath().ToString());
        Entry->SetStringField(TEXT("exporter"), Result.ExporterName);
        Entry->SetStringField(TEXT("error"), Result.Error);
        FString Line;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line);
        if (FJsonSerializer::Serialize(Entry, Writer))
        {
            Line += LINE_TERMINATOR;
            FFileHelper::SaveStringToFile(
                Line,
                *LogFile,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                &IFileManager::Get(),
                FILEWRITE_Append);
        }
    }
}

FUERingExportManager& FUERingExportManager::Get()
{
    static FUERingExportManager Instance;
    return Instance;
}

void FUERingExportManager::Initialize()
{
    if (!bInitialized)
    {
        bInitialized = true;
        RegisterExporter(MakeUnique<FUERingControlRigExporter>());
        RegisterExporter(MakeUnique<FUERingWidgetBlueprintExporter>());
        RegisterExporter(MakeUnique<FUERingAnimBlueprintExporter>());
        RegisterExporter(MakeUnique<FUERingBlueprintExporter>());
        RegisterExporter(MakeUnique<FUERingAnimationAssetExporter>());
        RegisterExporter(MakeUnique<FUERingAudioAssetExporter>());
        RegisterExporter(MakeUnique<FUERingMetaSoundExporter>());
        RegisterExporter(MakeUnique<FUERingMaterialExporter>());
        RegisterExporter(MakeUnique<FUERingNiagaraExporter>());
        RegisterExporter(MakeUnique<FUERingWorldExporter>());
        RegisterExporter(MakeUnique<FUERingDataAssetExporter>());
        RegisterExporter(MakeUnique<FUERingTableExporter>());
        RegisterExporter(MakeUnique<FUERingDomainGraphExporter>());
        RegisterExporter(MakeUnique<FUERingLevelSequenceExporter>());
        RegisterExporter(MakeUnique<FUERingPaperTileMapExporter>());
        RegisterExporter(MakeUnique<FUERingPaperTileSetExporter>());
        RegisterExporter(MakeUnique<FUERingDefinitionExporter>());
        RegisterExporter(MakeUnique<FUERingReflectionExporter>());
    }
}

void FUERingExportManager::Shutdown()
{
    Exporters.Reset();
    ExporterHandles.Reset();
    NextExporterHandle = 1;
    bInitialized = false;
}

uint64 FUERingExportManager::RegisterExporter(TUniquePtr<IUERingAssetExporter> Exporter)
{
    if (Exporter == nullptr)
    {
        return 0;
    }

    const uint64 Handle = NextExporterHandle++;
    ExporterHandles.Add(Handle, Exporter.Get());
    Exporters.Add(MoveTemp(Exporter));
    Exporters.Sort([](const TUniquePtr<IUERingAssetExporter>& Left, const TUniquePtr<IUERingAssetExporter>& Right)
    {
        if (Left->GetPriority() != Right->GetPriority())
        {
            return Left->GetPriority() > Right->GetPriority();
        }
        return Left->GetName().LexicalLess(Right->GetName());
    });
    return Handle;
}

bool FUERingExportManager::UnregisterExporter(const uint64 Handle)
{
    IUERingAssetExporter** Exporter = ExporterHandles.Find(Handle);
    if (Exporter == nullptr)
    {
        return false;
    }
    IUERingAssetExporter* ExporterPointer = *Exporter;
    ExporterHandles.Remove(Handle);
    return Exporters.RemoveAll([ExporterPointer](const TUniquePtr<IUERingAssetExporter>& Candidate)
    {
        return Candidate.Get() == ExporterPointer;
    }) == 1;
}

bool FUERingExportManager::CanExport(const FAssetData& AssetData) const
{
    return IsSupportedPackageName(AssetData.PackageName.ToString())
        && !IsIgnored(AssetData)
        && FindExporter(AssetData) != nullptr;
}

FUERingExportResult FUERingExportManager::ExportAsset(const FAssetData& AssetData) const
{
    using namespace UERingExportManager;

    FUERingExportResult Result;
    if (!IsSupportedPackageName(AssetData.PackageName.ToString()))
    {
        Result.Status = EUERingExportStatus::Unsupported;
        Result.Error = TEXT("Asset is outside project content mounts.");
        return Result;
    }
    if (IsIgnored(AssetData))
    {
        Result.Status = EUERingExportStatus::Unsupported;
        Result.Error = TEXT("Asset is excluded by UE Ring settings.");
        return Result;
    }

    const IUERingAssetExporter* Exporter = FindExporter(AssetData);
    if (Exporter == nullptr)
    {
        Result.Status = EUERingExportStatus::Unsupported;
        Result.Error = FString::Printf(
            TEXT("Unsupported asset class: %s"),
            *AssetData.AssetClassPath.ToString());
        return Result;
    }
    Result.ExporterName = Exporter->GetName().ToString();

    FString AssetLoadFailure;
    UObject* Asset = UERingExportManager::ResolveAssetObject(AssetData, AssetLoadFailure);
    if (Asset == nullptr && !Exporter->SupportsUnloadedAssets())
    {
        Result.Error = FString::Printf(
            TEXT("The asset could not be loaded: %s."),
            *AssetLoadFailure);
        return Result;
    }
    const bool bPackageDirtyAfterLoad = Asset != nullptr && Asset->GetOutermost()->IsDirty();
    if (bPackageDirtyAfterLoad && !IsRunningCommandlet())
    {
        Result.Error = TEXT("Save the asset before exporting it.");
        return Result;
    }

    const bool bIsMap = AssetData.IsInstanceOf(UWorld::StaticClass());
    const FString PackageName = AssetData.PackageName.ToString();
    const FString SourceFile = FPackageName::LongPackageNameToFilename(
        PackageName,
        bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
    if (!IFileManager::Get().FileExists(*SourceFile))
    {
        Result.Error = FString::Printf(TEXT("Source package does not exist: %s"), *SourceFile);
        return Result;
    }

    FUERingExportContext Context;
    Context.AssetData = AssetData;
    Context.Asset = Asset;
    Context.SourceFile = SourceFile;
    Context.RelativeSourceFile = SourceFile;
    FPaths::MakePathRelativeTo(Context.RelativeSourceFile, *FPaths::ProjectDir());
    Context.RelativeSourceFile = NormalizeRelativePath(Context.RelativeSourceFile);
    Context.OutputFile = GetSemanticFileForPackage(PackageName, bIsMap);
    Context.bPackageDirtyAfterLoad = bPackageDirtyAfterLoad;
    Context.RelativeSemanticFile = Context.OutputFile;
    FPaths::MakePathRelativeTo(Context.RelativeSemanticFile, *FPaths::ProjectDir());
    Context.RelativeSemanticFile = NormalizeRelativePath(Context.RelativeSemanticFile);
    Result.OutputFile = Context.OutputFile;

    const UUERingSettings* Settings = GetDefault<UUERingSettings>();
    Context.Profile = Settings->ExportProfile;
    const FString ProfileName = UERingExportProfileName(Context.Profile);
    if (Settings->bHashSourceAssets)
    {
        Context.SourceHash = CalculateSourceHash(SourceFile);
        if (Context.SourceHash.IsEmpty())
        {
            Result.Error = FString::Printf(TEXT("Could not hash source package: %s"), *SourceFile);
            return Result;
        }
        Context.InputFingerprint = CalculateInputFingerprint(
            Asset,
            Context.SourceHash,
            Exporter->GetName(),
            ProfileName);
        if (HasSameExportState(
                Context.OutputFile,
                Context.SourceHash,
                Exporter->GetName(),
                ProfileName,
                Context.InputFingerprint,
                PackageName,
                AssetData.GetSoftObjectPath().ToString())
            && (!Settings->bIncludeMarkdownSummary
                || FUERingSummaryWriter::HaveConfiguredSummaries(Context.OutputFile)))
        {
            if (Settings->bIncludeGraphVisualizations)
            {
                FString ExistingJson;
                TSharedPtr<FJsonObject> ExistingRoot;
                const TSharedPtr<FJsonObject>* ExistingSemantics = nullptr;
                if (!FFileHelper::LoadFileToString(ExistingJson, *Context.OutputFile)
                    || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ExistingJson), ExistingRoot)
                    || !ExistingRoot.IsValid()
                    || !ExistingRoot->TryGetObjectField(TEXT("semantics"), ExistingSemantics)
                    || ExistingSemantics == nullptr)
                {
                    Result.Error = FString::Printf(
                        TEXT("Could not read current semantics while ensuring graph artifacts: %s"),
                        *Context.OutputFile);
                    return Result;
                }
                FString MermaidFile;
                FString GraphvizFile;
                if (!FUERingDerivedArtifactWriter::WriteGraphArtifacts(
                    Context.OutputFile,
                    (*ExistingSemantics).ToSharedRef(),
                    MermaidFile,
                    GraphvizFile,
                    Result.Error))
                {
                    return Result;
                }
            }
            else
            {
                FUERingDerivedArtifactWriter::RemoveGraphArtifacts(Context.OutputFile);
            }
            if (!Settings->bIncludeMarkdownSummary)
            {
                FUERingSummaryWriter::Remove(Context.OutputFile);
            }
            if (!Settings->bGenerateChangeSummaries)
            {
                FUERingDerivedArtifactWriter::RemoveChangeSummary(Context.OutputFile);
            }
            Result.Status = EUERingExportStatus::Unchanged;
            return Result;
        }
    }

    FUERingSemanticPayload Payload;
    if (!Exporter->BuildPayload(Context, Payload, Result.Error))
    {
        if (Result.Error.IsEmpty())
        {
            Result.Error = FString::Printf(TEXT("Exporter %s failed without an error message."), *Result.ExporterName);
        }
        return Result;
    }
    if (Context.bPackageDirtyAfterLoad)
    {
        const TSharedRef<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
        Diagnostic->SetStringField(TEXT("code"), TEXT("enginePostLoadTransform"));
        Diagnostic->SetStringField(TEXT("severity"), TEXT("warning"));
        Diagnostic->SetStringField(
            TEXT("message"),
            TEXT("The commandlet exported the deterministic engine-loaded state after PostLoad modified the package; the source asset was not saved."));
        Payload.Diagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));
    }

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.asset"));
    Root->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    Root->SetNumberField(TEXT("semanticRevision"), UE_RING_SEMANTIC_REVISION);

    Root->SetStringField(TEXT("exporter"), Exporter->GetName().ToString());
    Root->SetStringField(TEXT("profile"), ProfileName);
    Root->SetStringField(TEXT("inputFingerprint"), Context.InputFingerprint);

    const TSharedRef<FJsonObject> AssetObject = MakeShared<FJsonObject>();
    AssetObject->SetStringField(TEXT("packageName"), PackageName);
    AssetObject->SetStringField(TEXT("objectPath"), AssetData.GetSoftObjectPath().ToString());
    AssetObject->SetStringField(TEXT("assetClass"), AssetData.AssetClassPath.GetAssetName().ToString());
    AssetObject->SetStringField(
        TEXT("nativeClass"),
        Asset != nullptr ? Asset->GetClass()->GetPathName() : AssetData.AssetClassPath.ToString());
    AssetObject->SetStringField(
        TEXT("packageGuid"),
        Asset != nullptr
            ? Asset->GetOutermost()->GetPersistentGuid().ToString(EGuidFormats::DigitsWithHyphensLower)
            : FString());
    AssetObject->SetStringField(TEXT("sourceFile"), Context.RelativeSourceFile);
    AssetObject->SetStringField(TEXT("sourceHash"), Context.SourceHash);
    Root->SetObjectField(TEXT("asset"), AssetObject);

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FName> HardDependencies;
    TArray<FName> SoftDependencies;
    TArray<FName> ManagementDependencies;
    TArray<FName> Referencers;
    AssetRegistry.GetDependencies(
        AssetData.PackageName,
        HardDependencies,
        UE::AssetRegistry::EDependencyCategory::Package,
        UE::AssetRegistry::EDependencyQuery::Hard);
    AssetRegistry.GetDependencies(
        AssetData.PackageName,
        SoftDependencies,
        UE::AssetRegistry::EDependencyCategory::Package,
        UE::AssetRegistry::EDependencyQuery::Soft);
    AssetRegistry.GetDependencies(
        AssetData.PackageName,
        ManagementDependencies,
        UE::AssetRegistry::EDependencyCategory::Manage);
    AssetRegistry.GetReferencers(AssetData.PackageName, Referencers);

    HardDependencies.Append(Payload.AdditionalHardDependencies);
    SoftDependencies.Append(Payload.AdditionalSoftDependencies);

    const TSharedRef<FJsonObject> Dependencies = MakeShared<FJsonObject>();
    if (!HardDependencies.IsEmpty())
    {
        Dependencies->SetArrayField(TEXT("hard"), StringArray(NamesToStrings(HardDependencies)));
    }
    if (!SoftDependencies.IsEmpty())
    {
        Dependencies->SetArrayField(TEXT("soft"), StringArray(NamesToStrings(SoftDependencies)));
    }
    if (!ManagementDependencies.IsEmpty())
    {
        Dependencies->SetArrayField(TEXT("management"), StringArray(NamesToStrings(ManagementDependencies)));
    }
    if (!Referencers.IsEmpty())
    {
        Dependencies->SetArrayField(TEXT("referencers"), StringArray(NamesToStrings(Referencers)));
    }
    if (!Dependencies->Values.IsEmpty())
    {
        Root->SetObjectField(TEXT("dependencies"), Dependencies);
    }
    const bool bHasDomainSemantics = UERingDomainSemanticBuilder::AddDomainSemantics(
        Context,
        Payload.Semantics);
    Root->SetObjectField(TEXT("semantics"), Payload.Semantics);
    if (!Payload.Omissions.IsEmpty())
    {
        Root->SetArrayField(TEXT("omissions"), Payload.Omissions);
    }
    Root->SetObjectField(
        TEXT("reconstruction"),
        UERingDomainSemanticBuilder::BuildReconstructionIR(
            Context,
            Result.ExporterName,
            Payload.Semantics,
            Payload.Omissions,
            bHasDomainSemantics));
    if (!Payload.CppLinks.IsEmpty())
    {
        Root->SetArrayField(TEXT("cppLinks"), Payload.CppLinks);
    }
    if (!Payload.Diagnostics.IsEmpty())
    {
        Root->SetArrayField(TEXT("diagnostics"), Payload.Diagnostics);
    }

    TSharedPtr<FJsonObject> PreviousRoot;
    if (Settings->bGenerateChangeSummaries && IFileManager::Get().FileExists(*Context.OutputFile))
    {
        FString PreviousJson;
        FFileHelper::LoadFileToString(PreviousJson, *Context.OutputFile);
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PreviousJson), PreviousRoot);
    }

    FString Json;
    const bool bSerialized = Settings->bPrettyJson
        ? SerializeJson<TPrettyJsonPrintPolicy<TCHAR>>(Root, Json)
        : SerializeJson<TCondensedJsonPrintPolicy<TCHAR>>(Root, Json);
    if (!bSerialized)
    {
        Result.Error = TEXT("Could not serialize USEM JSON.");
        return Result;
    }
    Json += LINE_TERMINATOR;

    if (!WriteAtomically(Context.OutputFile, Json, Result.Error))
    {
        return Result;
    }
    if (Settings->bIncludeGraphVisualizations)
    {
        FString MermaidFile;
        FString GraphvizFile;
        if (!FUERingDerivedArtifactWriter::WriteGraphArtifacts(
            Context.OutputFile,
            Payload.Semantics,
            MermaidFile,
            GraphvizFile,
            Result.Error))
        {
            return Result;
        }
    }
    else
    {
        FUERingDerivedArtifactWriter::RemoveGraphArtifacts(Context.OutputFile);
    }
    if (Settings->bGenerateChangeSummaries && PreviousRoot.IsValid())
    {
        FString DiffFile;
        if (!FUERingDerivedArtifactWriter::WriteChangeSummary(
            Context.OutputFile,
            PreviousRoot.ToSharedRef(),
            Root,
            DiffFile,
            Result.Error))
        {
            return Result;
        }
    }
    else if (!Settings->bGenerateChangeSummaries)
    {
        FUERingDerivedArtifactWriter::RemoveChangeSummary(Context.OutputFile);
    }
    if (Settings->bIncludeMarkdownSummary)
    {
        if (!FUERingSummaryWriter::Write(Context.OutputFile, Root, Result.Error))
        {
            return Result;
        }
    }
    else
    {
        FUERingSummaryWriter::Remove(Context.OutputFile);
    }

    Result.Status = EUERingExportStatus::Exported;
    return Result;
}

TArray<FUERingExportResult> FUERingExportManager::ExportAssets(const TArray<FAssetData>& Assets) const
{
    IFileManager::Get().Delete(
        *FPaths::Combine(GetOutputRoot(), TEXT("logs/export-errors.jsonl")),
        false,
        true);

    TArray<FUERingExportResult> Results;
    Results.Reserve(Assets.Num());
    for (const FAssetData& Asset : Assets)
    {
        FUERingExportResult Result = ExportAsset(Asset);
        UERingExportManager::AppendErrorLog(Asset, Result, GetOutputRoot());
        Results.Add(MoveTemp(Result));
    }
    return Results;
}

void FUERingExportManager::CanonicalizeAssetsByPackage(TArray<FAssetData>& Assets) const
{
    auto IsPreferred = [this](const FAssetData& Candidate, const FAssetData& Existing)
    {
        const bool bCandidateExportable = CanExport(Candidate);
        const bool bExistingExportable = CanExport(Existing);
        if (bCandidateExportable != bExistingExportable)
        {
            return bCandidateExportable;
        }

        const bool bCandidateRedirector = Candidate.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector");
        const bool bExistingRedirector = Existing.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector");
        if (bCandidateRedirector != bExistingRedirector)
        {
            return !bCandidateRedirector;
        }

        const FName CanonicalName(*FPackageName::GetLongPackageAssetName(Candidate.PackageName.ToString()));
        const bool bCandidateCanonicalName = Candidate.AssetName == CanonicalName;
        const bool bExistingCanonicalName = Existing.AssetName == CanonicalName;
        if (bCandidateCanonicalName != bExistingCanonicalName)
        {
            return bCandidateCanonicalName;
        }
        return Candidate.GetSoftObjectPath().ToString() < Existing.GetSoftObjectPath().ToString();
    };

    TMap<FName, FAssetData> AssetsByPackage;
    for (const FAssetData& Asset : Assets)
    {
        FAssetData* Existing = AssetsByPackage.Find(Asset.PackageName);
        if (Existing == nullptr || IsPreferred(Asset, *Existing))
        {
            AssetsByPackage.Add(Asset.PackageName, Asset);
        }
    }
    AssetsByPackage.GenerateValueArray(Assets);
    Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
    {
        if (Left.PackageName != Right.PackageName)
        {
            return Left.PackageName.LexicalLess(Right.PackageName);
        }
        return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
    });
}

bool FUERingExportManager::PrepareFullExport(FString& OutError) const
{
    const UUERingSettings* Settings = GetDefault<UUERingSettings>();
    TArray<FString> Subdirectories = { TEXT("diffs"), TEXT("bundles"), TEXT("tombstones"), TEXT("logs") };
    if (!Settings->bIncludeGraphVisualizations)
    {
        Subdirectories.Add(TEXT("graphs"));
    }
    if (!Settings->bIncludeCppIndex)
    {
        Subdirectories.Add(TEXT("cpp"));
        Subdirectories.Add(TEXT("reports"));
    }
    else if (!Settings->bIncludeBlueprintMigrationReport)
    {
        Subdirectories.Add(TEXT("reports"));
    }

    FString Root = FPaths::ConvertRelativePathToFull(GetOutputRoot());
    FPaths::NormalizeDirectoryName(Root);
    const FString RootWithSlash = Root + TEXT("/");
    for (const FString& Subdirectory : Subdirectories)
    {
        FString Target = FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, Subdirectory));
        FPaths::NormalizeDirectoryName(Target);
        if (!Target.StartsWith(RootWithSlash, ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(TEXT("Refusing to clean output outside the UE Ring root: %s"), *Target);
            return false;
        }
        if (IFileManager::Get().DirectoryExists(*Target)
            && !IFileManager::Get().DeleteDirectory(*Target, false, true))
        {
            OutError = FString::Printf(TEXT("Could not clean previous full-export output: %s"), *Target);
            return false;
        }
    }
    return true;
}

bool FUERingExportManager::FinalizeFullExport(
    const TArray<FAssetData>& Assets,
    FString& OutError) const
{
    auto NormalizedFullPath = [](const FString& Path)
    {
        FString FullPath = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(FullPath);
        return FullPath;
    };

    const UUERingSettings* Settings = GetDefault<UUERingSettings>();
    TSet<FString> ExpectedFiles;
    TSet<FString> ExpectedSummaries;
    TSet<FString> ExpectedGraphs;
    for (const FAssetData& Asset : Assets)
    {
        const FString SemanticFile = GetSemanticFileForPackage(
            Asset.PackageName.ToString(),
            Asset.IsInstanceOf(UWorld::StaticClass()));
        ExpectedFiles.Add(NormalizedFullPath(SemanticFile));
        if (Settings->bIncludeMarkdownSummary)
        {
            TArray<FString> SummaryFiles;
            FUERingSummaryWriter::GetConfiguredSummaryFiles(SemanticFile, SummaryFiles);
            for (const FString& SummaryFile : SummaryFiles)
            {
                ExpectedSummaries.Add(NormalizedFullPath(SummaryFile));
            }
        }
        if (Settings->bIncludeGraphVisualizations)
        {
            FString MermaidFile;
            FString GraphvizFile;
            FUERingDerivedArtifactWriter::GetGraphArtifactFiles(
                SemanticFile, MermaidFile, GraphvizFile);
            ExpectedGraphs.Add(NormalizedFullPath(MermaidFile));
            ExpectedGraphs.Add(NormalizedFullPath(GraphvizFile));
        }
    }

    for (const TCHAR* Subdirectory : { TEXT("content"), TEXT("maps") })
    {
        TArray<FString> SemanticFiles;
        IFileManager::Get().FindFilesRecursive(
            SemanticFiles,
            *FPaths::Combine(GetOutputRoot(), Subdirectory),
            TEXT("*.uesem.json"),
            true,
            false);
        for (const FString& SemanticFile : SemanticFiles)
        {
            if (ExpectedFiles.Contains(NormalizedFullPath(SemanticFile)))
            {
                continue;
            }
            if (!IFileManager::Get().Delete(*SemanticFile, false, true))
            {
                OutError = FString::Printf(TEXT("Could not remove orphan semantic output: %s"), *SemanticFile);
                return false;
            }
            FUERingSummaryWriter::Remove(SemanticFile);
            FUERingDerivedArtifactWriter::RemoveArtifacts(SemanticFile);
        }
    }

    auto RemoveUnexpectedFiles = [NormalizedFullPath, &OutError](
        const FString& Root,
        const FString& Pattern,
        const TSet<FString>& Expected,
        const TCHAR* Kind)
    {
        TArray<FString> Files;
        IFileManager::Get().FindFilesRecursive(Files, *Root, *Pattern, true, false);
        for (const FString& File : Files)
        {
            if (Expected.Contains(NormalizedFullPath(File)))
            {
                continue;
            }
            if (!IFileManager::Get().Delete(*File, false, true))
            {
                OutError = FString::Printf(TEXT("Could not remove orphan %s output: %s"), Kind, *File);
                return false;
            }
        }
        return true;
    };

    for (const TCHAR* Subdirectory : { TEXT("content"), TEXT("maps") })
    {
        const FString Root = FPaths::Combine(GetOutputRoot(), Subdirectory);
        if (!RemoveUnexpectedFiles(Root, TEXT("*.uesem.md"), ExpectedSummaries, TEXT("summary"))
            || !RemoveUnexpectedFiles(Root, TEXT("*.uesem.zh-CN.md"), ExpectedSummaries, TEXT("summary")))
        {
            return false;
        }
    }
    const FString GraphRoot = FPaths::Combine(GetOutputRoot(), TEXT("graphs"));
    if (!RemoveUnexpectedFiles(GraphRoot, TEXT("*.callgraph.mmd"), ExpectedGraphs, TEXT("graph"))
        || !RemoveUnexpectedFiles(GraphRoot, TEXT("*.callgraph.dot"), ExpectedGraphs, TEXT("graph")))
    {
        return false;
    }

    auto RemoveEmptyChildDirectories = [](const FString& Root)
    {
        TArray<FString> Directories;
        IFileManager::Get().IterateDirectoryRecursively(
            *Root,
            [&Directories](const TCHAR* FilenameOrDirectory, const bool bIsDirectory)
            {
                if (bIsDirectory)
                {
                    Directories.Add(FilenameOrDirectory);
                }
                return true;
            });
        Directories.Sort([](const FString& Left, const FString& Right)
        {
            return Left.Len() > Right.Len();
        });
        for (const FString& Directory : Directories)
        {
            IFileManager::Get().DeleteDirectory(*Directory, false, false);
        }
    };
    RemoveEmptyChildDirectories(FPaths::Combine(GetOutputRoot(), TEXT("content")));
    RemoveEmptyChildDirectories(FPaths::Combine(GetOutputRoot(), TEXT("maps")));
    RemoveEmptyChildDirectories(GraphRoot);
    return true;
}

FString FUERingExportManager::GetOutputRoot() const
{
    FString Root = GetDefault<UUERingSettings>()->OutputRoot.Path;
    if (Root.IsEmpty())
    {
        Root = TEXT(".uesem");
    }
    if (FPaths::IsRelative(Root))
    {
        Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Root);
    }
    FPaths::NormalizeDirectoryName(Root);
    return Root;
}

bool FUERingExportManager::IsSupportedPackageName(const FString& PackageName) const
{
    return UERingExportManager::IsProjectContentPath(PackageName, true);
}

FString FUERingExportManager::GetExclusionReason(const FAssetData& AssetData) const
{
    const UUERingSettings* Settings = GetDefault<UUERingSettings>();
    const FString PackageName = AssetData.PackageName.ToString();
    if (AssetData.AssetClassPath == UObjectRedirector::StaticClass()->GetClassPathName()
        || AssetData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
    {
        return TEXT("objectRedirector");
    }
    if (PackageName.Contains(TEXT("/__ExternalActors__/"), ESearchCase::IgnoreCase)
        || PackageName.Contains(TEXT("/__ExternalObjects__/"), ESearchCase::IgnoreCase))
    {
        return TEXT("generatedExternalActor");
    }
    for (const FString& Pattern : Settings->IgnoredPaths)
    {
        if (!Pattern.IsEmpty() && PackageName.MatchesWildcard(Pattern, ESearchCase::IgnoreCase))
        {
            return TEXT("ignoredPath");
        }
    }

    const FString AssetClass = AssetData.AssetClassPath.ToString();
    for (const FSoftClassPath& IgnoredClass : Settings->IgnoredClasses)
    {
        if (AssetClass.Equals(IgnoredClass.ToString(), ESearchCase::IgnoreCase))
        {
            return TEXT("ignoredClass");
        }
    }
    return FString();
}

FString FUERingExportManager::GetSemanticFileForPackage(const FString& PackageName, const bool bIsMap) const
{
    if (!UERingExportManager::IsProjectContentPath(PackageName, false))
    {
        return FString();
    }
    FString PackageRelativePath = PackageName;
    PackageRelativePath.RemoveFromStart(TEXT("/"));
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        GetOutputRoot(),
        bIsMap ? TEXT("maps") : TEXT("content"),
        PackageRelativePath + TEXT(".uesem.json")));
}

const IUERingAssetExporter* FUERingExportManager::FindExporter(const FAssetData& AssetData) const
{
    for (const TUniquePtr<IUERingAssetExporter>& Exporter : Exporters)
    {
        if (Exporter->CanExport(AssetData))
        {
            return Exporter.Get();
        }
    }
    return nullptr;
}

bool FUERingExportManager::IsIgnored(const FAssetData& AssetData) const
{
    return !GetExclusionReason(AssetData).IsEmpty();
}
