// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingsLocal.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "CommonInputSubsystem.h"
#include "GenericPlatform/GenericPlatformFramePacer.h"
#include "Player/LyraLocalPlayer.h"
#include "Performance/LatencyMarkerModule.h"
#include "Performance/LyraPerformanceStatTypes.h"
#include "ICommonUIModule.h"
#include "CommonUISettings.h"
#include "SoundControlBusMix.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Performance/LyraPerformanceSettings.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "HAL/PlatformFramePacer.h"
#include "Development/LyraPlatformEmulationSettings.h"
#include "SoundControlBus.h"
#include "AudioModulationStatics.h"
#include "Audio/LyraAudioSettings.h"
#include "Audio/LyraAudioMixEffectsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingsLocal)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_BinauralSettingControlledByOS, "Platform.Trait.BinauralSettingControlledByOS");

namespace PerfStatTags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_SupportsLatencyStats, "Platform.Trait.SupportsLatencyStats");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_SupportsLatencyMarkers, "Platform.Trait.SupportsLatencyMarkers");
}

//////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
// 控制 PIE 中是否应用用户帧率限制。
static TAutoConsoleVariable<bool> CVarApplyFrameRateSettingsInPIE(TEXT("Lyra.Settings.ApplyFrameRateSettingsInPIE"),
	false,
	TEXT("Should we apply frame rate settings in PIE?"),
	ECVF_Default);

// 控制 PIE 中是否应用前端菜单专用性能策略。
static TAutoConsoleVariable<bool> CVarApplyFrontEndPerformanceOptionsInPIE(TEXT("Lyra.Settings.ApplyFrontEndPerformanceOptionsInPIE"),
	false,
	TEXT("Do we apply front-end specific performance options in PIE?"),
	ECVF_Default);

// 控制 PIE 中是否应用体验或平台模拟产生的设备配置。
static TAutoConsoleVariable<bool> CVarApplyDeviceProfilesInPIE(TEXT("Lyra.Settings.ApplyDeviceProfilesInPIE"),
	false,
	TEXT("Should we apply experience/platform emulated device profiles in PIE?"),
	ECVF_Default);
#endif

//////////////////////////////////////////////////////////////////////
// 主机式帧率节奏由设备配置决定帧同步方式与目标刷新率。
// Console frame pacing

// 保存设备配置驱动的主机目标 FPS，负值表示未指定。
static TAutoConsoleVariable<int32> CVarDeviceProfileDrivenTargetFps(
	TEXT("Lyra.DeviceProfile.Console.TargetFPS"),
	-1,
	TEXT("Target FPS when being driven by device profile"),
	ECVF_Default | ECVF_Preview);

// 保存设备配置驱动的主机帧同步类型，对应 r.GTSyncType。
static TAutoConsoleVariable<int32> CVarDeviceProfileDrivenFrameSyncType(
	TEXT("Lyra.DeviceProfile.Console.FrameSyncType"),
	-1,
	TEXT("Sync type when being driven by device profile. Corresponds to r.GTSyncType"),
	ECVF_Default | ECVF_Preview);

//////////////////////////////////////////////////////////////////////
// 移动端帧率节奏使用平台支持的离散 FPS 档位，并按档位约束画质。
// Mobile frame pacing

// 保存设备配置指定的移动端默认帧率。
static TAutoConsoleVariable<int32> CVarDeviceProfileDrivenMobileDefaultFrameRate(
	TEXT("Lyra.DeviceProfile.Mobile.DefaultFrameRate"),
	30,
	TEXT("Default FPS when being driven by device profile"),
	ECVF_Default | ECVF_Preview);

// 保存设备配置允许的移动端最高帧率。
static TAutoConsoleVariable<int32> CVarDeviceProfileDrivenMobileMaxFrameRate(
	TEXT("Lyra.DeviceProfile.Mobile.MaxFrameRate"),
	120,
	TEXT("Max FPS when being driven by device profile"),
	ECVF_Default | ECVF_Preview);

//////////////////////////////////////////////////////////////////////

// 保存按 FPS 阈值配置的移动端整体画质上限表。
static TAutoConsoleVariable<FString> CVarMobileQualityLimits(
	TEXT("Lyra.DeviceProfile.Mobile.OverallQualityLimits"),
	TEXT(""),
	TEXT("List of limits on resolution quality of the form \"FPS:MaxQuality,FPS2:MaxQuality2,...\", kicking in when FPS is at or above the threshold"),
	ECVF_Default | ECVF_Preview);

// 保存按 FPS 阈值配置的移动端分辨率画质上限表。
static TAutoConsoleVariable<FString> CVarMobileResolutionQualityLimits(
	TEXT("Lyra.DeviceProfile.Mobile.ResolutionQualityLimits"),
	TEXT(""),
	TEXT("List of limits on resolution quality of the form \"FPS:MaxResQuality,FPS2:MaxResQuality2,...\", kicking in when FPS is at or above the threshold"),
	ECVF_Default | ECVF_Preview);

// 保存按 FPS 阈值配置的移动端分辨率画质建议表。
static TAutoConsoleVariable<FString> CVarMobileResolutionQualityRecommendation(
	TEXT("Lyra.DeviceProfile.Mobile.ResolutionQualityRecommendation"),
	TEXT("0:75"),
	TEXT("List of limits on resolution quality of the form \"FPS:Recommendation,FPS2:Recommendation2,...\", kicking in when FPS is at or above the threshold"),
	ECVF_Default | ECVF_Preview);

//////////////////////////////////////////////////////////////////////

// 将所有可伸缩性通道初始化为未覆盖状态，供后续从设备配置填充。
FLyraScalabilitySnapshot::FLyraScalabilitySnapshot()
{
	static_assert(sizeof(Scalability::FQualityLevels) == 88, "This function may need to be updated to account for new members");

	Qualities.ResolutionQuality = -1.0f;
	Qualities.ViewDistanceQuality = -1;
	Qualities.AntiAliasingQuality = -1;
	Qualities.ShadowQuality = -1;
	Qualities.GlobalIlluminationQuality = -1;
	Qualities.ReflectionQuality = -1;
	Qualities.PostProcessQuality = -1;
	Qualities.TextureQuality = -1;
	Qualities.EffectsQuality = -1;
	Qualities.FoliageQuality = -1;
	Qualities.ShadingQuality = -1;
}

//////////////////////////////////////////////////////////////////////

template<typename T>
struct TMobileQualityWrapper
{
private:
	T DefaultValue;
	TAutoConsoleVariable<FString>& WatchedVar;
	FString LastSeenCVarString;

	struct FLimitPair
	{
		int32 Limit = 0;
		T Value = T(0);
	};

	TArray<FLimitPair> Thresholds;

public:
	// 保存默认值和受监视的控制台变量，延迟解析移动端画质阈值。
	TMobileQualityWrapper(T InDefaultValue, TAutoConsoleVariable<FString>& InWatchedVar)
		: DefaultValue(InDefaultValue)
		, WatchedVar(InWatchedVar)
	{
	}

	// 返回不高于测试值的最高阈值所对应的配置值；无匹配项时使用默认值。
	T Query(int32 TestValue)
	{
		UpdateCache();

		for (const FLimitPair& Pair : Thresholds)
		{
			if (TestValue >= Pair.Limit)
			{
				return Pair.Value;
			}
		}

		return DefaultValue;
	}

	// 返回数值最低的首个阈值；未配置任何阈值时返回 INDEX_NONE。
	// Returns the first threshold value or INDEX_NONE if there aren't any
	int32 GetFirstThreshold()
	{
		UpdateCache();
		return (Thresholds.Num() > 0) ? Thresholds[0].Limit : INDEX_NONE;
	}

	// 返回所有阈值对中的最小配置值；没有阈值对时返回 DefaultIfNoPairs。
	// Returns the lowest value of all the pairs or DefaultIfNoPairs if there are no pairs
	T GetLowestValue(T DefaultIfNoPairs)
	{
		UpdateCache();
		
		T Result = DefaultIfNoPairs;
		bool bFirstValue = true;
		for (const FLimitPair& Pair : Thresholds)
		{
			if (bFirstValue)
			{
				Result = Pair.Value;
				bFirstValue = false;
			}
			else
			{
				Result = FMath::Min(Result, Pair.Value);
			}
		}
		
		return Result;
	}

private:
	// 控制台变量文本变化时重新解析并排序“阈值:值”配置；格式错误时清空缓存。
	void UpdateCache()
	{
		const FString CurrentValue = WatchedVar.GetValueOnGameThread();
		if (!CurrentValue.Equals(LastSeenCVarString, ESearchCase::CaseSensitive))
		{
			LastSeenCVarString = CurrentValue;

			Thresholds.Reset();

			// 将“阈值:值,阈值:值”形式的控制台变量解析为限制对。
			// Parse the thresholds
			int32 ScanIndex = 0;
			while (ScanIndex < LastSeenCVarString.Len())
			{
				const int32 ColonIndex = LastSeenCVarString.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ScanIndex);
				if (ColonIndex > 0)
				{
					const int32 CommaIndex = LastSeenCVarString.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, ColonIndex);
					const int32 EndOfPairIndex = (CommaIndex != INDEX_NONE) ? CommaIndex : LastSeenCVarString.Len();

					FLimitPair Pair;
					LexFromString(Pair.Limit, *LastSeenCVarString.Mid(ScanIndex, ColonIndex - ScanIndex));
					LexFromString(Pair.Value, *LastSeenCVarString.Mid(ColonIndex + 1, EndOfPairIndex - ColonIndex - 1));
					Thresholds.Add(Pair);

					ScanIndex = EndOfPairIndex + 1;
				}
				else
				{
				
					UE_LOG(LogConsoleResponse, Error, TEXT("Malformed value for '%s'='%s', expecting a ':'"),
						*IConsoleManager::Get().FindConsoleObjectName(WatchedVar.AsVariable()),
						*LastSeenCVarString);
					Thresholds.Reset();
					break;
				}
			}

			// 按阈值从低到高排序，Query 会返回不高于测试值的最高适用档。
			// Sort the pairs
			Thresholds.Sort([](const FLimitPair A, const FLimitPair B) { return A.Limit < B.Limit; });
		}
	}
};

namespace LyraSettingsHelpers
{
	// 检查 CommonUI 平台特性集合是否包含指定标签。
	bool HasPlatformTrait(FGameplayTag Tag)
	{
		return ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(Tag);
	}

	// 返回所有整数型可伸缩性通道中的最高等级，不计入浮点型 ResolutionQuality。
	// Returns the max level from the integer scalability settings (ignores ResolutionQuality)
	int32 GetHighestLevelOfAnyScalabilityChannel(const Scalability::FQualityLevels& ScalabilityQuality)
	{
		static_assert(sizeof(Scalability::FQualityLevels) == 88, "This function may need to be updated to account for new members");

		int32 MaxScalability =						ScalabilityQuality.ViewDistanceQuality;
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.AntiAliasingQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.ShadowQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.GlobalIlluminationQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.ReflectionQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.PostProcessQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.TextureQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.EffectsQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.FoliageQuality);
		MaxScalability = FMath::Max(MaxScalability, ScalabilityQuality.ShadingQuality);

		return (MaxScalability >= 0) ? MaxScalability : -1;
	}

	// 重新读取指定后缀的设备配置可伸缩性覆盖，并记录该快照是否包含有效覆盖。
	void FillScalabilitySettingsFromDeviceProfile(FLyraScalabilitySnapshot& Mode, const FString& Suffix = FString())
	{
		static_assert(sizeof(Scalability::FQualityLevels) == 88, "This function may need to be updated to account for new members");

		// 读取前先恢复默认快照，才能准确区分设备配置未覆盖的通道；测试中可能动态切换设备配置，因此每次都清空并重新填充。
		// Default out before filling so we can correctly mark non-overridden scalability values.
		// It's technically possible to swap device profile when testing so safest to clear and refill
		Mode = FLyraScalabilitySnapshot();

		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.ResolutionQuality%s"), *Suffix), Mode.Qualities.ResolutionQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.ViewDistanceQuality%s"), *Suffix), Mode.Qualities.ViewDistanceQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.AntiAliasingQuality%s"), *Suffix), Mode.Qualities.AntiAliasingQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.ShadowQuality%s"), *Suffix), Mode.Qualities.ShadowQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.GlobalIlluminationQuality%s"), *Suffix), Mode.Qualities.GlobalIlluminationQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.ReflectionQuality%s"), *Suffix), Mode.Qualities.ReflectionQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.PostProcessQuality%s"), *Suffix), Mode.Qualities.PostProcessQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.TextureQuality%s"), *Suffix), Mode.Qualities.TextureQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.EffectsQuality%s"), *Suffix), Mode.Qualities.EffectsQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.FoliageQuality%s"), *Suffix), Mode.Qualities.FoliageQuality);
		Mode.bHasOverrides |= UDeviceProfileManager::GetScalabilityCVar(FString::Printf(TEXT("sg.ShadingQuality%s"), *Suffix), Mode.Qualities.ShadingQuality);
	}

	// 缓存并查询移动端整体画质阈值表。
	TMobileQualityWrapper<int32> OverallQualityLimits(-1, CVarMobileQualityLimits);
	// 缓存并查询移动端分辨率画质上限表。
	TMobileQualityWrapper<float> ResolutionQualityLimits(100.0f, CVarMobileResolutionQualityLimits);
	// 缓存并查询移动端分辨率画质建议表。
	TMobileQualityWrapper<float> ResolutionQualityRecommendations(75.0f, CVarMobileResolutionQualityRecommendation);

	// 返回指定帧率阈值下适用的整体画质上限。
	int32 GetApplicableOverallQualityLimit(int32 FrameRate)
	{
		return OverallQualityLimits.Query(FrameRate);
	}

	// 返回指定帧率阈值下适用的分辨率画质上限。
	float GetApplicableResolutionQualityLimit(int32 FrameRate)
	{
		return ResolutionQualityLimits.Query(FrameRate);
	}

	// 返回指定帧率阈值下建议采用的分辨率画质。
	float GetApplicableResolutionQualityRecommendation(int32 FrameRate)
	{
		return ResolutionQualityRecommendations.Query(FrameRate);
	}

	// 在不超过目标 FPS 的前提下，选择平台支持且兼容整体画质限制的最高移动端帧率，找不到时回退到设备默认值。
	int32 ConstrainFrameRateToBeCompatibleWithOverallQuality(int32 FrameRate, int32 OverallQuality)
	{
		const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
		const TArray<int32>& PossibleRates = PlatformSettings->MobileFrameRateLimits;

		// 在不超过用户目标帧率的前提下，选择平台支持且与目标整体画质限制兼容的最高 FPS 档位。
		// Choose the closest frame rate (without going over) to the user preferred one that is supported and compatible with the desired overall quality
		int32 LimitIndex = PossibleRates.FindLastByPredicate([=](const int32& TestRate)
		{
			const bool bAtOrBelowDesiredRate = (TestRate <= FrameRate);

			const int32 LimitQuality = GetApplicableResolutionQualityLimit(TestRate);
			const bool bQualityDoesntExceedLimit = (LimitQuality < 0) || (OverallQuality <= LimitQuality);
			
			const bool bIsSupported = ULyraSettingsLocal::IsSupportedMobileFramePace(TestRate);

			return bAtOrBelowDesiredRate && bQualityDoesntExceedLimit && bIsSupported;
		});

		return PossibleRates.IsValidIndex(LimitIndex) ? PossibleRates[LimitIndex] : ULyraSettingsLocal::GetDefaultMobileFrameRate();
	}

	// 返回设备配置首次开始限制整体画质的帧率阈值；没有限制时返回 INDEX_NONE。
	// Returns the first frame rate at which overall quality is restricted/limited by the current device profile
	int32 GetFirstFrameRateWithQualityLimit()
	{
		return OverallQualityLimits.GetFirstThreshold();
	}

	// 返回所有帧率阈值对应的最低整体画质上限；没有限制时返回 -1。
	// Returns the lowest quality at which there's a limit on the overall frame rate (or -1 if there is no limit)
	int32 GetLowestQualityWithFrameRateLimit()
	{
		return OverallQualityLimits.GetLowestValue(-1);
	}
}

//////////////////////////////////////////////////////////////////////

// 绑定应用激活状态变化，并按平台能力初始化细粒度画质和延迟统计默认值。
ULyraSettingsLocal::ULyraSettingsLocal()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && FSlateApplication::IsInitialized())
	{
		OnApplicationActivationStateChangedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ThisClass::OnAppActivationStateChanged);
	}

	bEnableScalabilitySettings = ULyraPlatformSpecificRenderingSettings::Get()->bSupportsGranularVideoQualitySettings;

	SetToDefaults();
}

// 恢复本地设置默认值，并从平台配置取得默认帧率、音频及设备配置选项。
void ULyraSettingsLocal::SetToDefaults()
{
	Super::SetToDefaults();

	bUseHeadphoneMode = false;
	bUseHDRAudioMode = false;
	bSoundControlBusMixLoaded = false;
	bEnableLatencyTrackingStats = ULyraSettingsLocal::DoesPlatformSupportLatencyTrackingStats();

	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	UserChosenDeviceProfileSuffix = PlatformSettings->DefaultDeviceProfileSuffix;
	DesiredUserChosenDeviceProfileSuffix = UserChosenDeviceProfileSuffix;

	FrameRateLimit_InMenu = 144.0f;
	FrameRateLimit_WhenBackgrounded = 30.0f;
	FrameRateLimit_OnBattery = 60.0f;

	MobileFrameRateLimit = GetDefaultMobileFrameRate();
	DesiredMobileFrameRateLimit = MobileFrameRateLimit;
}

// 加载本地配置，校正移动端画质与帧率限制，并应用音频和设备配置相关状态。
void ULyraSettingsLocal::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);

	// 主机式平台通过 rhi.SyncInterval 和帧节奏器限帧，因此关闭父类基于 FrameRateLimit 的限帧。
	// Console platforms use rhi.SyncInterval to limit framerate
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::ConsoleStyle)
	{
		FrameRateLimit = 0.0f;
	}

	// 将存档中的耳机模式作为期望值重新应用到 HRTF 控制台变量。
	// Enable HRTF if needed
	bDesiredHeadphoneMode = bUseHeadphoneMode;
	SetHeadphoneModeEnabled(bUseHeadphoneMode);

	ApplyLatencyTrackingStatSetting();

	DesiredUserChosenDeviceProfileSuffix = UserChosenDeviceProfileSuffix;

	LyraSettingsHelpers::FillScalabilitySettingsFromDeviceProfile(DeviceDefaultScalabilitySettings);

	DesiredMobileFrameRateLimit = MobileFrameRateLimit;
	ClampMobileQuality();

	
	PerfStatSettingsChangedEvent.Broadcast();
}

// 从当前引擎状态回填设置对象，并重新应用非分辨率设置。
void ULyraSettingsLocal::ResetToCurrentSettings()
{
	Super::ResetToCurrentSettings();

	bDesiredHeadphoneMode = bUseHeadphoneMode;

	UserChosenDeviceProfileSuffix = DesiredUserChosenDeviceProfileSuffix;

	MobileFrameRateLimit = DesiredMobileFrameRateLimit;
}

// 对象销毁前解除应用激活状态监听。
void ULyraSettingsLocal::BeginDestroy()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(OnApplicationActivationStateChangedHandle);
	}

	Super::BeginDestroy();
}

// 返回全局 Lyra 本地用户设置实例。
ULyraSettingsLocal* ULyraSettingsLocal::Get()
{
	return GEngine ? CastChecked<ULyraSettingsLocal>(GEngine->GetGameUserSettings()) : nullptr;
}

// 确认当前视频模式，并同步保存已确认的窗口模式、分辨率和刷新率。
void ULyraSettingsLocal::ConfirmVideoMode()
{
	Super::ConfirmVideoMode();

	SetMobileFPSMode(DesiredMobileFrameRateLimit);
}

// 合并两个帧率上限：正数取较小值，值小于等于 0 表示不限帧并让另一项决定结果。
// Combines two limits, always taking the minimum of the two (with special handling for values of <= 0 meaning unlimited)
float CombineFrameRateLimits(float Limit1, float Limit2)
{
	if (Limit1 <= 0.0f)
	{
		return Limit2;
	}
	else if (Limit2 <= 0.0f)
	{
		return Limit1;
	}
	else
	{
		return FMath::Min(Limit1, Limit2);
	}
}

// 综合常驻、菜单、电池及后台限制返回当前有效帧率上限；编辑器中可按控制台变量跳过附加限制。
float ULyraSettingsLocal::GetEffectiveFrameRateLimit()
{
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();

#if WITH_EDITOR
	if (GIsEditor && !CVarApplyFrameRateSettingsInPIE.GetValueOnGameThread())
	{
		return Super::GetEffectiveFrameRateLimit();
	}
#endif

	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::ConsoleStyle)
	{
		return 0.0f;
	}

	float EffectiveFrameRateLimit = Super::GetEffectiveFrameRateLimit();

	if (ShouldUseFrontendPerformanceSettings())
	{
		EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, FrameRateLimit_InMenu);
	}

	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::DesktopStyle)
	{
		if (FPlatformMisc::IsRunningOnBattery())
		{
			EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, FrameRateLimit_OnBattery);
		}

		if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().IsActive())
		{
			EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, FrameRateLimit_WhenBackgrounded);
		}

		if (GetDynamicResolutionFrameRateTarget() != 0.0f)
		{
			EffectiveFrameRateLimit = CombineFrameRateLimits(EffectiveFrameRateLimit, GetDynamicResolutionFrameRateTarget());
		}
	}

 	return EffectiveFrameRateLimit;
}

// 返回当前可伸缩性设置中各整数画质通道的最高等级。
int32 ULyraSettingsLocal::GetHighestLevelOfAnyScalabilityChannel() const
{
	return LyraSettingsHelpers::GetHighestLevelOfAnyScalabilityChannel(ScalabilityQuality);
}

// 将活动可伸缩性快照中有效的覆盖值写入目标画质等级。
void ULyraSettingsLocal::OverrideQualityLevelsToScalabilityMode(const FLyraScalabilitySnapshot& InMode, Scalability::FQualityLevels& InOutLevels)
{
	static_assert(sizeof(Scalability::FQualityLevels) == 88, "This function may need to be updated to account for new members");

	// 对快照中有效的非负通道执行强制覆盖，未配置的负值通道保留输入值。
	// Overrides any valid (non-negative) settings
	InOutLevels.ResolutionQuality = (InMode.Qualities.ResolutionQuality >= 0.f) ? InMode.Qualities.ResolutionQuality : InOutLevels.ResolutionQuality;
	InOutLevels.ViewDistanceQuality = (InMode.Qualities.ViewDistanceQuality >= 0) ? InMode.Qualities.ViewDistanceQuality : InOutLevels.ViewDistanceQuality;
	InOutLevels.AntiAliasingQuality = (InMode.Qualities.AntiAliasingQuality >= 0) ? InMode.Qualities.AntiAliasingQuality : InOutLevels.AntiAliasingQuality;
	InOutLevels.ShadowQuality = (InMode.Qualities.ShadowQuality >= 0) ? InMode.Qualities.ShadowQuality : InOutLevels.ShadowQuality;
	InOutLevels.GlobalIlluminationQuality = (InMode.Qualities.GlobalIlluminationQuality >= 0) ? InMode.Qualities.GlobalIlluminationQuality : InOutLevels.GlobalIlluminationQuality;
	InOutLevels.ReflectionQuality = (InMode.Qualities.ReflectionQuality >= 0) ? InMode.Qualities.ReflectionQuality : InOutLevels.ReflectionQuality;
	InOutLevels.PostProcessQuality = (InMode.Qualities.PostProcessQuality >= 0) ? InMode.Qualities.PostProcessQuality : InOutLevels.PostProcessQuality;
	InOutLevels.TextureQuality = (InMode.Qualities.TextureQuality >= 0) ? InMode.Qualities.TextureQuality : InOutLevels.TextureQuality;
	InOutLevels.EffectsQuality = (InMode.Qualities.EffectsQuality >= 0) ? InMode.Qualities.EffectsQuality : InOutLevels.EffectsQuality;
	InOutLevels.FoliageQuality = (InMode.Qualities.FoliageQuality >= 0) ? InMode.Qualities.FoliageQuality : InOutLevels.FoliageQuality;
	InOutLevels.ShadingQuality = (InMode.Qualities.ShadingQuality >= 0) ? InMode.Qualities.ShadingQuality : InOutLevels.ShadingQuality;
}

// 将各画质通道限制在设备配置允许的最高等级内。
void ULyraSettingsLocal::ClampQualityLevelsToDeviceProfile(const Scalability::FQualityLevels& ClampLevels, Scalability::FQualityLevels& InOutLevels)
{
	static_assert(sizeof(Scalability::FQualityLevels) == 88, "This function may need to be updated to account for new members");

	// 以每个有效的非负设备配置值作为上限，低于上限的用户画质选择仍予保留。
	// Clamps any valid (non-negative) settings
	InOutLevels.ResolutionQuality = (ClampLevels.ResolutionQuality >= 0.f) ? FMath::Min(ClampLevels.ResolutionQuality, InOutLevels.ResolutionQuality) : InOutLevels.ResolutionQuality;
	InOutLevels.ViewDistanceQuality = (ClampLevels.ViewDistanceQuality >= 0) ? FMath::Min(ClampLevels.ViewDistanceQuality, InOutLevels.ViewDistanceQuality) : InOutLevels.ViewDistanceQuality;
	InOutLevels.AntiAliasingQuality = (ClampLevels.AntiAliasingQuality >= 0) ? FMath::Min(ClampLevels.AntiAliasingQuality, InOutLevels.AntiAliasingQuality) : InOutLevels.AntiAliasingQuality;
	InOutLevels.ShadowQuality = (ClampLevels.ShadowQuality >= 0) ? FMath::Min(ClampLevels.ShadowQuality, InOutLevels.ShadowQuality) : InOutLevels.ShadowQuality;
	InOutLevels.GlobalIlluminationQuality = (ClampLevels.GlobalIlluminationQuality >= 0) ? FMath::Min(ClampLevels.GlobalIlluminationQuality, InOutLevels.GlobalIlluminationQuality) : InOutLevels.GlobalIlluminationQuality;
	InOutLevels.ReflectionQuality = (ClampLevels.ReflectionQuality >= 0) ? FMath::Min(ClampLevels.ReflectionQuality, InOutLevels.ReflectionQuality) : InOutLevels.ReflectionQuality;
	InOutLevels.PostProcessQuality = (ClampLevels.PostProcessQuality >= 0) ? FMath::Min(ClampLevels.PostProcessQuality, InOutLevels.PostProcessQuality) : InOutLevels.PostProcessQuality;
	InOutLevels.TextureQuality = (ClampLevels.TextureQuality >= 0) ? FMath::Min(ClampLevels.TextureQuality, InOutLevels.TextureQuality) : InOutLevels.TextureQuality;
	InOutLevels.EffectsQuality = (ClampLevels.EffectsQuality >= 0) ? FMath::Min(ClampLevels.EffectsQuality, InOutLevels.EffectsQuality) : InOutLevels.EffectsQuality;
	InOutLevels.FoliageQuality = (ClampLevels.FoliageQuality >= 0) ? FMath::Min(ClampLevels.FoliageQuality, InOutLevels.FoliageQuality) : InOutLevels.FoliageQuality;
	InOutLevels.ShadingQuality = (ClampLevels.ShadingQuality >= 0) ? FMath::Min(ClampLevels.ShadingQuality, InOutLevels.ShadingQuality) : InOutLevels.ShadingQuality;
}

// 体验加载完成后重新应用可能受设备配置影响的设置。
void ULyraSettingsLocal::OnExperienceLoaded()
{
	ReapplyThingsDueToPossibleDeviceProfileChange();
}

// 热修复设备配置生效后重新应用依赖设备配置的设置。
void ULyraSettingsLocal::OnHotfixDeviceProfileApplied()
{
	ReapplyThingsDueToPossibleDeviceProfileChange();
}

// 重新读取设备配置画质覆盖并应用非分辨率设置。
void ULyraSettingsLocal::ReapplyThingsDueToPossibleDeviceProfileChange()
{
	ApplyNonResolutionSettings();
}

// 更新是否采用前端菜单性能策略，并在状态变化时重算有效帧率上限。
void ULyraSettingsLocal::SetShouldUseFrontendPerformanceSettings(bool bInFrontEnd)
{
	bInFrontEndForPerformancePurposes = bInFrontEnd;
	UpdateEffectiveFrameRateLimit();
}

// 返回当前是否应采用前端菜单性能策略；编辑器中可由控制台变量禁止。
bool ULyraSettingsLocal::ShouldUseFrontendPerformanceSettings() const
{
#if WITH_EDITOR
	if (GIsEditor && !CVarApplyFrontEndPerformanceOptionsInPIE.GetValueOnGameThread())
	{
		return false;
	}
#endif

	return bInFrontEndForPerformancePurposes;
}

// 返回指定性能统计项的显示模式；未配置时返回隐藏。
ELyraStatDisplayMode ULyraSettingsLocal::GetPerfStatDisplayState(ELyraDisplayablePerformanceStat Stat) const
{
	if (const ELyraStatDisplayMode* pMode = DisplayStatList.Find(Stat))
	{
		return *pMode;
	}
	else
	{
		return ELyraStatDisplayMode::Hidden;
	}
}

// 更新指定性能统计项的显示模式，并广播设置变化事件。
void ULyraSettingsLocal::SetPerfStatDisplayState(ELyraDisplayablePerformanceStat Stat, ELyraStatDisplayMode DisplayMode)
{
	if (DisplayMode == ELyraStatDisplayMode::Hidden)
	{
		DisplayStatList.Remove(Stat);
	}
	else
	{
		DisplayStatList.FindOrAdd(Stat) = DisplayMode;
	}
	PerfStatSettingsChangedEvent.Broadcast();
}

// 根据平台特性标签判断是否支持延迟闪烁标记。
bool ULyraSettingsLocal::DoesPlatformSupportLatencyMarkers()
{
	return ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(PerfStatTags::TAG_Platform_Trait_SupportsLatencyMarkers);
}

// 更新延迟闪烁标记开关，并广播状态变化事件。
void ULyraSettingsLocal::SetEnableLatencyFlashIndicators(const bool bNewVal)
{
	if (bNewVal != bEnableLatencyFlashIndicators)
	{
		bEnableLatencyFlashIndicators = bNewVal;
		LatencyFlashInidicatorSettingsChangedEvent.Broadcast();
	}	
}

// 更新延迟统计开关，立即应用模块状态并广播变化事件。
void ULyraSettingsLocal::SetEnableLatencyTrackingStats(const bool bNewVal)
{
	if (bNewVal != bEnableLatencyTrackingStats)
	{
		bEnableLatencyTrackingStats = bNewVal;

		ApplyLatencyTrackingStatSetting();

		LatencyStatIndicatorSettingsChangedEvent.Broadcast();
	}
}

// 按设置启停延迟标记模块；模块不可用时不执行操作。
void ULyraSettingsLocal::ApplyLatencyTrackingStatSetting()
{
	// 设置加载时也会调用本函数；若 Slate 尚未初始化，说明当前可能是无界面的 Cook 等目标，不具备延迟统计使用环境，直接返回。
	// Since this function will be called on load of the settings, we check if the slate app is initalized.
	// If it isn't then we are not in a target which can even have latency stats (like a headless cooker) so we
	// will exit early and do nothing.
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	
	// 平台不支持延迟统计时不尝试启停延迟标记模块。
	// Don't bother doing anything if the platform doesn't even support tracking stats.
	if (!DoesPlatformSupportLatencyTrackingStats())
	{
		return;
	}
	
	// 将用户设置同步到所有注册的 ILatencyMarkerModule 实现。
	// Actually enable or disable the latency marker modules based on this setting
	TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		LatencyMarkerModule->SetEnabled(bEnableLatencyTrackingStats);
	}

	UE_CLOG(!LatencyMarkerModules.IsEmpty(),
		LogConsoleResponse,
		Log,
		TEXT("%s %d Latency Marker Module(s)"),
		bEnableLatencyTrackingStats ? TEXT("Enabled") : TEXT("Disabled"), LatencyMarkerModules.Num());
}

// 根据平台特性标签判断是否支持延迟统计采集。
bool ULyraSettingsLocal::DoesPlatformSupportLatencyTrackingStats()
{
	return ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(PerfStatTags::TAG_Platform_Trait_SupportsLatencyStats);
}

// 返回当前显示 Gamma 值。
float ULyraSettingsLocal::GetDisplayGamma() const
{
	return DisplayGamma;
}

// 保存显示 Gamma 值并立即同步到引擎全局显示 Gamma。
void ULyraSettingsLocal::SetDisplayGamma(float InGamma)
{
	DisplayGamma = InGamma;
	ApplyDisplayGamma();
}

// 将当前 Gamma 值写入 GEngine；引擎尚未创建时不处理。
void ULyraSettingsLocal::ApplyDisplayGamma()
{
	if (GEngine)
	{
		GEngine->DisplayGamma = DisplayGamma;
	}
}

// 返回使用电池供电时的帧率上限。
float ULyraSettingsLocal::GetFrameRateLimit_OnBattery() const
{
	return FrameRateLimit_OnBattery;
}

// 更新电池供电帧率上限并重算当前有效限制。
void ULyraSettingsLocal::SetFrameRateLimit_OnBattery(float NewLimitFPS)
{
	FrameRateLimit_OnBattery = NewLimitFPS;
	UpdateEffectiveFrameRateLimit();
}

// 返回前端菜单中的帧率上限。
float ULyraSettingsLocal::GetFrameRateLimit_InMenu() const
{
	return FrameRateLimit_InMenu;
}

// 更新前端菜单帧率上限并重算当前有效限制。
void ULyraSettingsLocal::SetFrameRateLimit_InMenu(float NewLimitFPS)
{
	FrameRateLimit_InMenu = NewLimitFPS;
	UpdateEffectiveFrameRateLimit();
}

// 返回应用位于后台时的帧率上限。
float ULyraSettingsLocal::GetFrameRateLimit_WhenBackgrounded() const
{
	return FrameRateLimit_WhenBackgrounded;
}

// 更新后台帧率上限并重算当前有效限制。
void ULyraSettingsLocal::SetFrameRateLimit_WhenBackgrounded(float NewLimitFPS)
{
	FrameRateLimit_WhenBackgrounded = NewLimitFPS;
	UpdateEffectiveFrameRateLimit();
}

// 返回始终生效的自定义帧率上限。
float ULyraSettingsLocal::GetFrameRateLimit_Always() const
{
	return GetFrameRateLimit();
}

// 更新常驻帧率上限并重算当前有效限制。
void ULyraSettingsLocal::SetFrameRateLimit_Always(float NewLimitFPS)
{
	SetFrameRateLimit(NewLimitFPS);
	UpdateEffectiveFrameRateLimit();
}

// 非专用服务器环境下重新计算有效帧率上限并写入引擎限帧控制台变量。
void ULyraSettingsLocal::UpdateEffectiveFrameRateLimit()
{
	if (!IsRunningDedicatedServer())
	{
		SetFrameRateLimitCVar(GetEffectiveFrameRateLimit());
	}
}

// 返回动态分辨率的目标帧率。
float ULyraSettingsLocal::GetDynamicResolutionFrameRateTarget() const
{
	return DynamicResolutionFrameTarget;
}

// 非专用服务器环境下记录动态分辨率目标帧率，帧时间预算由帧率节奏更新流程另行应用。
void ULyraSettingsLocal::SetDynamicResolutionFrameRateTarget(float NewDynamicResolutionFPS)
{
	if (!IsRunningDedicatedServer())
	{
		DynamicResolutionFrameTarget = NewDynamicResolutionFPS;
	}
}

// 返回设备配置指定的移动端默认帧率。
int32 ULyraSettingsLocal::GetDefaultMobileFrameRate()
{
	return CVarDeviceProfileDrivenMobileDefaultFrameRate.GetValueOnGameThread();
}

// 返回设备配置允许的移动端最高帧率。
int32 ULyraSettingsLocal::GetMaxMobileFrameRate()
{
	return CVarDeviceProfileDrivenMobileMaxFrameRate.GetValueOnGameThread();
}

// 检查目标 FPS 是否同时受平台帧率节奏器和设备配置支持。
bool ULyraSettingsLocal::IsSupportedMobileFramePace(int32 TestFPS)
{
	const bool bIsDefault = (TestFPS == GetDefaultMobileFrameRate());
	const bool bDoesNotExceedLimit = (TestFPS <= GetMaxMobileFrameRate());

	// 编辑器中通常是在模拟其他平台，因此允许所有帧率档位，不受本机帧节奏器能力限制。
	// Allow all paces in the editor, as we'd only be doing this when simulating another platform
	const bool bIsSupportedPace = FPlatformRHIFramePacer::SupportsFramePace(TestFPS) || GIsEditor;

	return bIsDefault || (bDoesNotExceedLimit && bIsSupportedPace);
}

// 返回设备配置首次开始限制整体画质的帧率阈值；未配置时返回 INDEX_NONE。
int32 ULyraSettingsLocal::GetFirstFrameRateWithQualityLimit() const
{
	return LyraSettingsHelpers::GetFirstFrameRateWithQualityLimit();
}

// 返回帧率限制表中的最低整体画质上限；未配置时返回 -1。
int32 ULyraSettingsLocal::GetLowestQualityWithFrameRateLimit() const
{
	return LyraSettingsHelpers::GetLowestQualityWithFrameRateLimit();
}

// 将移动端帧率与画质恢复为当前设备配置默认值，并同步期望帧率。
void ULyraSettingsLocal::ResetToMobileDeviceDefaults()
{
	// 将待应用值和已确认值都恢复为设备配置指定的移动端默认帧率。
	// Reset frame rate
	DesiredMobileFrameRateLimit = GetDefaultMobileFrameRate();
	MobileFrameRateLimit = DesiredMobileFrameRateLimit;
	
	// 以当前可伸缩性值为基础，再用设备配置中的有效默认通道覆盖。
	// Reset scalability
	Scalability::FQualityLevels DefaultLevels = Scalability::GetQualityLevels();
	OverrideQualityLevelsToScalabilityMode(DeviceDefaultScalabilitySettings, DefaultLevels);
	ScalabilityQuality = DefaultLevels;

	// 立即重新选择设备配置并应用对应帧率节奏。
	// Apply
	UpdateGameModeDeviceProfileAndFps();
}

// 移动端返回设备默认画质快照中的最高通道等级；其他模式固定返回最高预设等级 3。
int32 ULyraSettingsLocal::GetMaxSupportedOverallQualityLevel() const
{
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if ((PlatformSettings->FramePacingMode == ELyraFramePacingMode::MobileStyle) && DeviceDefaultScalabilitySettings.bHasOverrides)
	{
		return LyraSettingsHelpers::GetHighestLevelOfAnyScalabilityChannel(DeviceDefaultScalabilitySettings.Qualities);
	}
	else
	{
		return 3;
	}
}

// 在移动端帧率节奏模式下确认实际 FPS 档位；档位变化时立即刷新设备配置和帧率节奏。
void ULyraSettingsLocal::SetMobileFPSMode(int32 NewLimitFPS)
{
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::MobileStyle)
	{
		if (MobileFrameRateLimit != NewLimitFPS)
		{
			MobileFrameRateLimit = NewLimitFPS;
			UpdateGameModeDeviceProfileAndFps();
		}

		DesiredMobileFrameRateLimit = MobileFrameRateLimit;
	}
}

// 记录用户期望的移动端 FPS，按新旧档位重映射分辨率画质，并施加对应整体画质上限。
void ULyraSettingsLocal::SetDesiredMobileFrameRateLimit(int32 NewLimitFPS)
{
	const int32 OldLimitFPS = DesiredMobileFrameRateLimit;

	RemapMobileResolutionQuality(OldLimitFPS, NewLimitFPS);

	DesiredMobileFrameRateLimit = NewLimitFPS;

	ClampMobileFPSQualityLevels(/*bWriteBack=*/ false);
}

// 按期望移动端 FPS 对整体画质施加设备配置上限；可选择将受限结果写回引擎可伸缩性状态。
void ULyraSettingsLocal::ClampMobileFPSQualityLevels(bool bWriteBack)
{
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::MobileStyle)
	{
		const int32 QualityLimit = LyraSettingsHelpers::GetApplicableOverallQualityLimit(DesiredMobileFrameRateLimit);
		const int32 CurrentQualityLevel = GetHighestLevelOfAnyScalabilityChannel();
		if ((QualityLimit >= 0) && (CurrentQualityLevel > QualityLimit))
		{
			SetOverallScalabilityLevel(QualityLimit);

			if (bWriteBack)
			{
				Scalability::SetQualityLevels(ScalabilityQuality);
			}
			UE_LOG(LogConsoleResponse, Log, TEXT("Mobile FPS clamped overall quality (%d -> %d)."), CurrentQualityLevel, QualityLimit);
		}
	}
}

// 按设备默认值、当前帧率和画质上限约束移动端可伸缩性设置。
void ULyraSettingsLocal::ClampMobileQuality()
{
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::MobileStyle)
	{
		// 设备默认画质是已验证可运行的最高值，因此只将超限通道压到上限，保留用户主动选择的更低画质。
		// Clamp the resultant settings to the device default, it's known viable maximum.
		// This is a clamp rather than override to preserve allowed user settings
		Scalability::FQualityLevels CurrentLevels = Scalability::GetQualityLevels();

		/** 移动端保留当前 3D 分辨率画质，避免设备默认值在启动时覆盖用户选择。 */
		/** On mobile, disables the 3D Resolution clamp that reverts the setting set by the user on boot.*/
		bool bMobileDisableResolutionReset = true;
		if (bMobileDisableResolutionReset)
		{
			DeviceDefaultScalabilitySettings.Qualities.ResolutionQuality = CurrentLevels.ResolutionQuality;
		}

		ClampQualityLevelsToDeviceProfile(DeviceDefaultScalabilitySettings.Qualities, /*inout*/ CurrentLevels);
		Scalability::SetQualityLevels(CurrentLevels);

		// 按当前目标 FPS 对整体画质等级施加额外限制，并写回实际可伸缩性状态。
		// Clamp quality levels if required at the current frame rate
		ClampMobileFPSQualityLevels(/*bWriteBack=*/ true);

		const int32 MaxMobileFrameRate = GetMaxMobileFrameRate();
		const int32 DefaultMobileFrameRate = GetDefaultMobileFrameRate();
		
		ensureMsgf(DefaultMobileFrameRate <= MaxMobileFrameRate, TEXT("Default mobile frame rate (%d) is higher than the maximum mobile frame rate (%d)!"), DefaultMobileFrameRate, MaxMobileFrameRate);

		// 在设备上限内选择不超过用户目标值的最高受支持帧率，并据此限制 3D 分辨率。
		// Choose the closest supported frame rate to the user desired setting without going over the device imposed limit
		const TArray<int32>& PossibleRates = PlatformSettings->MobileFrameRateLimits;
		const int32 LimitIndex = PossibleRates.FindLastByPredicate([this](const int32& TestRate) { return (TestRate <= DesiredMobileFrameRateLimit) && IsSupportedMobileFramePace(TestRate); });
		const int32 ActualLimitFPS = PossibleRates.IsValidIndex(LimitIndex) ? PossibleRates[LimitIndex] : GetDefaultMobileFrameRate();

		ClampMobileResolutionQuality(ActualLimitFPS);
	}
}

// 将分辨率画质限制在目标 FPS 对应的设备配置上限内。
void ULyraSettingsLocal::ClampMobileResolutionQuality(int32 TargetFPS)
{
	// 将移动端 3D 分辨率百分比限制到目标 FPS 对应的设备配置上限。
	// Clamp mobile resolution quality
	float MaxMobileResQuality = LyraSettingsHelpers::GetApplicableResolutionQualityLimit(TargetFPS);
	float CurrentScaleNormalized = 0.0f;
	float CurrentScaleValue = 0.0f;
	float MinScaleValue = 0.0f;
	float MaxScaleValue = 0.0f;
	GetResolutionScaleInformationEx(CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
	if (CurrentScaleValue > MaxMobileResQuality)
	{
		UE_LOG(LogConsoleResponse, Log, TEXT("clamping mobile resolution quality max res: %f, %f, %f, %f, %f"), CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue, MaxMobileResQuality);
		SetResolutionScaleValueEx(MaxMobileResQuality);
	}
}

// 切换 FPS 档位时按原档位百分比位置映射分辨率画质，并应用新上限。
void ULyraSettingsLocal::RemapMobileResolutionQuality(int32 FromFPS, int32 ToFPS)
{
	// 移动端 3D 分辨率滑块表示最小值到当前 FPS 档位上限之间的归一化位置。
	// 切换 FPS 档位会改变上限，因此按原归一化比例映射到新区间，保持滑块的相对位置不变。
	// Mobile resolution quality slider is a normalized value that is lerped between min quality, max quality.
	// max quality can change depending on FPS mode. This code remaps the quality when FPS mode changes so that the normalized
	// value remains the same within the new range.
	float CurrentScaleNormalized = 0.0f;
	float CurrentScaleValue = 0.0f;
	float MinScaleValue = 0.0f;
	float MaxScaleValue = 0.0f;
	GetResolutionScaleInformationEx(CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
	float FromMaxMobileResQuality = LyraSettingsHelpers::GetApplicableResolutionQualityLimit(FromFPS);
	float ToMaxMobileResQuality = LyraSettingsHelpers::GetApplicableResolutionQualityLimit(ToFPS);
	float FromMobileScaledNormalizedValue = (CurrentScaleValue - MinScaleValue) / (FromMaxMobileResQuality - MinScaleValue);
	float ToResQuality = FMath::Lerp(MinScaleValue, ToMaxMobileResQuality, FromMobileScaledNormalizedValue);

	UE_LOG(LogConsoleResponse, Log, TEXT("Remap mobile resolution quality %f, %f, (%d,%d)"), CurrentScaleValue, ToResQuality, FromFPS, ToFPS);

	SetResolutionScaleValueEx(ToResQuality);
}

// 返回用户期望的设备配置画质后缀。
FString ULyraSettingsLocal::GetDesiredDeviceProfileQualitySuffix() const
{
	return DesiredUserChosenDeviceProfileSuffix;
}

// 记录期望的设备配置画质后缀，等待设备配置刷新时应用。
void ULyraSettingsLocal::SetDesiredDeviceProfileQualitySuffix(const FString& InDesiredSuffix)
{
	DesiredUserChosenDeviceProfileSuffix = InDesiredSuffix;
}

// 在平台允许修改时更新双耳耳机模式，并写入音频控制台变量和配置。
void ULyraSettingsLocal::SetHeadphoneModeEnabled(bool bEnabled)
{
	if (CanModifyHeadphoneModeEnabled())
	{
		static IConsoleVariable* BinauralSpatializationDisabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableBinauralSpatialization"));
		if (BinauralSpatializationDisabledCVar)
		{
			BinauralSpatializationDisabledCVar->Set(!bEnabled, ECVF_SetByGameSetting);

			// 仅在实际状态变化时更新配置并触发保存，避免无意义写盘。
			// Only save settings if the setting actually changed
			if (bUseHeadphoneMode != bEnabled)
			{
				bUseHeadphoneMode = bEnabled;
				SaveSettings();
			}
		}
	}
}

// 返回当前双耳耳机模式是否启用。
bool ULyraSettingsLocal::IsHeadphoneModeEnabled() const
{
	return bUseHeadphoneMode;
}

// 仅当操作系统未接管双耳设置且音频控制台变量可写时允许修改。
bool ULyraSettingsLocal::CanModifyHeadphoneModeEnabled() const
{
	static IConsoleVariable* BinauralSpatializationDisabledCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.DisableBinauralSpatialization"));
	const bool bHRTFOptionAvailable = BinauralSpatializationDisabledCVar && ((BinauralSpatializationDisabledCVar->GetFlags() & EConsoleVariableFlags::ECVF_SetByMask) <= EConsoleVariableFlags::ECVF_SetByGameSetting);

	const bool bBinauralSettingControlledByOS = LyraSettingsHelpers::HasPlatformTrait(TAG_Platform_Trait_BinauralSettingControlledByOS);

	return bHRTFOptionAvailable && !bBinauralSettingControlledByOS;
}

// 返回 HDR 音频动态范围模式是否启用。
bool ULyraSettingsLocal::IsHDRAudioModeEnabled() const
{
	return bUseHDRAudioMode;
}

// 更新 HDR 音频模式，并请求音频混音效果子系统切换动态范围链。
void ULyraSettingsLocal::SetHDRAudioModeEnabled(bool bEnabled)
{
	bUseHDRAudioMode = bEnabled;

	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetCurrentPlayWorld())
		{
			if (ULyraAudioMixEffectsSubsystem* LyraAudioMixEffectsSubsystem = World->GetSubsystem<ULyraAudioMixEffectsSubsystem>())
			{
				LyraAudioMixEffectsSubsystem->ApplyDynamicRangeEffectsChains(bEnabled);
			}
		}
	}
}

// 检查当前平台是否允许执行自动画质基准测试。
bool ULyraSettingsLocal::CanRunAutoBenchmark() const
{
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	return PlatformSettings->bSupportsAutomaticVideoQualityBenchmark;
}

// 判断是否应在启动时自动运行画质基准测试。
bool ULyraSettingsLocal::ShouldRunAutoBenchmarkAtStartup() const
{
	if (!CanRunAutoBenchmark())
	{
		return false;
	}

	if (LastCPUBenchmarkResult != -1)
	{
		// CPU 基准结果有效，说明自动基准测试已运行并成功加载结果。
		// Already run and loaded
		return false;
	}

	return true;
}

// 运行硬件基准测试、应用结果，并按参数选择是否立即保存。
void ULyraSettingsLocal::RunAutoBenchmark(bool bSaveImmediately)
{
	RunHardwareBenchmark();
	
	// 基准测试结果始终立即应用，仅由 bSaveImmediately 决定是否立刻写盘。
	// Always apply, optionally save
	ApplyScalabilitySettings();
	ApplyLatencyTrackingStatSetting();

	if (bSaveImmediately)
	{
		SaveSettings();
	}
}

// 将当前缓存的可伸缩性画质等级写入引擎。
void ULyraSettingsLocal::ApplyScalabilitySettings()
{
	Scalability::SetQualityLevels(ScalabilityQuality);
}

// 返回主音量。
float ULyraSettingsLocal::GetOverallVolume() const
{
	return OverallVolume;
}

// 更新主音量，并在控制总线可用时立即应用。
void ULyraSettingsLocal::SetOverallVolume(float InVolume)
{
	// 缓存 UI 提交的总音量值，稍后写入 Overall 控制总线。
	// Cache the incoming volume value
	OverallVolume = InVolume;

	// 若这是 UI 首次调用音量 Setter，控制总线及混音资产可能尚未加载，此时先完成加载。
	// Check to see if references to the control buses and control bus mixes have been loaded yet
	// Will likely need to be loaded if this function is the first time a setter has been called from the UI
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 继续更新音量前确认用户控制总线混音已成功加载。
	// Ensure it's been loaded before continuing
	ensureMsgf(bSoundControlBusMixLoaded, TEXT("UserControlBusMix Settings Failed to Load."));

	// 从缓存中找到 Overall 控制总线并更新其音量。
	// Locate the locally cached bus and set the volume on it
	if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("Overall")))
	{
		if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
		{
			SetVolumeForControlBus(ControlBusPtr, OverallVolume);
		}
	}
}

// 返回音乐音量。
float ULyraSettingsLocal::GetMusicVolume() const
{
	return MusicVolume;
}

// 更新音乐音量，并在控制总线可用时立即应用。
void ULyraSettingsLocal::SetMusicVolume(float InVolume)
{
	// 缓存 UI 提交的音乐音量值，稍后写入 Music 控制总线。
	// Cache the incoming volume value
	MusicVolume = InVolume;

	// 若这是 UI 首次调用音量 Setter，控制总线及混音资产可能尚未加载，此时先完成加载。
	// Check to see if references to the control buses and control bus mixes have been loaded yet
	// Will likely need to be loaded if this function is the first time a setter has been called from the UI
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 继续更新音量前确认用户控制总线混音已成功加载。
	// Ensure it's been loaded before continuing
	ensureMsgf(bSoundControlBusMixLoaded, TEXT("UserControlBusMix Settings Failed to Load."));

	// 从缓存中找到 Music 控制总线并更新其音量。
	// Locate the locally cached bus and set the volume on it
	if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("Music")))
	{
		if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
		{
			SetVolumeForControlBus(ControlBusPtr, MusicVolume);
		}
	}
}

// 返回音效音量。
float ULyraSettingsLocal::GetSoundFXVolume() const
{
	return SoundFXVolume;
}

// 更新音效音量，并在控制总线可用时立即应用。
void ULyraSettingsLocal::SetSoundFXVolume(float InVolume)
{
	// 缓存 UI 提交的音效音量值，稍后写入 SoundFX 控制总线。
	// Cache the incoming volume value
	SoundFXVolume = InVolume;

	// 若这是 UI 首次调用音量 Setter，控制总线及混音资产可能尚未加载，此时先完成加载。
	// Check to see if references to the control buses and control bus mixes have been loaded yet
	// Will likely need to be loaded if this function is the first time a setter has been called from the UI
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 继续更新音量前确认用户控制总线混音已成功加载。
	// Ensure it's been loaded before continuing
	ensureMsgf(bSoundControlBusMixLoaded, TEXT("UserControlBusMix Settings Failed to Load."));

	// 从缓存中找到 SoundFX 控制总线并更新其音量。
	// Locate the locally cached bus and set the volume on it
	if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("SoundFX")))
	{
		if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
		{
			SetVolumeForControlBus(ControlBusPtr, SoundFXVolume);
		}
	}
}

// 返回对白音量。
float ULyraSettingsLocal::GetDialogueVolume() const
{
	return DialogueVolume;
}

// 更新对白音量，并在控制总线可用时立即应用。
void ULyraSettingsLocal::SetDialogueVolume(float InVolume)
{
	// 缓存 UI 提交的对白音量值，稍后写入 Dialogue 控制总线。
	// Cache the incoming volume value
	DialogueVolume = InVolume;

	// 若这是 UI 首次调用音量 Setter，控制总线及混音资产可能尚未加载，此时先完成加载。
	// Check to see if references to the control buses and control bus mixes have been loaded yet
	// Will likely need to be loaded if this function is the first time a setter has been called from the UI
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 继续更新音量前确认用户控制总线混音已成功加载。
	// Ensure it's been loaded before continuing
	ensureMsgf(bSoundControlBusMixLoaded, TEXT("UserControlBusMix Settings Failed to Load."));

	// 从缓存中找到 Dialogue 控制总线并更新其音量。
	// Locate the locally cached bus and set the volume on it
	if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("Dialogue")))
	{
		if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
		{
			SetVolumeForControlBus(ControlBusPtr, DialogueVolume);
		}
	}
}

// 返回语音聊天音量。
float ULyraSettingsLocal::GetVoiceChatVolume() const
{
	return VoiceChatVolume;
}

// 更新语音聊天音量，并在控制总线可用时立即应用。
void ULyraSettingsLocal::SetVoiceChatVolume(float InVolume)
{
	// 缓存 UI 提交的语音聊天音量值，稍后写入 VoiceChat 控制总线。
	// Cache the incoming volume value
	VoiceChatVolume = InVolume;

	// 若这是 UI 首次调用音量 Setter，控制总线及混音资产可能尚未加载，此时先完成加载。
	// Check to see if references to the control buses and control bus mixes have been loaded yet
	// Will likely need to be loaded if this function is the first time a setter has been called from the UI
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 继续更新音量前确认用户控制总线混音已成功加载。
	// Ensure it's been loaded before continuing
	ensureMsgf(bSoundControlBusMixLoaded, TEXT("UserControlBusMix Settings Failed to Load."));

	// 从缓存中找到 VoiceChat 控制总线并更新其音量。
	// Locate the locally cached bus and set the volume on it
	if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("VoiceChat")))
	{
		if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
		{
			SetVolumeForControlBus(ControlBusPtr, VoiceChatVolume);
		}
	}
}

// 通过当前控制总线混音覆盖指定总线音量；总线或混音为空时不处理。
void ULyraSettingsLocal::SetVolumeForControlBus(USoundControlBus* InSoundControlBus, float InVolume)
{
	// 若控制总线及混音资产尚未加载，则先加载；直接调用本函数时同样可能发生这种情况。
	// Check to see if references to the control buses and control bus mixes have been loaded yet
	// Will likely need to be loaded if this function is the first time a setter has been called
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 继续更新混音前确认用户控制总线混音已成功加载。
	// Ensure it's been loaded before continuing
	ensureMsgf(bSoundControlBusMixLoaded, TEXT("UserControlBusMix Settings Failed to Load."));

	// 资产加载成功后取得当前播放世界，通过 AudioModulationStatics 更新指定控制总线的目标音量，并应用到缓存的用户控制总线混音。
	// Assuming everything has been loaded correctly, we retrieve the world and use AudioModulationStatics to update the Control Bus Volume values and
	// apply the settings to the cached User Control Bus Mix
	if (GEngine && InSoundControlBus && bSoundControlBusMixLoaded)
	{
			if (const UWorld* AudioWorld = GEngine->GetCurrentPlayWorld())
			{
				ensureMsgf(ControlBusMix, TEXT("Control Bus Mix failed to load."));

				// 创建该控制总线的混音阶段参数，并设置目标音量及平滑过渡时间。
				// Create and set the Control Bus Mix Stage Parameters
				FSoundControlBusMixStage UpdatedControlBusMixStage;
				UpdatedControlBusMixStage.Bus = InSoundControlBus;
				UpdatedControlBusMixStage.Value.TargetValue = InVolume;
				UpdatedControlBusMixStage.Value.AttackTime = 0.01f;
				UpdatedControlBusMixStage.Value.ReleaseTime = 0.01f;

				// UpdateMix 接收阶段数组，因此将本次单个阶段包装进数组。
				// Add the Control Bus Mix Stage to an Array as the UpdateMix function requires
				TArray<FSoundControlBusMixStage> UpdatedMixStageArray;
				UpdatedMixStageArray.Add(UpdatedControlBusMixStage);

				// 更新用户控制总线混音中与该总线匹配的阶段参数。
				// Modify the matching bus Mix Stage parameters on the User Control Bus Mix
				UAudioModulationStatics::UpdateMix(AudioWorld, ControlBusMix, UpdatedMixStageArray);
			}
	}
}

// 记录音频输出设备 ID，并广播设备切换请求。
void ULyraSettingsLocal::SetAudioOutputDeviceId(const FString& InAudioOutputDeviceId)
{
	AudioOutputDeviceId = InAudioOutputDeviceId;
	OnAudioOutputDeviceChanged.Broadcast(InAudioOutputDeviceId);
}

// 将当前安全区比例写入 Slate 全局安全区设置。
void ULyraSettingsLocal::ApplySafeZoneScale()
{
	SSafeZone::SetGlobalSafeZoneScale(GetSafeZone());
}

// 应用非分辨率用户设置，并同步 Gamma、音频、输入、设备配置和帧率节奏。
void ULyraSettingsLocal::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	// 若用户未在 UI 中触发过音量 Setter，应用非分辨率设置时控制总线混音可能仍未加载，此处补充加载。
	// Check if Control Bus Mix references have been loaded,
	// Might be false if applying non resolution settings without touching any of the setters from UI
	if (!bSoundControlBusMixLoaded)
	{
		LoadUserControlBusMix();
	}

	// 将缓存的各类别音量值逐一写入对应控制总线。
	// In this section, update each Control Bus to the currently cached UI settings
	{
		if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("Overall")))
		{
			if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
			{
				SetVolumeForControlBus(ControlBusPtr, OverallVolume);
			}
		}

		if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("Music")))
		{
			if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
			{
				SetVolumeForControlBus(ControlBusPtr, MusicVolume);
			}
		}

		if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("SoundFX")))
		{
			if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
			{
				SetVolumeForControlBus(ControlBusPtr, SoundFXVolume);
			}
		}

		if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("Dialogue")))
		{
			if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
			{
				SetVolumeForControlBus(ControlBusPtr, DialogueVolume);
			}
		}

		if (TObjectPtr<USoundControlBus>* ControlBusDblPtr = ControlBusMap.Find(TEXT("VoiceChat")))
		{
			if (USoundControlBus* ControlBusPtr = *ControlBusDblPtr)
			{
				SetVolumeForControlBus(ControlBusPtr, VoiceChatVolume);
			}
		}
	}

	if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetTypedOuter<ULocalPlayer>()))
	{
		InputSubsystem->SetGamepadInputType(ControllerPlatform);
	}

	if (bUseHeadphoneMode != bDesiredHeadphoneMode)
	{
		SetHeadphoneModeEnabled(bDesiredHeadphoneMode);
	}
	
	if (DesiredUserChosenDeviceProfileSuffix != UserChosenDeviceProfileSuffix)
	{
		UserChosenDeviceProfileSuffix = DesiredUserChosenDeviceProfileSuffix;
	}

	if (FApp::CanEverRender())
	{
		ApplyDisplayGamma();
		ApplySafeZoneScale();
		SetMobileFPSMode(DesiredMobileFrameRateLimit);
		UpdateGameModeDeviceProfileAndFps();
	}

	PerfStatSettingsChangedEvent.Broadcast();
}

// 返回统一画质等级；各通道不一致时返回自定义状态值。
int32 ULyraSettingsLocal::GetOverallScalabilityLevel() const
{
	int32 Result = Super::GetOverallScalabilityLevel();

	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::MobileStyle)
	{
		Result = GetHighestLevelOfAnyScalabilityChannel();
	}

	return Result;
}

// 将输入限制到 0 至 3 后设置统一画质；移动端保留分辨率画质，并在必要时降低期望 FPS 以满足画质限制。
void ULyraSettingsLocal::SetOverallScalabilityLevel(int32 Value)
{
	TGuardValue Guard(bSettingOverallQualityGuard, true);

	Value = FMath::Clamp(Value, 0, 3);

	float CurrentMobileResolutionQuality = ScalabilityQuality.ResolutionQuality;

	Super::SetOverallScalabilityLevel(Value);

	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	if (PlatformSettings->FramePacingMode == ELyraFramePacingMode::MobileStyle)
	{
		// 移动端将 3D 分辨率与整体画质预设解耦，因此恢复预设变更前的 ResolutionQuality。
		// Restore the resolution quality, mobile decouples this from overall quality
		ScalabilityQuality.ResolutionQuality = CurrentMobileResolutionQuality;

		// 移动端某些高画质等级不允许高 FPS；整体画质改变后，如有必要将目标帧率降到兼容档位。
		// Changing the overall quality can end up adjusting the frame rate on mobile since there are limits
		const int32 ConstrainedFrameRateLimit = LyraSettingsHelpers::ConstrainFrameRateToBeCompatibleWithOverallQuality(DesiredMobileFrameRateLimit, Value);
		if (ConstrainedFrameRateLimit != DesiredMobileFrameRateLimit)
		{
			SetDesiredMobileFrameRateLimit(ConstrainedFrameRateLimit);
		}
	}
}

// 更新控制器平台名称，并通知 CommonInput 子系统刷新游戏手柄类型。
void ULyraSettingsLocal::SetControllerPlatform(const FName InControllerPlatform)
{
	if (ControllerPlatform != InControllerPlatform)
	{
		ControllerPlatform = InControllerPlatform;

		// 将手柄类型同步到 Common Input 子系统，使当前 UI 使用的输入图标立即刷新。
		// Apply the change to the common input subsystem so that we refresh any input icons we're using.
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetTypedOuter<ULocalPlayer>()))
		{
			InputSubsystem->SetGamepadInputType(ControllerPlatform);
		}
	}
}

// 返回当前控制器平台名称。
FName ULyraSettingsLocal::GetControllerPlatform() const
{
	return ControllerPlatform;
}

// 加载并激活用户音量控制总线混音，缓存各分类控制总线后应用已保存音量。
void ULyraSettingsLocal::LoadUserControlBusMix()
{
	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetCurrentPlayWorld())
		{
			if (const ULyraAudioSettings* LyraAudioSettings = GetDefault<ULyraAudioSettings>())
			{
				USoundControlBus* OverallControlBus = nullptr;
				USoundControlBus* MusicControlBus = nullptr;
				USoundControlBus* SoundFXControlBus = nullptr;
				USoundControlBus* DialogueControlBus = nullptr;
				USoundControlBus* VoiceChatControlBus = nullptr;

				ControlBusMap.Empty();

				if (UObject* ObjPath = LyraAudioSettings->OverallVolumeControlBus.TryLoad())
				{
					if (USoundControlBus* SoundControlBus = Cast<USoundControlBus>(ObjPath))
					{
						OverallControlBus = SoundControlBus;
						ControlBusMap.Add(TEXT("Overall"), OverallControlBus);
					}
					else
					{
						ensureMsgf(SoundControlBus, TEXT("Overall Control Bus reference missing from Lyra Audio Settings."));
					}
				}

				if (UObject* ObjPath = LyraAudioSettings->MusicVolumeControlBus.TryLoad())
				{
					if (USoundControlBus* SoundControlBus = Cast<USoundControlBus>(ObjPath))
					{
						MusicControlBus = SoundControlBus;
						ControlBusMap.Add(TEXT("Music"), MusicControlBus);
					}
					else
					{
						ensureMsgf(SoundControlBus, TEXT("Music Control Bus reference missing from Lyra Audio Settings."));
					}
				}

				if (UObject* ObjPath = LyraAudioSettings->SoundFXVolumeControlBus.TryLoad())
				{
					if (USoundControlBus* SoundControlBus = Cast<USoundControlBus>(ObjPath))
					{
						SoundFXControlBus = SoundControlBus;
						ControlBusMap.Add(TEXT("SoundFX"), SoundFXControlBus);
					}
					else
					{
						ensureMsgf(SoundControlBus, TEXT("SoundFX Control Bus reference missing from Lyra Audio Settings."));
					}
				}

				if (UObject* ObjPath = LyraAudioSettings->DialogueVolumeControlBus.TryLoad())
				{
					if (USoundControlBus* SoundControlBus = Cast<USoundControlBus>(ObjPath))
					{
						DialogueControlBus = SoundControlBus;
						ControlBusMap.Add(TEXT("Dialogue"), DialogueControlBus);
					}
					else
					{
						ensureMsgf(SoundControlBus, TEXT("Dialogue Control Bus reference missing from Lyra Audio Settings."));
					}
				}

				if (UObject* ObjPath = LyraAudioSettings->VoiceChatVolumeControlBus.TryLoad())
				{
					if (USoundControlBus* SoundControlBus = Cast<USoundControlBus>(ObjPath))
					{
						VoiceChatControlBus = SoundControlBus;
						ControlBusMap.Add(TEXT("VoiceChat"), VoiceChatControlBus);
					}
					else
					{
						ensureMsgf(SoundControlBus, TEXT("VoiceChat Control Bus reference missing from Lyra Audio Settings."));
					}
				}

				if (UObject* ObjPath = LyraAudioSettings->UserSettingsControlBusMix.TryLoad())
				{
					if (USoundControlBusMix* SoundControlBusMix = Cast<USoundControlBusMix>(ObjPath))
					{
						ControlBusMix = SoundControlBusMix;

						const FSoundControlBusMixStage OverallControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, OverallControlBus, OverallVolume);
						const FSoundControlBusMixStage MusicControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, MusicControlBus, MusicVolume);
						const FSoundControlBusMixStage SoundFXControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, SoundFXControlBus, SoundFXVolume);
						const FSoundControlBusMixStage DialogueControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, DialogueControlBus, DialogueVolume);
						const FSoundControlBusMixStage VoiceChatControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, VoiceChatControlBus, VoiceChatVolume);

						TArray<FSoundControlBusMixStage> ControlBusMixStageArray;
						ControlBusMixStageArray.Add(OverallControlBusMixStage);
						ControlBusMixStageArray.Add(MusicControlBusMixStage);
						ControlBusMixStageArray.Add(SoundFXControlBusMixStage);
						ControlBusMixStageArray.Add(DialogueControlBusMixStage);
						ControlBusMixStageArray.Add(VoiceChatControlBusMixStage);

						UAudioModulationStatics::UpdateMix(World, ControlBusMix, ControlBusMixStageArray);

						bSoundControlBusMixLoaded = true;
					}
					else
					{
						ensureMsgf(SoundControlBusMix, TEXT("User Settings Control Bus Mix reference missing from Lyra Audio Settings."));
					}
				}
			}
		}
	}
}

// 应用激活或进入后台时重算有效帧率上限。
void ULyraSettingsLocal::OnAppActivationStateChanged(bool bIsActive)
{
	// 多窗口平台在应用失去或重新获得焦点时需重新合并后台帧率限制。
	// We might want to adjust the frame rate when the app loses/gains focus on multi-window platforms
	UpdateEffectiveFrameRateLimit();
}

// 根据体验、平台和用户画质选择设备配置，并在配置变化后刷新画质与帧率节奏。
void ULyraSettingsLocal::UpdateGameModeDeviceProfileAndFps()
{
	bool bApplyDeviceProfile = true;

#if WITH_EDITOR
	if (GIsEditor && !CVarApplyDeviceProfilesInPIE.GetValueOnGameThread())
	{
		bApplyDeviceProfile = false;
	}
#endif

	UDeviceProfileManager& Manager = UDeviceProfileManager::Get();

	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	const TArray<FLyraQualityDeviceProfileVariant>& UserFacingVariants = PlatformSettings->UserFacingDeviceProfileOptions;

	// TODO：可允许具体 Experience 提供设备配置后缀；下方查找逻辑已支持单独或与用户画质后缀组合使用，但当前没有赋值来源。
	//@TODO: Might want to allow specific experiences to specify a suffix to attempt to use as well
	// The code below will handle searching with this suffix (alone or in conjunction with the frame rate), but nothing sets it right now
	FString ExperienceSuffix;

	// 若用户所选画质变体要求的最低刷新率高于当前显示器能力，则沿列表向低档回退，最终使用平台默认后缀兜底。
	// Make sure the chosen setting is supported for the current display, walking down the list to try fallbacks
	const int32 PlatformMaxRefreshRate = FPlatformMisc::GetMaxRefreshRate();

	int32 SuffixIndex = UserFacingVariants.IndexOfByPredicate([&](const FLyraQualityDeviceProfileVariant& Data){ return Data.DeviceProfileSuffix == UserChosenDeviceProfileSuffix; });
	while (UserFacingVariants.IsValidIndex(SuffixIndex))
	{
		if (PlatformMaxRefreshRate >= UserFacingVariants[SuffixIndex].MinRefreshRate)
		{
			break;
		}
		else
		{
			--SuffixIndex;
		}
	}

	const FString EffectiveUserSuffix = UserFacingVariants.IsValidIndex(SuffixIndex) ? UserFacingVariants[SuffixIndex].DeviceProfileSuffix : PlatformSettings->DefaultDeviceProfileSuffix;

	// 按“平台+Experience+用户画质”“平台+用户画质”“平台+Experience”的优先级构造候选设备配置名。
	// Build up a list of names to try
	const bool bHadUserSuffix = !EffectiveUserSuffix.IsEmpty();
	const bool bHadExperienceSuffix = !ExperienceSuffix.IsEmpty();

	FString BasePlatformName = UDeviceProfileManager::GetPlatformDeviceProfileName();
	FName PlatformName; /* 非编辑器环境默认使用运行平台名称；编辑器中会根据 PIE 平台模拟设置覆盖。 */ // Default unless in editor
#if WITH_EDITOR
	if (GIsEditor)
	{
		const ULyraPlatformEmulationSettings* Settings = GetDefault<ULyraPlatformEmulationSettings>();
		const FName PretendBaseDeviceProfile = Settings->GetPretendBaseDeviceProfile();
		if (PretendBaseDeviceProfile != NAME_None)
		{
			BasePlatformName = PretendBaseDeviceProfile.ToString();
		}

		PlatformName = Settings->GetPretendPlatformName();
	}
#endif

	TArray<FString> ComposedNamesToFind;
	if (bHadExperienceSuffix && bHadUserSuffix)
	{
		ComposedNamesToFind.Add(BasePlatformName + TEXT("_") + ExperienceSuffix + TEXT("_") + EffectiveUserSuffix);
	}
	if (bHadUserSuffix)
	{
		ComposedNamesToFind.Add(BasePlatformName + TEXT("_") + EffectiveUserSuffix);
	}
	if (bHadExperienceSuffix)
	{
		ComposedNamesToFind.Add(BasePlatformName + TEXT("_") + ExperienceSuffix);
	}
	if (GIsEditor)
	{
		ComposedNamesToFind.Add(BasePlatformName);
	}

	// 按优先级选择首个实际可加载的候选设备配置。
	// See if any of the potential device profiles actually exists
	FString ActualProfileToApply;
	for (const FString& TestProfileName : ComposedNamesToFind)
	{
		if (Manager.HasLoadableProfileName(TestProfileName, PlatformName))
		{
			ActualProfileToApply = TestProfileName;
			UDeviceProfile* Profile = Manager.FindProfile(TestProfileName, /*bCreateOnFail=*/ false);
			if (Profile == nullptr)
			{
				Profile = Manager.CreateProfile(TestProfileName, TEXT(""), TestProfileName, *PlatformName.ToString());
			}

			UE_LOG(LogConsoleResponse, Log, TEXT("Profile %s exists"), *Profile->GetName());
			break;
		}
	}

	UE_LOG(LogConsoleResponse, Log, TEXT("UpdateGameModeDeviceProfileAndFps MaxRefreshRate=%d, ExperienceSuffix='%s', UserPicked='%s'->'%s', PlatformBase='%s', AppliedActual='%s'"), 
		PlatformMaxRefreshRate, *ExperienceSuffix, *UserChosenDeviceProfileSuffix, *EffectiveUserSuffix, *BasePlatformName, *ActualProfileToApply);

	// 仅在选中的配置与当前覆盖不同且允许应用设备配置时执行切换。
	// Apply the device profile if it's different to what we currently have
	if (ActualProfileToApply != CurrentAppliedDeviceProfileOverrideSuffix && bApplyDeviceProfile)
	{
		if (Manager.GetActiveDeviceProfileName() != ActualProfileToApply)
		{
			// 切换覆盖前先恢复平台默认设备配置，避免旧覆盖残留。
			// Restore the default first
			if (GIsEditor)
			{
#if ALLOW_OTHER_PLATFORM_CONFIG
				Manager.RestorePreviewDeviceProfile();
#endif
			}
			else
			{
				Manager.RestoreDefaultDeviceProfile();
			}

			// 若目标并非当前默认配置，则应用新的预览或运行时覆盖。
			// Apply the new one (if it wasn't the default)
			if (Manager.GetActiveDeviceProfileName() != ActualProfileToApply)
			{
				UDeviceProfile* NewDeviceProfile = Manager.FindProfile(ActualProfileToApply);
				ensureMsgf(NewDeviceProfile != nullptr, TEXT("DeviceProfile %s not found "), *ActualProfileToApply);
				if (NewDeviceProfile)
				{
					if (GIsEditor)
					{
#if ALLOW_OTHER_PLATFORM_CONFIG
						UE_LOG(LogConsoleResponse, Log, TEXT("Overriding *preview* device profile to %s"), *ActualProfileToApply);
						Manager.SetPreviewDeviceProfile(NewDeviceProfile);

						// 编辑器模拟平台切换后，从模拟设备配置重新读取默认可伸缩性上限。
						// Reload the default settings from the pretend profile
						LyraSettingsHelpers::FillScalabilitySettingsFromDeviceProfile(DeviceDefaultScalabilitySettings);
#endif
					}
					else
					{
						UE_LOG(LogConsoleResponse, Log, TEXT("Overriding device profile to %s"), *ActualProfileToApply);
						Manager.SetOverrideDeviceProfile(NewDeviceProfile);

						if (!bEnableScalabilitySettings)
						{
							// 平台不支持持久化独立可伸缩性设置时，仍同步当前引擎画质值，保证设置 API 查询结果是最新的。
							// We don't support persistence of the scalability settings but at least we may
							// provide up to date values if anybody queries them using the settings API.
							ScalabilityQuality = Scalability::GetQualityLevels();
						}
					}
				}
			}
		}
		CurrentAppliedDeviceProfileOverrideSuffix = ActualProfileToApply;
	}

	switch (PlatformSettings->FramePacingMode)
	{
	case ELyraFramePacingMode::MobileStyle:
		UpdateMobileFramePacing();
		break;
	case ELyraFramePacingMode::ConsoleStyle:
		UpdateConsoleFramePacing();
		break;
	case ELyraFramePacingMode::DesktopStyle:
		UpdateDesktopFramePacing();
		break;
	}
}

// 从设备配置读取主机目标 FPS 与同步类型，并配置平台帧率节奏。
void ULyraSettingsLocal::UpdateConsoleFramePacing()
{
	// 应用设备配置指定的帧同步类型和目标帧率节奏。
	// Apply device-profile-driven frame sync and frame pace
	const int32 FrameSyncType = CVarDeviceProfileDrivenFrameSyncType.GetValueOnGameThread();
	if (FrameSyncType != -1)
	{
		UE_LOG(LogConsoleResponse, Log, TEXT("Setting frame sync mode to %d."), FrameSyncType);
		SetSyncTypeCVar(FrameSyncType);
	}

	const int32 TargetFPS = CVarDeviceProfileDrivenTargetFps.GetValueOnGameThread();
	if (TargetFPS != -1)
	{
		UE_LOG(LogConsoleResponse, Log, TEXT("Setting frame pace to %d Hz."), TargetFPS);
		FPlatformRHIFramePacer::SetFramePace(TargetFPS);

		// 将目标 FPS 写入 CSV 性能分析元数据，便于后续统计区分帧率模式。
		// Set the CSV metadata and analytics Fps mode strings
#if CSV_PROFILER
		const FString TargetFramerateString = FString::Printf(TEXT("%d"), TargetFPS);
		CSV_METADATA(TEXT("TargetFramerate"), *TargetFramerateString);
#endif
	}
}

// 按当前有效帧率上限更新桌面动态分辨率帧时间预算。
void ULyraSettingsLocal::UpdateDesktopFramePacing()
{
	// 桌面端主限帧由父类依据 UpdateEffectiveFrameRateLimit 的结果处理；这里仅更新动态分辨率帧时间预算等次级效果。
	// For desktop the frame rate limit is handled by the parent class based on the value already
	// applied via UpdateEffectiveFrameRateLimit()
	// So this function is only doing 'second order' effects of desktop frame pacing preferences
	const float TargetFPS = GetEffectiveFrameRateLimit();
	const float ClampedFPS = (TargetFPS <= 0.0f) ? 60 : TargetFPS;
	UpdateDynamicResFrameTime(ClampedFPS);

	SetDynamicResolutionEnabled((GetDynamicResolutionFrameRateTarget() == 0.0f) ? false : true);
	GEngine->SetDynamicResolutionUserSetting(IsDynamicResolutionEnabled());
}

// 选择受平台支持且满足画质限制的移动端 FPS，并更新帧率节奏与动态分辨率预算。
void ULyraSettingsLocal::UpdateMobileFramePacing()
{
	// TODO：移动端尚未分别处理前端菜单状态或低电量状态下的帧率上限。
	//@TODO: Handle different limits for in-front-end or low-battery mode on mobile

	// 从平台离散档位中选择不超过已确认用户目标且受设备支持的最高帧率；无匹配时回退到设备默认值。
	// Choose the closest supported frame rate to the user desired setting without going over the device imposed limit
	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	const TArray<int32>& PossibleRates = PlatformSettings->MobileFrameRateLimits;
	const int32 LimitIndex = PossibleRates.FindLastByPredicate([this](const int32& TestRate) { return (TestRate <= MobileFrameRateLimit) && IsSupportedMobileFramePace(TestRate); });
	const int32 TargetFPS = PossibleRates.IsValidIndex(LimitIndex) ? PossibleRates[LimitIndex] : GetDefaultMobileFrameRate();

	UE_LOG(LogConsoleResponse, Log, TEXT("Setting frame pace to %d Hz."), TargetFPS);
	FPlatformRHIFramePacer::SetFramePace(TargetFPS);

	ClampMobileQuality();

	UpdateDynamicResFrameTime((float)TargetFPS);
}

// 将目标 FPS 换算为动态分辨率帧时间预算并写入控制台变量。
void ULyraSettingsLocal::UpdateDynamicResFrameTime(float TargetFPS)
{
	static IConsoleVariable* CVarDyResFrameTimeBudget = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.FrameTimeBudget"));
	if (CVarDyResFrameTimeBudget)
	{
		if (ensure(TargetFPS > 0.0f))
		{
			const float DyResFrameTimeBudget = 1000.0f / TargetFPS;
			CVarDyResFrameTimeBudget->Set(DyResFrameTimeBudget, ECVF_SetByGameSetting);
		}
	}
}

