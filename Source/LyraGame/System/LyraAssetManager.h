// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetManager.h"
#include "LyraAssetManagerStartupJob.h"
#include "Templates/SubclassOf.h"
#include "LyraAssetManager.generated.h"

#define UE_API LYRAGAME_API

class UPrimaryDataAsset;

class ULyraGameData;
class ULyraPawnData;

struct FLyraBundles
{
	static const FName Equipped;
};


/**
 * Lyra 的 AssetManager 实现，负责项目特定的核心数据加载、启动任务进度和已加载资产保活。
 * 通过 DefaultEngine.ini 的 AssetManagerClassName 配置为全局资产管理器。
 */
/**
 * ULyraAssetManager
 *
 *	Game implementation of the asset manager that overrides functionality and stores game-specific types.
 *	It is expected that most games will want to override AssetManager as it provides a good place for game-specific loading logic.
 *	This class is used by setting 'AssetManagerClassName' in DefaultEngine.ini.
 */
UCLASS(MinimalAPI, Config = Game)
class ULyraAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:

	UE_API ULyraAssetManager();

	// 返回项目配置的 ULyraAssetManager 单例；配置错误时触发致命错误。
	// Returns the AssetManager singleton object.
	static UE_API ULyraAssetManager& Get();

	// 解析 TSoftObjectPtr；资产未加载时执行同步加载，并可选择加入保活列表。
	// Returns the asset referenced by a TSoftObjectPtr.  This will synchronously load the asset if it's not already loaded.
	template<typename AssetType>
	static AssetType* GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	// 解析 TSoftClassPtr；类未加载时执行同步加载，并可选择加入保活列表。
	// Returns the subclass referenced by a TSoftClassPtr.  This will synchronously load the asset if it's not already loaded.
	template<typename AssetType>
	static TSubclassOf<AssetType> GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory = true);

	// 输出当前由 AssetManager 跟踪并保持在内存中的全部资产。
	// Logs all assets currently loaded and tracked by the asset manager.
	static UE_API void DumpLoadedAssets();

	UE_API const ULyraGameData& GetGameData();
	UE_API const ULyraPawnData* GetDefaultPawnData() const;

protected:
	template <typename GameDataClass>
	const GameDataClass& GetOrLoadTypedGameData(const TSoftObjectPtr<GameDataClass>& DataPath)
	{
		if (TObjectPtr<UPrimaryDataAsset> const * pResult = GameDataMap.Find(GameDataClass::StaticClass()))
		{
			return *CastChecked<GameDataClass>(*pResult);
		}

		// 核心 GameData 尚未加载时执行阻塞加载。
		// Does a blocking load if needed
		return *CastChecked<const GameDataClass>(LoadGameDataOfClass(GameDataClass::StaticClass(), DataPath, GameDataClass::StaticClass()->GetFName()));
	}


	static UE_API UObject* SynchronousLoadAsset(const FSoftObjectPath& AssetPath);
	static UE_API bool ShouldLogAssetLoads();

	// 以线程安全方式把已加载资产加入强引用集合，防止垃圾回收。
	// Thread safe way of adding a loaded asset to keep in memory.
	UE_API void AddLoadedAsset(const UObject* Asset);

	//~UAssetManager interface
	UE_API virtual void StartInitialLoading() override;
#if WITH_EDITOR
	UE_API virtual void PreBeginPIE(bool bStartSimulate) override;
#endif
	//~End of UAssetManager interface

	UE_API UPrimaryDataAsset* LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass, const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath, FPrimaryAssetType PrimaryAssetType);

protected:

	// 项目启动必须加载的全局 GameData 软引用。
	// Global game data asset to use.
	UPROPERTY(Config)
	TSoftObjectPtr<ULyraGameData> LyraGameDataPath;

	// 已加载的 GameData 强引用。
	// Loaded version of the game data
	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, TObjectPtr<UPrimaryDataAsset>> GameDataMap;

	// PlayerState 与 Experience 都未指定 PawnData 时使用的项目默认配置。
	// Pawn data used when spawning player pawns if there isn't one set on the player state.
	UPROPERTY(Config)
	TSoftObjectPtr<ULyraPawnData> DefaultPawnData;

private:
	// 按顺序执行并清空 StartupJobs，同时汇总各任务权重更新启动进度。
	// Flushes the StartupJobs array. Processes all startup work.
	UE_API void DoAllStartupJobs();

	// 初始化 GameplayCueManager 及其常驻 Cue。
	// Sets up the ability system
	UE_API void InitializeGameplayCueManager();

	// 加载期间周期性接收总体进度，可接入早期启动加载界面。
	// Called periodically during loads, could be used to feed the status to a loading screen
	UE_API void UpdateInitialGameContentLoadPercent(float GameContentPercent);

	// 启动阶段待执行的加权任务队列，用于计算整体加载进度。
	// The list of tasks to execute on startup. Used to track startup progress.
	TArray<FLyraAssetManagerStartupJob> StartupJobs;

private:
	
	// 由 AssetManager 保持强引用的已加载资产集合。
	// Assets loaded and tracked by the asset manager.
	UPROPERTY()
	TSet<TObjectPtr<const UObject>> LoadedAssets;

	// 修改 LoadedAssets 时使用的互斥锁。
	// Used for a scope lock when modifying the list of load assets.
	FCriticalSection LoadedAssetsCritical;
};


template<typename AssetType>
AssetType* ULyraAssetManager::GetAsset(const TSoftObjectPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	AssetType* LoadedAsset = nullptr;

	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();

	if (AssetPath.IsValid())
	{
		LoadedAsset = AssetPointer.Get();
		if (!LoadedAsset)
		{
			LoadedAsset = Cast<AssetType>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedAsset, TEXT("Failed to load asset [%s]"), *AssetPointer.ToString());
		}

		if (LoadedAsset && bKeepInMemory)
		{
			// 将同步加载结果加入 AssetManager 保活列表。
			// Added to loaded asset list.
			Get().AddLoadedAsset(Cast<UObject>(LoadedAsset));
		}
	}

	return LoadedAsset;
}

template<typename AssetType>
TSubclassOf<AssetType> ULyraAssetManager::GetSubclass(const TSoftClassPtr<AssetType>& AssetPointer, bool bKeepInMemory)
{
	TSubclassOf<AssetType> LoadedSubclass;

	const FSoftObjectPath& AssetPath = AssetPointer.ToSoftObjectPath();

	if (AssetPath.IsValid())
	{
		LoadedSubclass = AssetPointer.Get();
		if (!LoadedSubclass)
		{
			LoadedSubclass = Cast<UClass>(SynchronousLoadAsset(AssetPath));
			ensureAlwaysMsgf(LoadedSubclass, TEXT("Failed to load asset class [%s]"), *AssetPointer.ToString());
		}

		if (LoadedSubclass && bKeepInMemory)
		{
			// 将同步加载的 Class 加入 AssetManager 保活列表。
			// Added to loaded asset list.
			Get().AddLoadedAsset(Cast<UObject>(LoadedSubclass));
		}
	}

	return LoadedSubclass;
}

#undef UE_API
