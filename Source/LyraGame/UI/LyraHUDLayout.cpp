// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHUDLayout.h"

#include "CommonUIExtensions.h"
#include "CommonUISettings.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputSettings.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Input/CommonUIInputTypes.h"
#include "ICommonUIModule.h"
#include "LyraLogChannels.h"
#include "NativeGameplayTags.h"
#include "UI/Foundation/LyraControllerDisconnectedScreen.h"
#include "UI/LyraActivatableWidget.h"

#if WITH_EDITOR
#include "CommonUIVisibilitySubsystem.h"
#endif	// WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHUDLayout)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MENU, "UI.Layer.Menu");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_ACTION_ESCAPE, "UI.Action.Escape");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_Input_PrimarlyController, "Platform.Trait.Input.PrimarlyController");

// 初始化控制器断开页面状态，并默认要求主要使用手柄的平台特性。
ULyraHUDLayout::ULyraHUDLayout(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SpawnedControllerDisconnectScreen(nullptr)
{
	// 默认仅主要使用手柄的平台需要控制器断开页面。
	// By default, only primarily controller platforms require a disconnect screen. 
	PlatformRequiresControllerDisconnectScreen.AddTag(TAG_Platform_Trait_Input_PrimarlyController);
}

// 注册 Escape 菜单动作，并在平台需要断开提示时监听输入设备连接和用户配对变化。
void ULyraHUDLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(TAG_UI_ACTION_ESCAPE), false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEscapeAction)));

	// 平台支持断开页面时才绑定输入设备连接和用户配对变化委托。
	// If we can display a controller disconnect screen, then listen for the controller state change delegates
	if (ShouldPlatformDisplayControllerDisconnectScreen())
	{
		// 监听输入设备连接状态变化。
		// Bind to when input device connections change
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		DeviceMapper.GetOnInputDeviceConnectionChange().AddUObject(this, &ThisClass::HandleInputDeviceConnectionChanged);
		DeviceMapper.GetOnInputDevicePairingChange().AddUObject(this, &ThisClass::HandleInputDevicePairingChanged);	
	}
}

// 销毁布局时解除输入设备委托，并取消尚未执行的合并状态检查 Ticker。
void ULyraHUDLayout::NativeDestruct()
{
	Super::NativeDestruct();

	// 控件销毁时解除输入设备委托，防止子系统继续回调已失效布局。
	// Remove bindings to input device connection changing
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceMapper.GetOnInputDeviceConnectionChange().RemoveAll(this);
	DeviceMapper.GetOnInputDevicePairingChange().RemoveAll(this);

	if (RequestProcessControllerStateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RequestProcessControllerStateHandle);
		RequestProcessControllerStateHandle.Reset();
	}
}

// 将软引用的 Escape 菜单异步推入当前玩家的菜单层；类未配置时触发校验。
void ULyraHUDLayout::HandleEscapeAction()
{
	if (ensure(!EscapeMenuClass.IsNull()))
	{
		UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(GetOwningLocalPlayer(), TAG_UI_LAYER_MENU, EscapeMenuClass);
	}
}

// 仅当前本地玩家的设备连接状态变化时安排控制器断开检查。
void ULyraHUDLayout::HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId)
{
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();

	ensure(OwningLocalPlayerId.IsValid());

	// 连接变化不属于此布局的本地玩家时忽略。
	// This device connection change happened to a different player, ignore it for us.
	if (PlatformUserId != OwningLocalPlayerId)
	{
		return;
	}

	NotifyControllerStateChangeForDisconnectScreen();
}

// 输入设备的新旧平台用户涉及当前本地玩家时安排控制器断开检查。
void ULyraHUDLayout::HandleInputDevicePairingChanged(FInputDeviceId InputDeviceId, FPlatformUserId NewUserPlatformId, FPlatformUserId OldUserPlatformId)
{
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();

	ensure(OwningLocalPlayerId.IsValid());

	// 设备配对变化的旧用户或新用户是当前本地玩家时，安排重新检查。
	// If this pairing change was related to our local player, notify of a change.
	if (NewUserPlatformId == OwningLocalPlayerId || OldUserPlatformId == OwningLocalPlayerId)
	{
		NotifyControllerStateChangeForDisconnectScreen();	
	}
}

// 根据真实平台及编辑器模拟特性判断是否应启用控制器断开页面。
bool ULyraHUDLayout::ShouldPlatformDisplayControllerDisconnectScreen() const
{
	// 断开页面只应出现在主要使用手柄的平台。
	// We only want this menu on primarily controller platforms
	bool bHasAllRequiredTags = ICommonUIModule::GetSettings().GetPlatformTraits().HasAll(PlatformRequiresControllerDisconnectScreen);

	// 编辑器中同时检查 CommonUI 当前模拟的平台特征。
	// Check the tags that we may be emulating in the editor too
#if WITH_EDITOR
	const FGameplayTagContainer& PlatformEmulationTags = UCommonUIVisibilitySubsystem::Get(GetOwningLocalPlayer())->GetVisibilityTags();
	bHasAllRequiredTags |= PlatformEmulationTags.HasAll(PlatformRequiresControllerDisconnectScreen);
#endif	// WITH_EDITOR

	return bHasAllRequiredTags;
}

// 把同一帧多次设备状态变化合并为下一 Tick 的一次检查。
void ULyraHUDLayout::NotifyControllerStateChangeForDisconnectScreen()
{
	// 只有已绑定控制器状态委托的平台才应进入此通知路径。
	// We should only ever get here if we have bound to the controller state change delegates
	ensure(ShouldPlatformDisplayControllerDisconnectScreen());

	// 尚未排队时才创建下一 Tick 任务，将同一帧的多次状态变化合并。
	// If we haven't already, queue the processing of device state for next tick.
	if (!RequestProcessControllerStateHandle.IsValid())
	{
		RequestProcessControllerStateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
		{
			RequestProcessControllerStateHandle.Reset();
			ProcessControllerDevicesHavingChangedForDisconnectScreen();
			return false;
		}));
	}
}

// 检查当前平台用户映射的设备是否仍有已连接手柄，并据此显示或关闭断开页面。
void ULyraHUDLayout::ProcessControllerDevicesHavingChangedForDisconnectScreen()
{
	// 仅支持断开页面并已绑定委托的平台会执行实际检查。
	// We should only ever get here if we have bound to the controller state change delegates
	ensure(ShouldPlatformDisplayControllerDisconnectScreen());
	
	const FPlatformUserId OwningLocalPlayerId = GetOwningLocalPlayer()->GetPlatformUserId();
	
	ensure(OwningLocalPlayerId.IsValid());

	// 取得映射到当前平台用户的全部输入设备 ID。
	// Get all input devices mapped to our player
	const IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
    TArray<FInputDeviceId> MappedInputDevices;
    const int32 NumDevicesMappedToUser = DeviceMapper.GetAllInputDevicesForUser(OwningLocalPlayerId, OUT MappedInputDevices);

	// 检查该平台用户是否仍映射了任一已连接手柄。
    // Check if there are any other connected GAMEPAD devices mapped to this platform user. 
    bool bHasConnectedController = false;

    for (const FInputDeviceId MappedDevice : MappedInputDevices)
    {
    	if (DeviceMapper.GetInputDeviceConnectionState(MappedDevice) == EInputDeviceConnectionState::Connected)
    	{
    		const FHardwareDeviceIdentifier HardwareInfo = UInputDeviceSubsystem::Get()->GetInputDeviceHardwareIdentifier(MappedDevice);
    		if (HardwareInfo.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad)
    		{
    			bHasConnectedController = true;
    		}
    	}			
    }

	// 没有任何已连接手柄时显示重新连接提示页面。
    // If there are no gamepad input devices mapped to this user, then we want to pop the toast saying to re-connect them
    if (!bHasConnectedController)
    {
    	DisplayControllerDisconnectedMenu();
    }
	// 找到可用手柄后关闭当前断开页面。
	// Otherwise we can hide the screen if it is currently being shown
	else if (SpawnedControllerDisconnectScreen)
	{
		HideControllerDisconnectedMenu();
	}
}

// 将配置的控制器断开页面压入菜单层，并保存生成的可激活控件实例。
void ULyraHUDLayout::DisplayControllerDisconnectedMenu_Implementation()
{
	UE_LOG(LogLyra, Log, TEXT("[%hs] Display controller disconnected menu!"), __func__);

	if (ControllerDisconnectedScreen)
	{
		// 将控制器断开页面压入菜单层，并在异步创建完成后保存实例。
		// Push the "controller disconnected" widget to the menu layer
		SpawnedControllerDisconnectScreen = UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), TAG_UI_LAYER_MENU, ControllerDisconnectedScreen);
	}
}

// 从 CommonUI 层弹出当前断开页面并清空实例引用。
void ULyraHUDLayout::HideControllerDisconnectedMenu_Implementation()
{
	UE_LOG(LogLyra, Log, TEXT("[%hs] Hide controller disconnected menu!"), __func__);
	
	UCommonUIExtensions::PopContentFromLayer(SpawnedControllerDisconnectScreen);
	SpawnedControllerDisconnectScreen = nullptr;
}
