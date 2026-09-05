// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserSubsystem.h"
#include "Engine/CancellableAsyncAction.h"

#include "AsyncAction_CommonUserInitialize.generated.h"

#define UE_API COMMONUSER_API

enum class ECommonUserOnlineContext : uint8;
enum class ECommonUserPrivilege : uint8;
struct FInputDeviceId;

class FText;
class UObject;
struct FFrame;

/**
 * 封装不同用户初始化入口的可取消异步蓝图节点。
 */
/**
 * Async action to handle different functions for initializing users
 */
UCLASS(MinimalAPI)
class UAsyncAction_CommonUserInitialize : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	/**
	 * 通过 CommonUser 系统初始化本地玩家，包括平台特定登录和权限检查；无论成功失败都会广播 OnInitializationComplete。
	 *
	 * @param LocalPlayerIndex GameInstance 中目标 LocalPlayer 索引，0 为主玩家，1 及以上用于本地多人。
	 * @param PrimaryInputDevice 用户的主要输入设备；无效时使用系统默认设备。
	 * @param bCanUseGuestLogin 是否允许没有真实系统 NetId 的访客登录。
	 */
	/**
	 * Initializes a local player with the common user system, which includes doing platform-specific login and privilege checks.
	 * When the process has succeeded or failed, it will broadcast the OnInitializationComplete delegate.
	 *
	 * @param LocalPlayerIndex	Desired index of ULocalPlayer in Game Instance, 0 will be primary player and 1+ for local multiplayer
	 * @param PrimaryInputDevice Primary input device for the user, if invalid will use the system default
	 * @param bCanUseGuestLogin	If true, this player can be a guest without a real system net id
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser, meta = (BlueprintInternalUseOnly = "true"))
	static UE_API UAsyncAction_CommonUserInitialize* InitializeForLocalPlay(UCommonUserSubsystem* Target, int32 LocalPlayerIndex, FInputDeviceId PrimaryInputDevice, bool bCanUseGuestLogin);

	/**
	 * 尝试让现有本地用户登录平台在线后端，以启用完整在线游戏；完成后广播 OnInitializationComplete。
	 *
	 * @param LocalPlayerIndex GameInstance 中现有 LocalPlayer 的索引。
	 */
	/**
	 * Attempts to log an existing user into the platform-specific online backend to enable full online play
	 * When the process has succeeded or failed, it will broadcast the OnInitializationComplete delegate.
	 *
	 * @param LocalPlayerIndex	Index of existing LocalPlayer in Game Instance
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser, meta = (BlueprintInternalUseOnly = "true"))
	static UE_API UAsyncAction_CommonUserInitialize* LoginForOnlinePlay(UCommonUserSubsystem* Target, int32 LocalPlayerIndex);

	/** 用户初始化成功或失败时广播的蓝图委托。 */
	/** Call when initialization succeeds or fails */
	UPROPERTY(BlueprintAssignable)
	FCommonUserOnInitializeCompleteMulticast OnInitializationComplete;

	/** 将节点标记为失败，并在尚未完成时发送失败回调。 */
	/** Fail and send callbacks if needed */
	UE_API void HandleFailure();

	/** 子系统初始化回调包装器；节点仍有效时转发到 OnInitializationComplete。 */
	/** Wrapper delegate, will pass on to OnInitializationComplete if appropriate */
	UFUNCTION()
	UE_API virtual void HandleInitializationComplete(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

protected:
	/** 实际向 CommonUserSubsystem 发起初始化请求。 */
	/** Actually start the initialization */
	UE_API virtual void Activate() override;

	TWeakObjectPtr<UCommonUserSubsystem> Subsystem;
	FCommonUserInitializeParams Params;
};

#undef UE_API
