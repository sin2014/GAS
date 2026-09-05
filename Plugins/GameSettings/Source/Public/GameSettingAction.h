// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSetting.h"

#include "GameSettingAction.generated.h"

#define UE_API GAMESETTINGS_API

// 动作设置把按钮交互封装为自定义回调或可由注册表转发的命名事件。
//--------------------------------------
// UGameSettingAction
//--------------------------------------

class ULocalPlayer;

DECLARE_DELEGATE_TwoParams(UGameSettingCustomAction, UGameSetting* /*Setting*/, ULocalPlayer* /*LocalPlayer*/)

/**
 * 
 */
UCLASS(MinimalAPI)
class UGameSettingAction : public UGameSetting
{
	GENERATED_BODY()

public:
	UE_API UGameSettingAction();

public:

	DECLARE_EVENT_TwoParams(UGameSettingAction, FOnExecuteNamedAction, UGameSetting* /*Setting*/, FGameplayTag /*GameSettings_Action_Tag*/);
	FOnExecuteNamedAction OnExecuteNamedActionEvent;

public:

	FText GetActionText() const { return ActionText; }
	void SetActionText(FText Value) { ActionText = Value; }
#if !UE_BUILD_SHIPPING
	void SetActionText(const FString& Value) { SetActionText(FText::FromString(Value)); }
#endif

	FGameplayTag GetNamedAction() const { return NamedAction; }
	void SetNamedAction(FGameplayTag Value) { NamedAction = Value; }

	bool HasCustomAction() const { return CustomAction.IsBound(); }
	void SetCustomAction(UGameSettingCustomAction InAction) { CustomAction = InAction; }
	UE_API void SetCustomAction(TFunction<void(ULocalPlayer*)> InAction);

	// 动作默认不会把设置标记为脏，因为多数动作不可撤销，或只是显示制作人员与 EULA；若动作确实改变可撤销状态，可启用此标志以在触发时广播变化。
	/**
	 * By default actions don't dirty the settings, since the majority of them either do things you can't
	 * revert, or they're things like show the credits or Eula.  However if that's not true, set this flag
	 * to true to fire the change event when this action is fired.
	 */
	void SetDoesActionDirtySettings(bool Value) { bDirtyAction = Value; }

	UE_API virtual void ExecuteAction();

protected:
	// 初始化时校验动作来源、按钮文本和说明满足动作设置约束。
	/** UGameSettingValue */
	UE_API virtual void OnInitialized() override;

protected:
	FText ActionText;
	FGameplayTag NamedAction;
	UGameSettingCustomAction CustomAction;
	bool bDirtyAction = false;
};

#undef UE_API
