#include "UERingDerivedArtifactWriter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingExportManager.h"
#include "UERingSettings.h"
#include "UERingVersion.h"

namespace UERingDerivedArtifactWriter
{
    bool WriteAtomically(const FString& Filename, const FString& Contents, FString& OutError)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        const FString Temporary = Filename + TEXT(".tmp.") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        if (!FFileHelper::SaveStringToFile(
            Contents,
            *Temporary,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = FString::Printf(TEXT("Could not write derived artifact: %s"), *Filename);
            return false;
        }
        if (!IFileManager::Get().Move(*Filename, *Temporary, true, true, false, true))
        {
            IFileManager::Get().Delete(*Temporary, false, true);
            OutError = FString::Printf(TEXT("Could not replace derived artifact: %s"), *Filename);
            return false;
        }
        return true;
    }

    FString DerivedPath(const FString& SemanticFile, const TCHAR* Folder, const TCHAR* Extension)
    {
        FString Relative = SemanticFile;
        const FString RootWithSlash = FUERingExportManager::Get().GetOutputRoot() + TEXT("/");
        FPaths::MakePathRelativeTo(Relative, *RootWithSlash);
        Relative = FPaths::ChangeExtension(Relative, Extension);
        return FPaths::Combine(FUERingExportManager::Get().GetOutputRoot(), Folder, Relative);
    }

    FString EscapeLabel(FString Value)
    {
        Value.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Value.ReplaceInline(TEXT("\r"), TEXT(" "));
        Value.ReplaceInline(TEXT("\n"), TEXT(" "));
        Value.ReplaceInline(TEXT("["), TEXT("("));
        Value.ReplaceInline(TEXT("]"), TEXT(")"));
        return Value.Left(160);
    }

    void AddGraph(
        const TSharedRef<FJsonObject>& Graph,
        const int32 GraphIndex,
        FString& Mermaid,
        FString& Graphviz,
        int32& NextNodeIndex)
    {
        FString GraphName = FString::Printf(TEXT("Graph %d"), GraphIndex);
        Graph->TryGetStringField(TEXT("name"), GraphName);
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
        if (!Graph->TryGetArrayField(TEXT("nodes"), Nodes) || Nodes == nullptr)
        {
            return;
        }

        Mermaid += FString::Printf(TEXT("  subgraph G%d[\"%s\"]\n"), GraphIndex, *EscapeLabel(GraphName));
        Graphviz += FString::Printf(TEXT("  subgraph cluster_%d {\n    label=\"%s\";\n"), GraphIndex, *EscapeLabel(GraphName));
        TMap<FString, FString> NodeNames;
        TMap<FString, FString> PinOwners;
        for (const TSharedPtr<FJsonValue>& Value : *Nodes)
        {
            const TSharedPtr<FJsonObject> Node = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Node.IsValid())
            {
                continue;
            }
            FString Id;
            FString Title;
            FString Class;
            Node->TryGetStringField(TEXT("id"), Id);
            Node->TryGetStringField(TEXT("title"), Title);
            Node->TryGetStringField(TEXT("class"), Class);
            if (Id.IsEmpty())
            {
                continue;
            }
            const FString Name = FString::Printf(TEXT("N%d"), NextNodeIndex++);
            NodeNames.Add(Id, Name);
            const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
            if (Node->TryGetArrayField(TEXT("pins"), Pins) && Pins != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& PinValue : *Pins)
                {
                    const TSharedPtr<FJsonObject> Pin = PinValue.IsValid() ? PinValue->AsObject() : nullptr;
                    FString PinId;
                    if (Pin.IsValid() && Pin->TryGetStringField(TEXT("id"), PinId) && !PinId.IsEmpty())
                    {
                        PinOwners.Add(PinId, Id);
                    }
                }
            }
            const FString Label = EscapeLabel(Title.IsEmpty() ? Class : Title + TEXT("\\n") + Class);
            Mermaid += FString::Printf(TEXT("    %s[\"%s\"]\n"), *Name, *Label);
            Graphviz += FString::Printf(TEXT("    %s [label=\"%s\"];\n"), *Name, *Label);
        }
        if (Graph->TryGetArrayField(TEXT("links"), Links) && Links != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Links)
            {
                const TSharedPtr<FJsonObject> Link = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!Link.IsValid())
                {
                    continue;
                }
                FString From;
                FString To;
                Link->TryGetStringField(TEXT("fromPin"), From);
                Link->TryGetStringField(TEXT("toPin"), To);
                FString FromNode;
                FString ToNode;
                if (const FString* Owner = PinOwners.Find(From))
                {
                    FromNode = *Owner;
                }
                else if (!From.Split(TEXT(":"), &FromNode, nullptr))
                {
                    FromNode = From;
                }
                if (const FString* Owner = PinOwners.Find(To))
                {
                    ToNode = *Owner;
                }
                else if (!To.Split(TEXT(":"), &ToNode, nullptr))
                {
                    ToNode = To;
                }
                const FString* FromName = NodeNames.Find(FromNode);
                const FString* ToName = NodeNames.Find(ToNode);
                if (FromName != nullptr && ToName != nullptr)
                {
                    Mermaid += FString::Printf(TEXT("    %s --> %s\n"), **FromName, **ToName);
                    Graphviz += FString::Printf(TEXT("    %s -> %s;\n"), **FromName, **ToName);
                }
            }
        }
        Mermaid += TEXT("  end\n");
        Graphviz += TEXT("  }\n");
    }

    void AddOwnedObjectGraph(
        const TSharedRef<FJsonObject>& Semantics,
        FString& Mermaid,
        FString& Graphviz,
        int32& NextNodeIndex)
    {
        const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
        if ((!Semantics->TryGetArrayField(TEXT("objects"), Objects) || Objects == nullptr)
            && (!Semantics->TryGetArrayField(TEXT("nodes"), Objects) || Objects == nullptr))
        {
            return;
        }
        TMap<FString, FString> NodeNames;
        for (const TSharedPtr<FJsonValue>& Value : *Objects)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Object.IsValid())
            {
                continue;
            }
            FString Id;
            FString Name;
            FString Class;
            Object->TryGetStringField(TEXT("id"), Id);
            Object->TryGetStringField(TEXT("name"), Name);
            Object->TryGetStringField(TEXT("class"), Class);
            const FString NodeName = FString::Printf(TEXT("N%d"), NextNodeIndex++);
            NodeNames.Add(Id, NodeName);
            const FString Label = EscapeLabel(Name + TEXT("\\n") + Class);
            Mermaid += FString::Printf(TEXT("  %s[\"%s\"]\n"), *NodeName, *Label);
            Graphviz += FString::Printf(TEXT("  %s [label=\"%s\"];\n"), *NodeName, *Label);
        }
        FString Representation;
        Semantics->TryGetStringField(TEXT("representation"), Representation);
        if (Representation == TEXT("material-expression-graph-v1"))
        {
            const FString NodeName = FString::Printf(TEXT("N%d"), NextNodeIndex++);
            NodeNames.Add(TEXT("$material"), NodeName);
            Mermaid += FString::Printf(TEXT("  %s[\"Material Output\"]\n"), *NodeName);
            Graphviz += FString::Printf(TEXT("  %s [label=\"Material Output\"];\n"), *NodeName);
        }
        if ((!Semantics->TryGetArrayField(TEXT("edges"), Edges) || Edges == nullptr)
            && (!Semantics->TryGetArrayField(TEXT("connections"), Edges) || Edges == nullptr))
        {
            Edges = nullptr;
        }
        if (Edges != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Edges)
            {
                const TSharedPtr<FJsonObject> Edge = Value.IsValid() ? Value->AsObject() : nullptr;
                FString Source;
                FString Target;
                if (!Edge.IsValid())
                {
                    continue;
                }
                if (!Edge->TryGetStringField(TEXT("source"), Source))
                {
                    Edge->TryGetStringField(TEXT("sourceNode"), Source);
                }
                if (!Edge->TryGetStringField(TEXT("target"), Target))
                {
                    Edge->TryGetStringField(TEXT("targetNode"), Target);
                }
                const FString* SourceName = NodeNames.Find(Source);
                const FString* TargetName = NodeNames.Find(Target);
                if (SourceName != nullptr && TargetName != nullptr)
                {
                    Mermaid += FString::Printf(TEXT("  %s --> %s\n"), **SourceName, **TargetName);
                    Graphviz += FString::Printf(TEXT("  %s -> %s;\n"), **SourceName, **TargetName);
                }
            }
        }
    }

    FString PointerToken(FString Value)
    {
        Value.ReplaceInline(TEXT("~"), TEXT("~0"));
        Value.ReplaceInline(TEXT("/"), TEXT("~1"));
        return Value;
    }

    FString CompactValue(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid())
        {
            return TEXT("null");
        }
        FString Text;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
        const TSharedRef<FJsonObject> Wrapper = MakeShared<FJsonObject>();
        Wrapper->SetField(TEXT("value"), Value);
        if (!FJsonSerializer::Serialize(Wrapper, Writer))
        {
            return TEXT("<unserializable>");
        }
        const int32 Colon = Text.Find(TEXT(":"));
        if (Text.StartsWith(TEXT("{\"value\":")) && Text.EndsWith(TEXT("}")) && Colon != INDEX_NONE)
        {
            Text = Text.Mid(Colon + 1, Text.Len() - Colon - 2);
        }
        return Text.Left(1024);
    }

    struct FDiffState
    {
        int32 Added = 0;
        int32 Removed = 0;
        int32 Changed = 0;
        TArray<TSharedPtr<FJsonValue>> Entries;
    };

    void AddDiff(
        FDiffState& State,
        const FString& Path,
        const TCHAR* Change,
        const TSharedPtr<FJsonValue>& Before,
        const TSharedPtr<FJsonValue>& After)
    {
        if (FCString::Strcmp(Change, TEXT("added")) == 0)
        {
            ++State.Added;
        }
        else if (FCString::Strcmp(Change, TEXT("removed")) == 0)
        {
            ++State.Removed;
        }
        else
        {
            ++State.Changed;
        }
        if (State.Entries.Num() >= GetDefault<UUERingSettings>()->MaxDiffEntries)
        {
            return;
        }
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("path"), Path.IsEmpty() ? TEXT("/") : Path);
        Entry->SetStringField(TEXT("change"), Change);
        if (Before.IsValid())
        {
            Entry->SetStringField(TEXT("before"), CompactValue(Before));
        }
        if (After.IsValid())
        {
            Entry->SetStringField(TEXT("after"), CompactValue(After));
        }
        State.Entries.Add(MakeShared<FJsonValueObject>(Entry));
    }

    void DiffValues(
        const TSharedPtr<FJsonValue>& Before,
        const TSharedPtr<FJsonValue>& After,
        const FString& Path,
        FDiffState& State)
    {
        if (!Before.IsValid() && After.IsValid())
        {
            AddDiff(State, Path, TEXT("added"), nullptr, After);
            return;
        }
        if (Before.IsValid() && !After.IsValid())
        {
            AddDiff(State, Path, TEXT("removed"), Before, nullptr);
            return;
        }
        if (!Before.IsValid() || !After.IsValid())
        {
            return;
        }
        if (Before->Type != After->Type)
        {
            AddDiff(State, Path, TEXT("changed"), Before, After);
            return;
        }
        if (Before->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> BeforeObject = Before->AsObject();
            const TSharedPtr<FJsonObject> AfterObject = After->AsObject();
            TSet<FString> Keys;
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : BeforeObject->Values)
            {
                Keys.Add(Pair.Key);
            }
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : AfterObject->Values)
            {
                Keys.Add(Pair.Key);
            }
            TArray<FString> SortedKeys = Keys.Array();
            SortedKeys.Sort();
            for (const FString& Key : SortedKeys)
            {
                DiffValues(
                    BeforeObject->TryGetField(Key),
                    AfterObject->TryGetField(Key),
                    Path + TEXT("/") + PointerToken(Key),
                    State);
            }
            return;
        }
        if (Before->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& BeforeArray = Before->AsArray();
            const TArray<TSharedPtr<FJsonValue>>& AfterArray = After->AsArray();
            const int32 Count = FMath::Max(BeforeArray.Num(), AfterArray.Num());
            for (int32 Index = 0; Index < Count; ++Index)
            {
                DiffValues(
                    BeforeArray.IsValidIndex(Index) ? BeforeArray[Index] : nullptr,
                    AfterArray.IsValidIndex(Index) ? AfterArray[Index] : nullptr,
                    Path + TEXT("/") + FString::FromInt(Index),
                    State);
            }
            return;
        }
        if (CompactValue(Before) != CompactValue(After))
        {
            AddDiff(State, Path, TEXT("changed"), Before, After);
        }
    }
}

void FUERingDerivedArtifactWriter::GetGraphArtifactFiles(
    const FString& SemanticFile,
    FString& OutMermaidFile,
    FString& OutGraphvizFile)
{
    using namespace UERingDerivedArtifactWriter;
    OutMermaidFile = DerivedPath(SemanticFile, TEXT("graphs"), TEXT("callgraph.mmd"));
    OutGraphvizFile = DerivedPath(SemanticFile, TEXT("graphs"), TEXT("callgraph.dot"));
}

FString FUERingDerivedArtifactWriter::GetChangeSummaryFile(const FString& SemanticFile)
{
    using namespace UERingDerivedArtifactWriter;
    return DerivedPath(SemanticFile, TEXT("diffs"), TEXT("change.json"));
}

void FUERingDerivedArtifactWriter::RemoveGraphArtifacts(const FString& SemanticFile)
{
    FString MermaidFile;
    FString GraphvizFile;
    GetGraphArtifactFiles(SemanticFile, MermaidFile, GraphvizFile);
    IFileManager::Get().Delete(*MermaidFile, false, true);
    IFileManager::Get().Delete(*GraphvizFile, false, true);
}

void FUERingDerivedArtifactWriter::RemoveChangeSummary(const FString& SemanticFile)
{
    IFileManager::Get().Delete(*GetChangeSummaryFile(SemanticFile), false, true);
}

void FUERingDerivedArtifactWriter::RemoveArtifacts(const FString& SemanticFile)
{
    RemoveGraphArtifacts(SemanticFile);
    RemoveChangeSummary(SemanticFile);
}

bool FUERingDerivedArtifactWriter::MoveArtifacts(
    const FString& OldSemanticFile,
    const FString& NewSemanticFile,
    FString& OutError)
{
    FString OldMermaid;
    FString OldGraphviz;
    FString NewMermaid;
    FString NewGraphviz;
    GetGraphArtifactFiles(OldSemanticFile, OldMermaid, OldGraphviz);
    GetGraphArtifactFiles(NewSemanticFile, NewMermaid, NewGraphviz);
    const TArray<TPair<FString, FString>> Files = {
        { OldMermaid, NewMermaid },
        { OldGraphviz, NewGraphviz },
        { GetChangeSummaryFile(OldSemanticFile), GetChangeSummaryFile(NewSemanticFile) }
    };
    for (const TPair<FString, FString>& File : Files)
    {
        if (!IFileManager::Get().FileExists(*File.Key))
        {
            continue;
        }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File.Value), true);
        if (!IFileManager::Get().Move(*File.Value, *File.Key, true, true, false, true))
        {
            OutError = FString::Printf(
                TEXT("Could not move derived artifact %s to %s"),
                *File.Key,
                *File.Value);
            return false;
        }
    }
    return true;
}

bool FUERingDerivedArtifactWriter::WriteGraphArtifacts(
    const FString& SemanticFile,
    const TSharedRef<FJsonObject>& Semantics,
    FString& OutMermaidFile,
    FString& OutGraphvizFile,
    FString& OutError)
{
    using namespace UERingDerivedArtifactWriter;
    OutMermaidFile.Reset();
    OutGraphvizFile.Reset();
    OutError.Reset();

    FString Mermaid = TEXT("flowchart TD\n");
    FString Graphviz = TEXT("digraph UERing {\n  rankdir=LR;\n");
    int32 NextNodeIndex = 0;
    const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
    if (Semantics->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs != nullptr)
    {
        for (int32 Index = 0; Index < Graphs->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Graph = (*Graphs)[Index].IsValid() ? (*Graphs)[Index]->AsObject() : nullptr;
            if (Graph.IsValid())
            {
                AddGraph(Graph.ToSharedRef(), Index, Mermaid, Graphviz, NextNodeIndex);
            }
        }
    }
    else
    {
        AddOwnedObjectGraph(Semantics, Mermaid, Graphviz, NextNodeIndex);
    }
    Graphviz += TEXT("}\n");
    if (NextNodeIndex == 0)
    {
        RemoveGraphArtifacts(SemanticFile);
        return true;
    }

    GetGraphArtifactFiles(SemanticFile, OutMermaidFile, OutGraphvizFile);
    return WriteAtomically(OutMermaidFile, Mermaid, OutError)
        && WriteAtomically(OutGraphvizFile, Graphviz, OutError);
}

bool FUERingDerivedArtifactWriter::WriteChangeSummary(
    const FString& SemanticFile,
    const TSharedRef<FJsonObject>& PreviousRoot,
    const TSharedRef<FJsonObject>& CurrentRoot,
    FString& OutDiffFile,
    FString& OutError)
{
    using namespace UERingDerivedArtifactWriter;
    OutDiffFile.Reset();
    OutError.Reset();
    const TSharedPtr<FJsonObject>* PreviousAsset = nullptr;
    const TSharedPtr<FJsonObject>* CurrentAsset = nullptr;
    FString PreviousHash;
    FString CurrentHash;
    if (PreviousRoot->TryGetObjectField(TEXT("asset"), PreviousAsset) && PreviousAsset != nullptr)
    {
        (*PreviousAsset)->TryGetStringField(TEXT("sourceHash"), PreviousHash);
    }
    if (CurrentRoot->TryGetObjectField(TEXT("asset"), CurrentAsset) && CurrentAsset != nullptr)
    {
        (*CurrentAsset)->TryGetStringField(TEXT("sourceHash"), CurrentHash);
    }
    if (PreviousHash.IsEmpty())
    {
        return true;
    }

    FDiffState State;
    for (const TCHAR* Field : {
        TEXT("dependencies"),
        TEXT("semantics"),
        TEXT("reconstruction"),
        TEXT("cppLinks"),
        TEXT("diagnostics") })
    {
        DiffValues(
            PreviousRoot->TryGetField(Field),
            CurrentRoot->TryGetField(Field),
            FString(TEXT("/")) + Field,
            State);
    }
    for (const TCHAR* Field : {
        TEXT("packageName"),
        TEXT("objectPath"),
        TEXT("assetClass"),
        TEXT("nativeClass"),
        TEXT("packageGuid"),
        TEXT("sourceFile"),
        TEXT("semanticFile") })
    {
        DiffValues(
            PreviousAsset != nullptr ? (*PreviousAsset)->TryGetField(Field) : nullptr,
            CurrentAsset != nullptr ? (*CurrentAsset)->TryGetField(Field) : nullptr,
            FString(TEXT("/asset/")) + Field,
            State);
    }
    if (State.Added + State.Removed + State.Changed == 0)
    {
        return true;
    }
    const TSharedRef<FJsonObject> Diff = MakeShared<FJsonObject>();
    Diff->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.change"));
    Diff->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    FString PackageName;
    FString ChangedAtUtc;
    if (CurrentAsset != nullptr)
    {
        (*CurrentAsset)->TryGetStringField(TEXT("packageName"), PackageName);
        ChangedAtUtc = FDateTime::UtcNow().ToIso8601();
    }
    Diff->SetStringField(TEXT("packageName"), PackageName);
    Diff->SetStringField(TEXT("fromSourceHash"), PreviousHash);
    Diff->SetStringField(TEXT("toSourceHash"), CurrentHash);
    Diff->SetStringField(TEXT("changedAtUtc"), ChangedAtUtc);
    Diff->SetNumberField(TEXT("addedCount"), State.Added);
    Diff->SetNumberField(TEXT("removedCount"), State.Removed);
    Diff->SetNumberField(TEXT("changedCount"), State.Changed);
    Diff->SetNumberField(TEXT("totalChangeCount"), State.Added + State.Removed + State.Changed);
    Diff->SetBoolField(
        TEXT("truncated"),
        State.Added + State.Removed + State.Changed > State.Entries.Num());
    Diff->SetArrayField(TEXT("changes"), State.Entries);

    FString Json;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
    if (!FJsonSerializer::Serialize(Diff, Writer))
    {
        OutError = TEXT("Could not serialize semantic change summary.");
        return false;
    }
    Json += LINE_TERMINATOR;
    OutDiffFile = GetChangeSummaryFile(SemanticFile);
    return WriteAtomically(OutDiffFile, Json, OutError);
}
