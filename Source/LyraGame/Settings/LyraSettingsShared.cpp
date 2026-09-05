// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingsShared.h"

#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Culture.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Player/LyraLocalPlayer.h"
#include "Rendering/SlateRenderer.h"
#include "SubtitleDisplaySubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingsShared)

// 共享设置使用的固定存档槽名称。
static FString SHARED_SETTINGS_SLOT_NAME = TEXT("SharedGameSettings");

namespace LyraSettingsSharedCVars
{
	// 左摇杆内死区的控制台可调默认值。
	static float DefaultGamepadLeftStickInnerDeadZone = 0.25f;
	// 将左摇杆默认内死区暴露为控制台变量。
	static FAutoConsoleVariableRef CVarGamepadLeftStickInnerDeadZone(
		TEXT("gpad.DefaultLeftStickInnerDeadZone"),
		DefaultGamepadLeftStickInnerDeadZone,
		TEXT("Gamepad left stick inner deadzone")
	);

	// 右摇杆内死区的控制台可调默认值。
	static float DefaultGamepadRightStickInnerDeadZone = 0.25f;
	// 将右摇杆默认内死区暴露为控制台变量。
	static FAutoConsoleVariableRef CVarGamepadRightStickInnerDeadZone(
		TEXT("gpad.DefaultRightStickInnerDeadZone"),
		DefaultGamepadRightStickInnerDeadZone,
		TEXT("Gamepad right stick inner deadzone")
	);	
}

// 监听文化区域变化，并用控制台默认值初始化手柄摇杆死区。
ULyraSettingsShared::ULyraSettingsShared()
{
	FInternationalization::Get().OnCultureChanged().AddUObject(this, &ThisClass::OnCultureChanged);

	GamepadMoveStickDeadZone = LyraSettingsSharedCVars::DefaultGamepadLeftStickInnerDeadZone;
	GamepadLookStickDeadZone = LyraSettingsSharedCVars::DefaultGamepadRightStickInnerDeadZone;
}

// 返回当前共享设置存档的数据版本号。
int32 ULyraSettingsShared::GetLatestDataVersion() const
{
	// 版本 0：尚未继承 ULocalPlayerSaveGame 的旧数据格式。
	// 版本 1：首个正式的共享设置存档格式。
	// 0 = before subclassing ULocalPlayerSaveGame
	// 1 = first proper version
	return 1;
}

// 为指定本地玩家创建不从存档加载的临时共享设置对象。
ULyraSettingsShared* ULyraSettingsShared::CreateTemporarySettings(const ULyraLocalPlayer* LocalPlayer)
{
	// 临时对象不从磁盘读取，但仍按可正常保存的玩家存档对象完成初始化。
	// This is not loaded from disk but should be set up to save
	ULyraSettingsShared* SharedSettings = Cast<ULyraSettingsShared>(CreateNewSaveGameForLocalPlayer(ULyraSettingsShared::StaticClass(), LocalPlayer, SHARED_SETTINGS_SLOT_NAME));

	SharedSettings->ApplySettings();

	return SharedSettings;
}

// 同步加载玩家共享设置；存档不存在或加载失败时创建默认对象。
ULyraSettingsShared* ULyraSettingsShared::LoadOrCreateSettings(const ULyraLocalPlayer* LocalPlayer)
{
	// 同步读取玩家存档会阻塞主线程。
	// This will stall the main thread while it loads
	ULyraSettingsShared* SharedSettings = Cast<ULyraSettingsShared>(LoadOrCreateSaveGameForLocalPlayer(ULyraSettingsShared::StaticClass(), LocalPlayer, SHARED_SETTINGS_SLOT_NAME));

	SharedSettings->ApplySettings();

	return SharedSettings;
}

// 异步加载玩家共享设置，并始终通过委托返回已加载或新建的对象。
bool ULyraSettingsShared::AsyncLoadOrCreateSettings(const ULyraLocalPlayer* LocalPlayer, FOnSettingsLoadedEvent Delegate)
{
	FOnLocalPlayerSaveGameLoadedNative Lambda = FOnLocalPlayerSaveGameLoadedNative::CreateLambda([Delegate]
		(ULocalPlayerSaveGame* LoadedSave)
		{
			ULyraSettingsShared* LoadedSettings = CastChecked<ULyraSettingsShared>(LoadedSave);
			
			LoadedSettings->ApplySettings();

			Delegate.ExecuteIfBound(LoadedSettings);
		});

	return ULocalPlayerSaveGame::AsyncLoadOrCreateSaveGameForLocalPlayer(ULyraSettingsShared::StaticClass(), LocalPlayer, SHARED_SETTINGS_SLOT_NAME, Lambda);
}

// 异步保存共享设置到当前玩家的固定存档槽。
void ULyraSettingsShared::SaveSettings()
{
	// 共享设置允许本次保存失败，因此仅调度异步写盘，不阻塞调用方。
	// Schedule an async save because it's okay if it fails
	AsyncSaveGameToSlotForLocalPlayer();

	// TODO_BH：应在提升数据版本后把增强输入设置的迁移或保存逻辑移入序列化流程。
	// TODO_BH: Move this to the serialize function instead with a bumped version number
	if (UEnhancedInputLocalPlayerSubsystem* System = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwningPlayer))
	{
		if (UEnhancedInputUserSettings* InputSettings = System->GetUserSettings())
		{
			InputSettings->AsyncSaveSettings();
		}
	}
}

// 应用字幕、后台音频、文化区域及增强输入用户设置等共享偏好。
void ULyraSettingsShared::ApplySettings()
{
	ApplySubtitleOptions();
	ApplyBackgroundAudioSettings();
	ApplyCultureSettings();

	if (UEnhancedInputLocalPlayerSubsystem* System = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwningPlayer))
	{
		if (UEnhancedInputUserSettings* InputSettings = System->GetUserSettings())
		{
			InputSettings->ApplySettings();
		}
	}
}

// 将色觉辅助强度限制在有效范围内，并立即更新 Slate 渲染器。
void ULyraSettingsShared::SetColorBlindStrength(int32 InColorBlindStrength)
{
	InColorBlindStrength = FMath::Clamp(InColorBlindStrength, 0, 10);
	if (ColorBlindStrength != InColorBlindStrength)
	{
		ColorBlindStrength = InColorBlindStrength;
		FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(
			(EColorVisionDeficiency)(int32)ColorBlindMode, (int32)ColorBlindStrength, true, false);
	}
}

// 设置手柄输入 API 偏好，并在 Windows 上切换相应增强输入映射上下文。
void ULyraSettingsShared::SetGamepadInputAPIOption(const ELyraGamepadInputAPIOption NewValue)
{
	const bool bWasValueChanged = ChangeValueAndDirty(GamepadInputAPIOptions, NewValue);

	// 值未变化时无需重新配置平台首选输入设备。
	// We dont have any other additional work to do if the value wasn't changed.
	if (!bWasValueChanged)
	{
		return;
	}

	// 平台接口要求以逗号分隔的字符串按优先顺序列出手柄输入 API。
	// A comma-separated list of preferred gamepad APIs
	FString GamepadAPIOptions = TEXT("");

	switch (NewValue)
	{
	case ELyraGamepadInputAPIOption::Legacy:
		GamepadAPIOptions = TEXT("XInput,WinDualShock");
		break;
	case ELyraGamepadInputAPIOption::Modern:
		GamepadAPIOptions = TEXT("GameInput");
		break;
	default:
		checkNoEntry();
		break;
	}

	FGenericPlatformMisc::SetPreferredInputDevices(*GamepadAPIOptions);
}

// 返回色觉辅助强度。
int32 ULyraSettingsShared::GetColorBlindStrength() const
{
	return ColorBlindStrength;
}

// 更新色觉辅助模式并立即应用到 Slate 渲染器。
void ULyraSettingsShared::SetColorBlindMode(EColorBlindMode InMode)
{
	if (ColorBlindMode != InMode)
	{
		ColorBlindMode = InMode;
		FSlateApplication::Get().GetRenderer()->SetColorVisionDeficiencyType(
			(EColorVisionDeficiency)(int32)ColorBlindMode, (int32)ColorBlindStrength, true, false);
	}
}

// 返回当前色觉辅助模式。
EColorBlindMode ULyraSettingsShared::GetColorBlindMode() const
{
	return ColorBlindMode;
}

// 将当前字幕格式选项应用到本地玩家的字幕显示子系统。
void ULyraSettingsShared::ApplySubtitleOptions()
{
	if (USubtitleDisplaySubsystem* SubtitleSystem = USubtitleDisplaySubsystem::Get(OwningPlayer))
	{
		FSubtitleFormat SubtitleFormat;
		SubtitleFormat.SubtitleTextSize = SubtitleTextSize;
		SubtitleFormat.SubtitleTextColor = SubtitleTextColor;
		SubtitleFormat.SubtitleTextBorder = SubtitleTextBorder;
		SubtitleFormat.SubtitleBackgroundOpacity = SubtitleBackgroundOpacity;

		SubtitleSystem->SetSubtitleDisplayOptions(SubtitleFormat);
	}
}

//////////////////////////////////////////////////////////////////////

// 更新后台音频偏好并立即应用。
void ULyraSettingsShared::SetAllowAudioInBackgroundSetting(ELyraAllowBackgroundAudioSetting NewValue)
{
	if (ChangeValueAndDirty(AllowAudioInBackground, NewValue))
	{
		ApplyBackgroundAudioSettings();
	}
}

// 仅对主本地玩家设置应用失焦时的全局音频音量倍率。
void ULyraSettingsShared::ApplyBackgroundAudioSettings()
{
	if (OwningPlayer && OwningPlayer->IsPrimaryPlayer())
	{
		FApp::SetUnfocusedVolumeMultiplier((AllowAudioInBackground != ELyraAllowBackgroundAudioSetting::Off) ? 1.0f : 0.0f);
	}
}

//////////////////////////////////////////////////////////////////////

// 应用待定文化区域并写入配置；重置请求则清除用户文化区域覆盖。
void ULyraSettingsShared::ApplyCultureSettings()
{
	if (bResetToDefaultCulture)
	{
		const FCulturePtr SystemDefaultCulture = FInternationalization::Get().GetDefaultCulture();
		check(SystemDefaultCulture.IsValid());

		const FString CultureToApply = SystemDefaultCulture->GetName();
		if (FInternationalization::Get().SetCurrentCulture(CultureToApply))
		{
			// 删除用户配置中的显式 Culture 项，使后续启动继续采用系统默认区域文化。
			// Clear this string
			GConfig->RemoveKey(TEXT("Internationalization"), TEXT("Culture"), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		bResetToDefaultCulture = false;
	}
	else if (!PendingCulture.IsEmpty())
	{
		// SetCurrentCulture 可能广播区域文化变化并间接清空 PendingCulture，因此先复制待应用值供后续保存。
		// SetCurrentCulture may trigger PendingCulture to be cleared (if a culture change is broadcast) so we take a copy of it to work with
		const FString CultureToApply = PendingCulture;
		if (FInternationalization::Get().SetCurrentCulture(CultureToApply))
		{
			// 此处特意写入用户配置而非仅依赖玩家存档，因为登录前及加载画面早期就需要按该语言本地化文本。
			// Note: This is intentionally saved to the users config
			// We need to localize text before the player logs in and very early in the loading screen
			GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *CultureToApply, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		ClearPendingCulture();
	}
}

// 清除尚未应用的文化区域修改和重置请求。
void ULyraSettingsShared::ResetCultureToCurrentSettings()
{
	ClearPendingCulture();
	bResetToDefaultCulture = false;
}

// 返回等待应用的文化区域名称。
const FString& ULyraSettingsShared::GetPendingCulture() const
{
	return PendingCulture;
}

// 记录待应用的文化区域，并取消文化区域重置请求。
void ULyraSettingsShared::SetPendingCulture(const FString& NewCulture)
{
	PendingCulture = NewCulture;
	bResetToDefaultCulture = false;
	bIsDirty = true;
}

// 外部文化区域变化后清除待应用状态，避免覆盖最新结果。
void ULyraSettingsShared::OnCultureChanged()
{
	ClearPendingCulture();
	bResetToDefaultCulture = false;
}

// 清除尚未应用的文化区域名称，不改变“恢复系统默认”标记。
void ULyraSettingsShared::ClearPendingCulture()
{
	PendingCulture.Reset();
}

// 返回当前是否使用系统默认文化区域。
bool ULyraSettingsShared::IsUsingDefaultCulture() const
{
	FString Culture;
	GConfig->GetString(TEXT("Internationalization"), TEXT("Culture"), Culture, GGameUserSettingsIni);

	return Culture.IsEmpty();
}

// 标记下次应用时恢复系统默认文化区域。
void ULyraSettingsShared::ResetToDefaultCulture()
{
	ClearPendingCulture();
	bResetToDefaultCulture = true;
	bIsDirty = true;
}

//////////////////////////////////////////////////////////////////////

// 预留的输入灵敏度应用入口；当前实现不执行任何操作。
void ULyraSettingsShared::ApplyInputSensitivity()
{
	
}

