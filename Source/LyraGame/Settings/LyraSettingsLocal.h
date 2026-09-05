// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"

#include "LyraSettingsLocal.generated.h"

enum class ECommonInputType : uint8;
enum class ELyraDisplayablePerformanceStat : uint8;
enum class ELyraStatDisplayMode : uint8;

class ULyraLocalPlayer;
class UObject;
class USoundControlBus;
class USoundControlBusMix;
struct FFrame;

USTRUCT()
struct FLyraScalabilitySnapshot
{
	GENERATED_BODY()

	FLyraScalabilitySnapshot();

	Scalability::FQualityLevels Qualities;
	bool bActive = false;
	bool bHasOverrides = false;
};

// 保存并应用与当前设备相关的用户设置，包括显示、画质、帧率节奏、音频设备和本地性能统计选项。
// 此类数据由 UGameUserSettings 持久化，不应承载需要按玩家账户跨设备同步的偏好。
/**
 * ULyraSettingsLocal
 */
UCLASS()
class ULyraSettingsLocal : public UGameUserSettings
{
	GENERATED_BODY()

public:

	ULyraSettingsLocal();

	static ULyraSettingsLocal* Get();

	//~UObject interface
	virtual void BeginDestroy() override;
	//~End of UObject interface

	//~UGameUserSettings interface
	virtual void SetToDefaults() override;
	virtual void LoadSettings(bool bForceReload) override;
	virtual void ConfirmVideoMode() override;
	virtual float GetEffectiveFrameRateLimit() override;
	virtual void ResetToCurrentSettings() override;
	virtual void ApplyNonResolutionSettings() override;
	virtual int32 GetOverallScalabilityLevel() const override;
	virtual void SetOverallScalabilityLevel(int32 Value) override;
	//~End of UGameUserSettings interface

	void OnExperienceLoaded();
	void OnHotfixDeviceProfileApplied();

	//////////////////////////////////////////////////////////////////
	// 前端菜单状态及其性能策略。
	// Frontend state

public:
	void SetShouldUseFrontendPerformanceSettings(bool bInFrontEnd);
protected:
	bool ShouldUseFrontendPerformanceSettings() const;
private:
	bool bInFrontEndForPerformancePurposes = false;

	//////////////////////////////////////////////////////////////////
	// HUD 性能统计与延迟测量设置。
	// Performance stats
public:
	/** 返回指定性能统计项当前在 HUD 中的显示方式。 */
	/** Returns the display mode for the specified performance stat */
	ELyraStatDisplayMode GetPerfStatDisplayState(ELyraDisplayablePerformanceStat Stat) const;
	
	/** 设置指定性能统计项在 HUD 中的显示方式。 */
	/** Sets the display mode for the specified performance stat */
	void SetPerfStatDisplayState(ELyraDisplayablePerformanceStat Stat, ELyraStatDisplayMode DisplayMode);

	/** 当任一性能统计项的显示方式改变或设置被重新应用时触发。 */
	/** Fired when the display state for a performance stat has changed, or the settings are applied */
	DECLARE_EVENT(ULyraSettingsLocal, FPerfStatSettingsChanged);
	FPerfStatSettingsChanged& OnPerfStatDisplayStateChanged() { return PerfStatSettingsChangedEvent; }

	// 延迟闪烁标记：在画面中产生可供外部设备测量端到端输入延迟的视觉信号。
	// Latency flash indicators
	static bool DoesPlatformSupportLatencyMarkers();
	
	DECLARE_EVENT(ULyraSettingsLocal, FLatencyFlashInidicatorSettingChanged);
	UFUNCTION()
	void SetEnableLatencyFlashIndicators(const bool bNewVal);
	UFUNCTION()
	bool GetEnableLatencyFlashIndicators() const { return bEnableLatencyFlashIndicators; }
	FLatencyFlashInidicatorSettingChanged& OnLatencyFlashInidicatorSettingsChangedEvent() { return LatencyFlashInidicatorSettingsChangedEvent; }

	// 延迟统计跟踪：通过 ILatencyMarkerModule 收集游戏、渲染及总延迟等指标。
	// Latency tracking stats
	static bool DoesPlatformSupportLatencyTrackingStats();
	
	DECLARE_EVENT(ULyraSettingsLocal, FLatencyStatEnabledSettingChanged);
	FLatencyStatEnabledSettingChanged& OnLatencyStatIndicatorSettingsChangedEvent() { return LatencyStatIndicatorSettingsChangedEvent; }
	
	UFUNCTION()
	void SetEnableLatencyTrackingStats(const bool bNewVal);
	UFUNCTION()
	bool GetEnableLatencyTrackingStats() const { return bEnableLatencyTrackingStats; }

private:

	void ApplyLatencyTrackingStatSetting();
	
	// 记录各性能统计项在 HUD 中采用的显示方式。
	// List of stats to display in the HUD
	UPROPERTY(Config)
	TMap<ELyraDisplayablePerformanceStat, ELyraStatDisplayMode> DisplayStatList;

	// 供性能统计控件容器绑定的设置变更事件。
	// Event for display stat widget containers to bind to
	FPerfStatSettingsChanged PerfStatSettingsChangedEvent;

	// 为 true 时启用延迟闪烁标记，以便测量输入延迟。
	// If true, enable latency flash markers which can be used to measure input latency.
	UPROPERTY(Config)
	bool bEnableLatencyFlashIndicators = false;

	// 延迟闪烁标记设置改变时触发，供玩家输入逻辑绑定。
	// Event for when the latency flash indicator setting had changed for player input to bind to.
	FLatencyFlashInidicatorSettingChanged LatencyFlashInidicatorSettingsChangedEvent;

	// 延迟统计启用状态改变时触发。
	// Event for when the latency stats being toggled on or off has changed
	FLatencyStatEnabledSettingChanged LatencyStatIndicatorSettingsChangedEvent;

	// 为 true 时通过 ILatencyMarkerModule 跟踪延迟数据，从而允许显示延迟相关性能指标。
	// 默认值由平台能力决定：支持延迟统计的平台为 true，否则为 false。
	// If true, then the game will track latency stats via ILatencyMarkerModule modules.
	// This enables you to view some more latency oriented performance stats.
	// The default value is set to true if the platform supports it, false otherwise.
	UPROPERTY(Config)
	bool bEnableLatencyTrackingStats;

	//////////////////////////////////////////////////////////////////
	// 显示亮度与 Gamma 设置。
	// Brightness/Gamma
public:
	UFUNCTION()
	float GetDisplayGamma() const;
	UFUNCTION()
	void SetDisplayGamma(float InGamma);

private:
	void ApplyDisplayGamma();
	
	UPROPERTY(Config)
	float DisplayGamma = 2.2f;

	//////////////////////////////////////////////////////////////////
	// 通用显示与帧率上限设置。
	// Display
public:
	UFUNCTION()
	float GetFrameRateLimit_OnBattery() const;
	UFUNCTION()
	void SetFrameRateLimit_OnBattery(float NewLimitFPS);

	UFUNCTION()
	float GetFrameRateLimit_InMenu() const;
	UFUNCTION()
	void SetFrameRateLimit_InMenu(float NewLimitFPS);

	UFUNCTION()
	float GetFrameRateLimit_WhenBackgrounded() const;
	UFUNCTION()
	void SetFrameRateLimit_WhenBackgrounded(float NewLimitFPS);

	UFUNCTION()
	float GetFrameRateLimit_Always() const;
	UFUNCTION()
	void SetFrameRateLimit_Always(float NewLimitFPS);

	UFUNCTION()
	float GetDynamicResolutionFrameRateTarget() const;
	UFUNCTION()
	void SetDynamicResolutionFrameRateTarget(float NewDynamicResolutionFPS);

protected:
	void UpdateEffectiveFrameRateLimit();

private:
	UPROPERTY(Config)
	float FrameRateLimit_OnBattery;
	UPROPERTY(Config)
	float FrameRateLimit_InMenu;
	UPROPERTY(Config)
	float FrameRateLimit_WhenBackgrounded;

	//////////////////////////////////////////////////////////////////
	// 移动端帧率档位及画质限制设置。
	// Display - Mobile quality settings
public:
	
	static int32 GetDefaultMobileFrameRate();
	static int32 GetMaxMobileFrameRate();

	static bool IsSupportedMobileFramePace(int32 TestFPS);

	// 返回当前设备配置首次开始限制整体画质等级的帧率；若所有帧率均不限制则返回 INDEX_NONE。
	// Returns the first frame rate at which overall quality is restricted/limited by the current device profile
	int32 GetFirstFrameRateWithQualityLimit() const;

	// 返回存在帧率上限时所需的最低整体画质等级；没有此类限制时返回 -1。
	// Returns the lowest quality at which there's a limit on the overall frame rate (or -1 if there is no limit)
	int32 GetLowestQualityWithFrameRateLimit() const;

	void ResetToMobileDeviceDefaults();

	int32 GetMaxSupportedOverallQualityLevel() const;

private:
	void SetMobileFPSMode(int32 NewLimitFPS);

	void ClampMobileResolutionQuality(int32 TargetFPS);
	void RemapMobileResolutionQuality(int32 FromFPS, int32 ToFPS);

	void ClampMobileFPSQualityLevels(bool bWriteBack);
	void ClampMobileQuality();
	
	int32 GetHighestLevelOfAnyScalabilityChannel() const;

	/* 按当前活动可伸缩性模式的覆盖值修改输入画质等级。 */
	/* Modifies the input levels based on the active mode's overrides */
	void OverrideQualityLevelsToScalabilityMode(const FLyraScalabilitySnapshot& InMode, Scalability::FQualityLevels& InOutLevels);

	/* 按当前设备配置允许的默认上限约束输入画质等级。 */
	/* Clamps the input levels based on the active device profile's default allowed levels */
	void ClampQualityLevelsToDeviceProfile(const Scalability::FQualityLevels& ClampLevels, Scalability::FQualityLevels& InOutLevels);

public:
	int32 GetDesiredMobileFrameRateLimit() const { return DesiredMobileFrameRateLimit; }

	void SetDesiredMobileFrameRateLimit(int32 NewLimitFPS);

private:
	UPROPERTY(Config)
	int32 MobileFrameRateLimit = 30;

	FLyraScalabilitySnapshot DeviceDefaultScalabilitySettings;

	bool bSettingOverallQualityGuard = false;

	int32 DesiredMobileFrameRateLimit = 0;

private:

	//////////////////////////////////////////////////////////////////
	// 主机式设备配置画质预设。
	// Display - Console quality presets
public:
	UFUNCTION()
	FString GetDesiredDeviceProfileQualitySuffix() const;
	UFUNCTION()
	void SetDesiredDeviceProfileQualitySuffix(const FString& InDesiredSuffix);

protected:
	/** 根据当前游戏模式更新设备配置覆盖、帧率模式及其关联的画质限制。 */
	/** Updates device profiles, FPS mode etc for the current game mode */
	void UpdateGameModeDeviceProfileAndFps();

	void UpdateConsoleFramePacing();
	void UpdateDesktopFramePacing();
	void UpdateMobileFramePacing();

	void UpdateDynamicResFrameTime(float TargetFPS);

private:
	UPROPERTY(Transient)
	FString DesiredUserChosenDeviceProfileSuffix;

	UPROPERTY(Transient)
	FString CurrentAppliedDeviceProfileOverrideSuffix;

	UPROPERTY(config)
	FString UserChosenDeviceProfileSuffix;

	//////////////////////////////////////////////////////////////////
	// 分类别音量与耳机音频模式设置。
	// Audio - Volume
public:
	DECLARE_EVENT_OneParam(ULyraSettingsLocal, FAudioDeviceChanged, const FString& /*DeviceId*/);
	FAudioDeviceChanged OnAudioOutputDeviceChanged;

public:
	/** 返回当前实际生效的耳机空间化模式（HRTF）状态。 **/
	/** Returns if we're using headphone mode (HRTF) **/
	UFUNCTION()
	bool IsHeadphoneModeEnabled() const;

	/** 设置期望的耳机空间化模式（HRTF）；若 au.DisableBinauralSpatialization 强制禁用双耳空间化，该值不会实际生效。 */
	/** Enables or disables headphone mode (HRTF) - NOTE this setting will be overruled if au.DisableBinauralSpatialization is set */
	UFUNCTION()
	void SetHeadphoneModeEnabled(bool bEnabled);

	/** 返回平台是否允许用户切换耳机模式；平台强制开启或关闭时返回 false。 */
	/** Returns if we can enable/disable headphone mode (i.e., if it's not forced on or off by the platform) */
	UFUNCTION()
	bool CanModifyHeadphoneModeEnabled() const;

public:
	/** 用户期望使用的耳机模式状态；受平台或控制台变量约束时可能与实际状态不同。 **/
	/** Whether we *want* to use headphone mode (HRTF); may or may not actually be applied **/
	UPROPERTY(Transient)
	bool bDesiredHeadphoneMode;

private:
	/** 已保存并用于实际应用的耳机模式（HRTF）状态。 **/
	/** Whether to use headphone mode (HRTF) **/
	UPROPERTY(config)
	bool bUseHeadphoneMode;

public:
	/** 返回是否启用了高动态范围音频模式（HDR Audio）。 **/
	/** Returns if we're using High Dynamic Range Audio mode (HDR Audio) **/
	UFUNCTION()
	bool IsHDRAudioModeEnabled() const;

	/** 启用或禁用高动态范围音频模式（HDR Audio）。 */
	/** Enables or disables High Dynamic Range Audio mode (HDR Audio) */
	UFUNCTION()
	void SetHDRAudioModeEnabled(bool bEnabled);

	/** 保存是否使用高动态范围音频模式（HDR Audio）。 **/
	/** Whether to use High Dynamic Range Audio mode (HDR Audio) **/
	UPROPERTY(config)
	bool bUseHDRAudioMode;

public:
	/** 当前平台具备运行自动硬件性能基准测试的条件时返回 true。 */
	/** Returns true if this platform can run the auto benchmark */
	UFUNCTION(BlueprintCallable, Category = Settings)
	bool CanRunAutoBenchmark() const;

	/** 当前用户从未完成自动基准测试、应在启动时运行时返回 true。 */
	/** Returns true if this user should run the auto benchmark as it has never been run */
	UFUNCTION(BlueprintCallable, Category = Settings)
	bool ShouldRunAutoBenchmarkAtStartup() const;

	/** 运行自动基准测试并应用建议画质，可选择立即保存结果。 */
	/** Run the auto benchmark, optionally saving right away */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void RunAutoBenchmark(bool bSaveImmediately);

	/** 仅应用可伸缩性画质设置，不处理分辨率等其他用户设置。 */
	/** Apply just the quality scalability settings */
	void ApplyScalabilitySettings();

	UFUNCTION()
	float GetOverallVolume() const;
	UFUNCTION()
	void SetOverallVolume(float InVolume);

	UFUNCTION()
	float GetMusicVolume() const;
	UFUNCTION()
	void SetMusicVolume(float InVolume);

	UFUNCTION()
	float GetSoundFXVolume() const;
	UFUNCTION()
	void SetSoundFXVolume(float InVolume);

	UFUNCTION()
	float GetDialogueVolume() const;
	UFUNCTION()
	void SetDialogueVolume(float InVolume);

	UFUNCTION()
	float GetVoiceChatVolume() const;
	UFUNCTION()
	void SetVoiceChatVolume(float InVolume);

	//////////////////////////////////////////////////////////////////
	// 音频输出设备设置。
	// Audio - Sound
public:
	/** 返回用户选择的音频输出设备 ID。 */
	/** Returns the user's audio device id */
	UFUNCTION()
	FString GetAudioOutputDeviceId() const { return AudioOutputDeviceId; }

	/** 按设备 ID 切换用户的音频输出设备。 */
	/** Sets the user's audio device by id */
	UFUNCTION()
	void SetAudioOutputDeviceId(const FString& InAudioOutputDeviceId);

private:
	UPROPERTY(Config)
	FString AudioOutputDeviceId;
	
	void SetVolumeForSoundClass(FName ChannelName, float InVolume);
	

	//////////////////////////////////////////////////////////////////
	// UI 安全区缩放设置。
	// Safezone
public:
	UFUNCTION()
	bool IsSafeZoneSet() const { return SafeZoneScale != -1; }
	UFUNCTION()
	float GetSafeZone() const { return SafeZoneScale >= 0 ? SafeZoneScale : 0; }
	UFUNCTION()
	void SetSafeZone(float Value) { SafeZoneScale = Value; ApplySafeZoneScale(); }

	void ApplySafeZoneScale();
private:
	void SetVolumeForControlBus(USoundControlBus* InSoundControlBus, float InVolume);

	//////////////////////////////////////////////////////////////////
	// 输入设备显示类型与按键配置设置。
	// Keybindings
public:
	
	// 设置界面采用的手柄图标与命名方案。同一平台可能支持多种手柄，例如 Win64 可同时支持 Xbox 与 PlayStation 手柄。
	// Sets the controller representation to use, a single platform might support multiple kinds of controllers.  For
	// example, Win64 games could be played with both an XBox or Playstation controller.
	UFUNCTION()
	void SetControllerPlatform(const FName InControllerPlatform);
	UFUNCTION()
	FName GetControllerPlatform() const;

private:
	void LoadUserControlBusMix();

	UPROPERTY(Config)
	float OverallVolume = 1.0f;
	UPROPERTY(Config)
	float MusicVolume = 1.0f;
	UPROPERTY(Config)
	float SoundFXVolume = 1.0f;
	UPROPERTY(Config)
	float DialogueVolume = 1.0f;
	UPROPERTY(Config)
	float VoiceChatVolume = 1.0f;

	UPROPERTY(Transient)
	TMap<FName/*SoundClassName*/, TObjectPtr<USoundControlBus>> ControlBusMap;

	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> ControlBusMix = nullptr;

	UPROPERTY(Transient)
	bool bSoundControlBusMixLoaded;

	UPROPERTY(Config)
	float SafeZoneScale = -1;

	// 玩家当前所用手柄的类型名称。该名称对应当前平台注册的 UCommonInputBaseControllerData，
	// 各平台可在 <Platform>Game.ini 的 +ControllerData=... 条目中注册可用的手柄数据。
	/**
	 * The name of the controller the player is using.  This is maps to the name of a UCommonInputBaseControllerData
	 * that is available on this current platform.  The gamepad data are registered per platform, you'll find them
	 * in <Platform>Game.ini files listed under +ControllerData=...
	 */
	UPROPERTY(Config)
	FName ControllerPlatform;

	UPROPERTY(Config)
	FName ControllerPreset = TEXT("Default");

	/** 用户当前选择的输入配置名称。 */
	/** The name of the current input config that the user has selected. */
	UPROPERTY(Config)
	FName InputConfigName = TEXT("Default");

	// 自动录像及录像保留数量设置。
	// Replays
public:

	UFUNCTION()
	bool ShouldAutoRecordReplays() const { return bShouldAutoRecordReplays; }
	UFUNCTION()
	void SetShouldAutoRecordReplays(bool bEnabled) { bShouldAutoRecordReplays = bEnabled;}

	UFUNCTION()
	int32 GetNumberOfReplaysToKeep() const { return NumberOfReplaysToKeep; }
	UFUNCTION()
	void SetNumberOfReplaysToKeep(int32 InNumberOfReplays) { NumberOfReplaysToKeep = InNumberOfReplays; }

private:

	UPROPERTY(Config)
	bool bShouldAutoRecordReplays = false;

	UPROPERTY(Config)
	int32 NumberOfReplaysToKeep = 5;

private:
	void OnAppActivationStateChanged(bool bIsActive);
	void ReapplyThingsDueToPossibleDeviceProfileChange();

private:
	FDelegateHandle OnApplicationActivationStateChangedHandle;
};
