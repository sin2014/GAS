// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingScreen.h"

#include "Input/CommonUIInputTypes.h"
#include "Player/LyraLocalPlayer.h"
#include "Settings/LyraGameSettingRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingScreen)

class UGameSettingRegistry;

// 初始化设置界面时注册返回、应用和取消修改三个 CommonUI 动作，并保存绑定句柄。
void ULyraSettingScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BackHandle = RegisterUIActionBinding(FBindUIActionArgs(BackInputActionData, true, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBackAction)));
	ApplyHandle = RegisterUIActionBinding(FBindUIActionArgs(ApplyInputActionData, true, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleApplyAction)));
	CancelChangesHandle = RegisterUIActionBinding(FBindUIActionArgs(CancelChangesInputActionData, true, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCancelChangesAction)));
}

// 创建独立 Lyra 设置注册表并用当前本地玩家初始化后返回。
UGameSettingRegistry* ULyraSettingScreen::CreateRegistry()
{
	ULyraGameSettingRegistry* NewRegistry = NewObject<ULyraGameSettingRegistry>();

	if (ULyraLocalPlayer* LocalPlayer = CastChecked<ULyraLocalPlayer>(GetOwningLocalPlayer()))
	{
		NewRegistry->Initialize(LocalPlayer);
	}

	return NewRegistry;
}

// 优先弹出设置内导航；已在根级时应用修改并关闭设置界面。
void ULyraSettingScreen::HandleBackAction()
{
	if (AttemptToPopNavigation())
	{
		return;
	}

	ApplyChanges();

	DeactivateWidget();
}

// 应用当前设置修改但保持界面打开。
void ULyraSettingScreen::HandleApplyAction()
{
	ApplyChanges();
}

// 取消尚未应用的设置修改。
void ULyraSettingScreen::HandleCancelChangesAction()
{
	CancelChanges();
}

// 设置变脏时加入应用和取消动作，恢复干净后移除这两个绑定。
void ULyraSettingScreen::OnSettingsDirtyStateChanged_Implementation(bool bSettingsDirty)
{
	if (bSettingsDirty)
	{
		if (!GetActionBindings().Contains(ApplyHandle))
		{
			AddActionBinding(ApplyHandle);
		}
		if (!GetActionBindings().Contains(CancelChangesHandle))
		{
			AddActionBinding(CancelChangesHandle);
		}
	}
	else
	{
		RemoveActionBinding(ApplyHandle);
		RemoveActionBinding(CancelChangesHandle);
	}
}
