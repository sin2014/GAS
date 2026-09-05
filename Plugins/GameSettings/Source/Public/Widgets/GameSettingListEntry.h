// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"

#include "GameSettingListEntry.generated.h"

#define UE_API GAMESETTINGS_API

class FGameSettingEditableState;
enum class EGameSettingChangeReason : uint8;

class UAnalogSlider;
class UCommonButtonBase;
class UCommonTextBlock;
class UGameSetting;
class UGameSettingAction;
class UGameSettingCollectionPage;
class UGameSettingRotator;
class UGameSettingValueDiscrete;
class UGameSettingValueScalar;
class UObject;
class UPanelWidget;
class UUserWidget;
class UWidget;
struct FFocusEvent;
struct FFrame;
struct FGeometry;

//////////////////////////////////////////////////////////////////////////
// 以下分区定义设置列表条目基类；英文标题保留了早期类名。
// UAthenaChallengeListEntry
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, Abstract, NotBlueprintable, meta = (Category = "Settings", DisableNativeTick))
class UGameSettingListEntryBase : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UE_API virtual void SetSetting(UGameSetting* InSetting);
	UE_API virtual void SetDisplayNameOverride(const FText& OverrideName);

protected:
	UE_API virtual void NativeOnEntryReleased() override;
	UE_API virtual void OnSettingChanged();
	UE_API virtual void HandleEditConditionChanged(UGameSetting* InSetting);
	UE_API virtual void RefreshEditableState(const FGameSettingEditableState& InEditableState);
	
protected:
	// 处理手柄焦点向子控件转移。
	// Focus transitioning to subwidgets for the gamepad
	UE_API virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	UFUNCTION(BlueprintImplementableEvent)
	UE_API UWidget* GetPrimaryGamepadFocusWidget();

protected:
	bool bSuspendChangeUpdates = false;

	UPROPERTY()
	TObjectPtr<UGameSetting> Setting;

	FText DisplayNameOverride = FText::GetEmpty();

private:
	void HandleSettingChanged(UGameSetting* InSetting, EGameSettingChangeReason Reason);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UUserWidget> Background;
};


// 通用设置条目在基础条目上增加设置名称显示。
//////////////////////////////////////////////////////////////////////////
// UGameSettingListEntry_Setting
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (Category = "Settings", DisableNativeTick))
class UGameSettingListEntry_Setting : public UGameSettingListEntryBase
{
	GENERATED_BODY()

public:
	UE_API virtual void SetSetting(UGameSetting* InSetting) override;
	
	// 以下为蓝图必须绑定的名称控件。
private:	// Bound Widgets
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonTextBlock> Text_SettingName;
};


// 离散值设置条目通过轮转器和增减按钮选择有限选项。
//////////////////////////////////////////////////////////////////////////
// UGameSettingListEntrySetting_Discrete
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (Category = "Settings", DisableNativeTick))
class UGameSettingListEntrySetting_Discrete : public UGameSettingListEntry_Setting
{
	GENERATED_BODY()

public:
	UE_API virtual void SetSetting(UGameSetting* InSetting) override;
	
protected:
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeOnEntryReleased() override;

	UE_API void HandleOptionDecrease();
	UE_API void HandleOptionIncrease();
	UE_API void HandleRotatorChangedValue(int32 Value, bool bUserInitiated);

	UE_API void Refresh();
	UE_API virtual void OnSettingChanged() override;
	UE_API virtual void HandleEditConditionChanged(UGameSetting* InSetting) override;
	UE_API virtual void RefreshEditableState(const FGameSettingEditableState& InEditableState) override;

protected:
	UPROPERTY()
	TObjectPtr<UGameSettingValueDiscrete> DiscreteSetting;

	// 以下为蓝图必须绑定的值面板、轮转器和增减按钮。
private:	// Bound Widgets
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> Panel_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UGameSettingRotator> Rotator_SettingValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Decrease;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Increase;
};

//////////////////////////////////////////////////////////////////////////
// 标量设置条目通过滑块显示、编辑并格式化连续数值。
// UGameSettingListEntrySetting_Scalar
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (Category = "Settings", DisableNativeTick))
class UGameSettingListEntrySetting_Scalar : public UGameSettingListEntry_Setting
{
	GENERATED_BODY()

public:
	UE_API virtual void SetSetting(UGameSetting* InSetting) override;

protected:
	UE_API void Refresh();
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeOnEntryReleased() override;
	UE_API virtual void OnSettingChanged() override;

	UFUNCTION()
	UE_API void HandleSliderValueChanged(float Value);
	UFUNCTION()
	UE_API void HandleSliderCaptureEnded();

	UFUNCTION(BlueprintImplementableEvent)
	UE_API void OnValueChanged(float Value);

	UFUNCTION(BlueprintImplementableEvent)
	UE_API void OnDefaultValueChanged(float DefaultValue);

	UE_API virtual void RefreshEditableState(const FGameSettingEditableState& InEditableState) override;

protected:
	UPROPERTY()
	TObjectPtr<UGameSettingValueScalar> ScalarSetting;

	// 以下为蓝图必须绑定的值面板、滑块和数值文本。
private:	// Bound Widgets
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UPanelWidget> Panel_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UAnalogSlider> Slider_SettingValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonTextBlock> Text_SettingValue;
};


//////////////////////////////////////////////////////////////////////////
// 动作设置条目通过按钮触发自定义或命名动作。
// UGameSettingListEntrySetting_Action
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (Category = "Settings", DisableNativeTick))
class UGameSettingListEntrySetting_Action : public UGameSettingListEntry_Setting
{
	GENERATED_BODY()

public:
	UE_API virtual void SetSetting(UGameSetting* InSetting) override;

protected:
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeOnEntryReleased() override;
	UE_API virtual void RefreshEditableState(const FGameSettingEditableState& InEditableState) override;

	UE_API void HandleActionButtonClicked();

	UFUNCTION(BlueprintImplementableEvent)
	UE_API void OnSettingAssigned(const FText& ActionText);

protected:
	UPROPERTY()
	TObjectPtr<UGameSettingAction> ActionSetting;

	// 以下为蓝图必须绑定的动作按钮。
private:	// Bound Widgets

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Action;
};

//////////////////////////////////////////////////////////////////////////
// 导航设置条目通过按钮进入分页集合的子设置。
// UGameSettingListEntrySetting_Navigation
//////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (Category = "Settings", DisableNativeTick))
class UGameSettingListEntrySetting_Navigation : public UGameSettingListEntry_Setting
{
	GENERATED_BODY()

public:
	UE_API virtual void SetSetting(UGameSetting* InSetting) override;

protected:
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeOnEntryReleased() override;
	UE_API virtual void RefreshEditableState(const FGameSettingEditableState& InEditableState) override;

	UE_API void HandleNavigationButtonClicked();

	UFUNCTION(BlueprintImplementableEvent)
	UE_API void OnSettingAssigned(const FText& ActionText);

protected:
	UPROPERTY()
	TObjectPtr<UGameSettingCollectionPage> CollectionSetting;

	// 以下为蓝图必须绑定的导航按钮。
private:	// Bound Widgets

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UCommonButtonBase> Button_Navigate;
};

#undef UE_API
