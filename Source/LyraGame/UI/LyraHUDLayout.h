// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraActivatableWidget.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"

#include "LyraHUDLayout.generated.h"

class UCommonActivatableWidget;
class UObject;
class ULyraControllerDisconnectedScreen;

// 玩家 HUD 的根布局控件，通常由 Experience 的 Add Widgets 动作创建，并负责暂停菜单和控制器断开提示。
/**
 * ULyraHUDLayout
 *
 *	Widget used to lay out the player's HUD (typically specified by an Add Widgets action in the experience)
 */
UCLASS(Abstract, BlueprintType, Blueprintable, Meta = (DisplayName = "Lyra HUD Layout", Category = "Lyra|HUD"))
class ULyraHUDLayout : public ULyraActivatableWidget
{
	GENERATED_BODY()

public:

	ULyraHUDLayout(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

protected:
	void HandleEscapeAction();
	
	// 输入设备连接状态改变时检查所属玩家是否仍有可用手柄；没有时请求显示断开页面。
	/** 
	* Callback for when controllers are disconnected. This will check if the player now has 
	* no mapped input devices to them, which would mean that they can't play the game.
	* 
	* If this is the case, then call DisplayControllerDisconnectedMenu.
	*/
	void HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId);

	// 输入设备重新分配给平台用户时重新检查，决定是否可以关闭控制器断开页面。
	/**
	* Callback for when controllers change their owning platform user. We will use this to check
	* if we no longer need to display the "Controller Disconnected" menu
	*/
	void HandleInputDevicePairingChanged(FInputDeviceId InputDeviceId, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId);
	
	// 将控制器状态检查合并到下一 Tick，避免同一帧多个设备或配对事件重复处理 UI。
	/**
	* Notify this widget that the state of controllers for the player have changed. Queue a timer for next tick to 
	* process them and see if we need to show/hide the "controller disconnected" widget.
	*/
	void NotifyControllerStateChangeForDisconnectScreen();

	// 枚举映射到所属平台用户的输入设备；没有已连接手柄时显示断开页面，否则关闭现有页面。
	/**
	 * This will check the state of the connected controllers to the player. If they do not have
	 * any controllers connected to them, then we should display the Disconnect menu. If they do have
	 * controllers connected to them, then we can hide the disconnect menu if its showing.
	 */
	virtual void ProcessControllerDevicesHavingChangedForDisconnectScreen();

	// 当前平台或编辑器模拟特征满足配置标签时，才允许显示控制器断开页面。
	/**
     * Returns true if this platform supports a "controller disconnected" screen. 
     */
    virtual bool ShouldPlatformDisplayControllerDisconnectScreen() const;
	
	// 将控制器断开页面压入 UI.Layer.Menu，并保存活动实例弱引用。
	/**
	* Pushes the ControllerDisconnectedMenuClass to the Menu layer (UI.Layer.Menu)
	*/
	UFUNCTION(BlueprintNativeEvent, Category="Controller Disconnect Menu")
	void DisplayControllerDisconnectedMenu();

	// 若控制器断开页面仍处于活动状态，则将其停用并清除引用。
	/**
	* Hides the controller disconnected menu if it is active.
	*/
	UFUNCTION(BlueprintNativeEvent, Category="Controller Disconnect Menu")
	void HideControllerDisconnectedMenu();
	
	// 玩家触发 Pause 或 Escape 时压入菜单层的暂停页面类型。
	/**
	 * The menu to be displayed when the user presses the "Pause" or "Escape" button 
	 */
	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UCommonActivatableWidget> EscapeMenuClass;

	// 所有映射手柄都断开时展示的阻塞页面类型。
	/** 
	* The widget which should be presented to the user if all of their controllers are disconnected.
	*/
	UPROPERTY(EditDefaultsOnly, Category="Controller Disconnect Menu")
	TSubclassOf<ULyraControllerDisconnectedScreen> ControllerDisconnectedScreen;

	// 显示控制器断开页面所需的平台特征；平台 ini 未配置时永不显示。
	/**
	 * The platform tags that are required in order to show the "Controller Disconnected" screen.
	 *
	 * If these tags are not set in the INI file for this platform, then the controller disconnect screen
	 * will not ever be displayed. 
	 */
	UPROPERTY(EditDefaultsOnly, Category="Controller Disconnect Menu")
	FGameplayTagContainer PlatformRequiresControllerDisconnectScreen;

	/** 当前活动控制器断开页面的弱引用。 */
	/** Pointer to the active "Controller Disconnected" menu if there is one. */
	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidget> SpawnedControllerDisconnectScreen;

	/** 下一 Tick 合并处理所属玩家控制器状态的 Ticker 句柄。 */
	/** Handle from the FSTicker for when we want to process the controller state of our player */
	FTSTicker::FDelegateHandle RequestProcessControllerStateHandle;
};
