// Copyright Epic Games, Inc. All Rights Reserved.


#include "Audio/LyraAudioMixEffectsSubsystem.h"

#include "AudioMixerBlueprintLibrary.h"
#include "AudioModulationStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LoadingScreenManager.h"
#include "LyraAudioSettings.h"
#include "Settings/LyraSettingsLocal.h"
#include "Sound/SoundEffectSubmix.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAudioMixEffectsSubsystem)

class FSubsystemCollectionBase;

// 执行 WorldSubsystem 基础初始化；音频资源延迟到 PostInitialize 加载。
void ULyraAudioMixEffectsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

// 解除 LoadingScreenManager 可见性委托并停用 LoadingScreenMix，再执行子系统清理。
void ULyraAudioMixEffectsSubsystem::Deinitialize()
{
	if (ULoadingScreenManager* LoadingScreenManager = UGameInstance::GetSubsystem<ULoadingScreenManager>(GetWorld()->GetGameInstance()))
	{
		LoadingScreenManager->OnLoadingScreenVisibilityChangedDelegate().RemoveAll(this);
		ApplyOrRemoveLoadingScreenMix(false);
	}

	Super::Deinitialize();
}

// 仅当父类允许且 Outer 所属 WorldType 为 Game 或 PIE 时创建音频混音子系统。
bool ULyraAudioMixEffectsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	bool bShouldCreateSubsystem = Super::ShouldCreateSubsystem(Outer);

	if (Outer)
	{
		if (UWorld* World = Outer->GetWorld())
		{
			bShouldCreateSubsystem = DoesSupportWorldType(World->WorldType) && bShouldCreateSubsystem;
		}
	}

	return bShouldCreateSubsystem;
}

// 同步加载基础、加载界面、用户 ControlBusMix、各音量 Bus 及 HDR/LDR Submix 链，并绑定当前加载界面状态。
void ULyraAudioMixEffectsSubsystem::PostInitialize()
{
	Super::PostInitialize();

	if (const ULyraAudioSettings* LyraAudioSettings = GetDefault<ULyraAudioSettings>())
	{
		if (UObject* ObjPath = LyraAudioSettings->DefaultControlBusMix.TryLoad())
		{
			if (USoundControlBusMix* SoundControlBusMix = Cast<USoundControlBusMix>(ObjPath))
			{
				DefaultBaseMix = SoundControlBusMix;
			}
			else
			{
				ensureMsgf(SoundControlBusMix, TEXT("Default Control Bus Mix reference missing from Lyra Audio Settings."));
			}
		}

		if (UObject* ObjPath = LyraAudioSettings->LoadingScreenControlBusMix.TryLoad())
		{
			if (USoundControlBusMix* SoundControlBusMix = Cast<USoundControlBusMix>(ObjPath))
			{
				LoadingScreenMix = SoundControlBusMix;
			}
			else
			{
				ensureMsgf(SoundControlBusMix, TEXT("Loading Screen Control Bus Mix reference missing from Lyra Audio Settings."));
			}
		}

		if (UObject* ObjPath = LyraAudioSettings->UserSettingsControlBusMix.TryLoad())
		{
			if (USoundControlBusMix* SoundControlBusMix = Cast<USoundControlBusMix>(ObjPath))
			{
				UserMix = SoundControlBusMix;
			}
			else
			{
				ensureMsgf(SoundControlBusMix, TEXT("User Control Bus Mix reference missing from Lyra Audio Settings."));
			}
		}

		if (UObject* ObjPath = LyraAudioSettings->OverallVolumeControlBus.TryLoad())
		{
			if (USoundControlBus* SoundControlBus = Cast<USoundControlBus>(ObjPath))
			{
				OverallControlBus = SoundControlBus;
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
			}
			else
			{
				ensureMsgf(SoundControlBus, TEXT("VoiceChat Control Bus reference missing from Lyra Audio Settings."));
			}
		}

		// 同步加载 HDR 模式下的 Submix 及其效果预设链。
		// Load HDR Submix Effect Chain
		for (const FLyraSubmixEffectChainMap& SoftSubmixEffectChain : LyraAudioSettings->HDRAudioSubmixEffectChain)
		{
			FLyraAudioSubmixEffectsChain NewEffectChain;

			if (UObject* SubmixObjPath = SoftSubmixEffectChain.Submix.LoadSynchronous())
			{
				if (USoundSubmix* Submix = Cast<USoundSubmix>(SubmixObjPath))
				{
					NewEffectChain.Submix = Submix;
					TArray<USoundEffectSubmixPreset*> NewPresetChain;

					for (const TSoftObjectPtr<USoundEffectSubmixPreset>& SoftEffect : SoftSubmixEffectChain.SubmixEffectChain)
					{
						if (UObject* EffectObjPath = SoftEffect.LoadSynchronous())
						{
							if (USoundEffectSubmixPreset* SubmixPreset = Cast<USoundEffectSubmixPreset>(EffectObjPath))
							{
								NewPresetChain.Add(SubmixPreset);
							}
						}
					}

					NewEffectChain.SubmixEffectChain.Append(NewPresetChain);
				}
			}

			HDRSubmixEffectChain.Add(NewEffectChain);
		}

		// 同步加载 LDR 模式下的 Submix 及其效果预设链。
		// Load LDR Submix Effect Chain
		for (const FLyraSubmixEffectChainMap& SoftSubmixEffectChain : LyraAudioSettings->LDRAudioSubmixEffectChain)
		{
			FLyraAudioSubmixEffectsChain NewEffectChain;

			if (UObject* SubmixObjPath = SoftSubmixEffectChain.Submix.LoadSynchronous())
			{
				if (USoundSubmix* Submix = Cast<USoundSubmix>(SubmixObjPath))
				{
					NewEffectChain.Submix = Submix;
					TArray<USoundEffectSubmixPreset*> NewPresetChain;

					for (const TSoftObjectPtr<USoundEffectSubmixPreset>& SoftEffect : SoftSubmixEffectChain.SubmixEffectChain)
					{
						if (UObject* EffectObjPath = SoftEffect.LoadSynchronous())
						{
							if (USoundEffectSubmixPreset* SubmixPreset = Cast<USoundEffectSubmixPreset>(EffectObjPath))
							{
								NewPresetChain.Add(SubmixPreset);
							}
						}
					}

					NewEffectChain.SubmixEffectChain.Append(NewPresetChain);
				}
			}

			LDRSubmixEffectChain.Add(NewEffectChain);
		}
	}

	// 绑定加载界面可见性变化，并立即同步当前 LoadingScreenMix 状态。
	// Register with the loading screen manager
	if (ULoadingScreenManager* LoadingScreenManager = UGameInstance::GetSubsystem<ULoadingScreenManager>(GetWorld()->GetGameInstance()))
	{
		LoadingScreenManager->OnLoadingScreenVisibilityChangedDelegate().AddUObject(this, &ThisClass::OnLoadingScreenStatusChanged);
		ApplyOrRemoveLoadingScreenMix(LoadingScreenManager->GetLoadingScreenDisplayStatus());
	}
}

// World 开始玩法前激活基础和用户 Mix，将本地音量写入各 Bus Stage，并应用当前 HDR/LDR 效果链。
void ULyraAudioMixEffectsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (const UWorld* World = InWorld.GetWorld())
	{
		// World 开始玩法时激活默认基础混音。
		// Activate the default base mix
		if (DefaultBaseMix)
		{
			UAudioModulationStatics::ActivateBusMix(World, DefaultBaseMix);
		}

		// 读取本地用户保存的各分类音量和 HDR 音频偏好。
		// Retrieve the user settings
		if (const ULyraSettingsLocal* LyraSettingsLocal = GetDefault<ULyraSettingsLocal>())
		{
			// 激活用户混音，并用当前设置更新各 ControlBus Stage。
			// Activate the User Mix
			if (UserMix)
			{
				UAudioModulationStatics::ActivateBusMix(World, UserMix);

				if (OverallControlBus && MusicControlBus && SoundFXControlBus && DialogueControlBus && VoiceChatControlBus)
				{
					const FSoundControlBusMixStage OverallControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, OverallControlBus, LyraSettingsLocal->GetOverallVolume());
					const FSoundControlBusMixStage MusicControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, MusicControlBus, LyraSettingsLocal->GetMusicVolume());
					const FSoundControlBusMixStage SoundFXControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, SoundFXControlBus, LyraSettingsLocal->GetSoundFXVolume());
					const FSoundControlBusMixStage DialogueControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, DialogueControlBus, LyraSettingsLocal->GetDialogueVolume());
					const FSoundControlBusMixStage VoiceChatControlBusMixStage = UAudioModulationStatics::CreateBusMixStage(World, VoiceChatControlBus, LyraSettingsLocal->GetVoiceChatVolume());

					TArray<FSoundControlBusMixStage> ControlBusMixStageArray;
					ControlBusMixStageArray.Add(OverallControlBusMixStage);
					ControlBusMixStageArray.Add(MusicControlBusMixStage);
					ControlBusMixStageArray.Add(SoundFXControlBusMixStage);
					ControlBusMixStageArray.Add(DialogueControlBusMixStage);
					ControlBusMixStageArray.Add(VoiceChatControlBusMixStage);

					UAudioModulationStatics::UpdateMix(World, UserMix, ControlBusMixStageArray);
				}
			}

			ApplyDynamicRangeEffectsChains(LyraSettingsLocal->IsHDRAudioModeEnabled());
		}
	}
}

// 根据 HDR 开关选择要应用和清理的 Submix 链，覆盖共同 Submix，并只清除新模式不再使用的旧链。
void ULyraAudioMixEffectsSubsystem::ApplyDynamicRangeEffectsChains(bool bHDRAudio)
{
	TArray<FLyraAudioSubmixEffectsChain> AudioSubmixEffectsChainToApply;
	TArray<FLyraAudioSubmixEffectsChain> AudioSubmixEffectsChainToClear;

	// 选择 HDR 时应用 HDR 链并清理 LDR 链；选择 LDR 时执行相反操作。
	// If HDR Audio is selected, then we clear out any existing LDR Submix Effect Chain Overrides
	// otherwise the reverse is the case.
	if (bHDRAudio)
	{
		AudioSubmixEffectsChainToApply.Append(HDRSubmixEffectChain);
		AudioSubmixEffectsChainToClear.Append(LDRSubmixEffectChain);
	}
	else
	{
		AudioSubmixEffectsChainToApply.Append(LDRSubmixEffectChain);
		AudioSubmixEffectsChainToClear.Append(HDRSubmixEffectChain);
	}

	// 仅收集不会被新效果链再次覆盖的 Submix，避免不必要的先清除再覆盖。
	// We want to collect just the submixes we need to actually clear, otherwise they'll be overridden by the new settings
	TArray<USoundSubmix*> SubmixesLeftToClear;

	// 找出只存在于旧模式效果链中的 Submix，稍后单独清除其覆盖。
	// We want to get the submixes that are not being overridden by the new effect chains, so we can clear those out separately
	for (const FLyraAudioSubmixEffectsChain& EffectChainToClear : AudioSubmixEffectsChainToClear)
	{
		bool bAddToList = true;

		for (const FLyraAudioSubmixEffectsChain& SubmixEffectChain : AudioSubmixEffectsChainToApply)
		{
			if (SubmixEffectChain.Submix == EffectChainToClear.Submix)
			{
				bAddToList = false;

				break;
			}
		}

		if (bAddToList)
		{
			SubmixesLeftToClear.Add(EffectChainToClear.Submix);
		}
	}


	// 以短淡入时间应用新模式的 Submix 效果链覆盖。
	// Override submixes
	for (const FLyraAudioSubmixEffectsChain& SubmixEffectChain : AudioSubmixEffectsChainToApply)
	{
		if (SubmixEffectChain.Submix)
		{
			UAudioMixerBlueprintLibrary::SetSubmixEffectChainOverride(GetWorld(), SubmixEffectChain.Submix, SubmixEffectChain.SubmixEffectChain, 0.1f);

		}
	}

	// 清除新模式不再使用的旧 Submix 效果链覆盖。
	// Clear remaining submixes
	for (USoundSubmix* Submix : SubmixesLeftToClear)
	{
		UAudioMixerBlueprintLibrary::ClearSubmixEffectChainOverride(GetWorld(), Submix, 0.1f);
	}
}

// 把 LoadingScreenManager 的可见性变化转交给加载界面 Mix 启停函数。
void ULyraAudioMixEffectsSubsystem::OnLoadingScreenStatusChanged(bool bShowingLoadingScreen)
{
	ApplyOrRemoveLoadingScreenMix(bShowingLoadingScreen);
}

// 仅在期望状态变化且 Mix 与 World 有效时激活或停用 LoadingScreenMix，并记录已应用状态。
void ULyraAudioMixEffectsSubsystem::ApplyOrRemoveLoadingScreenMix(bool bWantsLoadingScreenMix)
{
	UWorld* World = GetWorld();

	if (bAppliedLoadingScreenMix != bWantsLoadingScreenMix && LoadingScreenMix && World)
	{
		if (bWantsLoadingScreenMix)
		{
			// 加载界面出现时激活专用混音。
			// Apply the mix
			UAudioModulationStatics::ActivateBusMix(World, LoadingScreenMix);
		}
		else
		{
			// 加载界面消失时停用专用混音。
			// Remove the mix
			UAudioModulationStatics::DeactivateBusMix(World, LoadingScreenMix);
		}
		bAppliedLoadingScreenMix = bWantsLoadingScreenMix;
	}
}

// 仅对 Game 和 PIE World 返回 true，编辑器预览及其他 WorldType 不创建该子系统。
bool ULyraAudioMixEffectsSubsystem::DoesSupportWorldType(const EWorldType::Type World) const
{
	// 仅在 Game 和 PIE World 中创建该子系统。
	// We only need this subsystem on Game worlds (PIE included)
	return (World == EWorldType::Game || World == EWorldType::PIE);
}

