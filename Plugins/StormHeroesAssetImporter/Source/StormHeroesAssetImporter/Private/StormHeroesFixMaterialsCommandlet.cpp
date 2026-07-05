#include "StormHeroesFixMaterialsCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "Materials/Material.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogStormHeroesFixMaterialsCommandlet, Log, All);

namespace
{
constexpr float StormHeroesFixedRoughness = 0.5f;

FString NormalizeStormHeroesMaterialContentPath(FString Path)
{
    Path.TrimStartAndEndInline();
    Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (Path.EndsWith(TEXT("/")))
    {
        Path.LeftChopInline(1);
    }
    return Path;
}

void ClearScalarInputConnection(FScalarMaterialInput& Input)
{
    Input.Expression = nullptr;
    Input.OutputIndex = 0;
    Input.InputName = NAME_None;
    Input.Mask = 0;
    Input.MaskR = 0;
    Input.MaskG = 0;
    Input.MaskB = 0;
    Input.MaskA = 0;
}

void CopyScalarInputConnection(FScalarMaterialInput& Destination, const FScalarMaterialInput& Source)
{
    Destination.UseConstant = 0;
    Destination.Expression = Source.Expression;
    Destination.OutputIndex = Source.OutputIndex;
    Destination.InputName = NAME_None;
    Destination.Mask = Source.Mask;
    Destination.MaskR = Source.MaskR;
    Destination.MaskG = Source.MaskG;
    Destination.MaskB = Source.MaskB;
    Destination.MaskA = Source.MaskA;
}

void SetRoughnessConstant(FScalarMaterialInput& Roughness)
{
    ClearScalarInputConnection(Roughness);
    Roughness.UseConstant = 1;
    Roughness.Constant = StormHeroesFixedRoughness;
}

bool IsFixedRoughness(const FScalarMaterialInput& Roughness)
{
    return Roughness.Expression == nullptr
        && Roughness.UseConstant != 0
        && FMath::IsNearlyEqual(Roughness.Constant, StormHeroesFixedRoughness, KINDA_SMALL_NUMBER);
}

FString DescribeConnection(const FScalarMaterialInput& Input)
{
    if (!Input.Expression)
    {
        return TEXT("<none>");
    }

    return FString::Printf(
        TEXT("%s [OutputIndex=%d Mask=%d%d%d%d%d]"),
        *Input.Expression->GetPathName(),
        Input.OutputIndex,
        Input.Mask,
        Input.MaskR,
        Input.MaskG,
        Input.MaskB,
        Input.MaskA);
}
}

UStormHeroesFixMaterialsCommandlet::UStormHeroesFixMaterialsCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UStormHeroesFixMaterialsCommandlet::Main(const FString& Params)
{
    FString RootPath;
    FParse::Value(*Params, TEXT("RootPath="), RootPath);
    RootPath = NormalizeStormHeroesMaterialContentPath(RootPath.IsEmpty() ? TEXT("/Game/Assets/Characters/StormHeroes") : RootPath);

    const bool bDryRun = FParse::Param(*Params, TEXT("DryRun"));
    const bool bVerifyOnly = FParse::Param(*Params, TEXT("VerifyOnly"));

    if (RootPath.IsEmpty() || !RootPath.StartsWith(TEXT("/Game/")))
    {
        UE_LOG(LogStormHeroesFixMaterialsCommandlet, Error, TEXT("Usage: -run=StormHeroesFixMaterials [-RootPath=/Game/Assets/Characters/StormHeroes] [-DryRun] [-VerifyOnly]"));
        return 1;
    }

    UE_LOG(
        LogStormHeroesFixMaterialsCommandlet,
        Display,
        TEXT("Scanning StormHeroes materials under %s. DryRun=%s VerifyOnly=%s"),
        *RootPath,
        bDryRun ? TEXT("true") : TEXT("false"),
        bVerifyOnly ? TEXT("true") : TEXT("false"));

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.ScanPathsSynchronous({ RootPath }, true);

    FARFilter Filter;
    Filter.PackagePaths.Add(*RootPath);
    Filter.bRecursivePaths = true;
    Filter.bRecursiveClasses = true;
    Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());

    TArray<FAssetData> MaterialAssets;
    AssetRegistry.GetAssets(Filter, MaterialAssets);
    MaterialAssets.Sort([](const FAssetData& Left, const FAssetData& Right)
    {
        return Left.GetObjectPathString() < Right.GetObjectPathString();
    });

    int32 LoadedMaterials = 0;
    int32 ModifiedMaterials = 0;
    int32 MovedRoughnessConnections = 0;
    int32 RoughnessConstantsSet = 0;
    int32 OverwrittenSpecularConnections = 0;
    int32 VerificationFailures = 0;
    TSet<UPackage*> PackagesToSaveSet;

    for (const FAssetData& MaterialData : MaterialAssets)
    {
        UMaterial* Material = Cast<UMaterial>(MaterialData.GetAsset());
        if (!Material)
        {
            UE_LOG(LogStormHeroesFixMaterialsCommandlet, Warning, TEXT("Could not load material asset: %s"), *MaterialData.GetObjectPathString());
            ++VerificationFailures;
            continue;
        }

        ++LoadedMaterials;

        UMaterialEditorOnlyData* EditorOnlyData = Material->GetEditorOnlyData();
        if (!EditorOnlyData)
        {
            UE_LOG(LogStormHeroesFixMaterialsCommandlet, Warning, TEXT("Material has no editor-only data: %s"), *Material->GetPathName());
            ++VerificationFailures;
            continue;
        }

        FScalarMaterialInput& Roughness = EditorOnlyData->Roughness;
        FScalarMaterialInput& Specular = EditorOnlyData->Specular;

        if (bVerifyOnly)
        {
            if (!IsFixedRoughness(Roughness))
            {
                UE_LOG(
                    LogStormHeroesFixMaterialsCommandlet,
                    Warning,
                    TEXT("VERIFY FAIL: Roughness is not fixed to constant 0.5. Material=%s Roughness=%s UseConstant=%d Constant=%.6f"),
                    *Material->GetPathName(),
                    *DescribeConnection(Roughness),
                    Roughness.UseConstant,
                    Roughness.Constant);
                ++VerificationFailures;
            }
            continue;
        }

        const bool bHadRoughnessConnection = Roughness.Expression != nullptr;
        const bool bNeedsRoughnessConstant = !IsFixedRoughness(Roughness);
        if (!bHadRoughnessConnection && !bNeedsRoughnessConstant)
        {
            continue;
        }

        ++ModifiedMaterials;
        if (bHadRoughnessConnection)
        {
            ++MovedRoughnessConnections;
            if (Specular.Expression && Specular.Expression != Roughness.Expression)
            {
                ++OverwrittenSpecularConnections;
                UE_LOG(
                    LogStormHeroesFixMaterialsCommandlet,
                    Warning,
                    TEXT("Overwriting existing Specular connection. Material=%s OldSpecular=%s NewSpecularFromRoughness=%s"),
                    *Material->GetPathName(),
                    *DescribeConnection(Specular),
                    *DescribeConnection(Roughness));
            }

            if (!bDryRun)
            {
                Material->PreEditChange(nullptr);
                CopyScalarInputConnection(Specular, Roughness);
                SetRoughnessConstant(Roughness);
                Material->PostEditChange();
                Material->MarkPackageDirty();
            }
        }
        else if (!bDryRun)
        {
            Material->PreEditChange(nullptr);
            SetRoughnessConstant(Roughness);
            Material->PostEditChange();
            Material->MarkPackageDirty();
        }

        if (bNeedsRoughnessConstant)
        {
            ++RoughnessConstantsSet;
        }

        if (UPackage* Package = Material->GetOutermost())
        {
            PackagesToSaveSet.Add(Package);
        }

        UE_LOG(
            LogStormHeroesFixMaterialsCommandlet,
            Display,
            TEXT("%s material: %s RoughnessConnectionMoved=%s"),
            bDryRun ? TEXT("Would fix") : TEXT("Fixed"),
            *Material->GetPathName(),
            bHadRoughnessConnection ? TEXT("true") : TEXT("false"));
    }

    if (!bDryRun && !bVerifyOnly && !PackagesToSaveSet.IsEmpty())
    {
        TArray<UPackage*> PackagesToSave = PackagesToSaveSet.Array();
        if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false))
        {
            UE_LOG(LogStormHeroesFixMaterialsCommandlet, Error, TEXT("Failed to save one or more modified material packages."));
            return 2;
        }
    }

    UE_LOG(
        LogStormHeroesFixMaterialsCommandlet,
        Display,
        TEXT("StormHeroes material fix finished. Materials=%d Modified=%d MovedRoughnessConnections=%d RoughnessConstantsSet=%d OverwrittenSpecularConnections=%d VerificationFailures=%d SavedPackages=%d"),
        LoadedMaterials,
        ModifiedMaterials,
        MovedRoughnessConnections,
        RoughnessConstantsSet,
        OverwrittenSpecularConnections,
        VerificationFailures,
        bDryRun || bVerifyOnly ? 0 : PackagesToSaveSet.Num());

    return VerificationFailures == 0 ? 0 : 3;
}
