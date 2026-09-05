// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"

#include "LyraControllerDisconnectedScreen.generated.h"

class UHorizontalBox;
class UObject;
class UCommonButtonBase;
struct FPlatformUserSelectionCompleteParams;

// 当本地玩家没有可用手柄时阻塞游戏的 CommonUI 页面，并可在严格用户配对平台上发起平台用户切换。
/**
 * A screen to display when the user has had all of their controllers disconnected and needs to
 * re-connect them to continue playing the game.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ULyraControllerDisconnectedScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	ULyraControllerDisconnectedScreen(const FObjectInitializer& ObjectInitializer);
	
protected:
	virtual void NativeOnActivated() override;

	virtual void HandleChangeUserClicked();

	// 平台用户选择器完成后调用，供项目接入实际的用户切换和登录状态迁移逻辑。
	/**
	 * Called when the user has changed after selecting the prompt to change platform users. 
	 */
	virtual void HandleChangeUserCompleted(const FPlatformUserSelectionCompleteParams& Params);

	// 当前平台或编辑器模拟特征满足全部所需标签时返回 true。
	/**
	 * Returns true if the Change User button should be displayed.
	 * This will check the ICommonUIModule's platform trait tags at runtime.
	 */
	virtual bool ShouldDisplayChangeUserButton() const;

	// 显示“切换用户”按钮所需的全部平台特征标签。
	/**
	 * Required platform traits that, when met, will display the "Change User" button
	 * allowing the player to change what signed in user is currently mapped to an input
	 * device.
	 */
	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer PlatformSupportsUserChangeTags;

	// 严格控制器用户配对平台显示的用户切换区域；缺少所需平台特征时折叠。
	/**
	 * Platforms that have "strict" user pairing requirements may want to allow you to change your user right from
	 * the in-game UI here. These platforms are tagged with "Platform.Trait.Input.HasStrictControllerPairing" in
	 * Common UI.
	 *
	 * This HBox will be set to invisible if the platform you are on does NOT have that platform trait.
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HBox_SwitchUser;

	// 调用平台用户选择器的按钮，仅在严格用户配对平台显示。
	/**
	* A button to handle changing the user on platforms with strict user pairing requirements.
	* 
	* @see HBox_SwitchUser
	*/
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Button_ChangeUser;
};
