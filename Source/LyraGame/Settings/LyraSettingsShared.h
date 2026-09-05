// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/SaveGame.h"
#include "SubtitleDisplayOptions.h"

#include "UObject/ObjectPtr.h"
#include "LyraSettingsShared.generated.h"

class UObject;
struct FFrame;

UENUM(BlueprintType)
enum class EColorBlindMode : uint8
{
	Off,
	// 绿色感知较弱或缺失的绿色盲模式。
	// Deuteranope (green weak/blind)
	Deuteranope,
	// 红色感知较弱或缺失的红色盲模式。
	// Protanope (red weak/blind)
	Protanope,
	// 蓝色感知较弱或缺失的蓝色盲模式。
	// Tritanope(blue weak / bind)
	Tritanope
};

UENUM(BlueprintType)
enum class ELyraAllowBackgroundAudioSetting : uint8
{
	Off,
	AllSounds,

	Num UMETA(Hidden),
};

UENUM()
enum class ELyraGamepadInputAPIOption : uint8
{
	Legacy,	/* 使用 XInput 与 WinDualShock 的传统输入后端。 */ // XInput + WinDualShock
	Modern,	/* 使用 GameInput API 的现代输入后端。 */ // GameInput API

	Num UMETA(Hidden),
};

UENUM(BlueprintType)
enum class ELyraGamepadSensitivity : uint8
{
	Invalid = 0		UMETA(Hidden),

	Slow			UMETA(DisplayName = "01 - Slow"),
	SlowPlus		UMETA(DisplayName = "02 - Slow+"),
	SlowPlusPlus	UMETA(DisplayName = "03 - Slow++"),
	Normal			UMETA(DisplayName = "04 - Normal"),
	NormalPlus		UMETA(DisplayName = "05 - Normal+"),
	NormalPlusPlus	UMETA(DisplayName = "06 - Normal++"),
	Fast			UMETA(DisplayName = "07 - Fast"),
	FastPlus		UMETA(DisplayName = "08 - Fast+"),
	FastPlusPlus	UMETA(DisplayName = "09 - Fast++"),
	Insane			UMETA(DisplayName = "10 - Insane"),

	MAX				UMETA(Hidden),
};

class ULyraLocalPlayer;

// ULyraSettingsShared 保存与具体设备无关、应跟随玩家账户的设置。
// 数据通过 USaveGame 按本地玩家分别保存，因此适合存放按键偏好、辅助功能和语言等个人选项，
// 也可以上传云端并在不同设备间共享；与机器能力或硬件相关的选项应存入 ULyraSettingsLocal。
/**
 * ULyraSettingsShared - The "Shared" settings are stored as part of the USaveGame system, these settings are not machine
 * specific like the local settings, and are safe to store in the cloud - and 'share' them.  Using the save game system
 * we can also store settings per player, so things like controller keybind preferences should go here, because if those
 * are stored in the local settings all users would get them.
 *
 */
UCLASS()
class ULyraSettingsShared : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	DECLARE_EVENT_OneParam(ULyraSettingsShared, FOnSettingChangedEvent, ULyraSettingsShared* Settings);
	FOnSettingChangedEvent OnSettingChanged;

public:

	ULyraSettingsShared();

	//~ULocalPlayerSaveGame interface
	int32 GetLatestDataVersion() const override;
	//~End of ULocalPlayerSaveGame interface

	bool IsDirty() const { return bIsDirty; }
	void ClearDirtyFlag() { bIsDirty = false; }

	/** 创建临时设置对象；用户存档加载完成后，该对象会被存档中的正式设置对象替换。 */
	/** Creates a temporary settings object, this will be replaced by one loaded from the user's save game */
	static ULyraSettingsShared* CreateTemporarySettings(const ULyraLocalPlayer* LocalPlayer);
	
	/** 同步加载或创建设置对象；玩家完成登录前不得调用。 */
	/** Synchronously loads a settings object, this is not valid to call before login */
	static ULyraSettingsShared* LoadOrCreateSettings(const ULyraLocalPlayer* LocalPlayer);

	DECLARE_DELEGATE_OneParam(FOnSettingsLoadedEvent, ULyraSettingsShared* Settings);

	/** 异步加载或创建设置对象，完成后调用 Delegate。 */
	/** Starts an async load of the settings object, calls Delegate on completion */
	static bool AsyncLoadOrCreateSettings(const ULyraLocalPlayer* LocalPlayer, FOnSettingsLoadedEvent Delegate);

	/** 将当前共享设置写入玩家存档。 */
	/** Saves the settings to disk */
	void SaveSettings();

	/** 将当前共享设置应用到对应玩家及其相关子系统。 */
	/** Applies the current settings to the player */
	void ApplySettings();
	
public:
	////////////////////////////////////////////////////////
	// 色觉辅助设置。
	// Color Blind Options

	UFUNCTION()
	EColorBlindMode GetColorBlindMode() const;
	UFUNCTION()
	void SetColorBlindMode(EColorBlindMode InMode);

	UFUNCTION()
	int32 GetColorBlindStrength() const;
	UFUNCTION()
	void SetColorBlindStrength(int32 InColorBlindStrength);

private:
	UPROPERTY()
	EColorBlindMode ColorBlindMode = EColorBlindMode::Off;

	UPROPERTY()
	int32 ColorBlindStrength = 10;

    ////////////////////////////////////////////////////////
	// 游戏手柄振动设置。
	// Gamepad Vibration
public:
	UFUNCTION()
	bool GetForceFeedbackEnabled() const { return bForceFeedbackEnabled; }

	UFUNCTION()
	void SetForceFeedbackEnabled(const bool NewValue) { ChangeValueAndDirty(bForceFeedbackEnabled, NewValue); }
	
private:
	/** 使用游戏手柄时是否启用力反馈。 */
	/** Is force feedback enabled when a controller is being used? */
	UPROPERTY()
	bool bForceFeedbackEnabled = true;

	////////////////////////////////////////////////////////
	// 游戏手柄摇杆死区设置。
	// Gamepad Deadzone
public:
	/** 返回移动摇杆的输入死区阈值。 */
	/** Getter for gamepad move stick dead zone value. */
	UFUNCTION()
	float GetGamepadMoveStickDeadZone() const { return GamepadMoveStickDeadZone; }

	/** 设置移动摇杆的输入死区阈值。 */
	/** Setter for gamepad move stick dead zone value. */
	UFUNCTION()
	void SetGamepadMoveStickDeadZone(const float NewValue) { ChangeValueAndDirty(GamepadMoveStickDeadZone, NewValue); }

	/** 返回视角摇杆的输入死区阈值。 */
	/** Getter for gamepad look stick dead zone value. */
	UFUNCTION()
	float GetGamepadLookStickDeadZone() const { return GamepadLookStickDeadZone; }

	/** 设置视角摇杆的输入死区阈值。 */
	/** Setter for gamepad look stick dead zone value. */
	UFUNCTION()
	void SetGamepadLookStickDeadZone(const float NewValue) { ChangeValueAndDirty(GamepadLookStickDeadZone, NewValue); }

private:
	/** 保存移动摇杆的输入死区阈值。 */
	/** Holds the gamepad move stick dead zone value. */
	UPROPERTY()
	float GamepadMoveStickDeadZone;

	/** 保存视角摇杆的输入死区阈值。 */
	/** Holds the gamepad look stick dead zone value. */
	UPROPERTY()
	float GamepadLookStickDeadZone;

	/////////////////////////////////////////////////
	// 游戏手柄输入 API 设置，仅 PC 平台可用。
	// Gamepad Input API (only available on PC)
	
	UPROPERTY()
	ELyraGamepadInputAPIOption GamepadInputAPIOptions;

public:
	UFUNCTION()
	ELyraGamepadInputAPIOption GetGamepadInputAPIOption() const { return GamepadInputAPIOptions; }
	UFUNCTION()
	void SetGamepadInputAPIOption(const ELyraGamepadInputAPIOption NewValue);

	////////////////////////////////////////////////////////
	// 游戏手柄自适应扳机触觉设置。
	// Gamepad Trigger Haptics
public:
	UFUNCTION()
	bool GetTriggerHapticsEnabled() const { return bTriggerHapticsEnabled; }
	UFUNCTION()
	void SetTriggerHapticsEnabled(const bool NewValue) { ChangeValueAndDirty(bTriggerHapticsEnabled, NewValue); }

	UFUNCTION()
	bool GetTriggerPullUsesHapticThreshold() const { return bTriggerPullUsesHapticThreshold; }
	UFUNCTION()
	void SetTriggerPullUsesHapticThreshold(const bool NewValue) { ChangeValueAndDirty(bTriggerPullUsesHapticThreshold, NewValue); }

	UFUNCTION()
	uint8 GetTriggerHapticStrength() const { return TriggerHapticStrength; }
	UFUNCTION()
	void SetTriggerHapticStrength(const uint8 NewValue) { ChangeValueAndDirty(TriggerHapticStrength, NewValue); }

	UFUNCTION()
	uint8 GetTriggerHapticStartPosition() const { return TriggerHapticStartPosition; }
	UFUNCTION()
	void SetTriggerHapticStartPosition(const uint8 NewValue) { ChangeValueAndDirty(TriggerHapticStartPosition, NewValue); }
	
private:
	/** 是否启用自适应扳机触觉效果。 */
	/** Are trigger haptics enabled? */
	UPROPERTY()
	bool bTriggerHapticsEnabled = false;
	/** 是否以扳机触觉效果的起始位置作为判定扳机按下的阈值。 */
	/** Does the game use the haptic feedback as its threshold for judging button presses? */
	UPROPERTY()
	bool bTriggerPullUsesHapticThreshold = true;
	/** 扳机触觉效果的强度。 */
	/** The strength of the trigger haptic effects. */
	UPROPERTY()
	uint8 TriggerHapticStrength = 8;
	/** 扳机触觉效果开始生效的位置。 */
	/** The start position of the trigger haptic effects */
	UPROPERTY()
	uint8 TriggerHapticStartPosition = 0;

	////////////////////////////////////////////////////////
	// 字幕开关与显示样式设置。
	// Subtitles
public:
	UFUNCTION()
	bool GetSubtitlesEnabled() const { return bEnableSubtitles; }
	UFUNCTION()
	void SetSubtitlesEnabled(bool Value) { ChangeValueAndDirty(bEnableSubtitles, Value); }

	UFUNCTION()
	ESubtitleDisplayTextSize GetSubtitlesTextSize() const { return SubtitleTextSize; }
	UFUNCTION()
	void SetSubtitlesTextSize(ESubtitleDisplayTextSize Value) { ChangeValueAndDirty(SubtitleTextSize, Value); ApplySubtitleOptions(); }

	UFUNCTION()
	ESubtitleDisplayTextColor GetSubtitlesTextColor() const { return SubtitleTextColor; }
	UFUNCTION()
	void SetSubtitlesTextColor(ESubtitleDisplayTextColor Value) { ChangeValueAndDirty(SubtitleTextColor, Value); ApplySubtitleOptions(); }

	UFUNCTION()
	ESubtitleDisplayTextBorder GetSubtitlesTextBorder() const { return SubtitleTextBorder; }
	UFUNCTION()
	void SetSubtitlesTextBorder(ESubtitleDisplayTextBorder Value) { ChangeValueAndDirty(SubtitleTextBorder, Value); ApplySubtitleOptions(); }

	UFUNCTION()
	ESubtitleDisplayBackgroundOpacity GetSubtitlesBackgroundOpacity() const { return SubtitleBackgroundOpacity; }
	UFUNCTION()
	void SetSubtitlesBackgroundOpacity(ESubtitleDisplayBackgroundOpacity Value) { ChangeValueAndDirty(SubtitleBackgroundOpacity, Value); ApplySubtitleOptions(); }

	void ApplySubtitleOptions();

private:
	UPROPERTY()
	bool bEnableSubtitles = true;

	UPROPERTY()
	ESubtitleDisplayTextSize SubtitleTextSize = ESubtitleDisplayTextSize::Medium;

	UPROPERTY()
	ESubtitleDisplayTextColor SubtitleTextColor = ESubtitleDisplayTextColor::White;

	UPROPERTY()
	ESubtitleDisplayTextBorder SubtitleTextBorder = ESubtitleDisplayTextBorder::None;

	UPROPERTY()
	ESubtitleDisplayBackgroundOpacity SubtitleBackgroundOpacity = ESubtitleDisplayBackgroundOpacity::Medium;

	////////////////////////////////////////////////////////
	// 可随玩家共享的音频行为设置。
	// Shared audio settings
public:
	UFUNCTION()
	ELyraAllowBackgroundAudioSetting GetAllowAudioInBackgroundSetting() const { return AllowAudioInBackground; }
	UFUNCTION()
	void SetAllowAudioInBackgroundSetting(ELyraAllowBackgroundAudioSetting NewValue);

	void ApplyBackgroundAudioSettings();

private:
	UPROPERTY()
	ELyraAllowBackgroundAudioSetting AllowAudioInBackground = ELyraAllowBackgroundAudioSetting::Off;

	////////////////////////////////////////////////////////
	// 区域文化与界面语言设置。
	// Culture / language
public:
	/** 返回等待应用的区域文化标识。 */
	/** Gets the pending culture */
	const FString& GetPendingCulture() const;

	/** 设置等待应用的区域文化标识，但暂不立即切换。 */
	/** Sets the pending culture to apply */
	void SetPendingCulture(const FString& NewCulture);

	// 区域文化切换后调用，用于同步相关设置状态。
	// Called when the culture changes.
	void OnCultureChanged();

	/** 清除尚未应用的区域文化设置。 */
	/** Clears the pending culture to apply */
	void ClearPendingCulture();

	bool IsUsingDefaultCulture() const;

	void ResetToDefaultCulture();
	bool ShouldResetToDefaultCulture() const { return bResetToDefaultCulture; }
	
	void ApplyCultureSettings();
	void ResetCultureToCurrentSettings();

private:
	/** 等待应用的区域文化标识。 */
	/** The pending culture to apply */
	UPROPERTY(Transient)
	FString PendingCulture;

	/* 为 true 时，在应用设置时恢复系统默认区域文化。 */
	/* If true, resets the culture to default. */
	bool bResetToDefaultCulture = false;

	////////////////////////////////////////////////////////
	// 鼠标视角灵敏度与坐标轴反转设置。
	// Gamepad Sensitivity
public:
	UFUNCTION()
	double GetMouseSensitivityX() const { return MouseSensitivityX; }
	UFUNCTION()
	void SetMouseSensitivityX(double NewValue) { ChangeValueAndDirty(MouseSensitivityX, NewValue); ApplyInputSensitivity(); }

	UFUNCTION()
	double GetMouseSensitivityY() const { return MouseSensitivityY; }
	UFUNCTION()
	void SetMouseSensitivityY(double NewValue) { ChangeValueAndDirty(MouseSensitivityY, NewValue); ApplyInputSensitivity(); }

	UFUNCTION()
	double GetTargetingMultiplier() const { return TargetingMultiplier; }
	UFUNCTION()
	void SetTargetingMultiplier(double NewValue) { ChangeValueAndDirty(TargetingMultiplier, NewValue); ApplyInputSensitivity(); }

	UFUNCTION()
	bool GetInvertVerticalAxis() const { return bInvertVerticalAxis; }
	UFUNCTION()
	void SetInvertVerticalAxis(bool NewValue) { ChangeValueAndDirty(bInvertVerticalAxis, NewValue); ApplyInputSensitivity(); }

	UFUNCTION()
	bool GetInvertHorizontalAxis() const { return bInvertHorizontalAxis; }
	UFUNCTION()
	void SetInvertHorizontalAxis(bool NewValue) { ChangeValueAndDirty(bInvertHorizontalAxis, NewValue); ApplyInputSensitivity(); }
	
private:
	/** 鼠标水平方向的视角灵敏度。 */
	/** Holds the mouse horizontal sensitivity */
	UPROPERTY()
	double MouseSensitivityX = 1.0;

	/** 鼠标垂直方向的视角灵敏度。 */
	/** Holds the mouse vertical sensitivity */
	UPROPERTY()
	double MouseSensitivityY = 1.0;

	/** 瞄准状态下额外乘到基础视角灵敏度上的倍率。 */
	/** Multiplier applied while Aiming down sights. */
	UPROPERTY()
	double TargetingMultiplier = 0.5;

	/** 为 true 时反转垂直视角输入轴。 */
	/** If true then the vertical look axis should be inverted */
	UPROPERTY()
	bool bInvertVerticalAxis = false;

	/** 为 true 时反转水平视角输入轴。 */
	/** If true then the horizontal look axis should be inverted */
	UPROPERTY()
	bool bInvertHorizontalAxis = false;
	
	////////////////////////////////////////////////////////
	// 游戏手柄视角与瞄准灵敏度预设。
	// Gamepad Sensitivity
public:
	UFUNCTION()
	ELyraGamepadSensitivity GetGamepadLookSensitivityPreset() const { return GamepadLookSensitivityPreset; }
	UFUNCTION()
	void SetLookSensitivityPreset(ELyraGamepadSensitivity NewValue) { ChangeValueAndDirty(GamepadLookSensitivityPreset, NewValue); ApplyInputSensitivity(); }

	UFUNCTION()
	ELyraGamepadSensitivity GetGamepadTargetingSensitivityPreset() const { return GamepadTargetingSensitivityPreset; }
	UFUNCTION()
	void SetGamepadTargetingSensitivityPreset(ELyraGamepadSensitivity NewValue) { ChangeValueAndDirty(GamepadTargetingSensitivityPreset, NewValue); ApplyInputSensitivity(); }

	void ApplyInputSensitivity();
	
private:
	UPROPERTY()
	ELyraGamepadSensitivity GamepadLookSensitivityPreset = ELyraGamepadSensitivity::Normal;
	UPROPERTY()
	ELyraGamepadSensitivity GamepadTargetingSensitivityPreset = ELyraGamepadSensitivity::Normal;
	
	////////////////////////////////////////////////////////
	/// Dirty and Change Reporting
private:
	template<typename T>
	bool ChangeValueAndDirty(T& CurrentValue, const T& NewValue)
	{
		if (CurrentValue != NewValue)
		{
			CurrentValue = NewValue;
			bIsDirty = true;
			OnSettingChanged.Broadcast(this);
			
			return true;
		}

		return false;
	}

	bool bIsDirty = false;
};
