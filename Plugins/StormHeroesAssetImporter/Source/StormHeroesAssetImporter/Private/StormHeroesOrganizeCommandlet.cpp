#include "StormHeroesOrganizeCommandlet.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Commandlets/Commandlet.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogStormHeroesOrganizeCommandlet, Log, All);

namespace
{
FString NormalizeStormHeroesOrganizeContentPath(FString Path)
{
    Path.TrimStartAndEndInline();
    Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (Path.EndsWith(TEXT("/")))
    {
        Path.LeftChopInline(1);
    }
    return Path;
}

FString GetDestinationSubdirectory(const FAssetData& AssetData)
{
    const FTopLevelAssetPath AssetClassPath = AssetData.AssetClassPath;

    if (AssetClassPath == UAnimSequence::StaticClass()->GetClassPathName())
    {
        return TEXT("Animations");
    }

    if (AssetClassPath == UMaterial::StaticClass()->GetClassPathName())
    {
        return TEXT("Materials");
    }

    if (AssetClassPath == UTexture::StaticClass()->GetClassPathName())
    {
        return TEXT("Textures");
    }

    if (AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
    {
        return TEXT("Textures");
    }

    return FString();
}

bool IsRootAssetThatShouldStayInPlace(const FAssetData& AssetData)
{
    const FTopLevelAssetPath AssetClassPath = AssetData.AssetClassPath;
    return AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName()
        || AssetClassPath == USkeleton::StaticClass()->GetClassPathName()
        || AssetClassPath == UPhysicsAsset::StaticClass()->GetClassPathName();
}

void CollectRedirectors(const FString& RootPath, TArray<UObjectRedirector*>& OutRedirectors)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(*RootPath);
    Filter.bRecursivePaths = true;
    Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    for (const FAssetData& RedirectorData : RedirectorAssets)
    {
        if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(RedirectorData.GetAsset()))
        {
            OutRedirectors.Add(Redirector);
        }
    }
}
}

UStormHeroesOrganizeCommandlet::UStormHeroesOrganizeCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UStormHeroesOrganizeCommandlet::Main(const FString& Params)
{
    FString HeroPath;
    FParse::Value(*Params, TEXT("HeroPath="), HeroPath);
    HeroPath = NormalizeStormHeroesOrganizeContentPath(HeroPath);

    if (HeroPath.IsEmpty() || !HeroPath.StartsWith(TEXT("/Game/")))
    {
        UE_LOG(LogStormHeroesOrganizeCommandlet, Error, TEXT("Usage: -run=StormHeroesOrganize -HeroPath=/Game/Assets/Characters/StormHeroes/Ragnaros"));
        return 1;
    }

    UE_LOG(LogStormHeroesOrganizeCommandlet, Display, TEXT("Organizing StormHeroes assets under %s"), *HeroPath);

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.ScanPathsSynchronous({ HeroPath }, true);

    FARFilter Filter;
    Filter.PackagePaths.Add(*HeroPath);
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDatas;
    AssetRegistry.GetAssets(Filter, AssetDatas);

    TArray<FAssetRenameData> RenameData;
    int32 RootAssetsKept = 0;
    int32 AssetsAlreadyOrganized = 0;
    int32 StaleDuplicatesConsolidated = 0;
    int32 UnsupportedAssets = 0;

    const FString AnimationsPath = HeroPath / TEXT("Animations");
    const FString MaterialsPath = HeroPath / TEXT("Materials");
    const FString TexturesPath = HeroPath / TEXT("Textures");
    AssetRegistry.AddPath(AnimationsPath);
    AssetRegistry.AddPath(MaterialsPath);
    AssetRegistry.AddPath(TexturesPath);

    for (const FAssetData& AssetData : AssetDatas)
    {
        if (AssetData.AssetClassPath == UObjectRedirector::StaticClass()->GetClassPathName())
        {
            continue;
        }

        const FString CurrentPackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
        const FString AssetName = AssetData.AssetName.ToString();
        const FString Subdirectory = GetDestinationSubdirectory(AssetData);

        if (Subdirectory.IsEmpty())
        {
            if (CurrentPackagePath == HeroPath && IsRootAssetThatShouldStayInPlace(AssetData))
            {
                ++RootAssetsKept;
            }
            else
            {
                ++UnsupportedAssets;
                UE_LOG(LogStormHeroesOrganizeCommandlet, Warning, TEXT("Leaving unsupported asset in place: %s (%s)"), *AssetData.GetObjectPathString(), *AssetData.AssetClassPath.ToString());
            }
            continue;
        }

        const FString DestinationPackagePath = HeroPath / Subdirectory;
        if (CurrentPackagePath == DestinationPackagePath)
        {
            ++AssetsAlreadyOrganized;
            continue;
        }

        if (!CurrentPackagePath.Equals(HeroPath, ESearchCase::IgnoreCase))
        {
            ++UnsupportedAssets;
            UE_LOG(LogStormHeroesOrganizeCommandlet, Warning, TEXT("Leaving asset in unexpected subdirectory: %s"), *AssetData.GetObjectPathString());
            continue;
        }

        const FString DestinationObjectPath = DestinationPackagePath / AssetName;
        if (FPackageName::DoesPackageExist(DestinationObjectPath))
        {
            UObject* SourceAsset = AssetData.GetAsset();
            const FString DestinationFullObjectPath = DestinationObjectPath + TEXT(".") + AssetName;
            UObject* DestinationAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *DestinationFullObjectPath);

            if (!SourceAsset || !DestinationAsset)
            {
                UE_LOG(LogStormHeroesOrganizeCommandlet, Error, TEXT("Destination exists but source/destination could not be loaded. Source=%s Destination=%s"), *AssetData.GetObjectPathString(), *DestinationFullObjectPath);
                return 2;
            }

            if (SourceAsset->GetClass() != DestinationAsset->GetClass())
            {
                UE_LOG(LogStormHeroesOrganizeCommandlet, Error, TEXT("Destination already exists with a different class. Source=%s (%s) Destination=%s (%s)"), *AssetData.GetObjectPathString(), *SourceAsset->GetClass()->GetName(), *DestinationFullObjectPath, *DestinationAsset->GetClass()->GetName());
                return 2;
            }

            TArray<UObject*> ObjectsToConsolidate;
            ObjectsToConsolidate.Add(SourceAsset);
            ObjectTools::FConsolidationResults Results = ObjectTools::ConsolidateObjects(DestinationAsset, ObjectsToConsolidate, false);
            if (!Results.InvalidConsolidationObjs.IsEmpty() || !Results.FailedConsolidationObjs.IsEmpty())
            {
                UE_LOG(LogStormHeroesOrganizeCommandlet, Error, TEXT("Could not consolidate stale duplicate. Source=%s Destination=%s Invalid=%d Failed=%d"), *AssetData.GetObjectPathString(), *DestinationFullObjectPath, Results.InvalidConsolidationObjs.Num(), Results.FailedConsolidationObjs.Num());
                return 2;
            }

            ++StaleDuplicatesConsolidated;
            UE_LOG(LogStormHeroesOrganizeCommandlet, Display, TEXT("Consolidated stale duplicate: %s -> %s"), *AssetData.GetObjectPathString(), *DestinationFullObjectPath);
            continue;
        }

        UObject* Asset = AssetData.GetAsset();
        if (!Asset)
        {
            UE_LOG(LogStormHeroesOrganizeCommandlet, Error, TEXT("Could not load asset for move: %s"), *AssetData.GetObjectPathString());
            return 3;
        }

        RenameData.Add(FAssetRenameData(Asset, DestinationPackagePath, AssetName));
        UE_LOG(LogStormHeroesOrganizeCommandlet, Display, TEXT("Queue move: %s -> %s"), *AssetData.GetObjectPathString(), *DestinationObjectPath);
    }

    if (!RenameData.IsEmpty())
    {
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        if (!AssetToolsModule.Get().RenameAssets(RenameData))
        {
            UE_LOG(LogStormHeroesOrganizeCommandlet, Error, TEXT("AssetTools RenameAssets failed."));
            return 4;
        }

    }

    TArray<UObjectRedirector*> Redirectors;
    CollectRedirectors(HeroPath, Redirectors);
    if (!Redirectors.IsEmpty())
    {
        UE_LOG(LogStormHeroesOrganizeCommandlet, Warning, TEXT("Leaving %d redirectors for ResavePackages -FixupRedirectors. Do not call AssetTools FixupReferencers from this commandlet because it can open Slate UI in commandlet mode."), Redirectors.Num());
    }

    UEditorLoadingAndSavingUtils::SaveDirtyPackages(false, true);
    AssetRegistry.ScanPathsSynchronous({ HeroPath }, true);

    UE_LOG(
        LogStormHeroesOrganizeCommandlet,
        Display,
        TEXT("StormHeroes organize finished. Moved=%d AlreadyOrganized=%d RootAssetsKept=%d StaleDuplicatesConsolidated=%d Unsupported=%d"),
        RenameData.Num(),
        AssetsAlreadyOrganized,
        RootAssetsKept,
        StaleDuplicatesConsolidated,
        UnsupportedAssets);

    return 0;
}
