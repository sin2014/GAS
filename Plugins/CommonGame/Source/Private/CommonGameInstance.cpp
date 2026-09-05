// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonGameInstance.h"

#include "CommonLocalPlayer.h"
#include "CommonSessionSubsystem.h"
#include "CommonUISettings.h"
#include "CommonUserSubsystem.h"
#include "GameUIManagerSubsystem.h"
#include "ICommonUIModule.h"
#include "LogCommonGame.h"
#include "Messaging/CommonGameDialog.h"
#include "Messaging/CommonMessagingSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonGameInstance)

// 创建游戏实例并初始化待加入会话和主玩家状态为空。
UCommonGameInstance::UCommonGameInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

// 筛选 CommonUser 系统消息，将严重错误交给主玩家消息子系统显示。
void UCommonGameInstance::HandleSystemMessage(FGameplayTag MessageType, FText Title, FText Message)
{
	ULocalPlayer* FirstPlayer = GetFirstGamePlayer();
	// 将严重的 CommonUser 消息转发到第一位玩家的错误对话框。
	// Forward severe ones to the error dialog for the first player
	if (FirstPlayer && MessageType.MatchesTag(FCommonUserTags::SystemMessage_Error))
	{
		if (UCommonMessagingSubsystem* Messaging = FirstPlayer->GetSubsystem<UCommonMessagingSubsystem>())
		{
			Messaging->ShowError(UCommonGameDialogDescriptor::CreateConfirmationOk(Title, Message));
		}
	}
}

// 主玩家失去在线游戏权限时显示错误并触发用户与会话重置。
void UCommonGameInstance::HandlePrivilegeChanged(const UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserAvailability OldAvailability, ECommonUserAvailability NewAvailability)
{
	// 默认显示错误；第一位玩家失去游戏权限时断开当前游戏流程。
	// By default show errors and disconnect if play privilege for first player is lost
	if (Privilege == ECommonUserPrivilege::CanPlay && OldAvailability == ECommonUserAvailability::NowAvailable && NewAvailability != ECommonUserAvailability::NowAvailable)
	{
		UE_LOG(LogCommonGame, Error, TEXT("HandlePrivilegeChanged: Player %d no longer has permission to play the game!"), UserInfo->LocalPlayerIndex);
		// TODO：具体游戏可在派生类中补充失去权限后的专属处理，例如返回主菜单。
		// TODO: Games can do something specific in subclass
		// ReturnToMainMenu();
	}
}

// 用户初始化完成后更新主玩家引用；失败时允许项目决定后续恢复策略。
void UCommonGameInstance::HandlerUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	// 派生游戏实例可覆盖此处以执行项目专属的重置流程。
	// Subclasses can override this
}

// 添加本地玩家并通知 UI 管理器为其创建或启用根布局。
int32 UCommonGameInstance::AddLocalPlayer(ULocalPlayer* NewPlayer, FPlatformUserId UserId)
{
	int32 ReturnVal = Super::AddLocalPlayer(NewPlayer, UserId);
	if (ReturnVal != INDEX_NONE)
	{
		if (!PrimaryPlayer.IsValid())
		{
			UE_LOG(LogCommonGame, Log, TEXT("AddLocalPlayer: Set %s to Primary Player"), *NewPlayer->GetName());
			PrimaryPlayer = NewPlayer;
		}
		
		GetSubsystem<UGameUIManagerSubsystem>()->NotifyPlayerAdded(Cast<UCommonLocalPlayer>(NewPlayer));
	}
	
	return ReturnVal;
}

// 移除本地玩家前通知 UI 管理器释放其根布局和策略记录。
bool UCommonGameInstance::RemoveLocalPlayer(ULocalPlayer* ExistingPlayer)
{
	if (PrimaryPlayer == ExistingPlayer)
	{
		// TODO：需要确认主玩家失效时是否应自动切换到其他本地玩家。
		//TODO: do we want to fall back to another player?
		PrimaryPlayer.Reset();
		UE_LOG(LogCommonGame, Log, TEXT("RemoveLocalPlayer: Unsetting Primary Player from %s"), *ExistingPlayer->GetName());
	}
	GetSubsystem<UGameUIManagerSubsystem>()->NotifyPlayerDestroyed(Cast<UCommonLocalPlayer>(ExistingPlayer));

	return Super::RemoveLocalPlayer(ExistingPlayer);
}

// 初始化游戏实例并在相关子系统就绪后绑定 CommonUser 和会话事件。
void UCommonGameInstance::Init()
{
	Super::Init();

	// 所有子系统初始化完成后，再建立 CommonUser、会话与游戏实例之间的事件连接。
	// After subsystems are initialized, hook them together
	FGameplayTagContainer PlatformTraits = ICommonUIModule::GetSettings().GetPlatformTraits();

	UCommonUserSubsystem* UserSubsystem = GetSubsystem<UCommonUserSubsystem>();
	if (ensure(UserSubsystem))
	{
		UserSubsystem->SetTraitTags(PlatformTraits);
		UserSubsystem->OnHandleSystemMessage.AddDynamic(this, &UCommonGameInstance::HandleSystemMessage);
		UserSubsystem->OnUserPrivilegeChanged.AddDynamic(this, &UCommonGameInstance::HandlePrivilegeChanged);
		UserSubsystem->OnUserInitializeComplete.AddDynamic(this, &UCommonGameInstance::HandlerUserInitialized);
	}

	UCommonSessionSubsystem* SessionSubsystem = GetSubsystem<UCommonSessionSubsystem>();
	if (ensure(SessionSubsystem))
	{
		SessionSubsystem->OnUserRequestedSessionEvent.AddUObject(this, &UCommonGameInstance::OnUserRequestedSession);
		SessionSubsystem->OnDestroySessionRequestedEvent.AddUObject(this, &UCommonGameInstance::OnDestroySessionRequested);
	}
}

// 请求具体游戏退出当前流程并清理用户、会话与待加入邀请状态。
void UCommonGameInstance::ResetUserAndSessionState()
{
	UCommonUserSubsystem* UserSubsystem = GetSubsystem<UCommonUserSubsystem>();
	if (ensure(UserSubsystem))
	{
		UserSubsystem->ResetUserState();
	}

	UCommonSessionSubsystem* SessionSubsystem = GetSubsystem<UCommonSessionSubsystem>();
	if (ensure(SessionSubsystem))
	{
		SessionSubsystem->CleanUpSessions();
	}
}

// 返回主菜单前重置用户、会话和 UI 状态，再调用父类切换流程。
void UCommonGameInstance::ReturnToMainMenu()
{
	// 默认在返回主菜单时重置用户和会话状态，避免旧流程残留。
	// By default when returning to main menu we should reset everything
	ResetUserAndSessionState();

	Super::ReturnToMainMenu();
}

// 接收平台邀请等外部会话请求并启动请求会话状态机。
void UCommonGameInstance::OnUserRequestedSession(const FPlatformUserId& PlatformUserId, UCommonSession_SearchResult* InRequestedSession, const FOnlineResultInformation& RequestedSessionResult)
{
	if (InRequestedSession)
	{
		SetRequestedSession(InRequestedSession);
	}
	else
	{
		HandleSystemMessage(FCommonUserTags::SystemMessage_Error, NSLOCTEXT("CommonGame", "Warning_RequestedSessionFailed", "Requested Session Failed"), RequestedSessionResult.ErrorText);
	}
}

// 在线子系统请求销毁会话时，先让游戏过渡到可安全退出的状态。
void UCommonGameInstance::OnDestroySessionRequested(const FPlatformUserId& PlatformUserId, const FName& SessionName)
{
	// 收到销毁会话请求时，项目必须先进入可以安全退出并销毁会话的状态。
	// When a session destroy is requested, please make sure that your project is in the right state to destroy the session and transition out of it

	UE_LOG(LogCommonGame, Verbose, TEXT("[%hs] PlatformUserId:%d, SessionName: %s)"), __FUNCTION__, PlatformUserId.GetInternalId(), *SessionName.ToString());

	ReturnToMainMenu();
}

// 保存或清除待加入会话；若当前允许则立即加入，否则启动重置过渡。
void UCommonGameInstance::SetRequestedSession(UCommonSession_SearchResult* InRequestedSession)
{
	RequestedSession = InRequestedSession;
	if (RequestedSession)
	{
		if (CanJoinRequestedSession())
		{
			JoinRequestedSession();
		}
		else
		{
			ResetGameAndJoinRequestedSession();
		}
	}
}

// 提供项目可覆盖的会话加入门禁；基类始终允许。
bool UCommonGameInstance::CanJoinRequestedSession() const
{
	// 基类默认允许立即加入外部请求的会话，具体游戏可增加状态限制。
	// Default behavior is always allow joining the requested session
	return true;
}

// 消费缓存的会话请求并交给会话子系统执行加入，防止重复处理。
void UCommonGameInstance::JoinRequestedSession()
{
	if (RequestedSession)
	{
		if (ULocalPlayer* const FirstPlayer = GetFirstGamePlayer())
		{
			UCommonSessionSubsystem* SessionSubsystem = GetSubsystem<UCommonSessionSubsystem>();
			if (ensure(SessionSubsystem))
			{
				// 开始处理会话请求后立即清空缓存，避免同一邀请被重复消费。
				// Clear our current requested session since we are now acting on it.
				UCommonSession_SearchResult* LocalRequestedSession = RequestedSession;
				RequestedSession = nullptr;
				SessionSubsystem->JoinSession(FirstPlayer->PlayerController, LocalRequestedSession);
			}
		}
	}
}

// 基类返回主菜单以准备加入；项目就绪后需再次调用 JoinRequestedSession。
void UCommonGameInstance::ResetGameAndJoinRequestedSession()
{
	// 基类通过返回主菜单准备加入会话；游戏进入可加入状态后必须显式调用 JoinRequestedSession。
	// Default behavior is to return to the main menu.  The game must call JoinRequestedSession when the game is in a ready state.
	ReturnToMainMenu();
}


//void UCommonGameInstance::OnPreLoadMap(const FString& MapName)
//{
//	if (!IsDedicatedServerInstance())
//	{
//		if (!bWasInLoadMap)
//		{
//			UGameUIManagerSubsystem* UIManager = GetSubsystem<UGameUIManagerSubsystem>();
//			for (ULocalPlayer* LocalPlayer : LocalPlayers)
//			{
//				UIManager->NotifyPlayerAdded(Cast<UCommonLocalPlayer>(LocalPlayer));
//			}
//		}
//	}
//}
