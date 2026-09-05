// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAssetManager.h"
#include "LyraLogChannels.h"
#include "LyraGameplayTags.h"
#include "LyraGameData.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnData.h"
#include "Misc/App.h"
#include "Stats/StatsMisc.h"
#include "Engine/Engine.h"
#include "AbilitySystem/LyraGameplayCueManager.h"
#include "Misc/ScopedSlowTask.h"
#include "System/LyraAssetManagerStartupJob.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAssetManager)

// 标识装备状态资源依赖的 Asset Bundle 名称。
const FName FLyraBundles::Equipped("Equipped");

//////////////////////////////////////////////////////////////////////

// 注册控制台命令，输出由 LyraAssetManager 显式保活的资产集合。
static FAutoConsoleCommand CVarDumpLoadedAssets(
	TEXT("Lyra.DumpLoadedAssets"),
	TEXT("Shows all assets that were loaded via the asset manager and are currently in memory."),
	FConsoleCommandDelegate::CreateStatic(ULyraAssetManager::DumpLoadedAssets)
);

//////////////////////////////////////////////////////////////////////

// 将启动表达式包装为带名称和权重的任务，供统一执行和汇总加载进度。
#define STARTUP_JOB_WEIGHTED(JobFunc, JobWeight) StartupJobs.Add(FLyraAssetManagerStartupJob(#JobFunc, [this](const FLyraAssetManagerStartupJob& StartupJob, TSharedPtr<FStreamableHandle>& LoadHandle){JobFunc;}, JobWeight))
// 使用默认权重 1 添加启动任务。
#define STARTUP_JOB(JobFunc) STARTUP_JOB_WEIGHTED(JobFunc, 1.f)

//////////////////////////////////////////////////////////////////////

// 初始化项目 AssetManager，并将默认 PawnData 缓存置空以等待配置或加载流程填充。
ULyraAssetManager::ULyraAssetManager()
{
	DefaultPawnData = nullptr;
}

// 从 GEngine 取得配置的 LyraAssetManager 单例；配置类型错误时以 Fatal 明确终止。
ULyraAssetManager& ULyraAssetManager::Get()
{
	check(GEngine);

	if (ULyraAssetManager* Singleton = Cast<ULyraAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogLyra, Fatal, TEXT("Invalid AssetManagerClassName in DefaultEngine.ini.  It must be set to LyraAssetManager!"));

	// 上方 Fatal 正常情况下会终止进程，此返回值仅用于满足编译器控制流分析。
	// Fatal error above prevents this from being called.
	return *NewObject<ULyraAssetManager>();
}

// 同步解析有效软路径：初始化后使用 StreamableManager，早期启动阶段回退到 TryLoad，并可选记录耗时。
UObject* ULyraAssetManager::SynchronousLoadAsset(const FSoftObjectPath& AssetPath)
{
	if (AssetPath.IsValid())
	{
		TUniquePtr<FScopeLogTime> LogTimePtr;

		if (ShouldLogAssetLoads())
		{
			LogTimePtr = MakeUnique<FScopeLogTime>(*FString::Printf(TEXT("Synchronously loaded asset [%s]"), *AssetPath.ToString()), nullptr, FScopeLogTime::ScopeLog_Seconds);
		}

		if (UAssetManager::IsInitialized())
		{
			return UAssetManager::GetStreamableManager().LoadSynchronous(AssetPath, false);
		}

		// AssetManager 尚未完成初始化时，直接通过软路径同步加载对象。
		// Use LoadObject if asset manager isn't ready yet.
		return AssetPath.TryLoad();
	}

	return nullptr;
}

// 首次调用时缓存命令行是否包含 LogAssetLoads，用于控制同步加载耗时日志。
bool ULyraAssetManager::ShouldLogAssetLoads()
{
	static bool bLogAssetLoads = FParse::Param(FCommandLine::Get(), TEXT("LogAssetLoads"));
	return bLogAssetLoads;
}

// 在线程安全锁保护下将有效资产加入强引用池，防止其在项目使用期间被垃圾回收。
void ULyraAssetManager::AddLoadedAsset(const UObject* Asset)
{
	if (ensureAlways(Asset))
	{
		FScopeLock LoadedAssetsLock(&LoadedAssetsCritical);
		LoadedAssets.Add(Asset);
	}
}

// 将强引用池中当前保活的全部资产及总数写入 Lyra 日志。
void ULyraAssetManager::DumpLoadedAssets()
{
	UE_LOG(LogLyra, Log, TEXT("========== Start Dumping Loaded Assets =========="));

	for (const UObject* LoadedAsset : Get().LoadedAssets)
	{
		UE_LOG(LogLyra, Log, TEXT("  %s"), *GetNameSafe(LoadedAsset));
	}

	UE_LOG(LogLyra, Log, TEXT("... %d assets in loaded pool"), Get().LoadedAssets.Num());
	UE_LOG(LogLyra, Log, TEXT("========== Finish Dumping Loaded Assets =========="));
}

// 完成 PrimaryAsset 扫描，排入 GameplayCue 与核心 GameData 启动任务，并同步执行整组任务。
void ULyraAssetManager::StartInitialLoading()
{
	SCOPED_BOOT_TIMING("ULyraAssetManager::StartInitialLoading");

	// Super 会完成 PrimaryAsset 扫描；即使后续加载延迟，也必须在此时先建立资产索引。
	// This does all of the scanning, need to do this now even if loads are deferred
	Super::StartInitialLoading();

	STARTUP_JOB(InitializeGameplayCueManager());

	{
		// 将全局 GameData 作为高权重启动任务加载。
		// Load base game data asset
		STARTUP_JOB_WEIGHTED(GetGameData(), 25.f);
	}

	// 执行全部启动任务并持续汇总加载进度。
	// Run all the queued up startup jobs
	DoAllStartupJobs();
}

// 取得 LyraGameplayCueManager 并加载配置为始终驻留的 GameplayCue 资源。
void ULyraAssetManager::InitializeGameplayCueManager()
{
	SCOPED_BOOT_TIMING("ULyraAssetManager::InitializeGameplayCueManager");

	ULyraGameplayCueManager* GCM = ULyraGameplayCueManager::Get();
	check(GCM);
	GCM->LoadAlwaysLoadedCues();
}


// 按配置软路径获取或加载全局 LyraGameData，并复用 AssetManager 的类型缓存。
const ULyraGameData& ULyraAssetManager::GetGameData()
{
	return GetOrLoadTypedGameData<ULyraGameData>(LyraGameDataPath);
}

// 解析默认 PawnData 软引用，必要时同步加载，并按 GetAsset 默认行为加入保活池。
const ULyraPawnData* ULyraAssetManager::GetDefaultPawnData() const
{
	return GetAsset(DefaultPawnData);
}

// 加载指定类型的核心 GameData 及其 PrimaryAsset 依赖、写入类型缓存，并在缺失时以 Fatal 阻止带错误数据运行。
UPrimaryDataAsset* ULyraAssetManager::LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass, const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath, FPrimaryAssetType PrimaryAssetType)
{
	UPrimaryDataAsset* Asset = nullptr;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading GameData Object"), STAT_GameData, STATGROUP_LoadTime);
	if (!DataClassPath.IsNull())
	{
#if WITH_EDITOR
		FScopedSlowTask SlowTask(0, FText::Format(NSLOCTEXT("LyraEditor", "BeginLoadingGameDataTask", "Loading GameData {0}"), FText::FromName(DataClass->GetFName())));
		const bool bShowCancelButton = false;
		const bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton, bAllowInPIE);
#endif
		UE_LOG(LogLyra, Log, TEXT("Loading GameData: %s ..."), *DataClassPath.ToString());
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("    ... GameData loaded!"), nullptr);

		// 编辑器中 PostLoad 可能按需递归进入此函数，因此先同步加载 PrimaryAsset 本体，再异步加载其 Bundle 依赖。
		// This can be called recursively in the editor because it is called on demand from PostLoad so force a sync load for primary asset and async load the rest in that case
		if (GIsEditor)
		{
			Asset = DataClassPath.LoadSynchronous();
			LoadPrimaryAssetsWithType(PrimaryAssetType);
		}
		else
		{
			TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssetsWithType(PrimaryAssetType);
			if (Handle.IsValid())
			{
				Handle->WaitUntilComplete(0.0f, false);

				// Handle 完成后应始终能取得对应 PrimaryDataAsset。
				// This should always work
				Asset = Cast<UPrimaryDataAsset>(Handle->GetLoadedAsset());
			}
		}
	}

	if (Asset)
	{
		GameDataMap.Add(DataClass, Asset);
	}
	else
	{
		// GameData 是不可缺失的核心依赖；加载失败会引发难以定位的连锁软故障，因此直接 Fatal。
		// It is not acceptable to fail to load any GameData asset. It will result in soft failures that are hard to diagnose.
		UE_LOG(LogLyra, Fatal, TEXT("Failed to load GameData asset at %s. Type %s. This is not recoverable and likely means you do not have the correct data to run %s."), *DataClassPath.ToString(), *PrimaryAssetType.ToString(), FApp::GetProjectName());
	}

	return Asset;
}


// 依次执行启动任务：专用服务器直接等待，客户端按权重和子步骤汇总进度，完成后清空任务队列。
void ULyraAssetManager::DoAllStartupJobs()
{
	SCOPED_BOOT_TIMING("ULyraAssetManager::DoAllStartupJobs");
	const double AllStartupJobsStartTime = FPlatformTime::Seconds();

	if (IsRunningDedicatedServer())
	{
		// 非命令行环境无需轮询进度，直接依次执行任务并等待异步 Handle。
		// No need for periodic progress updates, just run the jobs
		for (const FLyraAssetManagerStartupJob& StartupJob : StartupJobs)
		{
			StartupJob.DoJob();
		}
	}
	else
	{
		if (StartupJobs.Num() > 0)
		{
			float TotalJobValue = 0.0f;
			for (const FLyraAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				TotalJobValue += StartupJob.JobWeight;
			}

			float AccumulatedJobValue = 0.0f;
			for (FLyraAssetManagerStartupJob& StartupJob : StartupJobs)
			{
				const float JobValue = StartupJob.JobWeight;
				StartupJob.SubstepProgressDelegate.BindLambda([This = this, AccumulatedJobValue, JobValue, TotalJobValue](float NewProgress)
					{
						const float SubstepAdjustment = FMath::Clamp(NewProgress, 0.0f, 1.0f) * JobValue;
						const float OverallPercentWithSubstep = (AccumulatedJobValue + SubstepAdjustment) / TotalJobValue;

						This->UpdateInitialGameContentLoadPercent(OverallPercentWithSubstep);
					});

				StartupJob.DoJob();

				StartupJob.SubstepProgressDelegate.Unbind();

				AccumulatedJobValue += JobValue;

				UpdateInitialGameContentLoadPercent(AccumulatedJobValue / TotalJobValue);
			}
		}
		else
		{
			UpdateInitialGameContentLoadPercent(1.0f);
		}
	}

	StartupJobs.Empty();

	UE_LOG(LogLyra, Display, TEXT("All startup jobs took %.2f seconds to complete"), FPlatformTime::Seconds() - AllStartupJobsStartTime);
}

// 接收启动任务汇总进度，当前保留为连接早期启动加载界面的项目扩展点。
void ULyraAssetManager::UpdateInitialGameContentLoadPercent(float GameContentPercent)
{
	// 可将该总体进度转发给早期启动加载界面。
	// Could route this to the early startup loading screen
}

#if WITH_EDITOR
// PIE 开始前确保核心 GameData 已加载，并保留预加载目标 Experience 依赖的编辑器入口。
void ULyraAssetManager::PreBeginPIE(bool bStartSimulate)
{
	Super::PreBeginPIE(bStartSimulate);

	{
		FScopedSlowTask SlowTask(0, NSLOCTEXT("LyraEditor", "BeginLoadingPIEData", "Loading PIE Data"));
		const bool bShowCancelButton = false;
		const bool bAllowInPIE = true;
		SlowTask.MakeDialog(bShowCancelButton, bAllowInPIE);

		const ULyraGameData& LocalGameDataCommon = GetGameData();

		// 计时作用域刻意放在 GetGameData 之后，避免把 GameData 加载时间计入 PIE 预加载统计。
		// Intentionally after GetGameData to avoid counting GameData time in this timer
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("PreBeginPIE asset preloading complete"), nullptr);

		// 可在此预加载即将使用的 Experience 相关资产，例如综合 WorldSettings 默认值和开发者设置覆盖来确定目标。
		// You could add preloading of anything else needed for the experience we'll be using here
		// (e.g., by grabbing the default experience from the world settings + the experience override in developer settings)
	}
}
#endif
