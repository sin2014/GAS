#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "UERingSettings.generated.h"

UENUM()
enum class EUERingExportProfile : uint8
{
    Logic UMETA(DisplayName="Logic - gameplay and replaceable asset interfaces"),
    Reconstruction UMETA(DisplayName="Reconstruction - authoring data required to rebuild assets"),
    Forensics UMETA(DisplayName="Forensics - maximum diagnostic detail")
};

UERINGEDITOR_API FString UERingExportProfileName(EUERingExportProfile Profile);

UCLASS(config=EditorPerProjectUserSettings, defaultconfig, meta=(DisplayName="UE Ring Semantic Export"))
class UERINGEDITOR_API UUERingSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UUERingSettings();

    virtual FName GetCategoryName() const override;
    virtual FName GetSectionName() const override;
    virtual FText GetSectionText() const override;
    virtual FText GetSectionDescription() const override;

    UPROPERTY(config, EditAnywhere, Category="Export")
    bool bEnableAutoExport = true;

    UPROPERTY(config, EditAnywhere, Category="Export", meta=(RelativeToGameDir))
    FDirectoryPath OutputRoot;

    UPROPERTY(config, EditAnywhere, Category="Export")
    bool bExportOnAssetSave = true;

    UPROPERTY(config, EditAnywhere, Category="Synchronization")
    bool bDeleteSemanticOnAssetDelete = true;

    UPROPERTY(config, EditAnywhere, Category="Synchronization")
    bool bKeepTombstone = false;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeMarkdownSummary = true;

    UPROPERTY(config, EditAnywhere, Category="Content")
    TArray<FString> SummaryLanguages;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeSqliteIndex = true;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeCppIndex = true;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeBlueprintMigrationReport = true;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeNodePositions = false;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeEditorOnlyData = true;

    UPROPERTY(config, EditAnywhere, Category="Content")
    EUERingExportProfile ExportProfile = EUERingExportProfile::Logic;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bIncludeGraphVisualizations = true;

    UPROPERTY(config, EditAnywhere, Category="Content")
    bool bGenerateChangeSummaries = false;

    UPROPERTY(config, EditAnywhere, Category="Output")
    bool bHashSourceAssets = true;

    UPROPERTY(config, EditAnywhere, Category="Output")
    bool bPrettyJson = false;

    UPROPERTY(config, EditAnywhere, Category="Filtering")
    TArray<FString> IgnoredPaths;

    UPROPERTY(config, EditAnywhere, Category="Filtering")
    TArray<FSoftClassPath> IgnoredClasses;

    UPROPERTY(config, EditAnywhere, Category="Filtering")
    TArray<FString> PrivacyFilters;

    UPROPERTY(config, EditAnywhere, Category="Performance", meta=(ClampMin="1", ClampMax="5000"))
    int32 MaxDiffEntries = 500;

    UPROPERTY(config, EditAnywhere, Category="Performance", meta=(ClampMin="1", ClampMax="1024"))
    int32 MaxBundleContextMiB = 8;

    UPROPERTY(config, EditAnywhere, Category="Performance", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxMcpSemanticMiB = 4;
};
