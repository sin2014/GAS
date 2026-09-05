// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/IAssetRegistry.h"
#include "CollectionManagerModule.h"
#include "HAL/IConsoleManager.h"
#include "ICollectionContainer.h"
#include "ICollectionManager.h"
#include "Templates/Greater.h"
#include "UObject/SoftObjectPath.h"

class UWorld;

//////////////////////////////////////////////////////////////////////////

// 注册集合差异支撑分析命令，递归查找旧集合资产对新集合新增资产的直接和间接引用关系。
FAutoConsoleCommandWithWorldArgsAndOutputDevice GDiffCollectionReferenceSupport(
	TEXT("Lyra.DiffCollectionReferenceSupport"),
	TEXT("Usage:\n")
	TEXT("  Lyra.DiffCollectionReferenceSupport OldCollectionName NewCollectionName [Deduplicate]\n")
	TEXT("\n")
	TEXT("It will list the assets in Old that 'support' assets introduced in New (are referencers directly/indirectly)\n")
	TEXT("as well as any loose unsupported assets.\n")
	TEXT("\n")
	TEXT("The optional third argument controls whether or not multi-supported assets will be de-duplicated (true) or not (false)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Params, UWorld* World, FOutputDevice& Ar)
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();;
	const TSharedRef<ICollectionContainer>& CollectionContainer = FCollectionManagerModule::GetModule().Get().GetProjectCollectionContainer();

	if (Params.Num() < 2)
	{
		Ar.Log(TEXT("Expected two parameters"));
		return;
	}

	if (AssetRegistry.IsLoadingAssets())
	{
		Ar.Log(TEXT("Asset registry is still scanning, try again later"));
		return;
	}

	const bool bExcludeSecondInstanceOfMultiSupported = (Params.Num() >= 3) ? Params[2].ToBool() : true;

	TArray<FSoftObjectPath> OldPaths;
	if (!CollectionContainer->GetAssetsInCollection(FName(*Params[0]), ECollectionShareType::CST_All, /*out*/ OldPaths))
	{
		Ar.Log(FString::Printf(TEXT("Failed to find collection %s"), *Params[0]));
		return;
	}

	TArray<FSoftObjectPath> NewPaths;
	if (!CollectionContainer->GetAssetsInCollection(FName(*Params[1]), ECollectionShareType::CST_All, /*out*/ NewPaths))
	{
		Ar.Log(FString::Printf(TEXT("Failed to find collection %s"), *Params[1]));
		return;
	}

	TSet<FName> OldPathSet;
	Algo::Transform(OldPaths, OldPathSet, &FSoftObjectPath::GetLongPackageFName);

	TSet<FName> NewPathSet;
	Algo::Transform(NewPaths, NewPathSet, &FSoftObjectPath::GetLongPackageFName);

	TSet<FName> IntroducedAssetSet = NewPathSet.Difference(OldPathSet);

	// 记录每个新增资产由旧集合中的哪些资产直接或间接引用支撑。
	// Map from added asset to list of assets in old paths that supports it (if any)
	TMap<FName, TSet<FName>> SupportMap;
	TSet<FName> VisitedAssets;

	// 统计旧集合中每个支撑资产直接或间接覆盖的新增资产数量。
	// Map of count of newly added assets directly/indirectly supported by each supporter asset in the old paths
	TMap<FName, int32> SupporterCountMap;
	TMap<FName, TSet<FName>> SupporterToAddedMap;

	TFunction<void(const FName)> RecursivelyBuildSupport = [&](const FName IntroducedAssetPath)
	{
		// 当前新增资产可能已在其他资产的递归依赖遍历中处理过，避免重复和环。
		// Someone else may have already processed me as part of their dependencies
		if (!VisitedAssets.Contains(IntroducedAssetPath))
		{
			VisitedAssets.Add(IntroducedAssetPath);

			TArray<FName> Referencers;
			AssetRegistry.GetReferencers(IntroducedAssetPath, /*out*/ Referencers);

			for (const FName& Referencer : Referencers)
			{
				if (OldPathSet.Contains(Referencer))
				{
					// 旧集合资产直接引用该新增资产，记为直接支撑。
					// Direct support
					SupportMap.FindOrAdd(IntroducedAssetPath).Add(Referencer);
				}
				else
				{
					// 引用者不在旧集合中，继续向上递归查找间接支撑者。
					// Indirect, need to process recursively
					RecursivelyBuildSupport(Referencer);

					// 合并递归引用者已找到的支撑集合，形成当前资产的间接支撑关系。
					// Can now use the supports it indicated to build into our own
					TSet<FName>& MySupports = SupportMap.FindOrAdd(IntroducedAssetPath);
					if (TSet<FName>* pRecuriveReferencers = SupportMap.Find(Referencer))
					{
						MySupports.Append(*pRecuriveReferencers);
					}
				}
			}
		}
	};

	// 为全部新增资产构建直接和间接支撑关系。
	// Find the supporters
	for (const FName& IntroducedAssetPath : IntroducedAssetSet)
	{
		RecursivelyBuildSupport(IntroducedAssetPath);
	}

	// 汇总每个旧资产可支撑的新增资产集合和数量。
	// Count the strongest supporters
	for (const auto& KVP : SupportMap)
	{
		const FName SupportedAsset = KVP.Key;
		for (const FName& Supporter : KVP.Value)
		{
			SupporterToAddedMap.FindOrAdd(Supporter).Add(SupportedAsset);
		}
	}

	TSet<FName> AlreadyPrintedOut;

	Ar.Log(TEXT("List of supporters, sorted by count of newly added assets being supported"));
	SupporterCountMap.ValueSort(TGreater<int32>());
	for (const auto& KVP : SupporterToAddedMap)
	{
		const FName Supporter = KVP.Key;

		// 仅保留本次新增资产，并按选项排除已由更强支撑者输出的资产。
		// Filter to added assets (and exclude ones already printed if we were asked to)
		TArray<FName> AddedAssetsBeingSupported(KVP.Value.Array());
		AddedAssetsBeingSupported = AddedAssetsBeingSupported.FilterByPredicate([&](FName Test) { return IntroducedAssetSet.Contains(Test); });

		int32 IncludingMultisupportCount = AddedAssetsBeingSupported.Num();
		if (bExcludeSecondInstanceOfMultiSupported)
		{
			AddedAssetsBeingSupported = AddedAssetsBeingSupported.FilterByPredicate([&](FName Test)	{ return !AlreadyPrintedOut.Contains(Test); });
		}

		Algo::Sort(AddedAssetsBeingSupported, [](const FName LHS, const FName RHS) { return LHS.LexicalLess(RHS); });

		// 输出该旧资产支撑的新增资产数量和明细。
		// Print out the list
		Ar.Log(FString::Printf(TEXT("%s supports %d new assets:"), *Supporter.ToString(), IncludingMultisupportCount));
		for (const FName& AddedAsset : AddedAssetsBeingSupported)
		{
			const int32 AddedAssetSupportCount = SupportMap.FindChecked(AddedAsset).Num();
			Ar.Log(FString::Printf(TEXT("\t%s%s"), *AddedAsset.ToString(), (AddedAssetSupportCount > 1) ? TEXT(" [multi-supported]") : TEXT("")));
			AlreadyPrintedOut.Add(AddedAsset);
		}

		const int32 NumExcludedDueToMultiSupport = IncludingMultisupportCount - AddedAssetsBeingSupported.Num();
		if (NumExcludedDueToMultiSupport > 0)
		{
			Ar.Log(FString::Printf(TEXT("\tAnd %d more that were also supported by a previous supporter"), NumExcludedDueToMultiSupport));
		}
	}

	Ar.Log(TEXT("\n"));
	Ar.Log(TEXT("List of unsupported assets:"));
	for (const FName& AssetName : IntroducedAssetSet)
	{
		if (!SupportMap.Contains(AssetName) || (SupportMap.FindRef(AssetName).Num() == 0))
		{
			Ar.Log(FString::Printf(TEXT("\t%s"), *AssetName.ToString()));
			//@TODO：检查未被旧集合引用的新增资产是否由 PrimaryAsset 规则支撑。
			//@TODO: Check to see if this is instead supported by a primary asset maybe?
		}
	}
}));
