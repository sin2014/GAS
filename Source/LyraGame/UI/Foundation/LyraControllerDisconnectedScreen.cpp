// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraControllerDisconnectedScreen.h"

#include "Components/HorizontalBox.h"
#include "CommonButtonBase.h"
#include "CommonUISettings.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "ICommonUIModule.h"
#include "NativeGameplayTags.h"
#include "LyraLogChannels.h"

#if WITH_EDITOR
#include "CommonUIVisibilitySubsystem.h"
#endif	// WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraControllerDisconnectedScreen)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_Input_HasStrictControllerPairing, "Platform.Trait.Input.HasStrictControllerPairing");

// 初始化需要显示“切换用户”入口的平台特性要求。
ULyraControllerDisconnectedScreen::ULyraControllerDisconnectedScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 默认仅要求严格控制器用户配对的平台提供“切换用户”按钮。
	// By default, only strict pairing platforms will need this button.
	PlatformSupportsUserChangeTags.AddTag(TAG_Platform_Trait_Input_HasStrictControllerPairing);
}

// 激活断开提示时校验绑定控件，按平台严格配对能力显示切换用户入口并绑定按钮。
void ULyraControllerDisconnectedScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (!HBox_SwitchUser)
	{
		UE_LOG(LogLyra, Error, TEXT("Unable to find HBox_SwitchUser on Widget %s"), *GetNameSafe(this));
		return;
	}

	if (!Button_ChangeUser)
	{
		UE_LOG(LogLyra, Error, TEXT("Unable to find Button_ChangeUser on Widget %s"), *GetNameSafe(this));
		return;
	}

	HBox_SwitchUser->SetVisibility(ESlateVisibility::Collapsed);
	Button_ChangeUser->SetVisibility(ESlateVisibility::Hidden);

	if (ShouldDisplayChangeUserButton())
	{
		// 只有平台能提供“未配对输入设备”对应的有效用户 ID 时才显示切换入口，并非所有平台都支持此概念。
		// This is the platform user for "unpaired" input devices. Not every platform supports this, so
		// only set this to visible if the unpaired user is valid.
		const FPlatformUserId UnpairedUserId = IPlatformInputDeviceMapper::Get().GetUserForUnpairedInputDevices();
		if (UnpairedUserId.IsValid())
		{
			HBox_SwitchUser->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Button_ChangeUser->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}		
	}

	Button_ChangeUser->OnClicked().AddUObject(this, &ThisClass::HandleChangeUserClicked);
}

// 根据真实平台及编辑器模拟的平台特性判断是否支持切换控制器所属用户。
bool ULyraControllerDisconnectedScreen::ShouldDisplayChangeUserButton() const
{
	bool bRequiresChangeUserButton = ICommonUIModule::GetSettings().GetPlatformTraits().HasAll(PlatformSupportsUserChangeTags);

	// 编辑器中还要检查 CommonUI 可见性子系统正在模拟的平台特征。
	// Check the tags that we may be emulating in the editor too
#if WITH_EDITOR
	const FGameplayTagContainer& PlatformEmulationTags = UCommonUIVisibilitySubsystem::Get(GetOwningLocalPlayer())->GetVisibilityTags();
	bRequiresChangeUserButton |= PlatformEmulationTags.HasAll(PlatformSupportsUserChangeTags);
#endif	// WITH_EDITOR

	return bRequiresChangeUserButton;
}

// 取得当前玩家主输入设备并打开平台用户选择器，完成后转入用户切换回调。
void ULyraControllerDisconnectedScreen::HandleChangeUserClicked()
{
	ensure(ShouldDisplayChangeUserButton());

	UE_LOG(LogLyra, Log, TEXT("[%hs] Change user requested!"), __func__);

	const FPlatformUserId OwningPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();
	const FInputDeviceId DeviceId = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(OwningPlayerId);

	FGenericPlatformApplicationMisc::ShowPlatformUserSelector(
		DeviceId,
		EPlatformUserSelectorFlags::Default,
		[this](const FPlatformUserSelectionCompleteParams& Params)
		{
			HandleChangeUserCompleted(Params);
		});
}

// 接收平台用户选择结果；当前仅记录完成事件，实际登录和玩家重映射仍待项目接入。
void ULyraControllerDisconnectedScreen::HandleChangeUserCompleted(const FPlatformUserSelectionCompleteParams& Params)
{
	UE_LOG(LogLyra, Log, TEXT("[%hs] User change complete!"), __func__);

	// TODO：在此接入项目实际的用户切换、登录状态和本地玩家重映射逻辑。
	// TODO: Handle any user changing logic in your game here
}
