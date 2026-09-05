#pragma once

#include "CoreMinimal.h"

enum class EUERingBundleScope : uint8
{
    Asset,
    Folder,
    Module,
    PrimaryAssetType
};

struct FUERingBundleRequest
{
    EUERingBundleScope Scope = EUERingBundleScope::Asset;
    FString Value;
    bool bIncludeDirectDependencies = true;
};

class UERINGEDITOR_API FUERingBundleBuilder
{
public:
    static bool Preview(const FString& PackageName, TArray<FString>& OutFiles, FString& OutError);
    static bool Preview(const FUERingBundleRequest& Request, TArray<FString>& OutFiles, FString& OutError);
    static bool Build(const FString& PackageName, FString& OutBundleDirectory, FString& OutError);
    static bool Build(const FUERingBundleRequest& Request, FString& OutBundleDirectory, FString& OutError);
};
