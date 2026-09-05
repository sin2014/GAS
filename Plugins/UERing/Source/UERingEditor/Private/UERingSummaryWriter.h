#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FUERingSummaryWriter
{
public:
    static void GetConfiguredSummaryFiles(const FString& SemanticFile, TArray<FString>& OutFiles);
    static bool HaveConfiguredSummaries(const FString& SemanticFile);
    static bool Write(
        const FString& SemanticFile,
        const TSharedRef<FJsonObject>& Root,
        FString& OutError);
    static void Remove(const FString& SemanticFile);
    static bool Move(
        const FString& OldSemanticFile,
        const FString& NewSemanticFile,
        FString& OutError);
};
