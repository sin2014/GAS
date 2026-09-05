// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "LyraAudioMixEffectsSubsystem.generated.h"

#define UE_API LYRAGAME_API

class FSubsystemCollectionBase;
class UObject;
class USoundControlBus;
class USoundControlBusMix;
class USoundEffectSubmixPreset;
class USoundSubmix;
class UWorld;

USTRUCT()
struct FLyraAudioSubmixEffectsChain
{
	GENERATED_BODY()

	// 要应用效果链覆盖的 SoundSubmix。
	// Submix on which to apply the Submix Effect Chain Override
	UPROPERTY(Transient)
	TObjectPtr<USoundSubmix> Submix = nullptr;

	// 覆盖用效果预设链，按数组索引顺序处理。
	// Submix Effect Chain Override (Effects processed in Array index order)
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundEffectSubmixPreset>> SubmixEffectChain;
};

/**
 * 自动加载并激活默认/用户 ControlBusMix，把已保存音量应用到用户混音，
 * 同时根据 HDR 音频偏好切换 Submix 效果链，并随加载界面生命周期启停专用混音。
 */
/**
 * This subsystem is meant to automatically engage default and user control bus mixes
 * to retrieve previously saved user settings and apply them to the activated user mix.
 * Additionally, this subsystem will automatically apply HDR/LDR Audio Submix Effect Chain Overrides
 * based on the user's preference for HDR Audio. Submix Effect Chain Overrides are defined in the
 * Lyra Audio Settings.
 */
UCLASS(MinimalAPI)
class ULyraAudioMixEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem implementation Begin
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	// USubsystem implementation End

	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** 所有 WorldSubsystem 初始化后加载音频设置资源，并绑定加载界面状态。 */
	/** Called once all UWorldSubsystems have been initialized */
	UE_API virtual void PostInitialize() override;

	/** World 开始玩法前激活基础/用户混音，写入用户音量并应用动态范围效果链。 */
	/** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
	UE_API virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** 在 HDR 与 LDR Submix 效果链覆盖之间切换，并清理不再使用的旧覆盖。 */
	/** Set whether the HDR Audio Submix Effect Chain Override settings are applied */
	UE_API void ApplyDynamicRangeEffectsChains(bool bHDRAudio);
	
protected:
	UE_API void OnLoadingScreenStatusChanged(bool bShowingLoadingScreen);
	UE_API void ApplyOrRemoveLoadingScreenMix(bool bWantsLoadingScreenMix);
	
	// 判断指定 WorldType 是否需要创建本音频子系统。
	// Called when determining whether to create this Subsystem
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 从 LyraAudioSettings 加载的默认基础 ControlBusMix。
	// Default Sound Control Bus Mix retrieved from the Lyra Audio Settings
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> DefaultBaseMix = nullptr;

	// 加载界面显示期间临时激活的 ControlBusMix。
	// Loading Screen Sound Control Bus Mix retrieved from the Lyra Audio Settings
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> LoadingScreenMix = nullptr;

	// 承载用户各音量设置的 ControlBusMix。
	// User Sound Control Bus Mix retrieved from the Lyra Audio Settings
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBusMix> UserMix = nullptr;

	// 与 LyraSettingsLocal 总音量设置关联的 ControlBus。
	// Overall Sound Control Bus retrieved from the Lyra Audio Settings and linked to the UI and game settings in LyraSettingsLocal
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> OverallControlBus = nullptr;

	// 与 LyraSettingsLocal 音乐音量设置关联的 ControlBus。
	// Music Sound Control Bus retrieved from the Lyra Audio Settings and linked to the UI and game settings in LyraSettingsLocal
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> MusicControlBus = nullptr;

	// 与 LyraSettingsLocal 音效音量设置关联的 ControlBus。
	// SoundFX Sound Control Bus retrieved from the Lyra Audio Settings and linked to the UI and game settings in LyraSettingsLocal
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> SoundFXControlBus = nullptr;

	// 与 LyraSettingsLocal 对话音量设置关联的 ControlBus。
	// Dialogue Sound Control Bus retrieved from the Lyra Audio Settings and linked to the UI and game settings in LyraSettingsLocal
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> DialogueControlBus = nullptr;

	// 与 LyraSettingsLocal 语音聊天音量设置关联的 ControlBus。
	// VoiceChat Sound Control Bus retrieved from the Lyra Audio Settings and linked to the UI and game settings in LyraSettingsLocal
	UPROPERTY(Transient)
	TObjectPtr<USoundControlBus> VoiceChatControlBus = nullptr;

	// HDR 音频启用时应用的 Submix 效果链覆盖。
	// Submix Effect Chain Overrides to apply when HDR Audio is turned on
	UPROPERTY(Transient)
	TArray<FLyraAudioSubmixEffectsChain> HDRSubmixEffectChain;

	// HDR 音频关闭时应用的 LDR Submix 效果链覆盖。
	// Submix Effect hain Overrides to apply when HDR Audio is turned off
	UPROPERTY(Transient)
	TArray<FLyraAudioSubmixEffectsChain> LDRSubmixEffectChain;

	bool bAppliedLoadingScreenMix = false;
};

#undef UE_API
