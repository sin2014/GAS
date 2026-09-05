#include "UERingBundleBuilder.h"

#include "Algo/Unique.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingExportManager.h"
#include "UERingSettings.h"
#include "UERingVersion.h"

namespace UERingBundleBuilder
{
    bool NormalizeAbsolutePath(const FString& Path, FString& OutPath)
    {
        if (Path.IsEmpty())
        {
            return false;
        }
        OutPath = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(OutPath);
        return FPaths::CollapseRelativeDirectories(OutPath) && !FPaths::IsRelative(OutPath);
    }

    bool IsContainedPath(const FString& Path, const FString& Root)
    {
        FString NormalizedPath;
        FString NormalizedRoot;
        return NormalizeAbsolutePath(Path, NormalizedPath)
            && NormalizeAbsolutePath(Root, NormalizedRoot)
            && (FPaths::IsSamePath(NormalizedPath, NormalizedRoot)
                || FPaths::IsUnderDirectory(NormalizedPath, NormalizedRoot));
    }

    bool ResolveBundleTarget(
        const FString& Bundle,
        const FString& RelativeTarget,
        FString& OutTarget,
        FString& OutError)
    {
        if (RelativeTarget.IsEmpty()
            || !FPaths::IsRelative(RelativeTarget)
            || RelativeTarget.Contains(TEXT(":")))
        {
            OutError = FString::Printf(TEXT("Invalid relative bundle target: %s"), *RelativeTarget);
            return false;
        }
        OutTarget = FPaths::Combine(Bundle, RelativeTarget);
        if (!NormalizeAbsolutePath(OutTarget, OutTarget) || !IsContainedPath(OutTarget, Bundle))
        {
            OutError = FString::Printf(TEXT("Refusing to write outside the bundle root: %s"), *RelativeTarget);
            return false;
        }
        return true;
    }

    bool IsCppSourceExtension(const FString& Path)
    {
        const FString Extension = FPaths::GetExtension(Path, true).ToLower();
        return Extension == TEXT(".h")
            || Extension == TEXT(".hpp")
            || Extension == TEXT(".inl")
            || Extension == TEXT(".c")
            || Extension == TEXT(".cc")
            || Extension == TEXT(".cpp")
            || Extension == TEXT(".cxx")
            || Extension == TEXT(".ixx");
    }

    bool ResolveCppLinkSource(const FString& Source, FString& OutSource, FString& OutError)
    {
        TArray<FString> Candidates;
        if (FPaths::IsRelative(Source))
        {
            Candidates.Add(FPaths::Combine(FPlatformProcess::BaseDir(), Source));
            Candidates.Add(FPaths::Combine(FPaths::ProjectDir(), Source));
            Candidates.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), Source));
            Candidates.Add(FPaths::Combine(FPaths::EngineSourceDir(), Source));
            Candidates.Add(FPaths::Combine(FPaths::EngineDir(), Source));
        }
        else
        {
            Candidates.Add(Source);
        }

        const TArray<FString> AllowedRoots = {
            FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")),
            FPaths::ProjectPluginsDir(),
            FPaths::EngineSourceDir(),
            FPaths::EnginePluginsDir()
        };
        for (const FString& Candidate : Candidates)
        {
            FString Normalized;
            if (!NormalizeAbsolutePath(Candidate, Normalized)
                || !IFileManager::Get().FileExists(*Normalized))
            {
                continue;
            }
            bool bAllowed = IsCppSourceExtension(Normalized);
            if (bAllowed)
            {
                bAllowed = AllowedRoots.ContainsByPredicate([&Normalized](const FString& Root)
                {
                    return IsContainedPath(Normalized, Root);
                });
            }
            if (!bAllowed)
            {
                OutError = FString::Printf(
                    TEXT("Refusing cppLinks source outside approved C/C++ roots: %s"),
                    *Source);
                return false;
            }
            OutSource = MoveTemp(Normalized);
            return true;
        }
        return false;
    }

    bool CopyIntoBundle(const FString& Source, const FString& RelativeTarget, const FString& Bundle, FString& OutError)
    {
        if (!IFileManager::Get().FileExists(*Source))
        {
            return true;
        }
        FString Target;
        if (!ResolveBundleTarget(Bundle, RelativeTarget, Target, OutError))
        {
            return false;
        }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
        if (IFileManager::Get().Copy(*Target, *Source, true, true) != COPY_OK)
        {
            OutError = FString::Printf(TEXT("Could not copy %s into bundle."), *Source);
            return false;
        }
        return true;
    }

    FString SafePackageName(FString PackageName)
    {
        PackageName.RemoveFromStart(TEXT("/"));
        PackageName.ReplaceInline(TEXT("/"), TEXT("__"));
        FString Safe;
        Safe.Reserve(PackageName.Len());
        for (const TCHAR Character : PackageName)
        {
            Safe.AppendChar(
                FChar::IsAlnum(Character)
                    || Character == TEXT('_')
                    || Character == TEXT('-')
                    || Character == TEXT('.')
                    ? Character
                    : TEXT('_'));
        }
        while (Safe.Contains(TEXT("..")))
        {
            Safe.ReplaceInline(TEXT(".."), TEXT("__"));
        }
        return Safe.IsEmpty() ? TEXT("unnamed") : Safe;
    }

    bool IsPrivateKey(const FString& Key)
    {
        for (const FString& Pattern : GetDefault<UUERingSettings>()->PrivacyFilters)
        {
            if (!Pattern.IsEmpty() && Key.MatchesWildcard(Pattern, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    void ReplaceLocalPath(FString& Value, FString Path, const TCHAR* Placeholder)
    {
        if (Path.IsEmpty())
        {
            return;
        }
        FPaths::NormalizeDirectoryName(Path);
        Value.ReplaceInline(*Path, Placeholder, ESearchCase::IgnoreCase);
        Path.ReplaceInline(TEXT("/"), TEXT("\\"));
        Value.ReplaceInline(*Path, Placeholder, ESearchCase::IgnoreCase);
    }

    bool WriteSanitizedConfig(
        const FString& Source,
        const FString& RelativeTarget,
        const FString& Bundle,
        FString& OutError)
    {
        FConfigFile Config;
        Config.Read(Source);

        TArray<FString> SectionNames;
        for (const TPair<FString, FConfigSection>& Section : Config)
        {
            SectionNames.Add(Section.Key);
        }
        SectionNames.Sort();

        FString Sanitized;
        for (const FString& SectionName : SectionNames)
        {
            const FConfigSection* Section = Config.FindSection(*SectionName);
            if (Section == nullptr)
            {
                continue;
            }
            Sanitized += FString::Printf(TEXT("[%s]\n"), *SectionName);
            TArray<TPair<FString, FString>> Values;
            for (const TPair<FName, FConfigValue>& Pair : *Section)
            {
                FString Value = IsPrivateKey(Pair.Key.ToString())
                    ? TEXT("[REDACTED]")
                    : Pair.Value.GetSavedValue();
                ReplaceLocalPath(Value, FPaths::ProjectDir(), TEXT("${PROJECT_DIR}"));
                ReplaceLocalPath(Value, FPlatformProcess::UserDir(), TEXT("${USER_DIR}"));
                ReplaceLocalPath(Value, FPlatformProcess::BaseDir(), TEXT("${ENGINE_BIN_DIR}"));
                Values.Emplace(Pair.Key.ToString(), MoveTemp(Value));
            }
            Values.Sort([](const TPair<FString, FString>& Left, const TPair<FString, FString>& Right)
            {
                return Left.Key == Right.Key ? Left.Value < Right.Value : Left.Key < Right.Key;
            });
            for (const TPair<FString, FString>& Pair : Values)
            {
                Sanitized += FString::Printf(TEXT("%s=%s\n"), *Pair.Key, *Pair.Value);
            }
            Sanitized += TEXT("\n");
        }

        FString Target;
        if (!ResolveBundleTarget(Bundle, RelativeTarget, Target, OutError))
        {
            return false;
        }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
        if (!FFileHelper::SaveStringToFile(
            Sanitized,
            *Target,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = FString::Printf(TEXT("Could not write sanitized config %s into bundle."), *Source);
            return false;
        }
        return true;
    }

    bool LoadPrimarySemantic(
        const FString& PackageName,
        FString& OutSemanticFile,
        TSharedPtr<FJsonObject>& OutRoot,
        FString& OutError)
    {
        OutSemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, false);
        if (!IFileManager::Get().FileExists(*OutSemanticFile))
        {
            OutSemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, true);
        }
        FString Json;
        if (!FFileHelper::LoadFileToString(Json, *OutSemanticFile)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot)
            || !OutRoot.IsValid())
        {
            OutError = FString::Printf(TEXT("A valid semantic file is required before bundling: %s"), *PackageName);
            return false;
        }
        return true;
    }

    void AddExistingFile(const FString& Source, const FString& RelativeTarget, TArray<FString>& OutFiles)
    {
        if (IFileManager::Get().FileExists(*Source))
        {
            OutFiles.Add(RelativeTarget);
        }
    }

    FString ScopeName(const EUERingBundleScope Scope)
    {
        switch (Scope)
        {
        case EUERingBundleScope::Asset:
            return TEXT("asset");
        case EUERingBundleScope::Folder:
            return TEXT("folder");
        case EUERingBundleScope::Module:
            return TEXT("module");
        case EUERingBundleScope::PrimaryAssetType:
            return TEXT("primaryAssetType");
        default:
            return TEXT("unknown");
        }
    }

    bool ResolvePackages(
        const FUERingBundleRequest& Request,
        TArray<FString>& OutPackages,
        FString& OutError)
    {
        OutPackages.Reset();
        if (Request.Value.IsEmpty())
        {
            OutError = TEXT("A bundle scope value is required.");
            return false;
        }
        if (Request.Scope == EUERingBundleScope::Asset)
        {
            if (!FUERingExportManager::Get().IsSupportedPackageName(Request.Value))
            {
                OutError = TEXT("Asset bundle value must be a valid project content long package name.");
                return false;
            }
            OutPackages.Add(Request.Value);
            return true;
        }
        if (Request.Scope == EUERingBundleScope::Folder)
        {
            if (!FUERingExportManager::Get().IsSupportedPackageName(Request.Value))
            {
                OutError = TEXT("Folder bundle value must be a valid project content package path.");
                return false;
            }
            IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            TArray<FAssetData> Assets;
            Registry.GetAssetsByPath(FName(*Request.Value), Assets, true, true);
            for (const FAssetData& Asset : Assets)
            {
                if (FUERingExportManager::Get().CanExport(Asset))
                {
                    OutPackages.Add(Asset.PackageName.ToString());
                }
            }
        }
        else
        {
            const FString IndexFile = FPaths::Combine(
                FUERingExportManager::Get().GetOutputRoot(),
                TEXT("index/project.uesem.index.json"));
            FString Json;
            TSharedPtr<FJsonObject> Index;
            if (!FFileHelper::LoadFileToString(Json, *IndexFile)
                || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Index)
                || !Index.IsValid())
            {
                OutError = TEXT("A valid project semantic index is required for module and Primary Asset Type bundles.");
                return false;
            }
            const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
            if (Index->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& Value : *Assets)
                {
                    const TSharedPtr<FJsonObject> Asset = Value.IsValid() ? Value->AsObject() : nullptr;
                    FString Status;
                    FString PackageName;
                    if (!Asset.IsValid()
                        || !Asset->TryGetStringField(TEXT("status"), Status)
                        || Status != TEXT("ok")
                        || !Asset->TryGetStringField(TEXT("packageName"), PackageName))
                    {
                        continue;
                    }
                    FString Candidate;
                    const TCHAR* Field = Request.Scope == EUERingBundleScope::Module
                        ? TEXT("ownerModule")
                        : TEXT("primaryAssetId");
                    Asset->TryGetStringField(Field, Candidate);
                    const bool bMatches = Request.Scope == EUERingBundleScope::Module
                        ? Candidate.Equals(Request.Value, ESearchCase::IgnoreCase)
                        : Candidate.StartsWith(Request.Value + TEXT(":"), ESearchCase::IgnoreCase);
                    if (bMatches)
                    {
                        OutPackages.Add(PackageName);
                    }
                }
            }
        }
        OutPackages.Sort();
        OutPackages.SetNum(Algo::Unique(OutPackages));
        if (OutPackages.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("No exported assets matched bundle %s '%s'."),
                *ScopeName(Request.Scope),
                *Request.Value);
            return false;
        }
        return true;
    }

    bool WriteBundleSupplementalFiles(
        const FString& Bundle,
        const FUERingBundleRequest& Request,
        const TArray<FString>& RootPackages,
        FString& OutError)
    {
        TSet<FString> RootSet;
        for (const FString& PackageName : RootPackages)
        {
            RootSet.Add(PackageName);
        }
        TArray<FString> SemanticFiles;
        IFileManager::Get().FindFilesRecursive(
            SemanticFiles,
            *FPaths::Combine(Bundle, TEXT("assets")),
            TEXT("*.uesem.json"),
            true,
            false);

        struct FContextFile
        {
            FString PackageName;
            FString RelativeFile;
            int64 Bytes = 0;
            bool bRoot = false;
        };
        TArray<FContextFile> ContextFiles;
        for (const FString& File : SemanticFiles)
        {
            FString Json;
            TSharedPtr<FJsonObject> Root;
            if (!FFileHelper::LoadFileToString(Json, *File)
                || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
                || !Root.IsValid())
            {
                continue;
            }
            const TSharedPtr<FJsonObject>* Asset = nullptr;
            FString PackageName;
            if (Root->TryGetObjectField(TEXT("asset"), Asset) && Asset != nullptr)
            {
                (*Asset)->TryGetStringField(TEXT("packageName"), PackageName);
            }
            FString RelativeFile = File;
            const FString BundleWithSlash = Bundle + TEXT("/");
            FPaths::MakePathRelativeTo(RelativeFile, *BundleWithSlash);
            FPaths::NormalizeFilename(RelativeFile);
            ContextFiles.Add({ PackageName, RelativeFile, IFileManager::Get().FileSize(*File), RootSet.Contains(PackageName) });
        }
        ContextFiles.Sort([](const FContextFile& Left, const FContextFile& Right)
        {
            if (Left.bRoot != Right.bRoot)
            {
                return Left.bRoot;
            }
            return Left.PackageName == Right.PackageName
                ? Left.RelativeFile < Right.RelativeFile
                : Left.PackageName < Right.PackageName;
        });

        const int64 BudgetBytes = static_cast<int64>(GetDefault<UUERingSettings>()->MaxBundleContextMiB) * 1024 * 1024;
        int64 SelectedBytes = 0;
        int64 RootBytes = 0;
        int32 SelectedRootFiles = 0;
        int32 DeferredRootFiles = 0;
        const TSharedRef<FJsonObject> Plan = MakeShared<FJsonObject>();
        Plan->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.context-plan"));
        Plan->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
        Plan->SetStringField(TEXT("scope"), ScopeName(Request.Scope));
        Plan->SetStringField(TEXT("value"), Request.Value);
        Plan->SetNumberField(TEXT("budgetBytes"), BudgetBytes);
        TArray<TSharedPtr<FJsonValue>> JsonFiles;
        for (const FContextFile& File : ContextFiles)
        {
            if (File.bRoot)
            {
                RootBytes += File.Bytes;
            }
            const bool bRequiredFirstRoot = File.bRoot && SelectedRootFiles == 0;
            const bool bSelected = bRequiredFirstRoot || SelectedBytes + File.Bytes <= BudgetBytes;
            if (bSelected)
            {
                SelectedBytes += File.Bytes;
            }
            if (File.bRoot)
            {
                bSelected ? ++SelectedRootFiles : ++DeferredRootFiles;
            }
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("packageName"), File.PackageName);
            Entry->SetStringField(TEXT("file"), File.RelativeFile);
            Entry->SetNumberField(TEXT("bytes"), File.Bytes);
            Entry->SetStringField(TEXT("priority"), File.bRoot ? TEXT("root") : TEXT("dependency"));
            Entry->SetBoolField(TEXT("includeInInitialContext"), bSelected);
            JsonFiles.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Plan->SetNumberField(TEXT("rootBytes"), RootBytes);
        Plan->SetNumberField(TEXT("selectedBytes"), SelectedBytes);
        Plan->SetNumberField(TEXT("selectedRootFiles"), SelectedRootFiles);
        Plan->SetNumberField(TEXT("deferredRootFiles"), DeferredRootFiles);
        Plan->SetBoolField(TEXT("rootAssetsExceedBudget"), RootBytes > BudgetBytes);
        Plan->SetBoolField(TEXT("requiredRootExceedsBudget"), SelectedBytes > BudgetBytes);
        Plan->SetArrayField(TEXT("files"), JsonFiles);
        FString PlanJson;
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> PlanWriter =
            TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&PlanJson);
        FJsonSerializer::Serialize(Plan, PlanWriter);
        PlanJson += LINE_TERMINATOR;
        if (!FFileHelper::SaveStringToFile(
            PlanJson,
            *FPaths::Combine(Bundle, TEXT("context-plan.json")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = TEXT("Could not write bundle context plan.");
            return false;
        }

        IFileManager::Get().MakeDirectory(*FPaths::Combine(Bundle, TEXT("prompts")), true);
        const FString EnglishPrompt =
            TEXT("# UE project semantic analysis\n\n")
            TEXT("Read manifest.json and context-plan.json first. Load every file marked includeInInitialContext, then request deferred root or dependency semantics only when needed. Treat USEM JSON as authoritative, distinguish dedicated semantics from ReflectionFallback, cite package names, and never infer unavailable pixel, mesh, audio, or cooked binary payloads.\n");
        const FString ChinesePrompt =
            TEXT("# UE 项目语义分析\n\n")
            TEXT("先读取 manifest.json 与 context-plan.json，再加载所有 includeInInitialContext=true 的文件；只有分析需要时再补充延迟根资产或依赖语义。以 USEM JSON 为权威，区分专用语义与 ReflectionFallback，引用具体包名，不要臆测未导出的像素、网格、音频或 Cooked 二进制内容。\n");
        if (!FFileHelper::SaveStringToFile(
                EnglishPrompt,
                *FPaths::Combine(Bundle, TEXT("prompts/analysis.en.md")),
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
            || !FFileHelper::SaveStringToFile(
                ChinesePrompt,
                *FPaths::Combine(Bundle, TEXT("prompts/analysis.zh-CN.md")),
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = TEXT("Could not write bundle prompt templates.");
            return false;
        }
        return true;
    }
}

bool FUERingBundleBuilder::Preview(const FString& PackageName, TArray<FString>& OutFiles, FString& OutError)
{
    using namespace UERingBundleBuilder;
    OutFiles.Reset();
    OutError.Reset();

    FString PrimarySemantic;
    TSharedPtr<FJsonObject> PrimaryRoot;
    if (!LoadPrimarySemantic(PackageName, PrimarySemantic, PrimaryRoot, OutError))
    {
        return false;
    }
    OutFiles.Add(TEXT("assets/primary.uesem.json"));

    const TSharedPtr<FJsonObject>* Dependencies = nullptr;
    if (PrimaryRoot->TryGetObjectField(TEXT("dependencies"), Dependencies) && Dependencies != nullptr)
    {
        for (const TCHAR* Field : { TEXT("hard"), TEXT("soft") })
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!(*Dependencies)->TryGetArrayField(Field, Values) || Values == nullptr)
            {
                continue;
            }
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                FString DependencyPackage;
                if (!Value.IsValid() || !Value->TryGetString(DependencyPackage))
                {
                    continue;
                }
                FString Semantic = FUERingExportManager::Get().GetSemanticFileForPackage(DependencyPackage, false);
                if (!IFileManager::Get().FileExists(*Semantic))
                {
                    Semantic = FUERingExportManager::Get().GetSemanticFileForPackage(DependencyPackage, true);
                }
                AddExistingFile(
                    Semantic,
                    FPaths::Combine(TEXT("assets/dependencies"), SafePackageName(DependencyPackage) + TEXT(".uesem.json")),
                    OutFiles);
            }
        }
    }

    const FString OutputRoot = FUERingExportManager::Get().GetOutputRoot();
    const FString CppRoot = FPaths::Combine(OutputRoot, TEXT("cpp"));
    AddExistingFile(FPaths::Combine(CppRoot, TEXT("reflection.uesem.json")), TEXT("cpp/reflection.uesem.json"), OutFiles);
    AddExistingFile(FPaths::Combine(CppRoot, TEXT("source-index.uesem.json")), TEXT("cpp/source-index.uesem.json"), OutFiles);
    AddExistingFile(FPaths::Combine(OutputRoot, TEXT("index/dependencies.uesem.json")), TEXT("index/dependencies.uesem.json"), OutFiles);

    const TArray<TSharedPtr<FJsonValue>>* CppLinks = nullptr;
    if (PrimaryRoot->TryGetArrayField(TEXT("cppLinks"), CppLinks) && CppLinks != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *CppLinks)
        {
            const TSharedPtr<FJsonObject>* Link = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Link) || Link == nullptr)
            {
                continue;
            }
            for (const TCHAR* Field : { TEXT("header"), TEXT("source") })
            {
                FString SourcePath;
                if (!(*Link)->TryGetStringField(Field, SourcePath) || SourcePath.IsEmpty())
                {
                    continue;
                }
                FString ResolvedSource;
                OutError.Reset();
                if (!ResolveCppLinkSource(SourcePath, ResolvedSource, OutError))
                {
                    if (!OutError.IsEmpty())
                    {
                        return false;
                    }
                    continue;
                }
                FString Owner;
                (*Link)->TryGetStringField(TEXT("owner"), Owner);
                AddExistingFile(
                    ResolvedSource,
                    FPaths::Combine(TEXT("cpp/source"), SafePackageName(Owner) + TEXT("__") + FPaths::GetCleanFilename(ResolvedSource)),
                    OutFiles);
            }
        }
    }

    const FString ConfigRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
    FString NormalizedConfigRoot = ConfigRoot;
    FPaths::NormalizeDirectoryName(NormalizedConfigRoot);
    NormalizedConfigRoot += TEXT("/");
    TArray<FString> ConfigFiles;
    IFileManager::Get().FindFilesRecursive(ConfigFiles, *ConfigRoot, TEXT("*.ini"), true, false);
    for (const FString& Config : ConfigFiles)
    {
        FString RelativeConfig = FPaths::ConvertRelativePathToFull(Config);
        FPaths::NormalizeFilename(RelativeConfig);
        if (RelativeConfig.RemoveFromStart(NormalizedConfigRoot, ESearchCase::IgnoreCase))
        {
            OutFiles.Add(FPaths::Combine(TEXT("config"), RelativeConfig));
        }
    }
    OutFiles.Add(TEXT("manifest.json"));
    OutFiles.Add(TEXT("README.md"));
    OutFiles.Add(TEXT("context-plan.json"));
    OutFiles.Add(TEXT("prompts/analysis.en.md"));
    OutFiles.Add(TEXT("prompts/analysis.zh-CN.md"));
    OutFiles.Sort();
    OutFiles.SetNum(Algo::Unique(OutFiles));
    return true;
}

bool FUERingBundleBuilder::Build(
    const FString& PackageName,
    FString& OutBundleDirectory,
    FString& OutError)
{
    using namespace UERingBundleBuilder;
    OutError.Reset();

    const FString OutputRoot = FUERingExportManager::Get().GetOutputRoot();
    FString PrimarySemantic;
    TSharedPtr<FJsonObject> PrimaryRoot;
    if (!LoadPrimarySemantic(PackageName, PrimarySemantic, PrimaryRoot, OutError))
    {
        return false;
    }

    const FString BundlesRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(OutputRoot, TEXT("bundles")));
    OutBundleDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(BundlesRoot, SafePackageName(PackageName)));
    FString NormalizedBundlesRoot = BundlesRoot;
    FString NormalizedBundleDirectory = OutBundleDirectory;
    FPaths::NormalizeDirectoryName(NormalizedBundlesRoot);
    FPaths::NormalizeDirectoryName(NormalizedBundleDirectory);
    if (!IsContainedPath(NormalizedBundleDirectory, NormalizedBundlesRoot))
    {
        OutError = FString::Printf(TEXT("Refusing to build outside the configured bundle root: %s"), *OutBundleDirectory);
        return false;
    }
    if (IFileManager::Get().DirectoryExists(*OutBundleDirectory)
        && !IFileManager::Get().DeleteDirectory(*OutBundleDirectory, false, true))
    {
        OutError = FString::Printf(TEXT("Could not clean previous bundle directory: %s"), *OutBundleDirectory);
        return false;
    }
    IFileManager::Get().MakeDirectory(*OutBundleDirectory, true);
    if (!CopyIntoBundle(PrimarySemantic, TEXT("assets/primary.uesem.json"), OutBundleDirectory, OutError))
    {
        return false;
    }

    TArray<FString> IncludedAssets = { PackageName };
    const TSharedPtr<FJsonObject>* Dependencies = nullptr;
    if (PrimaryRoot->TryGetObjectField(TEXT("dependencies"), Dependencies) && Dependencies != nullptr)
    {
        for (const TCHAR* Field : { TEXT("hard"), TEXT("soft") })
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!(*Dependencies)->TryGetArrayField(Field, Values) || Values == nullptr)
            {
                continue;
            }
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                FString DependencyPackage;
                if (!Value.IsValid() || !Value->TryGetString(DependencyPackage))
                {
                    continue;
                }
                FString Semantic = FUERingExportManager::Get().GetSemanticFileForPackage(DependencyPackage, false);
                if (!IFileManager::Get().FileExists(*Semantic))
                {
                    Semantic = FUERingExportManager::Get().GetSemanticFileForPackage(DependencyPackage, true);
                }
                if (IFileManager::Get().FileExists(*Semantic))
                {
                    IncludedAssets.AddUnique(DependencyPackage);
                    if (!CopyIntoBundle(
                        Semantic,
                        FPaths::Combine(TEXT("assets/dependencies"), SafePackageName(DependencyPackage) + TEXT(".uesem.json")),
                        OutBundleDirectory,
                        OutError))
                    {
                        return false;
                    }
                }
            }
        }
    }
    IncludedAssets.Sort();

    const FString CppRoot = FPaths::Combine(OutputRoot, TEXT("cpp"));
    if (!CopyIntoBundle(FPaths::Combine(CppRoot, TEXT("reflection.uesem.json")), TEXT("cpp/reflection.uesem.json"), OutBundleDirectory, OutError)
        || !CopyIntoBundle(FPaths::Combine(CppRoot, TEXT("source-index.uesem.json")), TEXT("cpp/source-index.uesem.json"), OutBundleDirectory, OutError)
        || !CopyIntoBundle(FPaths::Combine(OutputRoot, TEXT("index/dependencies.uesem.json")), TEXT("index/dependencies.uesem.json"), OutBundleDirectory, OutError))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* CppLinks = nullptr;
    TSet<FString> CopiedSourceFiles;
    if (PrimaryRoot->TryGetArrayField(TEXT("cppLinks"), CppLinks) && CppLinks != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *CppLinks)
        {
            const TSharedPtr<FJsonObject>* Link = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Link) || Link == nullptr)
            {
                continue;
            }
            for (const TCHAR* Field : { TEXT("header"), TEXT("source") })
            {
                FString SourcePath;
                if (!(*Link)->TryGetStringField(Field, SourcePath) || SourcePath.IsEmpty())
                {
                    continue;
                }
                FString ResolvedSource;
                OutError.Reset();
                if (!ResolveCppLinkSource(SourcePath, ResolvedSource, OutError))
                {
                    if (!OutError.IsEmpty())
                    {
                        return false;
                    }
                    continue;
                }
                if (CopiedSourceFiles.Contains(ResolvedSource))
                {
                    continue;
                }
                CopiedSourceFiles.Add(ResolvedSource);
                FString Owner;
                (*Link)->TryGetStringField(TEXT("owner"), Owner);
                const FString TargetName = SafePackageName(Owner) + TEXT("__") + FPaths::GetCleanFilename(ResolvedSource);
                if (!CopyIntoBundle(
                    ResolvedSource,
                    FPaths::Combine(TEXT("cpp/source"), TargetName),
                    OutBundleDirectory,
                    OutError))
                {
                    return false;
                }
            }
        }
    }

    const FString ConfigRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
    FString NormalizedConfigRoot = ConfigRoot;
    FPaths::NormalizeDirectoryName(NormalizedConfigRoot);
    NormalizedConfigRoot += TEXT("/");
    TArray<FString> ConfigFiles;
    IFileManager::Get().FindFilesRecursive(
        ConfigFiles,
        *ConfigRoot,
        TEXT("*.ini"), true, false);
    ConfigFiles.Sort();
    for (const FString& Config : ConfigFiles)
    {
        FString RelativeConfig = FPaths::ConvertRelativePathToFull(Config);
        FPaths::NormalizeFilename(RelativeConfig);
        if (!RelativeConfig.RemoveFromStart(NormalizedConfigRoot, ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(TEXT("Config path is outside the project Config directory: %s"), *Config);
            return false;
        }
        if (!WriteSanitizedConfig(
            Config,
            FPaths::Combine(TEXT("config"), RelativeConfig),
            OutBundleDirectory,
            OutError))
        {
            return false;
        }
    }

    const TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
    Manifest->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.bundle"));
    Manifest->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    Manifest->SetStringField(TEXT("pluginVersion"), UE_RING_PLUGIN_VERSION);
    Manifest->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
    Manifest->SetStringField(TEXT("asset"), PackageName);
    const FString GeneratedAtUtc = FDateTime::UtcNow().ToIso8601();
    Manifest->SetStringField(TEXT("generatedAtUtc"), GeneratedAtUtc);
    TArray<TSharedPtr<FJsonValue>> JsonAssets;
    for (const FString& Asset : IncludedAssets)
    {
        JsonAssets.Add(MakeShared<FJsonValueString>(Asset));
    }
    Manifest->SetArrayField(TEXT("includedAssets"), JsonAssets);
    FString ManifestJson;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&ManifestJson);
    FJsonSerializer::Serialize(Manifest, Writer);
    ManifestJson += LINE_TERMINATOR;
    if (!FFileHelper::SaveStringToFile(
        ManifestJson,
        *FPaths::Combine(OutBundleDirectory, TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not write the bundle manifest.");
        return false;
    }

    const FString Readme = FString::Printf(
        TEXT("# UE Ring AI Bundle\n\nAsset: `%s`\n\nEngine: `%s`  \nPlugin: `%s`  \nUSEM schema: `%s`  \nSemantic snapshot: `%s`\n\nRead `manifest.json`, then `assets/primary.uesem.json`. Direct dependency semantics, C++ indexes, dependency graph, linked C++ source, and sanitized project configuration are included when available. No Unreal Editor or binary asset parsing is required.\n"),
        *PackageName,
        *FEngineVersion::Current().ToString(EVersionComponent::Patch),
        UE_RING_PLUGIN_VERSION,
        UE_RING_SCHEMA_VERSION,
        *GeneratedAtUtc);
    if (!FFileHelper::SaveStringToFile(
        Readme,
        *FPaths::Combine(OutBundleDirectory, TEXT("README.md")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not write the bundle README.");
        return false;
    }
    FUERingBundleRequest Request;
    Request.Scope = EUERingBundleScope::Asset;
    Request.Value = PackageName;
    return WriteBundleSupplementalFiles(
        OutBundleDirectory,
        Request,
        TArray<FString>{ PackageName },
        OutError);
}

bool FUERingBundleBuilder::Preview(
    const FUERingBundleRequest& Request,
    TArray<FString>& OutFiles,
    FString& OutError)
{
    using namespace UERingBundleBuilder;
    if (Request.Scope == EUERingBundleScope::Asset)
    {
        return Preview(Request.Value, OutFiles, OutError);
    }

    TArray<FString> Packages;
    if (!ResolvePackages(Request, Packages, OutError))
    {
        return false;
    }
    OutFiles.Reset();
    for (const FString& PackageName : Packages)
    {
        FString SemanticFile;
        TSharedPtr<FJsonObject> Root;
        if (!LoadPrimarySemantic(PackageName, SemanticFile, Root, OutError))
        {
            return false;
        }
        OutFiles.Add(FPaths::Combine(
            TEXT("assets/selected"),
            SafePackageName(PackageName) + TEXT(".uesem.json")));
        if (!Request.bIncludeDirectDependencies)
        {
            continue;
        }
        const TSharedPtr<FJsonObject>* Dependencies = nullptr;
        if (Root->TryGetObjectField(TEXT("dependencies"), Dependencies) && Dependencies != nullptr)
        {
            for (const TCHAR* Field : { TEXT("hard"), TEXT("soft") })
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!(*Dependencies)->TryGetArrayField(Field, Values) || Values == nullptr)
                {
                    continue;
                }
                for (const TSharedPtr<FJsonValue>& Value : *Values)
                {
                    FString DependencyPackage;
                    if (Value.IsValid() && Value->TryGetString(DependencyPackage))
                    {
                        FString DependencySemantic = FUERingExportManager::Get().GetSemanticFileForPackage(
                            DependencyPackage,
                            false);
                        if (!IFileManager::Get().FileExists(*DependencySemantic))
                        {
                            DependencySemantic = FUERingExportManager::Get().GetSemanticFileForPackage(
                                DependencyPackage,
                                true);
                        }
                        if (IFileManager::Get().FileExists(*DependencySemantic))
                        {
                            OutFiles.Add(FPaths::Combine(
                                TEXT("assets/dependencies"),
                                SafePackageName(DependencyPackage) + TEXT(".uesem.json")));
                        }
                    }
                }
            }
        }
    }
    for (const TCHAR* File : {
        TEXT("cpp/reflection.uesem.json"),
        TEXT("cpp/source-index.uesem.json"),
        TEXT("index/dependencies.uesem.json"),
        TEXT("manifest.json"),
        TEXT("README.md"),
        TEXT("context-plan.json"),
        TEXT("prompts/analysis.en.md"),
        TEXT("prompts/analysis.zh-CN.md") })
    {
        OutFiles.Add(File);
    }
    OutFiles.Sort();
    OutFiles.SetNum(Algo::Unique(OutFiles));
    return true;
}

bool FUERingBundleBuilder::Build(
    const FUERingBundleRequest& Request,
    FString& OutBundleDirectory,
    FString& OutError)
{
    using namespace UERingBundleBuilder;
    if (Request.Scope == EUERingBundleScope::Asset)
    {
        return Build(Request.Value, OutBundleDirectory, OutError);
    }

    TArray<FString> RootPackages;
    if (!ResolvePackages(Request, RootPackages, OutError))
    {
        return false;
    }
    const FString OutputRoot = FUERingExportManager::Get().GetOutputRoot();
    const FString BundlesRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(OutputRoot, TEXT("bundles")));
    const FString BundleName = ScopeName(Request.Scope) + TEXT("__") + SafePackageName(Request.Value);
    OutBundleDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(BundlesRoot, TEXT("collections"), BundleName));
    FString NormalizedBundlesRoot = BundlesRoot;
    FString NormalizedBundleDirectory = OutBundleDirectory;
    FPaths::NormalizeDirectoryName(NormalizedBundlesRoot);
    FPaths::NormalizeDirectoryName(NormalizedBundleDirectory);
    if (!IsContainedPath(NormalizedBundleDirectory, NormalizedBundlesRoot))
    {
        OutError = FString::Printf(TEXT("Refusing to build outside the configured bundle root: %s"), *OutBundleDirectory);
        return false;
    }
    if (IFileManager::Get().DirectoryExists(*OutBundleDirectory)
        && !IFileManager::Get().DeleteDirectory(*OutBundleDirectory, false, true))
    {
        OutError = FString::Printf(TEXT("Could not clean previous collection bundle: %s"), *OutBundleDirectory);
        return false;
    }
    IFileManager::Get().MakeDirectory(*OutBundleDirectory, true);

    TSet<FString> IncludedAssets;
    TSet<FString> RootPackageSet;
    for (const FString& PackageName : RootPackages)
    {
        RootPackageSet.Add(PackageName);
    }
    TArray<TSharedPtr<FJsonObject>> RootSemantics;
    for (const FString& PackageName : RootPackages)
    {
        FString SemanticFile;
        TSharedPtr<FJsonObject> Root;
        if (!LoadPrimarySemantic(PackageName, SemanticFile, Root, OutError))
        {
            return false;
        }
        RootSemantics.Add(Root);
        IncludedAssets.Add(PackageName);
        if (!CopyIntoBundle(
            SemanticFile,
            FPaths::Combine(TEXT("assets/selected"), SafePackageName(PackageName) + TEXT(".uesem.json")),
            OutBundleDirectory,
            OutError))
        {
            return false;
        }

        if (!Request.bIncludeDirectDependencies)
        {
            continue;
        }
        const TSharedPtr<FJsonObject>* Dependencies = nullptr;
        if (Root->TryGetObjectField(TEXT("dependencies"), Dependencies) && Dependencies != nullptr)
        {
            for (const TCHAR* Field : { TEXT("hard"), TEXT("soft") })
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!(*Dependencies)->TryGetArrayField(Field, Values) || Values == nullptr)
                {
                    continue;
                }
                for (const TSharedPtr<FJsonValue>& Value : *Values)
                {
                    FString DependencyPackage;
                    if (!Value.IsValid() || !Value->TryGetString(DependencyPackage))
                    {
                        continue;
                    }
                    if (RootPackageSet.Contains(DependencyPackage))
                    {
                        continue;
                    }
                    FString DependencySemantic = FUERingExportManager::Get().GetSemanticFileForPackage(
                        DependencyPackage,
                        false);
                    if (!IFileManager::Get().FileExists(*DependencySemantic))
                    {
                        DependencySemantic = FUERingExportManager::Get().GetSemanticFileForPackage(
                            DependencyPackage,
                            true);
                    }
                    if (IFileManager::Get().FileExists(*DependencySemantic))
                    {
                        IncludedAssets.Add(DependencyPackage);
                        if (!CopyIntoBundle(
                            DependencySemantic,
                            FPaths::Combine(TEXT("assets/dependencies"), SafePackageName(DependencyPackage) + TEXT(".uesem.json")),
                            OutBundleDirectory,
                            OutError))
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }

    const FString CppRoot = FPaths::Combine(OutputRoot, TEXT("cpp"));
    if (!CopyIntoBundle(FPaths::Combine(CppRoot, TEXT("reflection.uesem.json")), TEXT("cpp/reflection.uesem.json"), OutBundleDirectory, OutError)
        || !CopyIntoBundle(FPaths::Combine(CppRoot, TEXT("source-index.uesem.json")), TEXT("cpp/source-index.uesem.json"), OutBundleDirectory, OutError)
        || !CopyIntoBundle(FPaths::Combine(OutputRoot, TEXT("index/dependencies.uesem.json")), TEXT("index/dependencies.uesem.json"), OutBundleDirectory, OutError))
    {
        return false;
    }

    TSet<FString> CopiedSourceFiles;
    for (const TSharedPtr<FJsonObject>& Root : RootSemantics)
    {
        const TArray<TSharedPtr<FJsonValue>>* CppLinks = nullptr;
        if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("cppLinks"), CppLinks) || CppLinks == nullptr)
        {
            continue;
        }
        for (const TSharedPtr<FJsonValue>& Value : *CppLinks)
        {
            const TSharedPtr<FJsonObject> Link = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Link.IsValid())
            {
                continue;
            }
            for (const TCHAR* Field : { TEXT("header"), TEXT("source") })
            {
                FString SourcePath;
                if (!Link->TryGetStringField(Field, SourcePath) || SourcePath.IsEmpty())
                {
                    continue;
                }
                FString ResolvedSource;
                OutError.Reset();
                if (!ResolveCppLinkSource(SourcePath, ResolvedSource, OutError))
                {
                    if (!OutError.IsEmpty())
                    {
                        return false;
                    }
                    continue;
                }
                if (CopiedSourceFiles.Contains(ResolvedSource))
                {
                    continue;
                }
                CopiedSourceFiles.Add(ResolvedSource);
                FString Owner;
                Link->TryGetStringField(TEXT("owner"), Owner);
                if (!CopyIntoBundle(
                    ResolvedSource,
                    FPaths::Combine(TEXT("cpp/source"), SafePackageName(Owner) + TEXT("__") + FPaths::GetCleanFilename(ResolvedSource)),
                    OutBundleDirectory,
                    OutError))
                {
                    return false;
                }
            }
        }
    }

    const FString ConfigRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
    FString NormalizedConfigRoot = ConfigRoot;
    FPaths::NormalizeDirectoryName(NormalizedConfigRoot);
    NormalizedConfigRoot += TEXT("/");
    TArray<FString> ConfigFiles;
    IFileManager::Get().FindFilesRecursive(ConfigFiles, *ConfigRoot, TEXT("*.ini"), true, false);
    ConfigFiles.Sort();
    for (const FString& Config : ConfigFiles)
    {
        FString RelativeConfig = FPaths::ConvertRelativePathToFull(Config);
        FPaths::NormalizeFilename(RelativeConfig);
        if (!RelativeConfig.RemoveFromStart(NormalizedConfigRoot, ESearchCase::IgnoreCase)
            || !WriteSanitizedConfig(
                Config,
                FPaths::Combine(TEXT("config"), RelativeConfig),
                OutBundleDirectory,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(TEXT("Config path is outside the project Config directory: %s"), *Config);
            }
            return false;
        }
    }

    TArray<FString> SortedIncludedAssets = IncludedAssets.Array();
    SortedIncludedAssets.Sort();
    const TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
    Manifest->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.bundle"));
    Manifest->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    Manifest->SetStringField(TEXT("pluginVersion"), UE_RING_PLUGIN_VERSION);
    Manifest->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
    Manifest->SetStringField(TEXT("scope"), ScopeName(Request.Scope));
    Manifest->SetStringField(TEXT("scopeValue"), Request.Value);
    TArray<TSharedPtr<FJsonValue>> JsonRoots;
    for (const FString& PackageName : RootPackages)
    {
        JsonRoots.Add(MakeShared<FJsonValueString>(PackageName));
    }
    TArray<TSharedPtr<FJsonValue>> JsonAssets;
    for (const FString& PackageName : SortedIncludedAssets)
    {
        JsonAssets.Add(MakeShared<FJsonValueString>(PackageName));
    }
    Manifest->SetArrayField(TEXT("rootAssets"), JsonRoots);
    Manifest->SetArrayField(TEXT("includedAssets"), JsonAssets);
    FString ManifestJson;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> ManifestWriter =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&ManifestJson);
    FJsonSerializer::Serialize(Manifest, ManifestWriter);
    ManifestJson += LINE_TERMINATOR;
    if (!FFileHelper::SaveStringToFile(
        ManifestJson,
        *FPaths::Combine(OutBundleDirectory, TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not write collection bundle manifest.");
        return false;
    }

    const FString Readme = FString::Printf(
        TEXT("# UE Ring AI Collection Bundle\n\nScope: `%s`  \nValue: `%s`  \nRoot assets: `%d`  \nIncluded assets: `%d`\n\nRead `manifest.json`, then `context-plan.json`. All selected semantics remain available under `assets/selected`; direct dependency semantics are under `assets/dependencies`.\n"),
        *ScopeName(Request.Scope),
        *Request.Value,
        RootPackages.Num(),
        SortedIncludedAssets.Num());
    if (!FFileHelper::SaveStringToFile(
        Readme,
        *FPaths::Combine(OutBundleDirectory, TEXT("README.md")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not write collection bundle README.");
        return false;
    }
    return WriteBundleSupplementalFiles(
        OutBundleDirectory,
        Request,
        RootPackages,
        OutError);
}
