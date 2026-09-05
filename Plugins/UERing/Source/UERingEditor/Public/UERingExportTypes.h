#pragma once

#include "AssetRegistry/AssetData.h"
#include "Dom/JsonObject.h"
#include "UERingSettings.h"

enum class EUERingExportStatus : uint8
{
    Exported,
    Unchanged,
    Unsupported,
    Failed
};

struct FUERingExportResult
{
    EUERingExportStatus Status = EUERingExportStatus::Failed;
    FString ExporterName;
    FString OutputFile;
    FString Error;

    bool IsSuccess() const
    {
        return Status == EUERingExportStatus::Exported || Status == EUERingExportStatus::Unchanged;
    }
};

struct FUERingExportContext
{
    FAssetData AssetData;
    TObjectPtr<UObject> Asset = nullptr;
    FString SourceFile;
    FString RelativeSourceFile;
    FString RelativeSemanticFile;
    FString OutputFile;
    FString SourceHash;
    FString InputFingerprint;
    bool bPackageDirtyAfterLoad = false;
    EUERingExportProfile Profile = EUERingExportProfile::Logic;
};

struct FUERingSemanticPayload
{
    TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    TArray<FName> AdditionalHardDependencies;
    TArray<FName> AdditionalSoftDependencies;
    TArray<TSharedPtr<FJsonValue>> CppLinks;
    TArray<TSharedPtr<FJsonValue>> Diagnostics;
    TArray<TSharedPtr<FJsonValue>> Omissions;
};
