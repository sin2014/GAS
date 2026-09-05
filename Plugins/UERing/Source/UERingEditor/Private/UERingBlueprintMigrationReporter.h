#pragma once

#include "CoreMinimal.h"

class FUERingBlueprintMigrationReporter
{
public:
    static bool Rebuild(FString& OutError);
    static bool UpdatePackages(const TArray<FName>& PackageNames, FString& OutError);
};
