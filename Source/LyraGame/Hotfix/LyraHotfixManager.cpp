// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHotfixManager.h"
#include "UObject/UObjectIterator.h"
#include "Engine/NetDriver.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "Settings/LyraSettingsLocal.h"
#include "HAL/MemoryMisc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHotfixManager)

// 记录本进程成功应用 Game.ini 热修复的累计次数。
int32 ULyraHotfixManager::GameHotfixCounter = 0;

// 注册热修复完成回调，并在非 Shipping 构建中注册屏幕消息提供者。
ULyraHotfixManager::ULyraHotfixManager()
{
#if !UE_BUILD_SHIPPING
	OnScreenMessageHandle = FCoreDelegates::OnGetOnScreenMessages.AddUObject(this, &ULyraHotfixManager::GetOnScreenMessages);
#endif // !UE_BUILD_SHIPPING

	HotfixCompleteDelegateHandle = AddOnHotfixCompleteDelegate_Handle(FOnHotfixCompleteDelegate::CreateUObject(this, &ThisClass::OnHotfixCompleted));
}

// 执行 Online Hotfix Manager 的基础初始化。
void ULyraHotfixManager::Init()
{
	Super::Init();
}

// 热修复完成后重载服务器 NetDriver 的 DDoS 配置，必要时重应用活动设备配置并通知本地设置。
void ULyraHotfixManager::OnHotfixCompleted(EHotfixResult HotfixResult)
{
	// 热修复完成后为所有存活的服务器 NetDriver 重新加载 DDoS 检测配置，与 RepGraph 的处理保持一致。
	// Reload DDoS detection config for all live Net Drivers (mirrors RepGraph code)
	for (TObjectIterator<UNetDriver> It; It; ++It)
	{
		if (It->IsServer())
		{
			UE_LOG(LogHotfixManager, Log, TEXT("Reloading DDoS detection settings for NetDriver: %s"), *It->GetName());

			It->DDoS.InitConfig();
		}
	}

	if (bHasPendingDeviceProfileHotfix)
	{
		UE_LOG(LogHotfixManager, Log, TEXT("Re-applying Hotfixed DeviceProfile"));

		bHasPendingDeviceProfileHotfix = false;
		UDeviceProfileManager::Get().ReapplyDeviceProfile();

		ULyraSettingsLocal* GameSettings = ULyraSettingsLocal::Get();
		GameSettings->OnHotfixDeviceProfileApplied();
	}

#if ENABLE_SHARED_MEMORY_TRACKER
	FSharedMemoryTracker::PrintMemoryDiff(TEXT("Hotfix Complete"));
#endif
}

// 销毁时解除热修复完成委托和非 Shipping 屏幕消息委托。
ULyraHotfixManager::~ULyraHotfixManager()
{
	ClearOnHotfixCompleteDelegate_Handle(HotfixCompleteDelegateHandle);

#if !UE_BUILD_SHIPPING
	FCoreDelegates::OnGetOnScreenMessages.Remove(OnScreenMessageHandle);
#endif // !UE_BUILD_SHIPPING
}

// 除基类支持文件外，额外接受 AssetMigrations.ini，并在非 Shipping 构建中兼容调试前缀。
bool ULyraHotfixManager::WantsHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	bool bWantsProcessing = Super::WantsHotfixProcessing(FileHeader);
	if (!bWantsProcessing)
	{
		FString SupportedFiles[] = {
			TEXT("AssetMigrations.ini")
		};

		for (FString SupportedFile : SupportedFiles)
		{
#if !UE_BUILD_SHIPPING
			if (!DebugPrefix.IsEmpty())
			{
				SupportedFile = DebugPrefix + SupportedFile;
			}
#endif
			if (SupportedFile == FileHeader.FileName)
			{
				bWantsProcessing = true;
				break;
			}
		}
	}
	return bWantsProcessing;
}

// 解析 DeviceProfiles.ini 热修复涉及的配置名，记录活动配置是否受影响，再交由基类合并 INI。
bool ULyraHotfixManager::HotfixIniFile(const FString& FileName, const FString& IniData)
{
	if (!bHasPendingDeviceProfileHotfix && FileName.EndsWith(TEXT("DEVICEPROFILES.INI"), ESearchCase::IgnoreCase))
	{
		FConfigFile DeviceProfileHotfixConfig;
		DeviceProfileHotfixConfig.CombineFromBuffer(IniData, FileName);
		TSet<FString> Keys;
		for (const auto& DPSection : AsConst(DeviceProfileHotfixConfig))
		{
			FString DeviceProfileName, DeviceProfileClass;
			if (DPSection.Key.Split(TEXT(" "), &DeviceProfileName, &DeviceProfileClass) && DeviceProfileClass == *UDeviceProfile::StaticClass()->GetName())
			{
				Keys.Add(DeviceProfileName);
			}
		}

		// 仅当热修复涉及当前活动设备配置直接或间接引用的配置时，才在完成后重新应用设备配置。
		// Check if any of the hotfixed device profiles are referenced by the currently active profile(s):
		bHasPendingDeviceProfileHotfix = UDeviceProfileManager::Get().DoActiveProfilesReference(Keys);
		UE_LOG(LogHotfixManager, Log, TEXT("Active device profile was referenced by hotfix = %d"), (uint32)bHasPendingDeviceProfileHotfix);
	}

	return Super::HotfixIniFile(FileName, IniData);
}

// 直接接受 JSON 下载；其他文件由基类处理，成功应用 Game.ini 时递增计数并清除待处理状态。
bool ULyraHotfixManager::ApplyHotfixProcessing(const FCloudFileHeader& FileHeader)
{
	// 显式接受 JSON 文件，使热修复系统可以自动下载它们；其他文件仍交由父类判断和处理。
	// This allows json files to be downloaded automatically
	const FString Extension = FPaths::GetExtension(FileHeader.FileName);
	if (Extension == TEXT("json"))
	{
		return true;
	}
	
	const bool bResult = Super::ApplyHotfixProcessing(FileHeader);
	if (bResult && FileHeader.FileName.EndsWith(TEXT("GAME.INI"), ESearchCase::IgnoreCase))
	{
		GameHotfixCounter++;

		if (bHasPendingGameHotfix)
		{
			bHasPendingGameHotfix = false;
			OnPendingGameHotfixChanged.Broadcast(bHasPendingGameHotfix);
		}
	}

	return bResult;
}

// 仅对 /Engine/ 和 /Game/ 资产路径在 INI 补丁找不到对象时发出警告。
bool ULyraHotfixManager::ShouldWarnAboutMissingWhenPatchingFromIni(const FString& AssetPath) const
{
	return AssetPath.StartsWith(TEXT("/Engine/")) || AssetPath.StartsWith(TEXT("/Game/"));
}

// 调用基类按 INI 修补已加载资产，并在启用共享内存追踪时记录前后内存差异。
void ULyraHotfixManager::PatchAssetsFromIniFiles()
{
#if ENABLE_SHARED_MEMORY_TRACKER
	FSharedMemoryTracker::PrintMemoryDiff(TEXT("Start - PatchAssetsFromIniFiles"));
#endif

	Super::PatchAssetsFromIniFiles();

#if ENABLE_SHARED_MEMORY_TRACKER
	FSharedMemoryTracker::PrintMemoryDiff(TEXT("End - PatchAssetsFromIniFiles"));
#endif
}

// 检查待变更文件是否包含 Game.ini，首次发现时标记存在待处理游戏热修复并广播状态。
void ULyraHotfixManager::OnHotfixAvailablityCheck(const TArray<FCloudFileHeader>& PendingChangedFiles, const TArray<FCloudFileHeader>& PendingRemoveFiles)
{
	bool bNewPendingGameHotfix = false;
	for (int32 Idx = 0; Idx < PendingChangedFiles.Num(); Idx++)
	{
		if (PendingChangedFiles[Idx].FileName.EndsWith(TEXT("GAME.INI"), ESearchCase::IgnoreCase))
		{
			bNewPendingGameHotfix = true;
			break;
		}
	}

	if (bNewPendingGameHotfix && !bHasPendingGameHotfix)
	{
		bHasPendingGameHotfix = true;
		OnPendingGameHotfixChanged.Broadcast(bHasPendingGameHotfix);
	}
}

#if !UE_BUILD_SHIPPING
// 非 Shipping 构建的热修复屏幕消息扩展点；当前未添加任何消息。
void ULyraHotfixManager::GetOnScreenMessages(TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages)
{
	// TODO：在非 Shipping 构建中向屏幕输出待处理热修复或错误状态。
	// TODO Any messages/errors.
}
#endif // !UE_BUILD_SHIPPING

// 把插件变化触发的资产重补丁请求合并到下一次核心 Ticker，避免同帧重复执行。
void ULyraHotfixManager::RequestPatchAssetsFromIniFiles()
{
	if (!RequestPatchAssetsHandle.IsValid())
	{
		RequestPatchAssetsHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) {
			RequestPatchAssetsHandle.Reset();
			UE_LOG(LogHotfixManager, Display, TEXT("Hotfix manager re-calling PatchAssetsFromIniFiles due to new plugins"));
			PatchAssetsFromIniFiles();
			return false;
		}));
	}
}

// 编辑器中跳过远程热修复并以无变化成功结束；运行环境调用基类启动流程。
void ULyraHotfixManager::StartHotfixProcess()
{
	if (GIsEditor)
	{
		UE_LOG(LogHotfixManager, Display, TEXT("Hotfixing skipped in development mode."));
		FinalizeHotfixProcess(EHotfixResult::SuccessNoChange);
		return;
	}

#if ENABLE_SHARED_MEMORY_TRACKER
	FSharedMemoryTracker::PrintMemoryDiff(TEXT("StartHotfixProcess"));
#endif

	Super::StartHotfixProcess();
}

