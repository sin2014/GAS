// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TextFilterExpressionEvaluator.h"

#include "UObject/ObjectPtr.h"
#include "GameSettingFilterState.generated.h"

#define UE_API GAMESETTINGS_API

class ULocalPlayer;
class UGameSetting;
class UGameSettingCollection;

// 设置发生变化的原因。
/** Why did the setting change? */
enum class EGameSettingChangeReason : uint8
{
	Change,
	DependencyChanged,
	ResetToDefault,
	RestoreToInitial,
};

// 过滤状态集中描述设置系统支持的全部过滤条件。
/**
 * The filter state is intended to be any and all filtering we support.
 */
USTRUCT()
struct FGameSettingFilterState
{
	GENERATED_BODY()

public:

	UE_API FGameSettingFilterState();

	UPROPERTY()
	bool bIncludeDisabled = true;

	UPROPERTY()
	bool bIncludeHidden = false;

	UPROPERTY()
	bool bIncludeResetable = true;

	UPROPERTY()
	bool bIncludeNestedPages = false;

public:
	UE_API void SetSearchText(const FString& InSearchText);

	UE_API bool DoesSettingPassFilter(const UGameSetting& InSetting) const;

	UE_API void AddSettingToRootList(UGameSetting* InSetting);
	UE_API void AddSettingToAllowList(UGameSetting* InSetting);

	bool IsSettingInAllowList(const UGameSetting* InSetting) const
	{
		return SettingAllowList.Contains(InSetting);
	}
	
	const TArray<UGameSetting*>& GetSettingRootList() const { return SettingRootList; }
	bool IsSettingInRootList(const UGameSetting* InSetting) const
	{
		return SettingRootList.Contains(InSetting);
	}

private:
	FTextFilterExpressionEvaluator SearchTextEvaluator;

	UPROPERTY()
	TArray<TObjectPtr<UGameSetting>> SettingRootList;

	// 该集合非空时，仅允许其中列出的设置通过过滤。
	// If this is non-empty, then only settings in here are allowed
	UPROPERTY()
	TArray<TObjectPtr<UGameSetting>> SettingAllowList;
};

// 可编辑状态记录设置当前的可见性、启用状态以及形成该状态的原因。
/**
 * Editable state captures the current visibility and enabled state of a setting. As well
 * as the reasons it got into that state.
 */
class FGameSettingEditableState
{
public:
	FGameSettingEditableState()
		: bVisible(true)
		, bEnabled(true)
		, bResetable(true)
		, bHideFromAnalytics(false)
	{
	}

	bool IsVisible() const { return bVisible; }
	bool IsEnabled() const { return bEnabled; }
	bool IsResetable() const { return bResetable; }
	bool IsHiddenFromAnalytics() const { return bHideFromAnalytics; }
	const TArray<FText>& GetDisabledReasons() const { return DisabledReasons; }

#if !UE_BUILD_SHIPPING
	const TArray<FString>& GetHiddenReasons() const { return HiddenReasons; }
#endif

	const TArray<FString>& GetDisabledOptions() const { return DisabledOptions; }

	// 隐藏设置；无需面向用户的原因，但必须提供开发者原因。
	/** Hides the setting, you don't have to provide a user facing reason, but you do need to specify a developer reason. */
	UE_API void Hide(const FString& DevReason);

	// 禁用设置，并要求提供面向用户的禁用原因。
	/** Disables the setting, you need to provide a reason you disabled this setting. */
	UE_API void Disable(const FText& Reason);

	// 需要对用户隐藏的离散选项，目前仅用于家长控制。
	/** Discrete Options that should be hidden from the user. Currently used only by Parental Controls. */
	UE_API void DisableOption(const FString& Option);

	template<typename EnumType>
	void DisableEnumOption(EnumType InEnumValue)
	{
		DisableOption(StaticEnum<EnumType>()->GetNameStringByValue((int64)InEnumValue));
	}

	// 用户将当前页面恢复默认值时，阻止该设置被重置。
	/**
	 * Prevents the setting from being reset if the user resets the settings on the screen to their defaults.
	 */
	UE_API void UnableToReset();

	// 从分析上报中隐藏；例如平台专用条件可避免上报在该平台根本不存在的设置，从而减少噪声。
	/**
	 * Hide from analytics, you may want to do this if for example, we just want to prevent noise, such as platform
	 * specific edit conditions where it doesn't make sense to report settings for platforms where they don't exist.
	 */
	void HideFromAnalytics() { bHideFromAnalytics = true; }

	// 彻底淘汰该设置：界面隐藏、禁止恢复默认，并从分析上报中排除。
	/** Hides it in every way possible.  Hides it visually.  Marks it as Immutable for being reset.  Hides it from analytics. */
	void Kill(const FString& DevReason)
	{
		Hide(DevReason);
		HideFromAnalytics();
		UnableToReset();
	}

private:
	uint8 bVisible : 1;
	uint8 bEnabled : 1;
	uint8 bResetable : 1;
	uint8 bHideFromAnalytics : 1;

	TArray<FString> DisabledOptions;

	TArray<FText> DisabledReasons;

#if !UE_BUILD_SHIPPING
	TArray<FString> HiddenReasons;
#endif
};

// 编辑条件可监视游戏状态或其他设置，并据此调整可见性和可编辑性。
/**
 * Edit conditions can monitor the state of the game or of other settings and adjust the 
 * visibility.
 */
class FGameSettingEditCondition : public TSharedFromThis<FGameSettingEditCondition>
{
public:
	FGameSettingEditCondition() { }
	virtual ~FGameSettingEditCondition() { }

	DECLARE_EVENT_OneParam(FGameSettingEditCondition, FOnEditConditionChanged, bool);
	FOnEditConditionChanged OnEditConditionChangedEvent;

	// 广播编辑条件变化事件。
	/** Broadcasts Event*/
	void BroadcastEditConditionChanged()
	{
		OnEditConditionChangedEvent.Broadcast(true);
	}

	// 在设置初始化期间调用。
	/** Called during the setting Initialization */
	virtual void Initialize(const ULocalPlayer* InLocalPlayer)
	{
	}

	// 在设置正式应用时调用。
	/** Called when the setting is 'applied'. */
	virtual void SettingApplied(const ULocalPlayer* InLocalPlayer, UGameSetting* Setting) const
	{
	}

	// 在设置值变化时调用。
	/** Called when the setting is changed. */
	virtual void SettingChanged(const ULocalPlayer* InLocalPlayer, UGameSetting* Setting, EGameSettingChangeReason Reason) const
	{
	}

	// 设置需要重新评估编辑状态时调用，通常由依赖项变化或编辑条件广播变化事件触发。
	/**
	 * Called when the setting needs to re-evaluate edit state. Usually this is in response to a 
	 * dependency changing, or if this edit condition emits an OnEditConditionChangedEvent.
	 */
	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const
	{
	}

	// 生成编辑条件的调试文本，便于排查状态不符合预期的问题。
	/** Generate useful debugging text for this edit condition.  Helpful when things don't work as expected. */
	virtual FString ToString() const { return TEXT(""); }
};

#undef UE_API
