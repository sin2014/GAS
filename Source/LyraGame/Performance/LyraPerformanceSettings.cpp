// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerformanceSettings.h"

#include "Engine/PlatformSettingsManager.h"
#include "Misc/EnumRange.h"
#include "Performance/LyraPerformanceStatTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerformanceSettings)

//////////////////////////////////////////////////////////////////////

// 初始化移动端可选帧率档位为 20 至 120 FPS 的预设集合。
ULyraPlatformSpecificRenderingSettings::ULyraPlatformSpecificRenderingSettings()
{
	MobileFrameRateLimits.Append({ 20, 30, 45, 60, 90, 120 });
}

// 从平台设置管理器取得当前平台渲染设置，并要求结果必须存在。
const ULyraPlatformSpecificRenderingSettings* ULyraPlatformSpecificRenderingSettings::Get()
{
	ULyraPlatformSpecificRenderingSettings* Result = UPlatformSettingsManager::Get().GetSettingsForPlatform<ThisClass>();
	check(Result);
	return Result;
}

//////////////////////////////////////////////////////////////////////

// 初始化逐平台渲染设置、桌面帧率档位和默认允许全部统计项的用户可见分组。
ULyraPerformanceSettings::ULyraPerformanceSettings()
{
	PerPlatformSettings.Initialize(ULyraPlatformSpecificRenderingSettings::StaticClass());

	CategoryName = TEXT("Game");

	DesktopFrameRateLimits.Append({ 30, 60, 120, 144, 160, 165, 180, 200, 240, 360 });

	// 默认分组没有平台可见性限制，并允许全部性能统计项；平台配置可覆盖为更受限的分组。
	// Default to all stats are allowed
	FLyraPerformanceStatGroup& StatGroup = UserFacingPerformanceStats.AddDefaulted_GetRef();
	for (ELyraDisplayablePerformanceStat PerfStat : TEnumRange<ELyraDisplayablePerformanceStat>())
	{
		StatGroup.AllowedStats.Add(PerfStat);
	}
}

