// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "Engine/PlatformSettings.h"
#include "GameplayTagContainer.h"

#include "LyraPerformanceSettings.generated.h"

enum class ELyraDisplayablePerformanceStat : uint8;

class UObject;

// 描述设置 UI 中可选的一种平台设备配置变体。
// Describes one platform-specific device profile variant that the user can choose from in the UI
USTRUCT()
struct FLyraQualityDeviceProfileVariant
{
	GENERATED_BODY()

	// 此设备配置变体在选项界面中的显示名称。
	// The display name for this device profile variant (visible in the options screen)
	UPROPERTY(EditAnywhere)
	FText DisplayName;

	// 追加到当前平台基础设备配置名后的后缀，用于组合实际配置名称。
	// The suffix to append to the base device profile name for the current platform
	UPROPERTY(EditAnywhere)
	FString DeviceProfileSuffix;

	// 启用此模式所需的最低显示器刷新率；例如要求 120 Hz 时，连接 60 Hz 显示器不会提供该选项。
	// The minimum required refresh rate to enable this mode
	// (e.g., if this is set to 120 Hz and the device is connected
	// to a 60 Hz display, it won't be available)
	UPROPERTY(EditAnywhere)
	int32 MinRefreshRate = 0;
};

// 一组可由用户启用的性能统计项；只有平台特征通过可见性查询时才在设置中提供。
// Describes a set of performance stats that the user can enable in settings,
// predicated on passing a visibility query on platform traits
USTRUCT()
struct FLyraPerformanceStatGroup
{
	GENERATED_BODY()

	// 用平台特征判断此组统计项是否具备显示条件的查询。
	// A query on platform traits to determine whether or not it will be possible
	// to show a set of stats
	UPROPERTY(EditAnywhere, meta=(Categories = "Input,Platform.Trait"))
	FGameplayTagQuery VisibilityQuery;

	// 可见性查询通过后允许用户启用的统计项集合。
	// The set of stats to allow if the query passes
	UPROPERTY(EditAnywhere)
	TSet<ELyraDisplayablePerformanceStat> AllowedStats;
};

// 定义平台如何控制帧率节奏，以及向用户开放哪些整体图形设置。
// How hare frame pacing and overall graphics settings controlled/exposed for the platform?
UENUM()
enum class ELyraFramePacingMode : uint8
{
	// 桌面式：用户手动设置帧率上限，并可选择是否启用垂直同步。
	// Manual frame rate limits, user is allowed to choose whether or not to lock to vsync
	DesktopStyle,

	// 主机式：设备配置通过 Present 间隔决定帧同步和帧率限制。
	// Limits handled by choosing present intervals driven by device profiles
	ConsoleStyle,

	// 移动式：用户从设备配置和硬件实际允许的离散帧率档位中选择。
	// Limits handled by a user-facing choice of frame rate from among ones allowed by device profiles for the specific device
	MobileStyle
};

UCLASS(config=Game, defaultconfig)
class ULyraPlatformSpecificRenderingSettings : public UPlatformSettings
{
	GENERATED_BODY()

public:
	ULyraPlatformSpecificRenderingSettings();

	// 通过平台设置路由取得当前平台对应的渲染性能设置对象。
	// Helper method to get the performance settings object, directed via platform settings
	static const ULyraPlatformSpecificRenderingSettings* Get();

public:
	// 默认设备配置变体后缀。除当前平台只有一种变体外，通常应对应 UserFacingDeviceProfileOptions 中的一项。
	// 此值一般由平台专用 ini 配置，而不是由 UI 直接写入。
	// The default variant suffix to append, should typically be a member of
	// UserFacingDeviceProfileOptions unless there is only one for the current platform
	//
	// Note that this will usually be set from platform-specific ini files, not via the UI
	UPROPERTY(EditAnywhere, Config, Category=DeviceProfiles)
	FString DefaultDeviceProfileSuffix;

	// 设置中允许用户选择的设备配置变体，必须按目标帧率从低到高排序。
	// 当前显示器不满足所选变体的刷新率要求时，代码会向前回退到首个可用的低档变体。
	// 此列表一般由平台专用 ini 配置，而不是由 UI 直接写入。
	// The list of device profile variations to allow users to choose from in settings
	//
	// These should be sorted from slowest to fastest by target frame rate:
	//   If the current display doesn't support a user chosen refresh rate, we'll try
	//   previous entries until we find one that works
	//
	// Note that this will usually be set from platform-specific ini files, not via the UI
	UPROPERTY(EditAnywhere, Config, Category=DeviceProfiles)
	TArray<FLyraQualityDeviceProfileVariant> UserFacingDeviceProfileOptions;

	// 平台是否支持分别调整阴影、纹理等独立画质通道。
	// Does the platform support granular video quality settings?
	UPROPERTY(EditAnywhere, Config, Category=VideoSettings)
	bool bSupportsGranularVideoQualitySettings = true;

	// 平台是否允许运行自动画质基准测试；通常只有支持独立画质通道时才应启用。
	// Does the platform support running the automatic quality benchmark (typically this should only be true if bSupportsGranularVideoQualitySettings is also true)
	UPROPERTY(EditAnywhere, Config, Category=VideoSettings)
	bool bSupportsAutomaticVideoQualityBenchmark = true;

	// 当前平台采用的帧率节奏控制模式。
	// How is frame pacing controlled
	UPROPERTY(EditAnywhere, Config, Category=VideoSettings)
	ELyraFramePacingMode FramePacingMode = ELyraFramePacingMode::DesktopStyle;

	// 移动端可展示的候选帧率档位；最终还受设备配置中的 Lyra.DeviceProfile.Mobile.MaxFrameRate
	// 以及平台帧节奏器实际支持能力限制。
	// Potential frame rates to display for mobile
	// Note: This is further limited by Lyra.DeviceProfile.Mobile.MaxFrameRate from the
	// platform-specific device profile and what the platform frame pacer reports as supported
	UPROPERTY(EditAnywhere, Config, Category=VideoSettings, meta=(EditCondition="FramePacingMode==ELyraFramePacingMode::MobileStyle", ForceUnits=Hz))
	TArray<int32> MobileFrameRateLimits;
};

//////////////////////////////////////////////////////////////////////

// Lyra 项目级性能设置，定义桌面帧率选项以及可向用户开放的性能统计项。
/**
 * Project-specific performance profile settings.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Lyra Performance Settings"))
class ULyraPerformanceSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	ULyraPerformanceSettings();

private:
	// 仅用于在项目设置中暴露各平台配置供编辑，运行时代码无需直接访问此成员。
	// This is a special helper to expose the per-platform settings so they can be edited in the project settings
	// It never needs to be directly accessed
	UPROPERTY(EditAnywhere, Category = "PlatformSpecific")
	FPerPlatformSettings PerPlatformSettings;

public:
	// 桌面平台各类“帧率上限”视频设置向用户提供的候选值。
	// The list of frame rates to allow users to choose from in the various
	// "frame rate limit" video settings on desktop platforms
	UPROPERTY(EditAnywhere, Config, Category=Performance, meta=(ForceUnits=Hz))
	TArray<int32> DesktopFrameRateLimits;

	// 选项界面可按平台特征向用户开放的性能统计分组。
	// The list of performance stats that can be enabled in Options by the user
	UPROPERTY(EditAnywhere, Config, Category=Stats)
	TArray<FLyraPerformanceStatGroup> UserFacingPerformanceStats;
};
