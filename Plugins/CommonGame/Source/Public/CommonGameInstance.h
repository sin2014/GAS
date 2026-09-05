// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"

#include "CommonGameInstance.generated.h"

#define UE_API COMMONGAME_API

enum class ECommonUserAvailability : uint8;
enum class ECommonUserPrivilege : uint8;

class FText;
class UCommonUserInfo;
class UCommonSession_SearchResult;
struct FOnlineResultInformation;
class ULocalPlayer;
class USocialManager;
class UObject;
struct FFrame;
struct FGameplayTag;

UCLASS(MinimalAPI, Abstract, Config = Game)
class UCommonGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UE_API UCommonGameInstance(const FObjectInitializer& ObjectInitializer);
	
	// 处理 CommonUser 错误和警告；具体游戏可覆盖显示与恢复策略。
	/** Handles errors/warnings from CommonUser, can be overridden per game */
	UFUNCTION()
	UE_API virtual void HandleSystemMessage(FGameplayTag MessageType, FText Title, FText Message);

	UFUNCTION()
	UE_API virtual void HandlePrivilegeChanged(const UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserAvailability OldAvailability, ECommonUserAvailability NewAvailability);

	UFUNCTION()
	UE_API virtual void HandlerUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	// 重置用户和会话状态，通常用于玩家断开连接后的恢复流程。
	/** Call to reset user and session state, usually because a player has been disconnected */
	UE_API virtual void ResetUserAndSessionState();

	// 外部来源请求加入指定会话时，由 SetRequestedSession 接收；若 CanJoinRequestedSession 允许则立即 JoinRequestedSession，否则缓存请求并通过 ResetGameAndJoinRequestedSession 让游戏过渡到可加入状态。
	/**
	 * Requested Session Flow
	 *   Something requests the user to join a specific session (for example, a platform overlay via OnUserRequestedSession).
	 *   This request is handled in SetRequestedSession.
	 *   Check if we can join the requested session immediately (CanJoinRequestedSession).  If we can, join the requested session (JoinRequestedSession)
	 *   If not, cache the requested session and instruct the game to get into a state where the session can be joined (ResetGameAndJoinRequestedSession)
	 */
	// 处理用户从平台覆盖层等外部来源接受会话邀请的事件，供具体游戏覆盖。
	/** Handles user accepting a session invite from an external source (for example, a platform overlay). Intended to be overridden per game. */
	UE_API virtual void OnUserRequestedSession(const FPlatformUserId& PlatformUserId, UCommonSession_SearchResult* InRequestedSession, const FOnlineResultInformation& RequestedSessionResult);

	// 处理在线子系统发出的会话销毁请求。
	/** Handles OSS request that the session be destroyed */
	UE_API virtual void OnDestroySessionRequested(const FPlatformUserId& PlatformUserId, const FName& SessionName);

	// 返回当前缓存的待加入会话请求。
	/** Get the requested session */
	UCommonSession_SearchResult* GetRequestedSession() const { return RequestedSession; }
	// 设置或清除待加入会话；设置非空请求会启动会话加入流程。
	/** Set (or clear) the requested session. When this is set, the requested session flow begins. */
	UE_API virtual void SetRequestedSession(UCommonSession_SearchResult* InRequestedSession);
	// 判断当前状态是否允许加入请求会话，具体游戏可覆盖。
	/** Checks if the requested session can be joined. Can be overridden per game. */
	UE_API virtual bool CanJoinRequestedSession() const;
	// 消费并加入当前缓存的会话请求。
	/** Join the requested session */
	UE_API virtual void JoinRequestedSession();
	// 让游戏过渡到可以安全加入请求会话的状态。
	/** Get the game into a state to join the requested session */
	UE_API virtual void ResetGameAndJoinRequestedSession();
	
	UE_API virtual int32 AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId) override;
	UE_API virtual bool RemoveLocalPlayer(ULocalPlayer* ExistingPlayer) override;
	UE_API virtual void Init() override;
	UE_API virtual void ReturnToMainMenu() override;

private:
	// 记录当前主本地玩家。
	/** This is the primary player*/
	TWeakObjectPtr<ULocalPlayer> PrimaryPlayer;
	// 玩家通过外部邀请请求加入的会话。
	/** Session the player has requested to join */
	UPROPERTY()
	TObjectPtr<UCommonSession_SearchResult> RequestedSession;
};

#undef UE_API
