#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FUERingSqliteIndexer
{
public:
    static FString GetDatabaseFile(const FString& IndexDirectory);
    static bool IsIncrementalDatabaseCompatible(const FString& IndexDirectory);
    static bool FindContributorsTargetingAssets(
        const FString& IndexDirectory,
        const TSet<FString>& PackageNames,
        TSet<FString>& OutContributors);
    static bool Rebuild(
        const FString& IndexDirectory,
        const TSharedRef<FJsonObject>& ProjectIndex,
        const TSharedRef<FJsonObject>& DependencyGraph,
        const TSharedRef<FJsonObject>& ProjectGraph,
        FString& OutError);
    static bool Update(
        const FString& IndexDirectory,
        const TSharedRef<FJsonObject>& ProjectIndex,
        const TSharedRef<FJsonObject>& DependencyGraph,
        const TSharedRef<FJsonObject>& ProjectGraph,
        const TSet<FString>& IndexPackages,
        const TSet<FString>& ContributorPackages,
        FString& OutError);
};
