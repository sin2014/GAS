#include "UERingBlueprintMigrationReporter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingExportManager.h"
#include "UERingVersion.h"

namespace UERingBlueprintMigrationReporter
{
    struct FCandidate
    {
        FString PackageName;
        FString AssetClass;
        FString SemanticKind;
        FString SemanticFile;
        int32 GraphCount = 0;
        int32 NodeCount = 0;
        int32 VariableCount = 0;
        int32 ComponentCount = 0;
        int32 TimelineCount = 0;
        int32 NativeLinkCount = 0;
        int32 DiagnosticCount = 0;
        int32 Score = 0;
        FString Priority;
        TArray<FString> NativeSymbols;
        TArray<FString> RiskFlags;
        TArray<FString> Recommendations;
        FDateTime Timestamp = FDateTime::MinValue();
    };

    FString RelativeToProject(FString Path)
    {
        FPaths::MakePathRelativeTo(Path, *FPaths::ProjectDir());
        FPaths::NormalizeFilename(Path);
        return Path;
    }

    int32 ArrayCount(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        return Object->TryGetArrayField(Field, Values) && Values != nullptr ? Values->Num() : 0;
    }

    TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Strings)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(Strings.Num());
        for (const FString& String : Strings)
        {
            Values.Add(MakeShared<FJsonValueString>(String));
        }
        return Values;
    }

    bool WriteAtomically(const FString& Filename, const FString& Contents, FString& OutError)
    {
        if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
        {
            OutError = FString::Printf(TEXT("Could not create migration report directory: %s"), *FPaths::GetPath(Filename));
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
            OutError = FString::Printf(TEXT("Could not write migration report: %s"), *Filename);
            return false;
        }
        return true;
    }

    bool ReadCandidate(const FString& File, FCandidate& OutCandidate)
    {
        FString Json;
        TSharedPtr<FJsonObject> Root;
        const TSharedPtr<FJsonObject>* AssetPtr = nullptr;
        const TSharedPtr<FJsonObject>* SemanticsPtr = nullptr;
        if (!FFileHelper::LoadFileToString(Json, *File)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
            || !Root.IsValid()
            || !Root->TryGetObjectField(TEXT("asset"), AssetPtr)
            || AssetPtr == nullptr
            || !Root->TryGetObjectField(TEXT("semantics"), SemanticsPtr)
            || SemanticsPtr == nullptr)
        {
            return false;
        }

        const TSharedRef<FJsonObject> Asset = (*AssetPtr).ToSharedRef();
        const TSharedRef<FJsonObject> Semantics = (*SemanticsPtr).ToSharedRef();
        Asset->TryGetStringField(TEXT("packageName"), OutCandidate.PackageName);
        Asset->TryGetStringField(TEXT("assetClass"), OutCandidate.AssetClass);
        Semantics->TryGetStringField(TEXT("kind"), OutCandidate.SemanticKind);
        if (!OutCandidate.SemanticKind.Contains(TEXT("Blueprint")))
        {
            return false;
        }

        OutCandidate.SemanticFile = RelativeToProject(File);
        OutCandidate.GraphCount = ArrayCount(Semantics, TEXT("graphs"));
        OutCandidate.VariableCount = ArrayCount(Semantics, TEXT("variables"));
        OutCandidate.ComponentCount = ArrayCount(Semantics, TEXT("components"));
        OutCandidate.TimelineCount = ArrayCount(Semantics, TEXT("timelines"));
        const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
        if (Semantics->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& GraphValue : *Graphs)
            {
                const TSharedPtr<FJsonObject>* GraphPtr = nullptr;
                if (GraphValue.IsValid() && GraphValue->TryGetObject(GraphPtr) && GraphPtr != nullptr)
                {
                    OutCandidate.NodeCount += ArrayCount((*GraphPtr).ToSharedRef(), TEXT("nodes"));
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* CppLinks = nullptr;
        if (Root->TryGetArrayField(TEXT("cppLinks"), CppLinks) && CppLinks != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& LinkValue : *CppLinks)
            {
                const TSharedPtr<FJsonObject>* LinkPtr = nullptr;
                FString Symbol;
                if (LinkValue.IsValid()
                    && LinkValue->TryGetObject(LinkPtr)
                    && LinkPtr != nullptr
                    && (*LinkPtr)->TryGetStringField(TEXT("symbol"), Symbol)
                    && !Symbol.IsEmpty())
                {
                    OutCandidate.NativeSymbols.AddUnique(Symbol);
                }
            }
        }
        OutCandidate.NativeSymbols.Sort();
        OutCandidate.NativeLinkCount = OutCandidate.NativeSymbols.Num();
        OutCandidate.DiagnosticCount = ArrayCount(Root.ToSharedRef(), TEXT("diagnostics"));
        OutCandidate.Score = OutCandidate.NodeCount
            + OutCandidate.GraphCount * 5
            + OutCandidate.VariableCount * 2
            + OutCandidate.ComponentCount * 2
            + OutCandidate.TimelineCount * 8
            + OutCandidate.DiagnosticCount * 20;
        OutCandidate.Priority = OutCandidate.Score >= 120
            ? TEXT("high")
            : OutCandidate.Score >= 40 ? TEXT("medium") : TEXT("low");

        if (OutCandidate.NodeCount >= 100)
        {
            OutCandidate.RiskFlags.Add(TEXT("large-graph-surface"));
            OutCandidate.Recommendations.Add(TEXT("Split migration by graph or responsibility before translating behavior."));
        }
        if (OutCandidate.TimelineCount > 0)
        {
            OutCandidate.RiskFlags.Add(TEXT("timeline-behavior"));
            OutCandidate.Recommendations.Add(TEXT("Define explicit C++ timeline or curve ownership before migration."));
        }
        if (OutCandidate.DiagnosticCount > 0)
        {
            OutCandidate.RiskFlags.Add(TEXT("export-diagnostics"));
            OutCandidate.Recommendations.Add(TEXT("Resolve Blueprint compile/export diagnostics before migration."));
        }
        if (OutCandidate.NativeLinkCount == 0)
        {
            OutCandidate.RiskFlags.Add(TEXT("no-native-symbol-links"));
            OutCandidate.Recommendations.Add(TEXT("Establish the intended native base class and public API boundary first."));
        }
        else
        {
            OutCandidate.Recommendations.Add(TEXT("Review the recorded native symbol links as the initial C++ integration boundary."));
        }
        if (OutCandidate.Recommendations.IsEmpty())
        {
            OutCandidate.Recommendations.Add(TEXT("Migrate incrementally and preserve the exported pin defaults and execution order."));
        }
        OutCandidate.Timestamp = IFileManager::Get().GetTimeStamp(*File);
        return true;
    }

    void ReadStringArray(
        const TSharedRef<FJsonObject>& Object,
        const TCHAR* Field,
        TArray<FString>& OutValues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
        {
            return;
        }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString String;
            if (Value.IsValid() && Value->TryGetString(String))
            {
                OutValues.Add(String);
            }
        }
    }

    int32 NumberField(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        double Value = 0.0;
        Object->TryGetNumberField(Field, Value);
        return static_cast<int32>(Value);
    }

    bool ReadStoredCandidate(const TSharedRef<FJsonObject>& Object, FCandidate& OutCandidate)
    {
        const TSharedPtr<FJsonObject>* MetricsPtr = nullptr;
        if (!Object->TryGetStringField(TEXT("packageName"), OutCandidate.PackageName)
            || OutCandidate.PackageName.IsEmpty()
            || !Object->TryGetStringField(TEXT("assetClass"), OutCandidate.AssetClass)
            || !Object->TryGetStringField(TEXT("semanticKind"), OutCandidate.SemanticKind)
            || !Object->TryGetStringField(TEXT("semanticFile"), OutCandidate.SemanticFile)
            || !Object->TryGetStringField(TEXT("priority"), OutCandidate.Priority)
            || !Object->TryGetObjectField(TEXT("metrics"), MetricsPtr)
            || MetricsPtr == nullptr)
        {
            return false;
        }

        OutCandidate.Score = NumberField(Object, TEXT("complexityScore"));
        const TSharedRef<FJsonObject> Metrics = (*MetricsPtr).ToSharedRef();
        OutCandidate.GraphCount = NumberField(Metrics, TEXT("graphs"));
        OutCandidate.NodeCount = NumberField(Metrics, TEXT("nodes"));
        OutCandidate.VariableCount = NumberField(Metrics, TEXT("variables"));
        OutCandidate.ComponentCount = NumberField(Metrics, TEXT("components"));
        OutCandidate.TimelineCount = NumberField(Metrics, TEXT("timelines"));
        OutCandidate.NativeLinkCount = NumberField(Metrics, TEXT("nativeLinks"));
        OutCandidate.DiagnosticCount = NumberField(Metrics, TEXT("diagnostics"));
        ReadStringArray(Object, TEXT("nativeSymbols"), OutCandidate.NativeSymbols);
        ReadStringArray(Object, TEXT("riskFlags"), OutCandidate.RiskFlags);
        ReadStringArray(Object, TEXT("recommendations"), OutCandidate.Recommendations);

        const FString SemanticFile = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), OutCandidate.SemanticFile));
        if (!IFileManager::Get().FileExists(*SemanticFile))
        {
            return false;
        }
        OutCandidate.Timestamp = IFileManager::Get().GetTimeStamp(*SemanticFile);
        return true;
    }

    bool LoadStoredCandidates(TArray<FCandidate>& OutCandidates)
    {
        const FString ReportFile = FPaths::Combine(
            FUERingExportManager::Get().GetOutputRoot(),
            TEXT("reports/blueprint-cpp-migration.uesem.json"));
        FString Json;
        TSharedPtr<FJsonObject> Root;
        const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
        FString Schema;
        FString SchemaVersion;
        if (!FFileHelper::LoadFileToString(Json, *ReportFile)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
            || !Root.IsValid()
            || !Root->TryGetStringField(TEXT("schema"), Schema)
            || Schema != TEXT("com.ue-ring.usem.blueprint-cpp-migration")
            || !Root->TryGetStringField(TEXT("schemaVersion"), SchemaVersion)
            || SchemaVersion != UE_RING_SCHEMA_VERSION
            || !Root->TryGetArrayField(TEXT("candidates"), Candidates)
            || Candidates == nullptr)
        {
            return false;
        }

        OutCandidates.Reserve(Candidates->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Candidates)
        {
            const TSharedPtr<FJsonObject>* CandidateObject = nullptr;
            FCandidate Candidate;
            if (!Value.IsValid()
                || !Value->TryGetObject(CandidateObject)
                || CandidateObject == nullptr
                || !ReadStoredCandidate((*CandidateObject).ToSharedRef(), Candidate))
            {
                return false;
            }
            OutCandidates.Add(MoveTemp(Candidate));
        }
        return true;
    }

    bool WriteReport(TArray<FCandidate>& Candidates, FString& OutError)
    {
        Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
        {
            return Left.Score == Right.Score
                ? Left.PackageName < Right.PackageName
                : Left.Score > Right.Score;
        });

        FDateTime LatestTimestamp = FDateTime::MinValue();
        TArray<TSharedPtr<FJsonValue>> JsonCandidates;
        FString Markdown = TEXT("# Blueprint to C++ Migration Assistance\n\n");
        Markdown += TEXT("This report ranks evidence from exported Blueprint semantics. It does not generate or claim equivalent C++ code.\n\n");
        for (const FCandidate& Candidate : Candidates)
        {
            LatestTimestamp = FMath::Max(LatestTimestamp, Candidate.Timestamp);
            const TSharedRef<FJsonObject> JsonCandidate = MakeShared<FJsonObject>();
            JsonCandidate->SetStringField(TEXT("packageName"), Candidate.PackageName);
            JsonCandidate->SetStringField(TEXT("assetClass"), Candidate.AssetClass);
            JsonCandidate->SetStringField(TEXT("semanticKind"), Candidate.SemanticKind);
            JsonCandidate->SetStringField(TEXT("semanticFile"), Candidate.SemanticFile);
            JsonCandidate->SetStringField(TEXT("priority"), Candidate.Priority);
            JsonCandidate->SetNumberField(TEXT("complexityScore"), Candidate.Score);
            const TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
            Metrics->SetNumberField(TEXT("graphs"), Candidate.GraphCount);
            Metrics->SetNumberField(TEXT("nodes"), Candidate.NodeCount);
            Metrics->SetNumberField(TEXT("variables"), Candidate.VariableCount);
            Metrics->SetNumberField(TEXT("components"), Candidate.ComponentCount);
            Metrics->SetNumberField(TEXT("timelines"), Candidate.TimelineCount);
            Metrics->SetNumberField(TEXT("nativeLinks"), Candidate.NativeLinkCount);
            Metrics->SetNumberField(TEXT("diagnostics"), Candidate.DiagnosticCount);
            JsonCandidate->SetObjectField(TEXT("metrics"), Metrics);
            JsonCandidate->SetArrayField(TEXT("nativeSymbols"), StringValues(Candidate.NativeSymbols));
            JsonCandidate->SetArrayField(TEXT("riskFlags"), StringValues(Candidate.RiskFlags));
            JsonCandidate->SetArrayField(TEXT("recommendations"), StringValues(Candidate.Recommendations));
            JsonCandidates.Add(MakeShared<FJsonValueObject>(JsonCandidate));

            Markdown += FString::Printf(
                TEXT("## %s\n\n- Priority: `%s`\n- Complexity score: %d\n- Graphs / nodes: %d / %d\n- Variables / components / timelines: %d / %d / %d\n- Native links / diagnostics: %d / %d\n- Semantic file: `%s`\n"),
                *Candidate.PackageName,
                *Candidate.Priority,
                Candidate.Score,
                Candidate.GraphCount,
                Candidate.NodeCount,
                Candidate.VariableCount,
                Candidate.ComponentCount,
                Candidate.TimelineCount,
                Candidate.NativeLinkCount,
                Candidate.DiagnosticCount,
                *Candidate.SemanticFile);
            if (!Candidate.RiskFlags.IsEmpty())
            {
                Markdown += TEXT("- Risk flags: `") + FString::Join(Candidate.RiskFlags, TEXT("`, `")) + TEXT("`\n");
            }
            Markdown += TEXT("\nRecommendations:\n\n");
            for (const FString& Recommendation : Candidate.Recommendations)
            {
                Markdown += TEXT("- ") + Recommendation + TEXT("\n");
            }
            Markdown += TEXT("\n");
        }

        const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.blueprint-cpp-migration"));
        Root->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
        Root->SetStringField(
            TEXT("generatedAtUtc"),
            LatestTimestamp == FDateTime::MinValue() ? TEXT("1970-01-01T00:00:00Z") : LatestTimestamp.ToIso8601());
        Root->SetStringField(TEXT("method"), TEXT("semantic-evidence-ranking-v1"));
        Root->SetStringField(
            TEXT("disclaimer"),
            TEXT("This is an assistance report based on exported semantics, not generated equivalent C++ code."));
        Root->SetArrayField(TEXT("candidates"), JsonCandidates);

        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
        if (!FJsonSerializer::Serialize(Root, Writer))
        {
            OutError = TEXT("Could not serialize Blueprint migration report.");
            return false;
        }
        Json += LINE_TERMINATOR;

        const FString ReportDirectory = FPaths::Combine(
            FUERingExportManager::Get().GetOutputRoot(), TEXT("reports"));
        return WriteAtomically(
                FPaths::Combine(ReportDirectory, TEXT("blueprint-cpp-migration.uesem.json")),
                Json,
                OutError)
            && WriteAtomically(
                FPaths::Combine(ReportDirectory, TEXT("blueprint-cpp-migration.md")),
                Markdown,
                OutError);
    }
}

bool FUERingBlueprintMigrationReporter::Rebuild(FString& OutError)
{
    using namespace UERingBlueprintMigrationReporter;

    TArray<FString> SemanticFiles;
    IFileManager::Get().FindFilesRecursive(
        SemanticFiles,
        *FUERingExportManager::Get().GetOutputRoot(),
        TEXT("*.uesem.json"),
        true,
        false);
    SemanticFiles.Sort();

    TArray<FCandidate> Candidates;
    for (const FString& File : SemanticFiles)
    {
        FCandidate Candidate;
        if (ReadCandidate(File, Candidate))
        {
            Candidates.Add(MoveTemp(Candidate));
        }
    }
    return WriteReport(Candidates, OutError);
}

bool FUERingBlueprintMigrationReporter::UpdatePackages(
    const TArray<FName>& PackageNames,
    FString& OutError)
{
    using namespace UERingBlueprintMigrationReporter;

    if (PackageNames.IsEmpty())
    {
        return true;
    }

    TArray<FCandidate> Candidates;
    if (!LoadStoredCandidates(Candidates))
    {
        return Rebuild(OutError);
    }

    TSet<FString> DirtyPackages;
    for (const FName PackageName : PackageNames)
    {
        DirtyPackages.Add(PackageName.ToString());
    }
    Candidates.RemoveAll([&DirtyPackages](const FCandidate& Candidate)
    {
        return DirtyPackages.Contains(Candidate.PackageName);
    });

    for (const FString& PackageName : DirtyPackages)
    {
        for (const bool bIsMap : { false, true })
        {
            const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(
                PackageName,
                bIsMap);
            if (!IFileManager::Get().FileExists(*SemanticFile))
            {
                continue;
            }
            FCandidate Candidate;
            if (ReadCandidate(SemanticFile, Candidate))
            {
                Candidates.Add(MoveTemp(Candidate));
            }
        }
    }
    return WriteReport(Candidates, OutError);
}
