// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "GameplayTagContainer.h"
#include "LyraPlatformEmulationSettings.generated.h"

struct FPropertyChangedEvent;

/**
 * 在 PIE 中模拟目标平台 Trait、平台名称、DeviceProfile 和平台相关性能策略。
 */
/**
 * Platform emulation settings
 */
UCLASS(config=EditorPerProjectUserSettings, MinimalAPI)
class ULyraPlatformEmulationSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	ULyraPlatformEmulationSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	//~End of UDeveloperSettings interface

	FName GetPretendBaseDeviceProfile() const;
	FName GetPretendPlatformName() const;

private:
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(Categories="Input,Platform.Trait"))
	FGameplayTagContainer AdditionalPlatformTraitsToEnable;

	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(Categories="Input,Platform.Trait"))
	FGameplayTagContainer AdditionalPlatformTraitsToSuppress;

	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(GetOptions=GetKnownPlatformIds))
	FName PretendPlatform;

	// 模拟 ULyraSettingsLocal 应用设备专属配置时所使用的基础 DeviceProfile。
	// The base device profile to pretend we are using when emulating device-specific device profiles applied from ULyraSettingsLocal
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(GetOptions=GetKnownDeviceProfiles, EditCondition=bApplyDeviceProfilesInPIE))
	FName PretendBaseDeviceProfile;

	// PIE 中是否应用桌面平台帧率设置；帧率限制是引擎全局状态，可能影响其他编辑器窗口。
	// 测试后台帧率限制时，可能还需关闭编辑器的“Use Less CPU when in Background”。
	// Do we apply desktop-style frame rate settings in PIE?
	// (frame rate limits are an engine-wide setting so it's not always desirable to have enabled in the editor)
	// You may also want to disable the editor preference "Use Less CPU when in Background" if testing background frame rate limits
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(ConsoleVariable="Lyra.Settings.ApplyFrameRateSettingsInPIE"))
	bool bApplyFrameRateSettingsInPIE = false;

	// PIE 中是否应用前端专用性能选项；其驱动的大多数性能和可扩展性设置是全局的，
	// 多 PIE 窗口分别处于前端和游戏时会相互覆盖，最终只能保留其中一套设置。
	// Do we apply front-end specific performance options in PIE?
	// Most engine performance/scalability settings they drive are global, so if one PIE window
	// is in the front-end and the other is in-game one will win and the other gets stuck with those settings
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(ConsoleVariable="Lyra.Settings.ApplyFrontEndPerformanceOptionsInPIE"))
	bool bApplyFrontEndPerformanceOptionsInPIE = false;

	// PIE 中是否应用由 Experience 或模拟平台选择出的 DeviceProfile。
	// Should we apply experience/platform emulated device profiles in PIE?
	UPROPERTY(EditAnywhere, config, Category=PlatformEmulation, meta=(InlineEditConditionToggle, ConsoleVariable="Lyra.Settings.ApplyDeviceProfilesInPIE"))
	bool bApplyDeviceProfilesInPIE = false;

#if WITH_EDITOR
public:
	// PIE 启动时提示当前启用或抑制的 Trait、模拟平台和 DeviceProfile 覆盖。
	// Called by the editor engine to let us pop reminder notifications when cheats are active
	LYRAGAME_API void OnPlayInEditorStarted() const;

private:
	// 最近一次已应用的平台模拟名称，用于撤销旧平台 Trait 后再切换。
	// The last pretend platform we applied
	FName LastAppliedPretendPlatform;

private:
	void ApplySettings();
	void ChangeActivePretendPlatform(FName NewPlatformName);
#endif

public:
	//~UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	virtual void PostInitProperties() override;
#endif
	//~End of UObject interface

private:
	UFUNCTION()
	TArray<FName> GetKnownPlatformIds() const;

	UFUNCTION()
	TArray<FName> GetKnownDeviceProfiles() const;

	void PickReasonableBaseDeviceProfile();
};
