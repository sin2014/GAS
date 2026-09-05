#include "UERingSummaryWriter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UERingSettings.h"

namespace UERingSummaryWriter
{
    TArray<FString> GetLanguages()
    {
        TArray<FString> Languages;
        for (FString Language : GetDefault<UUERingSettings>()->SummaryLanguages)
        {
            Language.TrimStartAndEndInline();
            if (Language.Equals(TEXT("en"), ESearchCase::IgnoreCase))
            {
                Languages.AddUnique(TEXT("en"));
            }
            else if (Language.Equals(TEXT("zh-CN"), ESearchCase::IgnoreCase))
            {
                Languages.AddUnique(TEXT("zh-CN"));
            }
        }
        return Languages;
    }

    FString GetSummaryFile(const FString& SemanticFile, const FString& Language)
    {
        return Language == TEXT("en")
            ? FPaths::ChangeExtension(SemanticFile, TEXT("md"))
            : FPaths::ChangeExtension(SemanticFile, Language + TEXT(".md"));
    }

    bool WriteAtomically(const FString& Filename, const FString& Contents, FString& OutError)
    {
        if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
        {
            OutError = FString::Printf(TEXT("Could not create summary directory: %s"), *FPaths::GetPath(Filename));
            return false;
        }
        const FString TempFile = Filename + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(
                Contents,
                *TempFile,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
            || !IFileManager::Get().Move(*Filename, *TempFile, true, true, false, true))
        {
            IFileManager::Get().Delete(*TempFile, false, true);
            OutError = FString::Printf(TEXT("Could not write summary: %s"), *Filename);
            return false;
        }
        return true;
    }

    int32 ArrayCount(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        return Object->TryGetArrayField(Field, Values) && Values != nullptr ? Values->Num() : 0;
    }

    FString OptionalString(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        FString Value;
        Object->TryGetStringField(Field, Value);
        return Value;
    }

    void AddCount(
        FString& Summary,
        const TSharedRef<FJsonObject>& Semantics,
        const TCHAR* Field,
        const TCHAR* Label)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (Semantics->TryGetArrayField(Field, Values) && Values != nullptr)
        {
            Summary += FString::Printf(TEXT("- %s: %d\n"), Label, Values->Num());
        }
    }

    void AddMetric(
        FString& Summary,
        const TSharedRef<FJsonObject>& Semantics,
        const TCHAR* Field,
        const TCHAR* Label)
    {
        double Value = 0.0;
        if (Semantics->TryGetNumberField(Field, Value))
        {
            Summary += FString::Printf(TEXT("- %s: %lld\n"), Label, static_cast<int64>(Value));
        }
    }

    int32 CountNestedArrayEntries(const TSharedPtr<FJsonObject>& Object, const FString& Field)
    {
        if (!Object.IsValid())
        {
            return 0;
        }
        int32 Count = 0;
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            if (!Pair.Value.IsValid())
            {
                continue;
            }
            if (Pair.Value->Type == EJson::Array)
            {
                const TArray<TSharedPtr<FJsonValue>>& Values = Pair.Value->AsArray();
                Count += Pair.Key == Field ? Values.Num() : 0;
                for (const TSharedPtr<FJsonValue>& Value : Values)
                {
                    if (Value.IsValid() && Value->Type == EJson::Object)
                    {
                        Count += CountNestedArrayEntries(Value->AsObject(), Field);
                    }
                }
            }
            else if (Pair.Value->Type == EJson::Object)
            {
                Count += CountNestedArrayEntries(Pair.Value->AsObject(), Field);
            }
        }
        return Count;
    }

    void AddDomainMetrics(
        FString& Summary,
        const TSharedRef<FJsonObject>& Semantics,
        const FString& Kind,
        const bool bChinese)
    {
        if (Kind == TEXT("BehaviorTree")
            || Kind == TEXT("Blackboard")
            || Kind == TEXT("MaterialGraph")
            || Kind == TEXT("NiagaraGraph")
            || Kind == TEXT("PCGGraph")
            || Kind == TEXT("OwnedObjectGraph"))
        {
            AddMetric(Summary, Semantics, TEXT("objectCount"), bChinese ? TEXT("领域对象") : TEXT("Domain objects"));
            AddMetric(Summary, Semantics, TEXT("edgeCount"), bChinese ? TEXT("内部引用边") : TEXT("Internal reference edges"));
        }
        else if (Kind == TEXT("LevelSequence"))
        {
            AddMetric(Summary, Semantics, TEXT("bindingCount"), bChinese ? TEXT("绑定") : TEXT("Bindings"));
            AddMetric(Summary, Semantics, TEXT("possessableCount"), bChinese ? TEXT("可占有对象") : TEXT("Possessables"));
            AddMetric(Summary, Semantics, TEXT("spawnableCount"), bChinese ? TEXT("可生成对象") : TEXT("Spawnables"));
            const int32 Tracks = ArrayCount(Semantics, TEXT("globalTracks"))
                + CountNestedArrayEntries(Semantics, TEXT("tracks"));
            const int32 Sections = CountNestedArrayEntries(Semantics, TEXT("sections"));
            const int32 Channels = CountNestedArrayEntries(Semantics, TEXT("channels"));
            const int32 Keys = CountNestedArrayEntries(Semantics, TEXT("keys"));
            if (bChinese)
            {
                Summary += FString::Printf(
                    TEXT("- 轨道: %d\n- Section: %d\n- Channel: %d\n- 关键帧: %d\n"),
                    Tracks, Sections, Channels, Keys);
            }
            else
            {
                Summary += FString::Printf(
                    TEXT("- Tracks: %d\n- Sections: %d\n- Channels: %d\n- Keys: %d\n"),
                    Tracks, Sections, Channels, Keys);
            }
        }
        else if (Kind == TEXT("PaperTileMap"))
        {
            double Width = 0.0;
            double Height = 0.0;
            Semantics->TryGetNumberField(TEXT("mapWidth"), Width);
            Semantics->TryGetNumberField(TEXT("mapHeight"), Height);
            if (bChinese)
            {
                Summary += FString::Printf(
                    TEXT("- 地图尺寸: %lld x %lld\n"),
                    static_cast<int64>(Width),
                    static_cast<int64>(Height));
            }
            else
            {
                Summary += FString::Printf(
                    TEXT("- Map size: %lld x %lld\n"),
                    static_cast<int64>(Width),
                    static_cast<int64>(Height));
            }
            AddMetric(Summary, Semantics, TEXT("layerCount"), bChinese ? TEXT("图层") : TEXT("Layers"));
            AddMetric(Summary, Semantics, TEXT("occupiedCellCount"), bChinese ? TEXT("非空单元") : TEXT("Occupied cells"));
            AddMetric(Summary, Semantics, TEXT("segmentCount"), bChinese ? TEXT("稀疏行段") : TEXT("Sparse row segments"));
        }
        else if (Kind == TEXT("PaperTileSet"))
        {
            AddMetric(Summary, Semantics, TEXT("tileCount"), bChinese ? TEXT("Tile 数量") : TEXT("Tiles"));
            AddMetric(Summary, Semantics, TEXT("overrideCount"), bChinese ? TEXT("元数据覆盖") : TEXT("Metadata overrides"));
        }
    }

    FString BuildSummary(const TSharedRef<FJsonObject>& Root, const FString& Language)
    {
        const TSharedPtr<FJsonObject>* AssetPtr = nullptr;
        const TSharedPtr<FJsonObject>* DependenciesPtr = nullptr;
        const TSharedPtr<FJsonObject>* SemanticsPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("asset"), AssetPtr)
            || AssetPtr == nullptr
            || !Root->TryGetObjectField(TEXT("semantics"), SemanticsPtr)
            || SemanticsPtr == nullptr)
        {
            return FString();
        }

        const TSharedRef<FJsonObject> Asset = (*AssetPtr).ToSharedRef();
        Root->TryGetObjectField(TEXT("dependencies"), DependenciesPtr);
        const TSharedRef<FJsonObject> Dependencies = DependenciesPtr != nullptr
            ? (*DependenciesPtr).ToSharedRef()
            : MakeShared<FJsonObject>();
        const TSharedRef<FJsonObject> Semantics = (*SemanticsPtr).ToSharedRef();
        const FString Exporter = OptionalString(Root, TEXT("exporter"));
        const FString PackageName = OptionalString(Asset, TEXT("packageName"));
        const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
        const FString Kind = OptionalString(Semantics, TEXT("kind"));
        const int32 CppLinkCount = ArrayCount(Root, TEXT("cppLinks"));
        const int32 DiagnosticCount = ArrayCount(Root, TEXT("diagnostics"));

        FString SemanticSummary;
        if (Language == TEXT("zh-CN"))
        {
            AddCount(SemanticSummary, Semantics, TEXT("variables"), TEXT("变量"));
            AddCount(SemanticSummary, Semantics, TEXT("components"), TEXT("组件"));
            AddCount(SemanticSummary, Semantics, TEXT("graphs"), TEXT("图表"));
            AddCount(SemanticSummary, Semantics, TEXT("timelines"), TEXT("时间轴"));
            AddCount(SemanticSummary, Semantics, TEXT("widgets"), TEXT("本地控件"));
            AddCount(SemanticSummary, Semantics, TEXT("inheritedWidgets"), TEXT("继承控件"));
            AddCount(SemanticSummary, Semantics, TEXT("actors"), TEXT("Actor"));
            AddCount(SemanticSummary, Semantics, TEXT("rows"), TEXT("数据行"));
            AddCount(SemanticSummary, Semantics, TEXT("properties"), TEXT("属性"));
            AddDomainMetrics(SemanticSummary, Semantics, Kind, true);
            SemanticSummary += FString::Printf(
                TEXT("- 依赖：%d 个硬依赖，%d 个软依赖，%d 个管理依赖\n- 引用方：%d\n- C++ 链接：%d\n- 诊断：%d\n"),
                ArrayCount(Dependencies, TEXT("hard")),
                ArrayCount(Dependencies, TEXT("soft")),
                ArrayCount(Dependencies, TEXT("management")),
                ArrayCount(Dependencies, TEXT("referencers")),
                CppLinkCount,
                DiagnosticCount);
            const FString Limitations = Kind == TEXT("ReflectionFallback")
                ? TEXT("\n## 能力说明\n\n此资产使用反射回退导出。已包含序列化属性和引用，但自定义图表或未反射载荷可能不完整。\n")
                : FString();
            return FString::Printf(
                TEXT("# %s\n\n- 包：`%s`\n- 类：`%s`\n- 语义类型：`%s`\n- 源文件：`%s`\n- 源哈希：`%s`\n- 导出器：`%s %s`\n\n## 语义摘要\n\n%s%s\n相邻的确定性 USEM JSON 是权威数据源。\n"),
                *AssetName,
                *PackageName,
                *OptionalString(Asset, TEXT("nativeClass")),
                *Kind,
                *OptionalString(Asset, TEXT("sourceFile")),
                *OptionalString(Asset, TEXT("sourceHash")),
                *Exporter,
                TEXT(""),
                *SemanticSummary,
                *Limitations);
        }

        AddCount(SemanticSummary, Semantics, TEXT("variables"), TEXT("Variables"));
        AddCount(SemanticSummary, Semantics, TEXT("components"), TEXT("Components"));
        AddCount(SemanticSummary, Semantics, TEXT("graphs"), TEXT("Graphs"));
        AddCount(SemanticSummary, Semantics, TEXT("timelines"), TEXT("Timelines"));
        AddCount(SemanticSummary, Semantics, TEXT("widgets"), TEXT("Local widgets"));
        AddCount(SemanticSummary, Semantics, TEXT("inheritedWidgets"), TEXT("Inherited widgets"));
        AddCount(SemanticSummary, Semantics, TEXT("actors"), TEXT("Actors"));
        AddCount(SemanticSummary, Semantics, TEXT("rows"), TEXT("Rows"));
        AddCount(SemanticSummary, Semantics, TEXT("properties"), TEXT("Properties"));
        AddDomainMetrics(SemanticSummary, Semantics, Kind, false);
        SemanticSummary += FString::Printf(
            TEXT("- Dependencies: %d hard, %d soft, %d management\n- Referencers: %d\n- C++ links: %d\n- Diagnostics: %d\n"),
            ArrayCount(Dependencies, TEXT("hard")),
            ArrayCount(Dependencies, TEXT("soft")),
            ArrayCount(Dependencies, TEXT("management")),
            ArrayCount(Dependencies, TEXT("referencers")),
            CppLinkCount,
            DiagnosticCount);
        const FString Limitations = Kind == TEXT("ReflectionFallback")
            ? TEXT("\n## Capability note\n\nThis asset uses reflection fallback. Serialized properties and references are available, but custom graphs or non-reflected payloads may be incomplete.\n")
            : FString();
        return FString::Printf(
            TEXT("# %s\n\n- Package: `%s`\n- Class: `%s`\n- Kind: `%s`\n- Source: `%s`\n- Source hash: `%s`\n- Exporter: `%s`\n\n## Semantic summary\n\n%s%s\nThe adjacent deterministic USEM JSON is authoritative.\n"),
            *AssetName,
            *PackageName,
            *OptionalString(Asset, TEXT("nativeClass")),
            *Kind,
            *OptionalString(Asset, TEXT("sourceFile")),
            *OptionalString(Asset, TEXT("sourceHash")),
            *Exporter,
            *SemanticSummary,
            *Limitations);
    }
}

void FUERingSummaryWriter::GetConfiguredSummaryFiles(const FString& SemanticFile, TArray<FString>& OutFiles)
{
    OutFiles.Reset();
    for (const FString& Language : UERingSummaryWriter::GetLanguages())
    {
        OutFiles.Add(UERingSummaryWriter::GetSummaryFile(SemanticFile, Language));
    }
}

bool FUERingSummaryWriter::HaveConfiguredSummaries(const FString& SemanticFile)
{
    TArray<FString> Files;
    GetConfiguredSummaryFiles(SemanticFile, Files);
    for (const FString& File : Files)
    {
        if (!IFileManager::Get().FileExists(*File))
        {
            return false;
        }
    }
    return true;
}

bool FUERingSummaryWriter::Write(
    const FString& SemanticFile,
    const TSharedRef<FJsonObject>& Root,
    FString& OutError)
{
    const TArray<FString> Languages = UERingSummaryWriter::GetLanguages();
    for (const FString& Language : Languages)
    {
        const FString Contents = UERingSummaryWriter::BuildSummary(Root, Language);
        if (Contents.IsEmpty())
        {
            OutError = TEXT("Could not build Markdown summary from the USEM document.");
            return false;
        }
        if (!UERingSummaryWriter::WriteAtomically(
            UERingSummaryWriter::GetSummaryFile(SemanticFile, Language), Contents, OutError))
        {
            return false;
        }
    }

    const TArray<FString> KnownLanguages = { TEXT("en"), TEXT("zh-CN") };
    for (const FString& Language : KnownLanguages)
    {
        if (!Languages.Contains(Language))
        {
            IFileManager::Get().Delete(
                *UERingSummaryWriter::GetSummaryFile(SemanticFile, Language), false, true);
        }
    }
    return true;
}

void FUERingSummaryWriter::Remove(const FString& SemanticFile)
{
    for (const FString& Language : { FString(TEXT("en")), FString(TEXT("zh-CN")) })
    {
        IFileManager::Get().Delete(*UERingSummaryWriter::GetSummaryFile(SemanticFile, Language), false, true);
    }
}

bool FUERingSummaryWriter::Move(
    const FString& OldSemanticFile,
    const FString& NewSemanticFile,
    FString& OutError)
{
    for (const FString& Language : { FString(TEXT("en")), FString(TEXT("zh-CN")) })
    {
        const FString OldFile = UERingSummaryWriter::GetSummaryFile(OldSemanticFile, Language);
        if (!IFileManager::Get().FileExists(*OldFile))
        {
            continue;
        }
        const FString NewFile = UERingSummaryWriter::GetSummaryFile(NewSemanticFile, Language);
        if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(NewFile), true)
            || !IFileManager::Get().Move(*NewFile, *OldFile, true, true, false, true))
        {
            OutError = FString::Printf(TEXT("Could not move summary %s to %s"), *OldFile, *NewFile);
            return false;
        }
    }
    return true;
}
