// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncAction_CommonUserInitialize.h"

#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_CommonUserInitialize)

// 创建本地游戏初始化节点，补齐默认输入设备并配置 CanPlay、允许创建 LocalPlayer 及可选访客登录参数。
UAsyncAction_CommonUserInitialize* UAsyncAction_CommonUserInitialize::InitializeForLocalPlay(UCommonUserSubsystem* Target, int32 LocalPlayerIndex, FInputDeviceId PrimaryInputDevice, bool bCanUseGuestLogin)
{
	if (!PrimaryInputDevice.IsValid())
	{
		// 未指定有效输入设备时使用平台输入映射器的默认设备。
		// Set to default device
		PrimaryInputDevice = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
	}

	UAsyncAction_CommonUserInitialize* Action = NewObject<UAsyncAction_CommonUserInitialize>();

	Action->RegisterWithGameInstance(Target);

	if (Target && Action->IsRegistered())
	{
		Action->Subsystem = Target;
		
		Action->Params.RequestedPrivilege = ECommonUserPrivilege::CanPlay;
		Action->Params.LocalPlayerIndex = LocalPlayerIndex;
		Action->Params.PrimaryInputDevice = PrimaryInputDevice;
		Action->Params.bCanUseGuestLogin = bCanUseGuestLogin;
		Action->Params.bCanCreateNewLocalPlayer = true;
	}
	else
	{
		Action->SetReadyToDestroy();
	}

	return Action;
}

// 创建现有 LocalPlayer 的在线登录节点，要求 CanPlayOnline 且禁止创建新的本地玩家。
UAsyncAction_CommonUserInitialize* UAsyncAction_CommonUserInitialize::LoginForOnlinePlay(UCommonUserSubsystem* Target, int32 LocalPlayerIndex)
{
	UAsyncAction_CommonUserInitialize* Action = NewObject<UAsyncAction_CommonUserInitialize>();

	Action->RegisterWithGameInstance(Target);

	if (Target && Action->IsRegistered())
	{
		Action->Subsystem = Target;
		
		Action->Params.RequestedPrivilege = ECommonUserPrivilege::CanPlayOnline;
		Action->Params.LocalPlayerIndex = LocalPlayerIndex;
		Action->Params.bCanCreateNewLocalPlayer = false;
	}
	else
	{
		Action->SetReadyToDestroy();
	}

	return Action;
}

// 处理未能启动登录的早期失败，尽量附带现有 UserInfo，并统一转入完成回调和节点销毁。
void UAsyncAction_CommonUserInitialize::HandleFailure()
{
	const UCommonUserInfo* UserInfo = nullptr;
	if (Subsystem.IsValid())
	{
		UserInfo = Subsystem->GetUserInfoForLocalPlayerIndex(Params.LocalPlayerIndex);
	}
	HandleInitializationComplete(UserInfo, false, NSLOCTEXT("CommonUser", "LoginFailedEarly", "Unable to start login process"), Params.RequestedPrivilege, Params.OnlineContext);
}

// 在节点未取消时广播初始化结果，随后解除 GameInstance 注册并允许异步对象销毁。
void UAsyncAction_CommonUserInitialize::HandleInitializationComplete(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	if (ShouldBroadcastDelegates())
	{
		OnInitializationComplete.Broadcast(UserInfo, bSuccess, Error, RequestedPrivilege, OnlineContext);
	}

	SetReadyToDestroy();
}

// 绑定子系统完成回调并启动通用用户初始化；同步启动失败时延后一帧广播，避免 Activate 内重入蓝图。
void UAsyncAction_CommonUserInitialize::Activate()
{
	if (Subsystem.IsValid())
	{
		Params.OnUserInitializeComplete.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAsyncAction_CommonUserInitialize, HandleInitializationComplete));
		bool bSuccess = Subsystem->TryToInitializeUser(Params);

		if (!bSuccess)
		{
			// 将启动失败延后一帧处理，避免在 Activate 调用栈内同步触发完成委托。
			// Call failure next frame
			FTimerManager* TimerManager = GetTimerManager();
			
			if (TimerManager)
			{
				TimerManager->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAsyncAction_CommonUserInitialize::HandleFailure));
			}
		}
	}
	else
	{
		SetReadyToDestroy();
	}	
}

