// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonSessionSubsystem.h"
#include "AssetRegistry/AssetData.h"
#include "CommonUserTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineBeaconHost.h"
#include "OnlineSessionSettings.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonSessionSubsystem)

#if COMMONUSER_OSSV1
#include "Engine/World.h"
#include "OnlineSubsystemUtils.h"

// 会话广告和搜索用于区分 OSSv1 结果的 Schema 属性名和值。
FName SETTING_ONLINESUBSYSTEM_VERSION(TEXT("OSSv1"));
#else
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineSessionDelegates.h"
#include "Online/OnlineServicesEngineUtils.h"

// 会话广告和搜索用于区分 OSSv2 结果的 Schema 属性名和值。
FName SETTING_ONLINESUBSYSTEM_VERSION(TEXT("OSSv2"));
using namespace UE::Online;
#endif // COMMONUSER_OSSV1


// CommonSession 托管、搜索、加入、Travel 和 Beacon 流程使用的日志分类。
DECLARE_LOG_CATEGORY_EXTERN(LogCommonSession, Log, All);
DEFINE_LOG_CATEGORY(LogCommonSession);

#define LOCTEXT_NAMESPACE "CommonUser"

//////////////////////////////////////////////////////////////////////
//UCommonSession_SearchSessionRequest

// 搜索结束时以相同结果依次广播原生委托和蓝图动态委托。
void UCommonSession_SearchSessionRequest::NotifySearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	OnSearchFinished.Broadcast(bSucceeded, ErrorMessage);
	K2_OnSearchFinished.Broadcast(bSucceeded, ErrorMessage);
}


//////////////////////////////////////////////////////////////////////
//UCommonSession_SearchResult

#if COMMONUSER_OSSV1
// OSSv1 下返回底层 SessionId 字符串作为内部搜索结果描述。
FString UCommonSession_SearchResult::GetDescription() const
{
	return Result.GetSessionIdStr();
}

// OSSv1 下从 SessionSettings 读取字符串设置，并通过 bFoundValue 报告键是否存在。
void UCommonSession_SearchResult::GetStringSetting(FName Key, FString& Value, bool& bFoundValue) const
{
	bFoundValue = Result.Session.SessionSettings.Get<FString>(Key, /*out*/ Value);
}

// OSSv1 下从 SessionSettings 读取整数设置，并通过 bFoundValue 报告键是否存在。
void UCommonSession_SearchResult::GetIntSetting(FName Key, int32& Value, bool& bFoundValue) const
{
	bFoundValue = Result.Session.SessionSettings.Get<int32>(Key, /*out*/ Value);
}

// OSSv1 下返回底层会话当前可用的私有连接名额。
int32 UCommonSession_SearchResult::GetNumOpenPrivateConnections() const
{
	return Result.Session.NumOpenPrivateConnections;
}

// OSSv1 下返回底层会话当前可用的公开连接名额。
int32 UCommonSession_SearchResult::GetNumOpenPublicConnections() const
{
	return Result.Session.NumOpenPublicConnections;
}

// OSSv1 下返回会话配置的公开连接总容量。
int32 UCommonSession_SearchResult::GetMaxPublicConnections() const
{
	return Result.Session.SessionSettings.NumPublicConnections;
}

// OSSv1 下返回在线搜索测得的主机毫秒延迟。
int32 UCommonSession_SearchResult::GetPingInMs() const
{
	return Result.PingInMs;
}
#else
// OSSv2 下将 LobbyId 格式化为内部日志描述。
FString UCommonSession_SearchResult::GetDescription() const
{
	return ToLogString(Lobby->LobbyId);
}

// OSSv2 下从 Lobby Attribute 读取字符串值，属性缺失时只将 bFoundValue 置为 false。
void UCommonSession_SearchResult::GetStringSetting(FName Key, FString& Value, bool& bFoundValue) const
{
	if (const FSchemaVariant* VariantValue = Lobby->Attributes.Find(Key))
	{
		bFoundValue = true;
		Value = VariantValue->GetString();
	}
	else
	{
		bFoundValue = false;
	}
}

// OSSv2 下从 Lobby Attribute 读取 64 位整数并收窄为 int32，属性缺失时报告未找到。
void UCommonSession_SearchResult::GetIntSetting(FName Key, int32& Value, bool& bFoundValue) const
{
	if (const FSchemaVariant* VariantValue = Lobby->Attributes.Find(Key))
	{
		bFoundValue = true;
		Value = (int32)VariantValue->GetInt64();
	}
	else
	{
		bFoundValue = false;
	}
}

// OSSv2 Lobby 尚未实现私有连接名额，当前固定返回零。
int32 UCommonSession_SearchResult::GetNumOpenPrivateConnections() const
{
	// TODO：为 OSSv2 Lobby 或配套 Session 实现私有连接统计。
	// TODO:  Private connections
	return 0;
}

// OSSv2 下用 Lobby 最大成员数减去当前成员数计算公开空位。
int32 UCommonSession_SearchResult::GetNumOpenPublicConnections() const
{
	return Lobby->MaxMembers - Lobby->Members.Num();
}

// OSSv2 下返回 Lobby 的最大成员容量。
int32 UCommonSession_SearchResult::GetMaxPublicConnections() const
{
	return Lobby->MaxMembers;
}

// OSSv2 Lobby 不提供 Ping，当前固定返回零并等待 Session 层实现。
int32 UCommonSession_SearchResult::GetPingInMs() const
{
	// TODO：Lobby 本身没有 Ping 属性，需要通过配套 Session 查询实现。
	// TODO:  Not a property of lobbies.  Need to implement with sessions.
	return 0;
}
#endif //COMMONUSER_OSSV1


class FCommonOnlineSearchSettingsBase : public FGCObject
{
public:
	// 保存搜索请求 UObject，使搜索设置在异步查询期间通过 FGCObject 保持其生命周期。
	FCommonOnlineSearchSettingsBase(UCommonSession_SearchSessionRequest* InSearchRequest)
	{
		SearchRequest = InSearchRequest;
	}

	// 销毁搜索设置包装器，UObject 引用由 FGCObject 自动解除。
	virtual ~FCommonOnlineSearchSettingsBase() {}

	// 将 SearchRequest 报告给 GC，防止异步搜索完成前被回收。
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(SearchRequest);
	}

	// 返回 GC 引用来源的稳定调试名称。
	virtual FString GetReferencerName() const override
	{
		static const FString NameString = TEXT("FCommonOnlineSearchSettings");
		return NameString;
	}

public:
	TObjectPtr<UCommonSession_SearchSessionRequest> SearchRequest = nullptr;
};

#if COMMONUSER_OSSV1
//////////////////////////////////////////////////////////////////////
// FCommonSession_OnlineSessionSettings

class FCommonSession_OnlineSessionSettings : public FOnlineSessionSettings
{
public:

	// 初始化 OSSv1 托管会话设置，规范化公开容量并启用广告、邀请、进行中加入和可选 Presence。
	FCommonSession_OnlineSessionSettings(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 4)
	{
		NumPublicConnections = MaxNumPlayers;
		if (NumPublicConnections < 0)
		{
			NumPublicConnections = 0;
		}
		NumPrivateConnections = 0;
		bIsLANMatch = bIsLAN;
		bShouldAdvertise = true;
		bAllowJoinInProgress = true;
		bAllowInvites = true;
		bUsesPresence = bIsPresence;
		bAllowJoinViaPresence = true;
		bAllowJoinViaPresenceFriendsOnly = false;
	}

	// 销毁 OSSv1 会话设置对象，不持有额外外部资源。
	virtual ~FCommonSession_OnlineSessionSettings() {}
};

//////////////////////////////////////////////////////////////////////
// FCommonOnlineSearchSettingsOSSv1

class FCommonOnlineSearchSettingsOSSv1 : public FOnlineSessionSearch, public FCommonOnlineSearchSettingsBase
{
public:
	// 根据请求配置 OSSv1 LAN、结果数量、Ping 分桶、版本和可选 Lobby 搜索过滤条件。
	FCommonOnlineSearchSettingsOSSv1(UCommonSession_SearchSessionRequest* InSearchRequest)
		: FCommonOnlineSearchSettingsBase(InSearchRequest)
	{
		bIsLanQuery = (InSearchRequest->OnlineMode == ECommonSessionOnlineMode::LAN);
		MaxSearchResults = 10;
		PingBucketSize = 50;

		QuerySettings.Set(SETTING_ONLINESUBSYSTEM_VERSION, true, EOnlineComparisonOp::Equals);

		if (InSearchRequest->bUseLobbies)
		{
			QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		}
	}

	// 销毁 OSSv1 搜索设置，不持有额外异步句柄。
	virtual ~FCommonOnlineSearchSettingsOSSv1() {}
};
#else

class FCommonOnlineSearchSettingsOSSv2 : public FCommonOnlineSearchSettingsBase
{
public:
	// 配置 OSSv2 Lobby 搜索最大结果数，并只接受当前 OSSv2 Schema 版本的 Lobby。
	FCommonOnlineSearchSettingsOSSv2(UCommonSession_SearchSessionRequest* InSearchRequest)
		: FCommonOnlineSearchSettingsBase(InSearchRequest)
	{
		FindLobbyParams.MaxResults = 10;

		FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{ SETTING_ONLINESUBSYSTEM_VERSION, ESchemaAttributeComparisonOp::Equals, true });
	}
public:
	FFindLobbies::Params FindLobbyParams;
};

#endif // COMMONUSER_OSSV1

//////////////////////////////////////////////////////////////////////
// UCommonSession_HostSessionRequest

// 通过 AssetManager 将 MapID 解析为 World PrimaryAsset 的完整包名，无法解析时返回空字符串。
FString UCommonSession_HostSessionRequest::GetMapName() const
{
	FAssetData MapAssetData;
	if (UAssetManager::Get().GetPrimaryAssetData(MapID, /*out*/ MapAssetData))
	{
		return MapAssetData.PackageName.ToString();
	}
	else
	{
		return FString();
	}
}

// 将地图包名、LAN/listen 标志和 ExtraArgs 组合为托管端 ServerTravel URL。
FString UCommonSession_HostSessionRequest::ConstructTravelURL() const
{
	FString CombinedExtraArgs;

	if (OnlineMode == ECommonSessionOnlineMode::LAN)
	{
		CombinedExtraArgs += TEXT("?bIsLanMatch");
	}
	
	if (OnlineMode != ECommonSessionOnlineMode::Offline)
	{
		CombinedExtraArgs += TEXT("?listen");
	}

	for (const auto& KVP : ExtraArgs)
	{
		if (!KVP.Key.IsEmpty())
		{
			if (KVP.Value.IsEmpty())
			{
				CombinedExtraArgs += FString::Printf(TEXT("?%s"), *KVP.Key);
			}
			else
			{
				CombinedExtraArgs += FString::Printf(TEXT("?%s=%s"), *KVP.Key, *KVP.Value);
			}
		}
	}

	// 可在此扩展 DemoRec 等额外 Travel Option；当前未启用录制参数。
	//bIsRecordingDemo ? TEXT("?DemoRec") : TEXT(""));

	return FString::Printf(TEXT("%s%s"),
		*GetMapName(),
		*CombinedExtraArgs);
}

// 服务端代码验证地图 PrimaryAsset；纯客户端构建默认禁止托管并返回可展示错误。
bool UCommonSession_HostSessionRequest::ValidateAndLogErrors(FText& OutError) const
{
#if WITH_SERVER_CODE
	if (GetMapName().IsEmpty())
	{
		OutError = FText::Format(NSLOCTEXT("NetworkErrors", "InvalidMapFormat", "Can't find asset data for MapID {0}, hosting request failed."), FText::FromString(MapID.ToString()));
		return false;
	}

	return true;
#else
	// 纯客户端构建默认只连接专用服务器，因此不包含托管会话所需代码。
	// Client builds are only meant to connect to dedicated servers, they are missing the code to host a session by default
	// 派生请求可以覆写此限制，例如支持本地教程场景。
	// You can change this behavior in subclasses to handle something like a tutorial
	OutError = NSLOCTEXT("NetworkErrors", "ClientBuildCannotHost", "Client builds cannot host game sessions.");
	return false;
#endif
}

// 返回请求配置的最大玩家数，供派生类覆写实际容量策略。
int32 UCommonSession_HostSessionRequest::GetMaxPlayers() const
{
	return MaxPlayerCount;
}

//////////////////////////////////////////////////////////////////////
// UCommonSessionSubsystem

// 初始化会话子系统，绑定在线接口、Travel 失败和地图加载回调，并缓存是否为专用服务器。
void UCommonSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BindOnlineDelegates();
	GEngine->OnTravelFailure().AddUObject(this, &UCommonSessionSubsystem::TravelLocalSessionFailure);

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCommonSessionSubsystem::HandlePostLoadMap);

	UGameInstance* GameInstance = GetGameInstance();
	bIsDedicatedServer = GameInstance->IsDedicatedServerInstance();
}

// 按编译时 OSS 版本选择对应的会话事件绑定实现。
void UCommonSessionSubsystem::BindOnlineDelegates()
{
#if COMMONUSER_OSSV1
	BindOnlineDelegatesOSSv1();
#else
	BindOnlineDelegatesOSSv2();
#endif
}

#if COMMONUSER_OSSV1
// OSSv1 下绑定创建、启动、更新、结束、销毁、搜索、加入、邀请接受和会话失败回调。
void UCommonSessionSubsystem::BindOnlineDelegatesOSSv1()
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);

	const IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
	check(SessionInterface.IsValid());

	SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete));
	SessionInterface->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete));
	SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionComplete));
	SessionInterface->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionComplete));
	SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete));
	SessionInterface->AddOnDestroySessionRequestedDelegate_Handle(FOnDestroySessionRequestedDelegate::CreateUObject(this, &ThisClass::OnDestroySessionRequested));

//	SessionInterface->AddOnMatchmakingCompleteDelegate_Handle(FOnMatchmakingCompleteDelegate::CreateUObject(this, &ThisClass::OnMatchmakingComplete));
//	SessionInterface->AddOnCancelMatchmakingCompleteDelegate_Handle(FOnCancelMatchmakingCompleteDelegate::CreateUObject(this, &ThisClass::OnCancelMatchmakingComplete));

	SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete));
// 	SessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(FOnCancelFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnCancelFindSessionsComplete));
// 	SessionInterface->AddOnPingSearchResultsCompleteDelegate_Handle(FOnPingSearchResultsCompleteDelegate::CreateUObject(this, &ThisClass::OnPingSearchResultsComplete));
	SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete));

//	TWO_PARAM(OnSessionParticipantJoined, FName, const FUniqueNetId&);
//	THREE_PARAM(OnSessionParticipantLeft, FName, const FUniqueNetId&, EOnSessionParticipantLeftReason);
//	ONE_PARAM(OnQosDataRequested, FName);
//	TWO_PARAM(OnSessionCustomDataChanged, FName, const FOnlineSessionSettings&);
//	TWO_PARAM(OnSessionSettingsUpdated, FName, const FOnlineSessionSettings&);
//	THREE_PARAM(OnSessionParticipantSettingsUpdated, FName, const FUniqueNetId&, const FOnlineSessionSettings&);
//	FOUR_PARAM(OnSessionInviteReceived, const FUniqueNetId& /*UserId*/, const FUniqueNetId& /*FromId*/, const FString& /*AppId*/, const FOnlineSessionSearchResult& /*InviteResult*/);
//	THREE_PARAM(OnRegisterPlayersComplete, FName, const TArray< FUniqueNetIdRef >&, bool);
//	THREE_PARAM(OnUnregisterPlayersComplete, FName, const TArray< FUniqueNetIdRef >&, bool);

	SessionInterface->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &ThisClass::HandleSessionUserInviteAccepted));
	SessionInterface->AddOnSessionFailureDelegate_Handle(FOnSessionFailureDelegate::CreateUObject(this, &ThisClass::HandleSessionFailure));
}

#else

// OSSv2 下绑定在线 UI 发起的 Lobby 和 Session 加入请求；其他操作使用各异步调用的完成回调。
void UCommonSessionSubsystem::BindOnlineDelegatesOSSv2()
{
	// TODO：在 OSSv2 提供更多全局事件后补充绑定；多数 OSSv1 委托在 OSSv2 中已改为单次操作完成回调。
	// TODO: Bind OSSv2 delegates when they are available
	// Note that most OSSv1 delegates above are implemented as completion delegates in OSSv2 and don't need to be subscribed to
	TSharedPtr<IOnlineServices> OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	if (ensure(Lobbies))
	{
		LobbyJoinRequestedHandle = Lobbies->OnUILobbyJoinRequested().Add(this, &UCommonSessionSubsystem::OnLobbyJoinRequested);
	}

	ISessionsPtr Sessions = OnlineServices->GetSessionsInterface();
	if (ensure(Sessions))
	{
		SessionJoinRequestedHandle = Sessions->OnUISessionJoinRequested().Add(this, &UCommonSessionSubsystem::OnSessionJoinRequested);
	}
}
#endif

// 解除 Travel 和地图加载回调，并清理 OSSv1 会话失败委托后反初始化子系统。
void UCommonSessionSubsystem::Deinitialize()
{
#if COMMONUSER_OSSV1
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());

	if (OnlineSub)
	{
		// 引擎关闭期间 SessionInterface 可能已经失效，因此必须先判空。
		// During shutdown this may not be valid
		const IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface)
		{
			SessionInterface->ClearOnSessionFailureDelegates(this);
		}
	}
#endif // COMMONUSER_OSSV1

	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Deinitialize();
}

// 仅在不存在游戏专用派生类时创建基础 CommonSessionSubsystem，避免同一 GameInstance 重复实例。
bool UCommonSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// 存在游戏专用派生子系统时，由派生类替代基础实例。
	// Only create an instance if there is not a game-specific subclass
	return ChildClasses.Num() == 0;
}

// 创建默认在线托管请求，应用 Lobby、语音和主会话 Presence 默认配置。
UCommonSession_HostSessionRequest* UCommonSessionSubsystem::CreateOnlineHostSessionRequest()
{
	/** 游戏专用子系统可覆写工厂函数，调用方也可在创建后继续修改请求。 */
	/** Game-specific subsystems can override this or you can modify after creation */

	UCommonSession_HostSessionRequest* NewRequest = NewObject<UCommonSession_HostSessionRequest>(this);
	NewRequest->OnlineMode = ECommonSessionOnlineMode::Online;
	NewRequest->bUseLobbies = bUseLobbiesDefault;
	NewRequest->bUseLobbiesVoiceChat = bUseLobbiesVoiceChatDefault;

	// 匹配使用的主会话默认启用 Presence；有 Presence 语义的后端通常只允许主会话启用它。
	// We enable presence by default in the primary session used for matchmaking. For online systems that care about presence, only the primary session should have presence enabled
	NewRequest->bUsePresence = !IsRunningDedicatedServer();

	return NewRequest;
}

// 创建默认在线会话搜索请求，并应用项目的 Lobby 搜索默认开关。
UCommonSession_SearchSessionRequest* UCommonSessionSubsystem::CreateOnlineSearchSessionRequest()
{
	/** 游戏专用子系统可覆写工厂函数，调用方也可在创建后继续修改请求。 */
	/** Game-specific subsystems can override this or you can modify after creation */

	UCommonSession_SearchSessionRequest* NewRequest = NewObject<UCommonSession_SearchSessionRequest>(this);
	NewRequest->OnlineMode = ECommonSessionOnlineMode::Online;

	NewRequest->bUseLobbies = bUseLobbiesDefault;

	return NewRequest;
}

// 校验请求和托管玩家；离线模式直接 ServerTravel，在线模式异步创建会话，并发布预期的 InGame 展示状态。
void UCommonSessionSubsystem::HostSession(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* Request)
{
	if (Request == nullptr)
	{
		SetCreateSessionError(NSLOCTEXT("NetworkErrors", "InvalidRequest", "HostSession passed an invalid request."));
		OnCreateSessionComplete(NAME_None, false);
		return;
	}

	ULocalPlayer* LocalPlayer = (HostingPlayer != nullptr) ? HostingPlayer->GetLocalPlayer() : nullptr;
	if (LocalPlayer == nullptr && !bIsDedicatedServer)
	{
		SetCreateSessionError(NSLOCTEXT("NetworkErrors", "InvalidHostingPlayer", "HostingPlayer is invalid."));
		OnCreateSessionComplete(NAME_None, false);
		return;
	}

	FText OutError;
	if (!Request->ValidateAndLogErrors(OutError))
	{
		SetCreateSessionError(OutError);
		OnCreateSessionComplete(NAME_None, false);
		return;
	}

	if (Request->OnlineMode == ECommonSessionOnlineMode::Offline)
	{
		if (GetWorld()->GetNetMode() == NM_Client)
		{
			SetCreateSessionError(NSLOCTEXT("NetworkErrors", "CannotHostAsClient", "Cannot host offline game as client."));
			OnCreateSessionComplete(NAME_None, false);
			return;
		}
		else
		{
			// 离线托管无需创建在线会话，立即切换到请求指定的比赛 URL。
			// Offline so travel to the specified match URL immediately
			GetWorld()->ServerTravel(Request->ConstructTravelURL());
		}
	}
	else
	{
		CreateOnlineSessionInternal(LocalPlayer, Request);
	}

	NotifySessionInformationUpdated(ECommonSessionInformationState::InGame, Request->ModeNameForAdvertisement, Request->GetMapName());
}

// 重置创建结果、缓存成功后的 Travel URL，并分派到 OSSv1 或 OSSv2 创建实现。
void UCommonSessionSubsystem::CreateOnlineSessionInternal(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request)
{
	CreateSessionResult = FOnlineResultInformation();
	PendingTravelURL = Request->ConstructTravelURL();

#if COMMONUSER_OSSV1
	CreateOnlineSessionInternalOSSv1(LocalPlayer, Request);
#else
	CreateOnlineSessionInternalOSSv2(LocalPlayer, Request);
#endif
}

#if COMMONUSER_OSSV1
// OSSv1 下解析本地或专服 NetId，构建 GameSession 广告设置并发起异步 CreateSession。
void UCommonSessionSubsystem::CreateOnlineSessionInternalOSSv1(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request)
{
	const FName SessionName(NAME_GameSession);
	const int32 MaxPlayers = Request->GetMaxPlayers();

	IOnlineSubsystem* const OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);

	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);

	FUniqueNetIdPtr UserId;
	if (LocalPlayer)
	{
		UserId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	}
	else if (bIsDedicatedServer)
	{
		UserId = OnlineSub->GetIdentityInterface()->GetUniquePlayerId(DEDICATED_SERVER_USER_INDEX);
	}

	//@TODO：部分平台创建 LAN 会话也会走到此处，需要确认是否必须具备有效 UserId。
	//@TODO: You can get here on some platforms while trying to do a LAN session, does that require a valid user id?
	if (ensure(UserId.IsValid()))
	{
		FCommonSession_OnlineSessionSettings HostSettings(Request->OnlineMode == ECommonSessionOnlineMode::LAN, Request->bUsePresence, MaxPlayers);
		HostSettings.bUseLobbiesIfAvailable = Request->bUseLobbies;
		HostSettings.bUseLobbiesVoiceChatIfAvailable = Request->bUseLobbiesVoiceChat;
		HostSettings.Set(SETTING_GAMEMODE, Request->ModeNameForAdvertisement, EOnlineDataAdvertisementType::ViaOnlineService);
		HostSettings.Set(SETTING_MAPNAME, Request->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineService);
		//@TODO：按游戏匹配需求配置 SETTING_MATCHING_HOPPER；当前未写入固定 TeamDeathmatch 值。
		//@TODO: HostSettings.Set(SETTING_MATCHING_HOPPER, FString("TeamDeathmatch"), EOnlineDataAdvertisementType::DontAdvertise);
		HostSettings.Set(SETTING_MATCHING_TIMEOUT, 120.0f, EOnlineDataAdvertisementType::ViaOnlineService);
		HostSettings.Set(SETTING_SESSION_TEMPLATE_NAME, FString(TEXT("GameSession")), EOnlineDataAdvertisementType::ViaOnlineService);
		HostSettings.Set(SETTING_ONLINESUBSYSTEM_VERSION, true, EOnlineDataAdvertisementType::ViaOnlineService);

		Sessions->CreateSession(*UserId, SessionName, HostSettings);
		NotifySessionInformationUpdated(ECommonSessionInformationState::InGame, Request->ModeNameForAdvertisement, Request->GetMapName());
	}
	else
	{
		OnCreateSessionComplete(SessionName, false);
	}
}

#else

// OSSv2 下根据 bUseLobbies 创建 Session 或 Lobby，写入模式、地图和版本属性，并在完成回调中结束创建流程。
void UCommonSessionSubsystem::CreateOnlineSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request)
{
	const FName SessionName(NAME_GameSession);
	const int32 MaxPlayers = Request->GetMaxPlayers();
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());

	check(OnlineServices);

	FString ModeName = Request->ModeNameForAdvertisement;
	FString MapName = Request->GetMapName();

	if(!Request->bUseLobbies)
	{
		ISessionsPtr Sessions = OnlineServices->GetSessionsInterface();
		check(Sessions);
		FCreateSession::Params CreateParams;

		if (LocalPlayer)
		{
			CreateParams.LocalAccountId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
		}
		else if (bIsDedicatedServer)
		{
			// TODO：确定 OSSv2 专用服务器创建 Session 时应使用的 LocalAccountId 或无用户调用方式。
			// TODO what should this do for v2?
		}

		CreateParams.SessionName = SessionName;
		CreateParams.bPresenceEnabled = Request->bUsePresence;
		// TODO：将固定的 GameLobby SchemaName 提升为请求或配置参数。
		CreateParams.SessionSettings.SchemaName = FSchemaId(TEXT("GameLobby")); // TODO: make a parameter
		CreateParams.SessionSettings.NumMaxConnections = MaxPlayers;
		// TODO：根据托管请求验证并选择 Session JoinPolicy，而不是始终公开加入。
		CreateParams.SessionSettings.JoinPolicy = ESessionJoinPolicy::Public; // TODO: Check parameters

		CreateParams.SessionSettings.CustomSettings.Emplace(SETTING_GAMEMODE, Request->ModeNameForAdvertisement);
		CreateParams.SessionSettings.CustomSettings.Emplace(SETTING_MAPNAME, Request->GetMapName());
		//@TODO：按游戏匹配需求向 Session CustomSettings 添加 Matching Hopper。
		//@TODO: CreateParams.CustomSettings.Emplace(SETTING_MATCHING_HOPPER, FString("TeamDeathmatch"));
		CreateParams.SessionSettings.CustomSettings.Emplace(SETTING_MATCHING_TIMEOUT, 120.0f);
		CreateParams.SessionSettings.CustomSettings.Emplace(SETTING_SESSION_TEMPLATE_NAME, FString(TEXT("GameSession")));
		CreateParams.SessionSettings.CustomSettings.Emplace(SETTING_ONLINESUBSYSTEM_VERSION, true);

		Sessions->CreateSession(MoveTemp(CreateParams)).OnComplete(this, [this, SessionName, ModeName, MapName](const TOnlineResult<FCreateSession>& CreateResult)
			{
				OnCreateSessionComplete(SessionName, CreateResult.IsOk());
				NotifySessionInformationUpdated(ECommonSessionInformationState::InGame, ModeName, MapName);
			});
	}
	else
	{
		ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
		check(Lobbies);
		FCreateLobby::Params CreateParams;

		if (LocalPlayer)
		{
			CreateParams.LocalAccountId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
		}
		else if (bIsDedicatedServer)
		{
			// TODO：确定 OSSv2 专用服务器创建 Lobby 时应使用的 LocalAccountId 或无用户调用方式。
			// TODO what should this do for v2?
		}

		CreateParams.LocalName = SessionName;
		// TODO：将固定的 GameLobby SchemaId 提升为请求或配置参数。
		CreateParams.SchemaId = FSchemaId(TEXT("GameLobby")); // TODO: make a parameter
		CreateParams.bPresenceEnabled = Request->bUsePresence;
		CreateParams.MaxMembers = MaxPlayers;
		// TODO：根据托管请求验证并选择 Lobby JoinPolicy，而不是始终公开广告。
		CreateParams.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised; // TODO: Check parameters

		CreateParams.Attributes.Emplace(SETTING_GAMEMODE, Request->ModeNameForAdvertisement);
		CreateParams.Attributes.Emplace(SETTING_MAPNAME, Request->GetMapName());
		//@TODO：按游戏匹配需求向 Lobby Attributes 添加 Matching Hopper。
		//@TODO: CreateParams.Attributes.Emplace(SETTING_MATCHING_HOPPER, FString("TeamDeathmatch"));
		CreateParams.Attributes.Emplace(SETTING_MATCHING_TIMEOUT, 120.0f);
		CreateParams.Attributes.Emplace(SETTING_SESSION_TEMPLATE_NAME, FString(TEXT("GameSession")));
		CreateParams.Attributes.Emplace(SETTING_ONLINESUBSYSTEM_VERSION, true);

		CreateParams.UserAttributes.Emplace(SETTING_GAMEMODE, FString(TEXT("GameSession")));

		// TODO：创建 Lobby 时一并加入其他分屏 LocalPlayer。
		// TODO: Add splitscreen players

		Lobbies->CreateLobby(MoveTemp(CreateParams)).OnComplete(this, [this, SessionName, ModeName, MapName](const TOnlineResult<FCreateLobby>& CreateResult)
			{
				OnCreateSessionComplete(SessionName, CreateResult.IsOk());
				NotifySessionInformationUpdated(ECommonSessionInformationState::InGame, ModeName, MapName);
			});
	}
}

#endif

// 底层会话创建完成后处理可选分屏玩家注册；当前路径直接进入统一创建收尾。
void UCommonSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnCreateSessionComplete(SessionName: %s, bWasSuccessful: %d)"), *SessionName.ToString(), bWasSuccessful);

	// OSSv2 会在创建调用中一并加入分屏玩家，只有 OSSv1 需要额外注册路径。
#if COMMONUSER_OSSV1 // OSSv2 joins splitscreen players as part of the create call
	// 如果存在额外的分屏本地玩家，则把它注册到刚创建的会话。
	// Add the splitscreen player if one exists
#if 0 //@TODO:
	if (bWasSuccessful && LocalPlayers.Num() > 1)
	{
		IOnlineSessionPtr Sessions = Online::GetSessionInterface(GetWorld());
		if (Sessions.IsValid() && LocalPlayers[1]->GetPreferredUniqueNetId().IsValid())
		{
			Sessions->RegisterLocalPlayer(*LocalPlayers[1]->GetPreferredUniqueNetId(), NAME_GameSession,
				FOnRegisterLocalPlayerCompleteDelegate::CreateUObject(this, &ThisClass::OnRegisterLocalPlayerComplete_CreateSession));
		}
	}
	else
#endif
#endif
	{
		// 创建失败或只有一个本地用户时，无需等待额外分屏玩家注册。
		// We either failed or there is only a single local user
		FinishSessionCreation(bWasSuccessful);
	}
}

#if COMMONUSER_OSSV1
// OSSv1 托管端分屏玩家注册完成后，以注册结果继续统一会话创建收尾。
void UCommonSessionSubsystem::OnRegisterLocalPlayerComplete_CreateSession(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FinishSessionCreation(Result == EOnJoinSessionCompleteResult::Success);
}

// OSSv1 Session 启动完成时记录结果，并执行创建期间排队的会话销毁请求。
void UCommonSessionSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnStartSessionComplete(SessionName: %s, bWasSuccessful: %d)"), *SessionName.ToString(), bWasSuccessful);

	if (bWantToDestroyPendingSession)
	{
		CleanUpSessions();
	}
}
#endif // COMMONUSER_OSSV1

// 成功时创建可选 Host Beacon、广播结果并 ServerTravel；失败时补齐错误并恢复 OutOfGame 状态。
void UCommonSessionSubsystem::FinishSessionCreation(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		//@TODO：统一创建与加入完成委托的广播时机；调整方案时同步修改两条路径及其注释。
		//@TODO Synchronize timing of this with join callbacks, modify both places and the comments if plan changes
		CreateSessionResult = FOnlineResultInformation();
		CreateSessionResult.bWasSuccessful = true;

		if (bUseBeacons)
		{
			CreateHostReservationBeacon();
		}

		NotifyCreateSessionComplete(CreateSessionResult);

		// 会话与可选 Beacon 准备完成后切换到请求指定的比赛地图。
		// Travel to the specified match URL
		GetWorld()->ServerTravel(PendingTravelURL);
	}
	else
	{
		if (CreateSessionResult.bWasSuccessful || CreateSessionResult.ErrorText.IsEmpty())
		{
			// TODO：OSSv1 缺少可靠的会话错误码提取方式，因此只能回退为 GenericFailure。
			FString ReturnError = TEXT("GenericFailure"); // TODO: No good way to get session error codes out of OSSV1
			FText ReturnReason = NSLOCTEXT("NetworkErrors", "CreateSessionFailed", "Failed to create session.");

			CreateSessionResult.bWasSuccessful = false;
			CreateSessionResult.ErrorId = ReturnError;
			CreateSessionResult.ErrorText = ReturnReason;
		}

		UE_LOG(LogCommonSession, Error, TEXT("FinishSessionCreation(%s): %s"), *CreateSessionResult.ErrorId, *CreateSessionResult.ErrorText.ToString());

		NotifyCreateSessionComplete(CreateSessionResult);
		NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);
	}
}

#if COMMONUSER_OSSV1
// OSSv1 Session 更新完成回调当前只记录诊断日志。
void UCommonSessionSubsystem::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnUpdateSessionComplete(SessionName: %s, bWasSuccessful: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
}

// OSSv1 Session 结束后记录结果并继续执行会话销毁清理。
void UCommonSessionSubsystem::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnEndSessionComplete(SessionName: %s, bWasSuccessful: %s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	CleanUpSessions();
}

// OSSv1 Session 销毁完成后记录结果并清除待销毁标志。
void UCommonSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnDestroySessionComplete(SessionName: %s, bWasSuccessful: %s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	bWantToDestroyPendingSession = false;
}

// 将 OSSv1 的本地用户索引转换为 PlatformUserId，并广播平台发起的离开会话请求。
void UCommonSessionSubsystem::OnDestroySessionRequested(int32 LocalUserNum, FName SessionName)
{
	FPlatformUserId PlatformUserId = IPlatformInputDeviceMapper::Get().GetPlatformUserForUserIndex(LocalUserNum);

	NotifyDestroySessionRequested(PlatformUserId, SessionName);
}
#endif // COMMONUSER_OSSV1

// 校验搜索请求并按 OSS 版本创建受 GC 保护的搜索设置，再进入统一搜索入口。
void UCommonSessionSubsystem::FindSessions(APlayerController* SearchingPlayer, UCommonSession_SearchSessionRequest* Request)
{
	if (Request == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("FindSessions passed a null request"));
		return;
	}

#if COMMONUSER_OSSV1
	FindSessionsInternal(SearchingPlayer, MakeShared<FCommonOnlineSearchSettingsOSSv1>(Request));
#else
	FindSessionsInternal(SearchingPlayer, MakeShared<FCommonOnlineSearchSettingsOSSv2>(Request));
#endif // COMMONUSER_OSSV1
}

// 拒绝并发搜索，验证搜索玩家 LocalPlayer，保存活动设置后分派到对应 OSS 实现。
void UCommonSessionSubsystem::FindSessionsInternal(APlayerController* SearchingPlayer, const TSharedRef<FCommonOnlineSearchSettings>& InSearchSettings)
{
	if (SearchSettings.IsValid())
	{
		//@TODO：当前直接拒绝并发搜索的 API 体验较差；后续请求可复用正在进行的搜索结果，或排队等待前一个搜索成功或失败后再执行。
		//@TODO: This is a poor user experience for the API user, we should let the additional search piggyback and
		// just give it the same results as the currently pending one
		// (or enqueue the request and service it when the previous one finishes or fails)
		UE_LOG(LogCommonSession, Error, TEXT("A previous FindSessions call is still in progress, aborting"));
		SearchSettings->SearchRequest->NotifySearchFinished(false, LOCTEXT("Error_FindSessionAlreadyInProgress", "Session search already in progress"));
	}

	ULocalPlayer* LocalPlayer = (SearchingPlayer != nullptr) ? SearchingPlayer->GetLocalPlayer() : nullptr;
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("SearchingPlayer is invalid"));
		InSearchSettings->SearchRequest->NotifySearchFinished(false, LOCTEXT("Error_FindSessionBadPlayer", "Session search was not provided a local player"));
		return;
	}

	SearchSettings = InSearchSettings;
#if COMMONUSER_OSSV1
	FindSessionsInternalOSSv1(LocalPlayer);
#else
	FindSessionsInternalOSSv2(LocalPlayer);
#endif
}

#if COMMONUSER_OSSV1
// OSSv1 下添加 GameSession 模板过滤器并发起 FindSessions；未安排回调的同步失败手动进入完成处理。
void UCommonSessionSubsystem::FindSessionsInternalOSSv1(ULocalPlayer* LocalPlayer)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);
	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);

	SearchSettings->QuerySettings.Set(SETTING_SESSION_TEMPLATE_NAME, FString("GameSession"), EOnlineComparisonOp::Equals);

	if (!Sessions->FindSessions(*LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId(), StaticCastSharedRef<FCommonOnlineSearchSettingsOSSv1>(SearchSettings.ToSharedRef())))
	{
		// 部分搜索失败会在 FindSessions 内同步触发委托，其他失败不会，因此这里补充完成调用。
		// Some session search failures will call this delegate inside the function, others will not
		OnFindSessionsComplete(false);
	}
}

#else

// OSSv2 下按本地账户异步搜索 Lobby，过滤无所有者或无成员结果，填充请求对象并广播完成。
void UCommonSessionSubsystem::FindSessionsInternalOSSv2(ULocalPlayer* LocalPlayer)
{
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	check(Lobbies);

	FFindLobbies::Params FindLobbyParams = StaticCastSharedPtr<FCommonOnlineSearchSettingsOSSv2>(SearchSettings)->FindLobbyParams;
	FindLobbyParams.LocalAccountId = LocalPlayer->GetPreferredUniqueNetId().GetV2();

	Lobbies->FindLobbies(MoveTemp(FindLobbyParams)).OnComplete(this, [this, LocalSearchSettings = SearchSettings](const TOnlineResult<FFindLobbies>& FindResult)
	{
		if (LocalSearchSettings != SearchSettings)
		{
			// 活动搜索设置已被替换，说明该异步结果已废弃，直接忽略。
			// This was an abandoned search, ignore
			return;
		}
		const bool bWasSuccessful = FindResult.IsOk();
		UE_LOG(LogCommonSession, Log, TEXT("FindLobbies(bWasSuccessful: %s)"), *LexToString(bWasSuccessful));
		check(SearchSettings.IsValid());
		if (bWasSuccessful)
		{
			const FFindLobbies::Result& FindResults = FindResult.GetOkValue();
			SearchSettings->SearchRequest->Results.Reset(FindResults.Lobbies.Num());

			for (const TSharedRef<const FLobby>& Lobby : FindResults.Lobbies)
			{
				if (!Lobby->OwnerAccountId.IsValid())
				{
					UE_LOG(LogCommonSession, Verbose, TEXT("\tIgnoring Lobby with no owner (LobbyId: %s)"),
						*ToLogString(Lobby->LobbyId));
				}
				else if (Lobby->Members.Num() == 0)
				{
					UE_LOG(LogCommonSession, Verbose, TEXT("\tIgnoring Lobby with no members (UserId: %s)"),
						*ToLogString(Lobby->OwnerAccountId));
				}
				else
				{
					UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettings->SearchRequest);
					Entry->Lobby = Lobby;
					SearchSettings->SearchRequest->Results.Add(Entry);

					UE_LOG(LogCommonSession, Log, TEXT("\tFound lobby (UserId: %s, NumOpenConns: %d)"),
						*ToLogString(Lobby->OwnerAccountId), Lobby->MaxMembers - Lobby->Members.Num());
				}
			}
		}
		else
		{
			SearchSettings->SearchRequest->Results.Empty();
		}

		const FText ResultText = bWasSuccessful ? FText() : FindResult.GetErrorValue().GetText();

		SearchSettings->SearchRequest->NotifySearchFinished(bWasSuccessful, ResultText);
		SearchSettings.Reset();
	});
}
#endif // COMMONUSER_OSSV1

// 创建与托管参数匹配的快速搜索；完成后优先加入首个结果，否则用强引用保留的请求转为托管。
void UCommonSessionSubsystem::QuickPlaySession(APlayerController* JoiningOrHostingPlayer, UCommonSession_HostSessionRequest* HostRequest)
{
	UE_LOG(LogCommonSession, Log, TEXT("QuickPlay Requested"));

	if (HostRequest == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("QuickPlaySession passed a null request"));
		return;
	}

	TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequestPtr = TStrongObjectPtr<UCommonSession_HostSessionRequest>(HostRequest);
	TWeakObjectPtr<APlayerController> JoiningOrHostingPlayerPtr = TWeakObjectPtr<APlayerController>(JoiningOrHostingPlayer);

	UCommonSession_SearchSessionRequest* QuickPlayRequest = CreateOnlineSearchSessionRequest();
	QuickPlayRequest->OnSearchFinished.AddUObject(this, &UCommonSessionSubsystem::HandleQuickPlaySearchFinished, JoiningOrHostingPlayerPtr, HostRequestPtr);

	// 匹配使用的主会话默认启用 Presence；有 Presence 语义的后端通常只允许主会话启用它。
	// We enable presence by default on the primary session used for matchmaking. For online systems that care about presence, only the primary session should have presence enabled

	HostRequestPtr->bUseLobbies = bUseLobbiesDefault;
	HostRequestPtr->bUseLobbiesVoiceChat = bUseLobbiesVoiceChatDefault;
	HostRequestPtr->bUsePresence = true;
	QuickPlayRequest->bUseLobbies = bUseLobbiesDefault;

	NotifySessionInformationUpdated(ECommonSessionInformationState::Matchmaking);
	FindSessionsInternal(JoiningOrHostingPlayer, CreateQuickPlaySearchSettings(HostRequest, QuickPlayRequest));
}

// 按编译时 OSS 版本构造快速游戏搜索设置，供派生类覆写具体匹配过滤条件。
TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateQuickPlaySearchSettings(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest)
{
#if COMMONUSER_OSSV1
	return CreateQuickPlaySearchSettingsOSSv1(HostRequest, SearchRequest);
#else
	return CreateQuickPlaySearchSettingsOSSv2(HostRequest, SearchRequest);
#endif // COMMONUSER_OSSV1
}

#if COMMONUSER_OSSV1
// OSSv1 快速游戏默认只使用版本和 Lobby 基础过滤，不限制地图或模式，也不强制专用服务器。
TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateQuickPlaySearchSettingsOSSv1(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest)
{
	TSharedRef<FCommonOnlineSearchSettingsOSSv1> QuickPlaySearch = MakeShared<FCommonOnlineSearchSettingsOSSv1>(SearchRequest);

	/** 默认快速游戏不按地图或模式过滤；具体游戏可按需启用下面的条件。 */
	/** By default quick play does not want to include the map or game mode, games can fill this in as desired
	if (!HostRequest->ModeNameForAdvertisement.IsEmpty())
	{
		QuickPlaySearch->QuerySettings.Set(SETTING_GAMEMODE, HostRequest->ModeNameForAdvertisement, EOnlineComparisonOp::Equals);
	}

	if (!HostRequest->GetMapName().IsEmpty())
	{
		QuickPlaySearch->QuerySettings.Set(SETTING_MAPNAME, HostRequest->GetMapName(), EOnlineComparisonOp::Equals);
	} 
	*/

	// 可按项目需求将快速游戏限制为专用服务器搜索；当前未启用。
	// QuickPlaySearch->QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
	return QuickPlaySearch;
}

#else

// OSSv2 快速游戏默认只使用基础 Lobby 版本过滤，不限制地图或游戏模式。
TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateQuickPlaySearchSettingsOSSv2(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest)
{
	TSharedRef<FCommonOnlineSearchSettingsOSSv2> QuickPlaySearch = MakeShared<FCommonOnlineSearchSettingsOSSv2>(SearchRequest);

	/** 默认快速游戏不按地图或模式过滤；具体游戏可按需启用下面的 Lobby Filter。 */
	/** By default quick play does not want to include the map or game mode, games can fill this in as desired
	if (!HostRequest->ModeNameForAdvertisement.IsEmpty())
	{
		QuickPlaySearch->FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{SETTING_GAMEMODE, ESchemaAttributeComparisonOp::Equals, HostRequest->ModeNameForAdvertisement});
	}
	if (!HostRequest->GetMapName().IsEmpty())
	{
		QuickPlaySearch->FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{SETTING_MAPNAME, ESchemaAttributeComparisonOp::Equals, HostRequest->GetMapName()});
	}
	*/

	return QuickPlaySearch;
}

#endif // COMMONUSER_OSSV1

// 将“无结果但无错误”视为可托管状态；有结果时加入首项，无结果时托管，真实搜索错误则恢复 OutOfGame。
void UCommonSessionSubsystem::HandleQuickPlaySearchFinished(bool bSucceeded, const FText& ErrorMessage, TWeakObjectPtr<APlayerController> JoiningOrHostingPlayer, TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequest)
{
	const int32 ResultCount = SearchSettings->SearchRequest->Results.Num();
	UE_LOG(LogCommonSession, Log, TEXT("QuickPlay Search Finished %s (Results %d) (Error: %s)"), bSucceeded ? TEXT("Success") : TEXT("Failed"), ResultCount, *ErrorMessage.ToString());

	//@TODO：部分 OSS 层会把“没有会话”报告为失败，因此当前还要检查错误文本是否为空；应在 OSS2 中统一语义。
	//@TODO: We have to check if the error message is empty because some OSS layers report a failure just because there are no sessions.  Please fix with OSS 2.0.
	if (bSucceeded || ErrorMessage.IsEmpty())
	{
		// 有搜索结果时加入当前认为最优的结果。
		// Join the best search result.
		if (ResultCount > 0)
		{
			//@TODO：应按 Ping 或其他因素选择最佳结果；当前也未确认后端结果是否已排序。
			//@TODO: We should probably look at ping?  maybe some other factors to find the best.  Idk if they come pre-sorted or not.
			for (UCommonSession_SearchResult* Result : SearchSettings->SearchRequest->Results)
			{
				JoinSession(JoiningOrHostingPlayer.Get(), Result);
				return;
			}
		}
		else
		{
			HostSession(JoiningOrHostingPlayer.Get(), HostRequest.Get());
		}
	}
	else
	{
		//@TODO：快速游戏搜索错误目前没有向调用方提供专用失败委托，需要补充可观察错误结果。
		//@TODO: This sucks, need to tell someone.
		NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);
	}
}

// 标记待销毁、关闭 Reservation Beacon、发布 OutOfGame，并按 OSS 版本离开或销毁活动会话。
void UCommonSessionSubsystem::CleanUpSessions()
{
	bWantToDestroyPendingSession = true;

	if (bUseBeacons)
	{
		DestroyHostReservationBeacon();
	}

	NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);
#if COMMONUSER_OSSV1
	CleanUpSessionsOSSv1();
#else
	CleanUpSessionsOSSv2();
#endif // COMMONUSER_OSSV1
}

#if COMMONUSER_OSSV1
// OSSv1 下按 GameSession 当前状态选择 EndSession、等待状态转换或直接 DestroySession。
void UCommonSessionSubsystem::CleanUpSessionsOSSv1()
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);
	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);

	EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
	UE_LOG(LogCommonSession, Log, TEXT("Session state is %s"), EOnlineSessionState::ToString(SessionState));

	if (EOnlineSessionState::InProgress == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Ending session because of return to front end"));
		Sessions->EndSession(NAME_GameSession);
	}
	else if (EOnlineSessionState::Ending == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Waiting for session to end on return to main menu"));
	}
	else if (EOnlineSessionState::Ended == SessionState || EOnlineSessionState::Pending == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Destroying session on return to main menu"));
		Sessions->DestroySession(NAME_GameSession);
	}
	else if (EOnlineSessionState::Starting == SessionState || EOnlineSessionState::Creating == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Waiting for session to start, and then we will end it to return to main menu"));
	}
}

#else
// OSSv2 下使用首个本地账户离开当前 Lobby 和 GameSession；账户或 Lobby 无效时提前返回。
void UCommonSessionSubsystem::CleanUpSessionsOSSv2()
{
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();	
	ISessionsPtr Sessions = OnlineServices->GetSessionsInterface();
	
	FAccountId LocalPlayerId = GetAccountId(GetGameInstance()->GetFirstLocalPlayerController());
	
	if (!LocalPlayerId.IsValid())
	{
		return;
	}

	if (bUseLobbiesDefault)
	{
		FLobbyId LobbyId = GetLobbyId(NAME_GameSession);

		if (!LobbyId.IsValid())
		{
			return;
		}
		// TODO：让全部本地分屏玩家都离开 Lobby 和 Session，而不只处理主本地账户。
		// TODO:  Include all local players leave the lobby/session
		if (ensure(Lobbies))
		{
			Lobbies->LeaveLobby({ LocalPlayerId, LobbyId });
		}
	}
	if (ensure(Sessions))
	{
		Sessions->LeaveSession({ LocalPlayerId, NAME_GameSession, false });
	}
}

#endif // COMMONUSER_OSSV1

#if COMMONUSER_OSSV1
// OSSv1 搜索完成时过滤重复或外部回调，复制有效结果到请求 UObject，广播结果并释放活动搜索设置。
void UCommonSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnFindSessionsComplete(bWasSuccessful: %s)"), bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (!SearchSettings.IsValid())
	{
		// 失败搜索可能回调两次，其他系统发起的搜索也会触发全局委托；没有活动设置时忽略。
		// This could get called twice for failed session searches, or for a search requested by a different system
		return;
	}

	FCommonOnlineSearchSettingsOSSv1& SearchSettingsV1 = *StaticCastSharedPtr<FCommonOnlineSearchSettingsOSSv1>(SearchSettings);
	if (SearchSettingsV1.SearchState == EOnlineAsyncTaskState::InProgress)
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnFindSessionsComplete called when search is still in progress!"));
		return;
	}

	if (!ensure(SearchSettingsV1.SearchRequest))
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnFindSessionsComplete called with invalid search request object!"));
		return;
	}

	if (bWasSuccessful)
	{
		SearchSettingsV1.SearchRequest->Results.Reset(SearchSettingsV1.SearchResults.Num());

		for (const FOnlineSessionSearchResult& Result : SearchSettingsV1.SearchResults)
		{
			check(Result.IsValid());

			UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettingsV1.SearchRequest);
			Entry->Result = Result;
			SearchSettingsV1.SearchRequest->Results.Add(Entry);

			FString SessionId = TEXT("Unknown");
			if (Result.Session.SessionInfo.IsValid())
			{
				SessionId = Result.Session.SessionInfo->GetSessionId().ToString();
			}

			FString OwningUserId = TEXT("Unknown");
			if (Result.Session.OwningUserId.IsValid())
			{
				OwningUserId = Result.Session.OwningUserId->ToString();
			}

			UE_LOG(LogCommonSession, Log, TEXT("\tFound session (SessionId: %s, UserId: %s, UserName: %s, NumOpenPrivConns: %d, NumOpenPubConns: %d, Ping: %d ms"),
				*SessionId,
				*OwningUserId,
				*Result.Session.OwningUserName,
				Result.Session.NumOpenPrivateConnections,
				Result.Session.NumOpenPublicConnections,
				Result.PingInMs
				);
		}
	}
	else
	{
		SearchSettingsV1.SearchRequest->Results.Empty();
	}

	if (0)
	{
		// 用于手动测试 UI 的 OSSv1 伪造会话数据，当前由常量条件禁用。
		// Fake Sessions OSSV1
		for (int i = 0; i < 10; i++)
		{
			UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettings->SearchRequest);
			FOnlineSessionSearchResult FakeResult;
			FakeResult.Session.OwningUserName = TEXT("Fake User");
			FakeResult.Session.SessionSettings.NumPublicConnections = 10;
			FakeResult.Session.SessionSettings.bShouldAdvertise = true;
			FakeResult.Session.SessionSettings.bAllowJoinInProgress = true;
			FakeResult.PingInMs=99;
			Entry->Result = FakeResult;
			SearchSettingsV1.SearchRequest->Results.Add(Entry);
		}
	}
	
	SearchSettingsV1.SearchRequest->NotifySearchFinished(bWasSuccessful, bWasSuccessful ? FText() : LOCTEXT("Error_FindSessionV1Failed", "Find session failed"));
	SearchSettings.Reset();
}
#endif // COMMONUSER_OSSV1

// 校验加入玩家和结果，先从搜索属性更新可展示 Presence，再分派到对应 OSS 加入实现。
void UCommonSessionSubsystem::JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Request)
{

	if (Request == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("JoinSession passed a null request"));
		return;
	}

	ULocalPlayer* LocalPlayer = (JoiningPlayer != nullptr) ? JoiningPlayer->GetLocalPlayer() : nullptr;
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("JoiningPlayer is invalid"));
		return;
	}

	// ClientTravel 后不再保留原始模式和地图属性，因此先更新 Presence；加入或 Travel 失败时会恢复主菜单状态。
	// Update presence here since we won't have the raw game mode and map name keys after client travel. If joining/travel fails, it is reset to main menu 
	FString SessionGameMode, SessionMapName;
	bool bEmpty;
#if COMMONUSER_OSSV1
	Request->GetStringSetting(SETTING_GAMEMODE, SessionGameMode, bEmpty);
	Request->GetStringSetting(SETTING_MAPNAME, SessionMapName, bEmpty);
	NotifySessionInformationUpdated(ECommonSessionInformationState::InGame, SessionGameMode, SessionMapName);
#else
	if(Request->Lobby.IsValid())
	{
		Request->GetStringSetting(SETTING_GAMEMODE, SessionGameMode, bEmpty);
		Request->GetStringSetting(SETTING_MAPNAME, SessionMapName, bEmpty);
		NotifySessionInformationUpdated(ECommonSessionInformationState::InGame, SessionGameMode, SessionMapName);
	}
#endif // COMMONUSER_OSSV1

	JoinSessionInternal(LocalPlayer, Request);
}

// 按编译时 OSS 版本将已验证的 LocalPlayer 和搜索结果交给底层加入实现。
void UCommonSessionSubsystem::JoinSessionInternal(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request)
{
#if COMMONUSER_OSSV1
	JoinSessionInternalOSSv1(LocalPlayer, Request);
#else
	JoinSessionInternalOSSv2(LocalPlayer, Request);
#endif // COMMONUSER_OSSV1
}

#if COMMONUSER_OSSV1
// OSSv1 下将目标结果标记为主 Presence/Lobby 会话，并以本地用户 NetId 发起 JoinSession。
void UCommonSessionSubsystem::JoinSessionInternalOSSv1(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);
	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);
	
	// OSSv1 加入前必须显式把搜索结果标记为当前 Presence 会话。
	// We need to manually set that we want this to be our presence session
	Request->Result.Session.SessionSettings.bUsesPresence = true;
	Request->Result.Session.SessionSettings.bUseLobbiesIfAvailable = bUseLobbiesDefault;

	Sessions->JoinSession(*LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId(), NAME_GameSession, Request->Result);
}

// OSSv1 会话加入完成后处理可选分屏注册；当前直接进入统一加入收尾。
void UCommonSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// 如存在其他分屏玩家，应在 Travel 前把它们注册到已加入会话。
	// Add any splitscreen players if they exist
	//@TODO:
// 	if (Result == EOnJoinSessionCompleteResult::Success && LocalPlayers.Num() > 1)
// 	{
// 		IOnlineSessionPtr Sessions = Online::GetSessionInterface(GetWorld());
// 		if (Sessions.IsValid() && LocalPlayers[1]->GetPreferredUniqueNetId().IsValid())
// 		{
// 			Sessions->RegisterLocalPlayer(*LocalPlayers[1]->GetPreferredUniqueNetId(), NAME_GameSession,
// 				FOnRegisterLocalPlayerCompleteDelegate::CreateUObject(this, &UShooterGameInstance::OnRegisterJoiningLocalPlayerComplete));
// 		}
// 	}
// 	else
 	{
		FinishJoinSession(Result);
	}
}

// 额外分屏玩家注册完成后，以注册结果继续统一会话加入收尾。
void UCommonSessionSubsystem::OnRegisterJoiningLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FinishJoinSession(Result);
}

// 创建 PartyBeaconClient，解析 Beacon 地址并提交主用户预留；根据连接或预留结果通知、Travel 或清理会话。
void UCommonSessionSubsystem::ConnectToHostReservationBeacon()
{
	UWorld* const World = GetWorld();
	check(World);
	ReservationBeaconClient = World->SpawnActor<APartyBeaconClient>(APartyBeaconClient::StaticClass());
	check(ReservationBeaconClient.IsValid());

	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World);
	check(OnlineSub);
	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);
	FNamedOnlineSession* Session = Sessions->GetNamedSession(NAME_GameSession);
	check(Session);
	FString SessionIdStr = Session->GetSessionIdStr();

	FString ConnectInfo;
	Sessions->GetResolvedConnectString(NAME_GameSession, ConnectInfo, NAME_BeaconPort);

	IOnlineIdentityPtr Identity = OnlineSub->GetIdentityInterface();
	check(Identity);
	FUniqueNetIdWrapper DefaultNetId = Identity->GetUniquePlayerId(0);
	check(DefaultNetId.IsValid());

	FPlayerReservation PlayerReservation;
	PlayerReservation.UniqueId = *DefaultNetId;
	PlayerReservation.Platform = OnlineSub->GetLocalPlatformName();

	ReservationBeaconClient->OnHostConnectionFailure().BindWeakLambda(this, [this]()
		{
			// 只处理连接仍活动时的失败回调，正常关闭 Beacon 时不应再次报告加入失败。
			// We only want to react to failure calls while the connection is active, not when it closes
			if(ReservationBeaconClient->GetNetDriver())
			{
				FOnlineResultInformation JoinSessionResult;
				JoinSessionResult.bWasSuccessful = false;
				JoinSessionResult.ErrorId = TEXT("UnknownError");

				NotifyJoinSessionComplete(JoinSessionResult);
				NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);

				CleanUpSessions();
			}
		});

	ReservationBeaconClient->OnReservationRequestComplete().BindWeakLambda(this, [this](EPartyReservationResult::Type ReservationResponse)
		{
			if (ReservationResponse == EPartyReservationResult::ReservationAccepted || ReservationResponse == EPartyReservationResult::ReservationDuplicate)
			{
				FOnlineResultInformation JoinSessionResult;
				JoinSessionResult.bWasSuccessful = true;
				NotifyJoinSessionComplete(JoinSessionResult);

				InternalTravelToSession(NAME_GameSession);
			}
			else
			{
				FOnlineResultInformation JoinSessionResult;
				JoinSessionResult.bWasSuccessful = false;
				JoinSessionResult.ErrorId = TEXT("UnknownError");

				NotifyJoinSessionComplete(JoinSessionResult);
				NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);

				CleanUpSessions();
			}
		});

	ReservationBeaconClient->RequestReservation(ConnectInfo, SessionIdStr, *DefaultNetId, { PlayerReservation });
}

// OSSv1 加入成功后执行 Beacon 预留或直接通知并 Travel；失败时构造可读错误、恢复状态并清理会话。
void UCommonSessionSubsystem::FinishJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		if (bUseBeacons)
		{
			// 预留成功后由 Beacon 路径通知并调用 InternalTravelToSession；Travel 过程中 Beacon 会被销毁。
			// InternalTravelToSession and the notification will be called by the beacon after a successful reservation. The beacon will be destroyed during travel.
			ConnectToHostReservationBeacon();
		}
		else
		{
			//@TODO：统一加入与创建完成委托的广播时机；调整方案时同步修改两条路径及其注释。
			//@TODO Synchronize timing of this with create callbacks, modify both places and the comments if plan changes
			FOnlineResultInformation JoinSessionResult;
			JoinSessionResult.bWasSuccessful = true;
			NotifyJoinSessionComplete(JoinSessionResult);

			InternalTravelToSession(NAME_GameSession);
		}
	}
	else
	{
		FText ReturnReason;
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			ReturnReason = NSLOCTEXT("NetworkErrors", "SessionIsFull", "Game is full.");
			break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			ReturnReason = NSLOCTEXT("NetworkErrors", "SessionDoesNotExist", "Game no longer exists.");
			break;
		default:
			ReturnReason = NSLOCTEXT("NetworkErrors", "JoinSessionFailed", "Join failed.");
			break;
		}

		//@TODO：补充更细的加入错误映射和面向用户的恢复策略。
		//@TODO: Error handling
		UE_LOG(LogCommonSession, Error, TEXT("FinishJoinSession(Failed with Result: %s)"), *ReturnReason.ToString());

		// OSSv1 这里只提供枚举结果，没有可直接转换的 FOnlineError。
		// No FOnlineError to initialize from
		FOnlineResultInformation JoinSessionResult;
		JoinSessionResult.bWasSuccessful = false;
		// 当前只能用枚举名作为 ErrorId；由于没有扩展错误信息，该标识不够稳定。
		JoinSessionResult.ErrorId = LexToString(Result); // This is not robust but there is no extended information available
		JoinSessionResult.ErrorText = ReturnReason;
		NotifyJoinSessionComplete(JoinSessionResult);
		NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);

		// 加入失败后离开或销毁底层会话，避免残留半完成状态。
		// If the session join failed, we'll clean up the session
		CleanUpSessions();
	}
}

#else

// OSSv2 下按结果类型加入 Session 或 Lobby；成功后解析连接并 Travel，失败路径当前只记录在线错误。
void UCommonSessionSubsystem::JoinSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request)
{
	const FName SessionName(NAME_GameSession);
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);

	// 搜索结果没有 Lobby 数据时，将其视为直接 Session 结果。
	// If the request doesnt have a lobby assume it's a session
	if (!Request->Lobby.IsValid())
	{
		ISessionsPtr Sessions = OnlineServices->GetSessionsInterface();
		check(Sessions);
		FJoinSession::Params CreateParams;

		if (LocalPlayer)
		{
			CreateParams.LocalAccountId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
		}
		CreateParams.SessionName = SessionName;
		CreateParams.SessionId = Request->SessionID;
		UE::Online::FOnlineSessionId SessionID = Request->SessionID;

		Sessions->JoinSession(MoveTemp(CreateParams)).OnComplete(this, [this, SessionName](const TOnlineResult<FJoinSession>& JoinResult)
			{
				if (JoinResult.IsOk())
				{
					InternalTravelToSession(SessionName);
				}
				else
				{
					//@TODO：将 OSSv2 Session 加入错误转换为完成委托并恢复会话展示状态。
					//@TODO: Error handling
					UE_LOG(LogCommonSession, Error, TEXT("JoinLobby Failed with Result: %s"), *ToLogString(JoinResult.GetErrorValue()));
				}
			});
	}
	else
	{
		 	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
		 	check(Lobbies);
		 
		 	FJoinLobby::Params JoinParams;
			if (LocalPlayer)
			{
				JoinParams.LocalAccountId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
			}
		 	JoinParams.LocalName = SessionName;
		 	JoinParams.LobbyId = Request->Lobby->LobbyId;
		 	JoinParams.bPresenceEnabled = true;
		 
		 	// TODO：如存在分屏玩家，在 Travel 前加入同一 Lobby；参考 OSSv1 OnJoinSessionComplete 路径。
		 	// Add any splitscreen players if they exist //@TODO: See UCommonSessionSubsystem::OnJoinSessionComplete
		 
		 	Lobbies->JoinLobby(MoveTemp(JoinParams)).OnComplete(this, [this, SessionName](const TOnlineResult<FJoinLobby>& JoinResult)
		 	{
		 		if (JoinResult.IsOk())
		 		{
		 			InternalTravelToSession(SessionName);
		 		}
		 		else
		 		{
		 			//@TODO：将 OSSv2 Lobby 加入错误转换为完成委托并恢复会话展示状态。
		 			//@TODO: Error handling
		 			UE_LOG(LogCommonSession, Error, TEXT("JoinLobby Failed with Result: %s"), *ToLogString(JoinResult.GetErrorValue()));
		 		}
		 	});
	}
}

// 处理 OSSv2 在线 UI 的 Session 加入请求，将本地 AccountId 解析为 PlatformUserId 并广播统一请求结果。
void UCommonSessionSubsystem::OnSessionJoinRequested(const UE::Online::FUISessionJoinRequested& EventParams)
{
	TSharedPtr<IOnlineServices> OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	IAuthPtr Auth = OnlineServices->GetAuthInterface();
	check(Auth);
	TOnlineResult<FAuthGetLocalOnlineUserByOnlineAccountId> Account = Auth->GetLocalOnlineUserByOnlineAccountId({ EventParams.LocalAccountId });
	if (Account.IsOk())
	{
		FPlatformUserId PlatformUserId = Account.GetOkValue().AccountInfo->PlatformUserId;
		UCommonSession_SearchResult* RequestedSession = nullptr;
		FOnlineResultInformation ResultInfo;
		if (EventParams.Result.IsOk())
		{
			RequestedSession = NewObject<UCommonSession_SearchResult>(this);
			RequestedSession->SessionID = EventParams.Result.GetOkValue();
		}
		else
		{
			ResultInfo.FromOnlineError(EventParams.Result.GetErrorValue());
		}
		NotifyUserRequestedSession(PlatformUserId, RequestedSession, ResultInfo);
	}
	else
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnJoinSessionRequested: Failed to get account by local user id %s"), *UE::Online::ToLogString(EventParams.LocalAccountId));
	}
}

// 处理 OSSv2 在线 UI 的 Lobby 加入请求，将 Lobby 或错误包装为统一搜索结果后广播。
void UCommonSessionSubsystem::OnLobbyJoinRequested(const UE::Online::FUILobbyJoinRequested& EventParams)
{
	TSharedPtr<IOnlineServices> OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	IAuthPtr Auth = OnlineServices->GetAuthInterface();
	check(Auth);
	TOnlineResult<FAuthGetLocalOnlineUserByOnlineAccountId> Account = Auth->GetLocalOnlineUserByOnlineAccountId({ EventParams.LocalAccountId });
	if (Account.IsOk())
	{
		FPlatformUserId PlatformUserId = Account.GetOkValue().AccountInfo->PlatformUserId;
		UCommonSession_SearchResult* RequestedSession = nullptr;
		FOnlineResultInformation ResultInfo;
		if (EventParams.Result.IsOk())
		{
			RequestedSession = NewObject<UCommonSession_SearchResult>(this);
			RequestedSession->Lobby = EventParams.Result.GetOkValue();
		}
		else
		{
			ResultInfo.FromOnlineError(EventParams.Result.GetErrorValue());
		}
		NotifyUserRequestedSession(PlatformUserId, RequestedSession, ResultInfo);
	}
	else
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnJoinLobbyRequested: Failed to get account by local user id %s"), *UE::Online::ToLogString(EventParams.LocalAccountId));
	}
}

// 从 PlayerController 的 LocalPlayer 首选 NetId 提取 OSSv2 AccountId，无有效映射时返回空标识。
UE::Online::FAccountId UCommonSessionSubsystem::GetAccountId(APlayerController* PlayerController) const
{
	if (const ULocalPlayer* const LocalPlayer = PlayerController->GetLocalPlayer())
	{
		FUniqueNetIdRepl LocalPlayerIdRepl = LocalPlayer->GetPreferredUniqueNetId();
		if (LocalPlayerIdRepl.IsValid())
		{
			return LocalPlayerIdRepl.GetV2();
		}
	}
	return FAccountId();
}

// 查询主本地账户已加入的 Lobby，并返回 LocalName 与指定会话名匹配的 LobbyId。
UE::Online::FLobbyId UCommonSessionSubsystem::GetLobbyId(const FName SessionName) const
{
	FAccountId LocalUserId = GetAccountId(GetGameInstance()->GetFirstLocalPlayerController());
	if (LocalUserId.IsValid())
	{
		IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
		check(OnlineServices);
		ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
		check(Lobbies);
		TOnlineResult<FGetJoinedLobbies> JoinedLobbies = Lobbies->GetJoinedLobbies({ LocalUserId });
		if (JoinedLobbies.IsOk())
		{
			for (const TSharedRef<const FLobby>& Lobby : JoinedLobbies.GetOkValue().Lobbies)
			{
				if (Lobby->LocalName == SessionName)
				{
					return Lobby->LobbyId;
				}
			}
		}
	}
	return FLobbyId();
}

#endif // COMMONUSER_OSSV1
// 使用首个本地 PlayerController 解析 OSSv1 Session 或 OSSv2 Lobby/Session 连接串，允许委托修改后执行绝对 ClientTravel。
void UCommonSessionSubsystem::InternalTravelToSession(const FName SessionName)
{
	//@TODO：理想情况下应使用触发加入的玩家而不是首个本地玩家；当前所有本地玩家会同时 Travel，影响通常有限。
	//@TODO: Ideally we'd use triggering player instead of first (they're all gonna go at once so it probably doesn't matter)
	APlayerController* const PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (PlayerController == nullptr)
	{
		FText ReturnReason = NSLOCTEXT("NetworkErrors", "InvalidPlayerController", "Invalid Player Controller");
		UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession(Failed due to %s)"), *ReturnReason.ToString());
		return;
	}

	FString URL;
#if COMMONUSER_OSSV1
	// OSSv1 通过命名 Session 解析游戏端口连接字符串。
	// travel to session
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);

	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions.IsValid());

	if (!Sessions->GetResolvedConnectString(SessionName, URL))
	{
		FText FailReason = NSLOCTEXT("NetworkErrors", "TravelSessionFailed", "Travel to Session failed.");
		UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession(%s)"), *FailReason.ToString());
		return;
	}
#else
	TSharedPtr<IOnlineServices> OnlineServices = GetServices(GetWorld(), EOnlineServices::Default);
	check(OnlineServices);

	FAccountId LocalUserId = GetAccountId(PlayerController);
	if (LocalUserId.IsValid())
	{
		TOnlineResult<FGetResolvedConnectString> Result;
		if (!bUseLobbiesDefault)
		{
			ISessionsPtr Sessions = OnlineServices->GetSessionsInterface();
			check(Sessions)
			TOnlineResult<FGetSessionByName> SessionResult = Sessions->GetSessionByName({ SessionName });

			if (SessionResult.IsOk())
			{
				Result = OnlineServices->GetResolvedConnectString({ LocalUserId, FLobbyId(), SessionResult.GetOkValue().Session->GetSessionId(), NAME_GamePort });
			}
		}
		else
		{
			Result = OnlineServices->GetResolvedConnectString({ LocalUserId, GetLobbyId(SessionName) });
		}

		if (ensure(Result.IsOk()))
		{
			URL = Result.GetOkValue().ResolvedConnectString;
		}
	}
#endif // COMMONUSER_OSSV1

	// Travel 前允许游戏代码追加认证 Token 或其他连接 URL 参数。
	// Allow modification of the URL prior to travel
	OnPreClientTravelEvent.Broadcast(URL);

	PlayerController->ClientTravel(URL, TRAVEL_Absolute);
}

// 将平台来源的加入请求以相同参数广播给原生和蓝图监听者。
void UCommonSessionSubsystem::NotifyUserRequestedSession(const FPlatformUserId& PlatformUserId, UCommonSession_SearchResult* RequestedSession, const FOnlineResultInformation& RequestedSessionResult)
{
	OnUserRequestedSessionEvent.Broadcast(PlatformUserId, RequestedSession, RequestedSessionResult);
	K2_OnUserRequestedSessionEvent.Broadcast(PlatformUserId, RequestedSession, RequestedSessionResult);
}

// 将会话加入结果广播给原生和蓝图完成委托。
void UCommonSessionSubsystem::NotifyJoinSessionComplete(const FOnlineResultInformation& Result)
{
	OnJoinSessionCompleteEvent.Broadcast(Result);
	K2_OnJoinSessionCompleteEvent.Broadcast(Result);
}

// 将会话创建结果广播给原生和蓝图完成委托。
void UCommonSessionSubsystem::NotifyCreateSessionComplete(const FOnlineResultInformation& Result)
{
	OnCreateSessionCompleteEvent.Broadcast(Result);
	K2_OnCreateSessionCompleteEvent.Broadcast(Result);
}

// 将可展示的会话状态、游戏模式和地图同步广播给原生及蓝图监听者。
void UCommonSessionSubsystem::NotifySessionInformationUpdated(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName)
{
	OnSessionInformationChangedEvent.Broadcast(SessionStatus, GameMode, MapName);
	K2_OnSessionInformationChangedEvent.Broadcast(SessionStatus, GameMode, MapName);
}

// 将平台用户的离开或销毁会话请求广播给原生和蓝图监听者。
void UCommonSessionSubsystem::NotifyDestroySessionRequested(const FPlatformUserId& PlatformUserId, const FName& SessionName)
{
	OnDestroySessionRequestedEvent.Broadcast(PlatformUserId, SessionName);
	K2_OnDestroySessionRequestedEvent.Broadcast(PlatformUserId, SessionName);
}

// 将同步校验错误写入待广播的创建结果，并使用 InternalFailure 作为统一错误 ID。
void UCommonSessionSubsystem::SetCreateSessionError(const FText& ErrorText)
{
	CreateSessionResult.bWasSuccessful = false;
	CreateSessionResult.ErrorId = TEXT("InternalFailure");

	// TODO：Shipping 构建可根据允许暴露的诊断信息量，将具体文本替换为通用错误提示。
	// TODO May want to replace with a generic error text in shipping builds depending on how much data you want to give users
	CreateSessionResult.ErrorText = ErrorText;
}

// 创建 Beacon 监听器和 PartyBeaconHost，复用已有状态或按配置初始化预留容量，并开放请求接收。
void UCommonSessionSubsystem::CreateHostReservationBeacon()
{
	check(!BeaconHostListener.IsValid());
	check(!ReservationBeaconHost.IsValid());

	UWorld* const World = GetWorld();
	BeaconHostListener = World->SpawnActor<AOnlineBeaconHost>(AOnlineBeaconHost::StaticClass());
	check(BeaconHostListener.IsValid());
	verify(BeaconHostListener->InitHost());

	ReservationBeaconHost = World->SpawnActor<APartyBeaconHost>(APartyBeaconHost::StaticClass());
	check(ReservationBeaconHost.IsValid());

	if (ReservationBeaconHostState)
	{
		ReservationBeaconHost->InitFromBeaconState(&*ReservationBeaconHostState);
	}
	else
	{
		// TODO：当前使用配置属性的默认值初始化 Beacon 参数，后续可按具体会话请求动态配置。
		// TODO: We are using the default hard-coded values for the parameters for now, but they are configurable
		ReservationBeaconHost->InitHostBeacon(BeaconTeamCount, BeaconTeamSize, BeaconMaxReservations, NAME_GameSession);
		ReservationBeaconHostState = ReservationBeaconHost->GetState();
	}

	BeaconHostListener->RegisterHost(ReservationBeaconHost.Get());
	BeaconHostListener->PauseBeaconRequests(false);
}

// 从监听器注销 Reservation Host，并销毁、清空 Beacon Listener 与 Host Actor；保留 HostState 供后续复用。
void UCommonSessionSubsystem::DestroyHostReservationBeacon()
{
	if (BeaconHostListener.IsValid() && ReservationBeaconHost.IsValid())
	{
		BeaconHostListener->UnregisterHost(ReservationBeaconHost->GetBeaconType());
	}
	if (BeaconHostListener.IsValid())
	{
		BeaconHostListener->Destroy();
		BeaconHostListener = nullptr;
	}
	if (ReservationBeaconHost.IsValid())
	{
		ReservationBeaconHost->Destroy();
		ReservationBeaconHost = nullptr;
	}
}

#if COMMONUSER_OSSV1
// OSSv1 全局会话失败回调当前只记录用户和失败类型，尚未触发恢复或错误通知。
void UCommonSessionSubsystem::HandleSessionFailure(const FUniqueNetId& NetId, ESessionFailure::Type FailureType)
{
	UE_LOG(LogCommonSession, Warning, TEXT("UCommonSessionSubsystem::HandleSessionFailure(NetId: %s, FailureType: %s)"), *NetId.ToDebugString(), LexToString(FailureType));
	
	//@TODO：根据失败类型通知游戏、恢复前端状态并清理底层会话。
	//@TODO: Probably need to do a bit more...
}

// OSSv1 邀请接受回调将本地用户索引和搜索结果包装为统一平台加入请求；失败时构造有限错误信息。
void UCommonSessionSubsystem::HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 LocalUserIndex, FUniqueNetIdPtr AcceptingUserId, const FOnlineSessionSearchResult& SearchResult)
{
	FPlatformUserId PlatformUserId = IPlatformInputDeviceMapper::Get().GetPlatformUserForUserIndex(LocalUserIndex);

	UCommonSession_SearchResult* RequestedSession = nullptr;
	FOnlineResultInformation ResultInfo;
	if (bWasSuccessful)
	{
		RequestedSession = NewObject<UCommonSession_SearchResult>(this);
		RequestedSession->Result = SearchResult;
	}
	else
	{
		// OSSv1 邀请失败没有可直接转换的 FOnlineError，只能手动构造结果。
		// No FOnlineError to initialize from
		ResultInfo.bWasSuccessful = false;
		// 没有扩展错误信息时只能使用不够稳定的固定错误 ID。
		ResultInfo.ErrorId = TEXT("failed"); // This is not robust but there is no extended information available
		ResultInfo.ErrorText = LOCTEXT("Error_SessionUserInviteAcceptedFailed", "Failed to handle the join request");
	}
	NotifyUserRequestedSession(PlatformUserId, RequestedSession, ResultInfo);
}

#endif // COMMONUSER_OSSV1

// 只处理当前 GameInstance World 的全局 Travel 失败，记录原因并将可展示会话状态恢复为 OutOfGame。
void UCommonSessionSubsystem::TravelLocalSessionFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ReasonString)
{
	// 该委托是全局的，而 PIE 可同时存在多个 GameInstance，因此必须过滤到当前子系统所属 World。
	// The delegate for this is global, but PIE can have more than one game instance, so make
	// sure it's being raised for the same world this game instance subsystem is associated with
	if (World != GetWorld())
	{
		return;
	}

	UE_LOG(LogCommonSession, Warning, TEXT("TravelLocalSessionFailure(World: %s, FailureType: %s, ReasonString: %s)"),
		*GetPathNameSafe(World),
		ETravelFailure::ToString(FailureType),
		*ReasonString);

	// TODO：待成功通知改到 Travel 真正完成后，再广播这里的失败；当前先报成功再报 Travel 失败会让监听者困惑。
	// TODO:  Broadcast this failure when we are also able to broadcast a success. Presently we broadcast a success before starting the travel, so a failure after a success is confusing.
	//FOnlineResultInformation JoinSessionResult;
	//JoinSessionResult.bWasSuccessful = false;
	//JoinSessionResult.ErrorId = ReasonString; // TODO:  Is this an adequate ErrorId?
	//JoinSessionResult.ErrorText = FText::FromString(ReasonString);
	//NotifyJoinSessionComplete(JoinSessionResult);
	NotifySessionInformationUpdated(ECommonSessionInformationState::OutOfGame);
}

// 当前 Game/PIE World 加载完成后，OSSv1 托管端更新广告地图全包名，并按需重新创建 Reservation Beacon。
void UCommonSessionSubsystem::HandlePostLoadMap(UWorld* World)
{
	// 忽略空 World 回调。
	// Ignore null worlds.
	if (!World)
	{
		return;
	}

	// 编辑器中可能加载其他 GameInstance 的 World，只处理当前子系统所属实例。
	// Ignore any world that isn't part of this game instance, which can be the case in the editor.
	if (World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// 只有 Game 或 PIE World 才需要更新在线会话广告。
	// We don't care about updating the session unless the world type is game/pie.
	if (!(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		return;
	}

#if COMMONUSER_OSSV1
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);

	const IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
	check(SessionInterface.IsValid());

	const FName SessionName(NAME_GameSession);
	FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(SessionName);

	// 当前是会话主机时更新广告中的地图名称。
	// If we're hosting a session, update the advertised map name.
	if (CurrentSession != nullptr && CurrentSession->bHosting)
	{
		// 必须使用与 HostRequest::GetMapName 一致的完整包路径；World::GetMapName 只返回短名称。
		// This needs to be the full package path to match the host GetMapName function, World->GetMapName is currently the short name - update host settings
		CurrentSession->SessionSettings.Set(SETTING_MAPNAME, UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()), EOnlineDataAdvertisementType::ViaOnlineService);

		SessionInterface->UpdateSession(SessionName, CurrentSession->SessionSettings, true);

		if (bUseBeacons)
		{
			CreateHostReservationBeacon();
		}
	}
#endif // COMMONUSER_OSSV1
}

#undef LOCTEXT_NAMESPACE
