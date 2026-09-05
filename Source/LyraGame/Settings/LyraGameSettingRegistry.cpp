// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameSettingRegistry.h"

#include "GameSettingCollection.h"
#include "LyraSettingsLocal.h"
#include "LyraSettingsShared.h"
#include "Player/LyraLocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameSettingRegistry)

DEFINE_LOG_CATEGORY(LogLyraGameSettingRegistry);

#define LOCTEXT_NAMESPACE "Lyra"

//--------------------------------------
// ULyraGameSettingRegistry
//--------------------------------------

// 构造 Lyra 设置注册表实例。
ULyraGameSettingRegistry::ULyraGameSettingRegistry()
{
}

// 返回本地玩家持有的 Lyra 设置注册表；玩家为空时返回空指针。
ULyraGameSettingRegistry* ULyraGameSettingRegistry::Get(ULyraLocalPlayer* InLocalPlayer)
{
	ULyraGameSettingRegistry* Registry = FindObject<ULyraGameSettingRegistry>(InLocalPlayer, TEXT("LyraGameSettingRegistry"), EFindObjectFlags::ExactClass);
	if (Registry == nullptr)
	{
		Registry = NewObject<ULyraGameSettingRegistry>(InLocalPlayer, TEXT("LyraGameSettingRegistry"));
		Registry->Initialize(InLocalPlayer);
	}

	return Registry;
}

// 检查注册表及其全部顶层设置集合是否已完成初始化。
bool ULyraGameSettingRegistry::IsFinishedInitializing() const
{
	if (Super::IsFinishedInitializing())
	{
		if (ULyraLocalPlayer* LocalPlayer = Cast<ULyraLocalPlayer>(OwningLocalPlayer))
		{
			if (LocalPlayer->GetSharedSettings() == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

// 为本地玩家创建并注册视频、音频、游戏、键鼠和手柄五类顶层设置页面。
void ULyraGameSettingRegistry::OnInitialize(ULocalPlayer* InLocalPlayer)
{
	ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(InLocalPlayer);

	VideoSettings = InitializeVideoSettings(LyraLocalPlayer);
	InitializeVideoSettings_FrameRates(VideoSettings, LyraLocalPlayer);
	RegisterSetting(VideoSettings);

	AudioSettings = InitializeAudioSettings(LyraLocalPlayer);
	RegisterSetting(AudioSettings);

	GameplaySettings = InitializeGameplaySettings(LyraLocalPlayer);
	RegisterSetting(GameplaySettings);

	MouseAndKeyboardSettings = InitializeMouseAndKeyboardSettings(LyraLocalPlayer);
	RegisterSetting(MouseAndKeyboardSettings);

	GamepadSettings = InitializeGamepadSettings(LyraLocalPlayer);
	RegisterSetting(GamepadSettings);
}

// 应用本地与共享设置并分别持久化，然后调用基类完成保存流程。
void ULyraGameSettingRegistry::SaveChanges()
{
	Super::SaveChanges();
	
	if (ULyraLocalPlayer* LocalPlayer = Cast<ULyraLocalPlayer>(OwningLocalPlayer))
	{
		// 必须应用本机用户设置才能让分辨率等选项生效；应用过程也会间接保存这些设置。
		// Game user settings need to be applied to handle things like resolution, this saves indirectly
		LocalPlayer->GetLocalSettings()->ApplySettings(false);
		
		LocalPlayer->GetSharedSettings()->ApplySettings();
		LocalPlayer->GetSharedSettings()->SaveSettings();
	}
}

#undef LOCTEXT_NAMESPACE

