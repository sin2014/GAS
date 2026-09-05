// Copyright Epic Games, Inc. All Rights Reserved.
#include "CommonUserBasicPresence.h"
#include "CommonSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "CommonUserTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUserBasicPresence)


#if COMMONUSER_OSSV1
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlinePresenceInterface.h"
#else
#include "Online/OnlineServicesEngineUtils.h"
#include "Online/Presence.h"
#endif

// Basic Presence 会话状态映射和后端更新使用的日志分类。
DECLARE_LOG_CATEGORY_EXTERN(LogUserBasicPresence, Log, All);
DEFINE_LOG_CATEGORY(LogUserBasicPresence);

// 构造基础 Presence 子系统，配置键和值由 Engine 配置文件加载。
UCommonUserBasicPresence::UCommonUserBasicPresence()
{

}

// 强制初始化 CommonSessionSubsystem，并监听其可展示会话信息变化以推送 Presence。
void UCommonUserBasicPresence::Initialize(FSubsystemCollectionBase& Collection)
{
	UCommonSessionSubsystem* CommonSession = Collection.InitializeDependency<UCommonSessionSubsystem>();
	if(ensure(CommonSession))
	{
		CommonSession->OnSessionInformationChangedEvent.AddUObject(this, &UCommonUserBasicPresence::OnNotifySessionInformationChanged);
	}
}

// 子系统反初始化入口当前不执行显式解绑或其他清理。
void UCommonUserBasicPresence::Deinitialize()
{

}

// 将 OutOfGame、Matchmaking 和 InGame 会话状态映射为配置的后端 Presence 状态键，未知值记录错误。
FString UCommonUserBasicPresence::SessionStateToBackendKey(ECommonSessionInformationState SessionStatus)
{
	switch (SessionStatus)
	{
	case ECommonSessionInformationState::OutOfGame:
		return PresenceStatusMainMenu;
		break;
	case ECommonSessionInformationState::Matchmaking:
		return PresenceStatusMatchmaking;
		break;
	case ECommonSessionInformationState::InGame:
		return PresenceStatusInGame;
		break;
	default:
		UE_LOG(LogUserBasicPresence, Error, TEXT("UCommonUserBasicPresence::SessionStateToBackendKey: Found unknown enum value %d"), (uint8)SessionStatus);
		return TEXT("Unknown");
		break;

	}
}

// 非专服且总开关启用时，清理地图 URL 并为所有具备有效在线账户的本地玩家更新状态、模式和地图 Presence。
void UCommonUserBasicPresence::OnNotifySessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName)
{
	if (bEnableSessionsBasedPresence && !GetGameInstance()->IsDedicatedServerInstance())
	{
		// 地图参数可能是完整 URL，仅保留最后一个斜杠后的地图名称用于 Presence。
		// trim the map name since its a URL
		FString MapNameTruncated = MapName;
		if (!MapNameTruncated.IsEmpty())
		{
			int LastIndexOfSlash = 0;
			MapNameTruncated.FindLastChar('/', LastIndexOfSlash);
			MapNameTruncated = MapNameTruncated.RightChop(LastIndexOfSlash + 1);
		}

#if COMMONUSER_OSSV1
		IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
		if(OnlineSub)
		{
			IOnlinePresencePtr Presence = OnlineSub->GetPresenceInterface();
			if(Presence)
			{
				FOnlineUserPresenceStatus UpdatedPresence;
				// 只有具备有效 UniqueNetId 的用户才会收到更新，因此可将 Presence 状态标记为 Online。
				UpdatedPresence.State = EOnlinePresenceState::Online; // We'll only send the presence update if the user has a valid UniqueNetId, so we can assume they are Online
				UpdatedPresence.StatusStr = *SessionStateToBackendKey(SessionStatus);
				UpdatedPresence.Properties.Emplace(PresenceKeyGameMode, GameMode);
				UpdatedPresence.Properties.Emplace(PresenceKeyMapName, MapNameTruncated);

				for (const ULocalPlayer* LocalPlayer : GetGameInstance()->GetLocalPlayers())
				{
					if (LocalPlayer && LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId() != nullptr)
					{
						Presence->SetPresence(*LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId(), UpdatedPresence);
					}
				}
			}
		}

#else

		UE::Online::IOnlineServicesPtr OnlineServices = UE::Online::GetServices(GetWorld());
		check(OnlineServices);
		UE::Online::IPresencePtr Presence = OnlineServices->GetPresenceInterface();
		if(Presence)
		{
			for (const ULocalPlayer* LocalPlayer : GetGameInstance()->GetLocalPlayers())
			{
				if (LocalPlayer && LocalPlayer->GetPreferredUniqueNetId().IsV2())
				{
					UE::Online::FPartialUpdatePresence::Params UpdateParams;
					UpdateParams.LocalAccountId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
					UpdateParams.Mutations.StatusString.Emplace(*SessionStateToBackendKey(SessionStatus));
					UpdateParams.Mutations.UpdatedProperties.AddVariant(PresenceKeyGameMode, GameMode);
					UpdateParams.Mutations.UpdatedProperties.AddVariant(PresenceKeyMapName, MapNameTruncated);

					Presence->PartialUpdatePresence(MoveTemp(UpdateParams));
				}
			}

		}
#endif
	}
}
