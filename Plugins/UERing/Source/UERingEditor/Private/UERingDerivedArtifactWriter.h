#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FUERingDerivedArtifactWriter
{
public:
    static void GetGraphArtifactFiles(
        const FString& SemanticFile,
        FString& OutMermaidFile,
        FString& OutGraphvizFile);

    static FString GetChangeSummaryFile(const FString& SemanticFile);

    static void RemoveGraphArtifacts(const FString& SemanticFile);
    static void RemoveChangeSummary(const FString& SemanticFile);
    static void RemoveArtifacts(const FString& SemanticFile);
    static bool MoveArtifacts(
        const FString& OldSemanticFile,
        const FString& NewSemanticFile,
        FString& OutError);

    static bool WriteGraphArtifacts(
        const FString& SemanticFile,
        const TSharedRef<FJsonObject>& Semantics,
        FString& OutMermaidFile,
        FString& OutGraphvizFile,
        FString& OutError);

    static bool WriteChangeSummary(
        const FString& SemanticFile,
        const TSharedRef<FJsonObject>& PreviousRoot,
        const TSharedRef<FJsonObject>& CurrentRoot,
        FString& OutDiffFile,
        FString& OutError);
};
