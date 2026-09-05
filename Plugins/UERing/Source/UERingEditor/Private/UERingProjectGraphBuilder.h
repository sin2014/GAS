#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FUERingProjectGraphBuilder
{
public:
    static FString GetGraphFile(const FString& IndexDirectory);
    static bool Rebuild(
        const FString& IndexDirectory,
        const TSharedRef<FJsonObject>& ProjectIndex,
        const TSharedRef<FJsonObject>& DependencyGraph,
        TSharedPtr<FJsonObject>& OutGraph,
        FString& OutError);
    static bool Update(
        const FString& IndexDirectory,
        const TSharedRef<FJsonObject>& ProjectIndex,
        const TSharedRef<FJsonObject>& DependencyGraph,
        const TSet<FString>& ContributorPackages,
        TSharedPtr<FJsonObject>& OutGraph,
        FString& OutError);
    static bool BuildContributions(
        const TSharedRef<FJsonObject>& ProjectIndex,
        const TSharedRef<FJsonObject>& DependencyGraph,
        const TSet<FString>& ContributorPackages,
        TSharedPtr<FJsonObject>& OutGraph);
    static bool MaterializeFromSqlite(
        const FString& IndexDirectory,
        const TSharedRef<FJsonObject>& ProjectIndex,
        TSharedPtr<FJsonObject>& OutGraph,
        FString& OutError);
};
