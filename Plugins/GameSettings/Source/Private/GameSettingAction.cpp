// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingAction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingAction)

#define LOCTEXT_NAMESPACE "GameSetting"

//--------------------------------------
// 动作设置将按钮交互转发为自定义回调或命名动作事件。
// UGameSettingAction
//--------------------------------------

// 创建动作设置；具体行为由后续绑定的自定义回调或命名动作标签提供。
UGameSettingAction::UGameSettingAction()
{

}

// 初始化动作设置并校验动作来源、按钮文本和说明均已正确配置。
void UGameSettingAction::OnInitialized()
{
	Super::OnInitialized();

#if !UE_BUILD_SHIPPING
	ensureMsgf(HasCustomAction() || NamedAction.IsValid(), TEXT("Action settings need either a custom action or a named action."));
	ensureMsgf(!ActionText.IsEmpty(), TEXT("You must provide a ActionText for settings with actions."));
	ensureMsgf(!DescriptionRichText.IsEmpty(), TEXT("You must provide a description for settings with actions."));
#endif
}

// 保存自定义动作回调，供用户激活动作设置时调用。
void UGameSettingAction::SetCustomAction(TFunction<void(ULocalPlayer*)> InAction)
{
	CustomAction = UGameSettingCustomAction::CreateLambda([InAction](UGameSetting* /*Setting*/, ULocalPlayer* InLocalPlayer) {
		InAction(InLocalPlayer);
	});
}

// 调用绑定的自定义动作；需要时广播设置变化，再发送命名动作事件供注册表集中转发。
void UGameSettingAction::ExecuteAction()
{
	if (HasCustomAction())
	{
		CustomAction.ExecuteIfBound(this, LocalPlayer);
	}
	else
	{
		OnExecuteNamedActionEvent.Broadcast(this, NamedAction);
	}
	
	if (bDirtyAction)
	{
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

#undef LOCTEXT_NAMESPACE

