// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/WeakObjectPtr.h"
#include "PartyBeaconClient.h"
#include "PartyBeaconHost.h"
#include "PartyBeaconState.h"
#if! COMMONUSER_OSSV1
#include "Online/Sessions.h"
#endif



class APlayerController;
class AOnlineBeaconHost;
class ULocalPlayer;
namespace ETravelFailure { enum Type : int; }
struct FOnlineResultInformation;

#if COMMONUSER_OSSV1
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#else
#include "Online/Lobbies.h"
#include "Online/OnlineAsyncOpHandle.h"
#endif // COMMONUSER_OSSV1

#include "CommonSessionSubsystem.generated.h"

class UWorld;
class FCommonSession_OnlineSessionSettings;

#if COMMONUSER_OSSV1
class FCommonOnlineSearchSettingsOSSv1;
using FCommonOnlineSearchSettings = FCommonOnlineSearchSettingsOSSv1;
#else
class FCommonOnlineSearchSettingsOSSv2;
using FCommonOnlineSearchSettings = FCommonOnlineSearchSettingsOSSv2;
#endif // COMMONUSER_OSSV1


//////////////////////////////////////////////////////////////////////
// UCommonSession_HostSessionRequest

/** 指定游戏会话使用的在线功能和连接方式。 */
/** Specifies the online features and connectivity that should be used for a game session */
UENUM(BlueprintType)
enum class ECommonSessionOnlineMode : uint8
{
	Offline,
	LAN,
	Online
};

/** 保存创建和托管游戏会话所需参数的请求对象。 */
/** A request object that stores the parameters used when hosting a gameplay session */
UCLASS(MinimalAPI, BlueprintType)
class UCommonSession_HostSessionRequest : public UObject
{
	GENERATED_BODY()

public:
	/** 指定会话是完整在线模式、局域网模式还是离线模式。 */
	/** Indicates if the session is a full online session or a different type */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	ECommonSessionOnlineMode OnlineMode;

	/** 后端支持时，是否创建由玩家托管的 Lobby。 */
	/** True if this request should create a player-hosted lobbies if available */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	bool bUseLobbies;

	/** 后端支持时，是否为创建的 Lobby 启用语音聊天。 */
	/** True if this request should create a lobby with enabled voice chat in available */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	bool bUseLobbiesVoiceChat;

	/** 是否让创建的会话出现在用户 Presence 信息中。 */
	/** True if this request should create a session that will appear in the user's presence information */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	bool bUsePresence;

	/** 匹配和广告会话时用于标识游戏模式类型的字符串。 */
	/** String used during matchmaking to specify what type of game mode this is */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	FString ModeNameForAdvertisement;

	/** 游戏开始时加载的地图，必须是有效的顶层 World PrimaryAsset。 */
	/** The map that will be loaded at the start of gameplay, this needs to be a valid Primary Asset top-level map */
	UPROPERTY(BlueprintReadWrite, Category=Session, meta=(AllowedTypes="World"))
	FPrimaryAssetId MapID;

	/** 作为 URL Option 传给游戏地图的附加键值参数。 */
	/** Extra arguments passed as URL options to the game */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	TMap<FString, FString> ExtraArgs;

	/** 每个游戏会话允许的最大玩家数量。 */
	/** Maximum players allowed per gameplay session */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	int32 MaxPlayerCount = 16;

public:
	/** 返回实际使用的最大玩家数，派生请求可覆写计算方式。 */
	/** Returns the maximum players that should actually be used, could be overridden in child classes */
	COMMONUSER_API virtual int32 GetMaxPlayers() const;

	/** 返回游戏过程中使用的完整地图包名。 */
	/** Returns the full map name that will be used during gameplay */
	COMMONUSER_API virtual FString GetMapName() const;

	/** 构造传给 ServerTravel 的完整地图 URL 和 Option。 */
	/** Constructs the full URL that will be passed to ServerTravel */
	COMMONUSER_API virtual FString ConstructTravelURL() const;

	/** 校验托管请求；有效时返回 true，无效时返回 false、填写错误并记录日志。 */
	/** Returns true if this request is valid, returns false and logs errors if it is not */
	COMMONUSER_API virtual bool ValidateAndLogErrors(FText& OutError) const;
};


//////////////////////////////////////////////////////////////////////
// UCommonSession_SearchResult

/** 在线系统返回的可加入游戏会话结果对象。 */
/** A result object returned from the online system that describes a joinable game session */
UCLASS(MinimalAPI, BlueprintType)
class UCommonSession_SearchResult : public UObject
{
	GENERATED_BODY()

public:
	/** 返回供日志诊断使用的内部会话描述，不面向最终用户显示。 */
	/** Returns an internal description of the session, not meant to be human readable */
	UFUNCTION(BlueprintCallable, Category=Session)
	COMMONUSER_API FString GetDescription() const;

	/** 读取任意字符串会话设置；键不存在时 bFoundValue 为 false。 */
	/** Gets an arbitrary string setting, bFoundValue will be false if the setting does not exist */
	UFUNCTION(BlueprintPure, Category=Sessions)
	COMMONUSER_API void GetStringSetting(FName Key, FString& Value, bool& bFoundValue) const;

	/** 读取任意整数会话设置；键不存在时 bFoundValue 为 false。 */
	/** Gets an arbitrary integer setting, bFoundValue will be false if the setting does not exist */
	UFUNCTION(BlueprintPure, Category = Sessions)
	COMMONUSER_API void GetIntSetting(FName Key, int32& Value, bool& bFoundValue) const;

	/** 当前仍可用的私有连接名额数量。 */
	/** The number of private connections that are available */
	UFUNCTION(BlueprintPure, Category=Sessions)
	COMMONUSER_API int32 GetNumOpenPrivateConnections() const;

	/** 当前仍可用的公开连接名额数量。 */
	/** The number of publicly available connections that are available */
	UFUNCTION(BlueprintPure, Category=Sessions)
	COMMONUSER_API int32 GetNumOpenPublicConnections() const;

	/** 公开连接的总容量，包括已经被占用的名额。 */
	/** The maximum number of publicly available connections that could be available, including already filled connections */
	UFUNCTION(BlueprintPure, Category = Sessions)
	COMMONUSER_API int32 GetMaxPublicConnections() const;

	/** 到搜索结果主机的毫秒延迟；MAX_QUERY_PING 表示不可达。 */
	/** Ping to the search result, MAX_QUERY_PING is unreachable */
	UFUNCTION(BlueprintPure, Category=Sessions)
	COMMONUSER_API int32 GetPingInMs() const;

public:
	/** 指向 OSS 版本对应的平台会话或 Lobby 实现数据。 */
	/** Pointer to the platform-specific implementation */
#if COMMONUSER_OSSV1
	FOnlineSessionSearchResult Result;
#else
	TSharedPtr<const UE::Online::FLobby> Lobby;

	UE::Online::FOnlineSessionId SessionID;
#endif // COMMONUSER_OSSV1

};


//////////////////////////////////////////////////////////////////////
// UCommonSession_SearchSessionRequest

/** 会话搜索完成时调用的原生和蓝图委托。 */
/** Delegates called when a session search completes */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCommonSession_FindSessionsFinished, bool bSucceeded, const FText& ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonSession_FindSessionsFinishedDynamic, bool, bSucceeded, FText, ErrorMessage);

/** 描述会话搜索条件的请求对象；搜索完成后会写入结果并触发完成委托。 */
/** Request object describing a session search, this object will be updated once the search has completed */
UCLASS(MinimalAPI, BlueprintType)
class UCommonSession_SearchSessionRequest : public UObject
{
	GENERATED_BODY()

public:
	/** 指定要搜索完整在线会话、局域网会话还是其他连接模式。 */
	/** Indicates if the this is looking for full online games or a different type like LAN */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	ECommonSessionOnlineMode OnlineMode;

	/** 为 true 时在支持情况下搜索玩家托管 Lobby；为 false 时只搜索已注册服务器会话。 */
	/** True if this request should look for player-hosted lobbies if they are available, false will only search for registered server sessions */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	bool bUseLobbies;

	/** 找到的全部会话；在 OnSearchFinished 调用时有效。 */
	/** List of all found sessions, will be valid when OnSearchFinished is called */
	UPROPERTY(BlueprintReadOnly, Category=Session)
	TArray<TObjectPtr<UCommonSession_SearchResult>> Results;

	/** 会话搜索完成时调用的原生委托。 */
	/** Native Delegate called when a session search completes */
	FCommonSession_FindSessionsFinished OnSearchFinished;

	/** 由子系统调用，用于统一执行原生和蓝图搜索完成委托。 */
	/** Called by subsystem to execute finished delegates */
	COMMONUSER_API void NotifySearchFinished(bool bSucceeded, const FText& ErrorMessage);

private:
	/** 会话搜索完成时广播给蓝图的委托。 */
	/** Delegate called when a session search completes */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Search Finished", AllowPrivateAccess = true))
	FCommonSession_FindSessionsFinishedDynamic K2_OnSearchFinished;
};


//////////////////////////////////////////////////////////////////////
// CommonSessionSubsystem Events

/**
 * 本地用户从平台 Overlay 等外部来源请求加入会话时触发。游戏通常应将玩家切换到该会话。
 * @param LocalPlatformUserId 接受邀请的本地平台用户；此时用户可能尚未登录，因此使用 PlatformUserId。
 * @param RequestedSession 请求加入的会话；处理请求失败时可为空。
 * @param RequestedSessionResult 请求处理结果。
 */
/**
 * Event triggered when the local user has requested to join a session from an external source, for example from a platform overlay.
 * Generally, the game should transition the player into the session.
 * @param LocalPlatformUserId the local user id that accepted the invitation. This is a platform user id because the user might not be signed in yet.
 * @param RequestedSession the requested session. Can be null if there was an error processing the request.
 * @param RequestedSessionResult result of the requested session processing
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FCommonSessionOnUserRequestedSession, const FPlatformUserId& /*LocalPlatformUserId*/, UCommonSession_SearchResult* /*RequestedSession*/, const FOnlineResultInformation& /*RequestedSessionResult*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCommonSessionOnUserRequestedSession_Dynamic, const FPlatformUserId&, LocalPlatformUserId, UCommonSession_SearchResult*, RequestedSession, const FOnlineResultInformation&, RequestedSessionResult);

/**
 * 底层会话加入完成后、成功情况下开始服务器 Travel 前触发。
 * 参数说明加入是否成功，以及是否存在会阻止 Travel 的错误。
 * @param Result 会话加入结果。
 */
/**
 * Event triggered when a session join has completed, after joining the underlying session and before traveling to the server if it was successful.
 * The event parameters indicate if this was successful, or if there was an error that will stop it from traveling.
 * @param Result result of the session join
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCommonSessionOnJoinSessionComplete, const FOnlineResultInformation& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonSessionOnJoinSessionComplete_Dynamic, const FOnlineResultInformation&, Result);

/**
 * 托管会话创建完成后、开始加载目标地图前触发。
 * 参数说明创建是否成功，以及是否存在会阻止 Travel 的错误。
 * @param Result 会话创建结果。
 */
/**
 * Event triggered when a session creation for hosting has completed, right before it travels to the map.
 * The event parameters indicate if this was successful, or if there was an error that will stop it from traveling.
 * @param Result result of the session join
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCommonSessionOnCreateSessionComplete, const FOnlineResultInformation& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonSessionOnCreateSessionComplete_Dynamic, const FOnlineResultInformation&, Result);

/**
 * 本地用户从平台 Overlay 等外部来源请求销毁或离开会话时触发，游戏应让玩家退出当前会话。
 * @param LocalPlatformUserId 发起请求的本地平台用户；用户可能尚未登录，因此使用 PlatformUserId。
 * @param SessionName 会话名称标识。
 */
/**
 * Event triggered when the local user has requested to destroy a session from an external source, for example from a platform overlay.
 * The game should transition the player out of the session.
 * @param LocalPlatformUserId the local user id that made the destroy request. This is a platform user id because the user might not be signed in yet.
 * @param SessionName the name identifier for the session.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCommonSessionOnDestroySessionRequested, const FPlatformUserId& /*LocalPlatformUserId*/, const FName& /*SessionName*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonSessionOnDestroySessionRequested_Dynamic, const FPlatformUserId&, LocalPlatformUserId, const FName&, SessionName);

/**
 * 会话加入成功、连接字符串解析完成且客户端 Travel 尚未开始时触发。
 * @param URL 已解析并包含附加参数的会话连接 URL，可由监听者继续修改。
 */
/**
 * Event triggered when a session join has completed, after resolving the connect string and prior to the client traveling.
 * @param URL resolved connection string for the session with any additional arguments
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FCommonSessionOnPreClientTravel, FString& /*URL*/);

/**
 * 会话生态进入可向用户展示的状态时触发。
 * 该状态只用于 Rich Presence 等展示功能；在线流程应使用 OnCreateSessionComplete 或 OnJoinSessionComplete。
 */
/**
 * Event triggered at different points in the session ecosystem that represent a user-presentable state of the session.
 * This should not be used for online functionality (use OnCreateSessionComplete or OnJoinSessionComplete for those) but for features such as rich presence
 */
UENUM(BlueprintType)
enum class ECommonSessionInformationState : uint8
{
	OutOfGame,
	Matchmaking,
	InGame
};
DECLARE_MULTICAST_DELEGATE_ThreeParams(FCommonSessionOnSessionInformationChanged, ECommonSessionInformationState /*SessionStatus*/, const FString& /*GameMode*/, const FString& /*MapName*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCommonSessionOnSessionInformationChanged_Dynamic, ECommonSessionInformationState, SessionStatus, const FString&, GameMode, const FString&, MapName);

//////////////////////////////////////////////////////////////////////
// UCommonSessionSubsystem

/**
 * 处理托管和加入在线游戏请求的 GameInstanceSubsystem。
 * 每个 GameInstance 创建一个实例，可由蓝图或 C++ 访问；存在游戏专用派生子系统时不会创建该基类实例。
 */
/** 
 * Game subsystem that handles requests for hosting and joining online games.
 * One subsystem is created for each game instance and can be accessed from blueprints or C++ code.
 * If a game-specific subclass exists, this base subsystem will not be created.
 */
UCLASS(MinimalAPI, BlueprintType, Config=Engine)
class UCommonSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCommonSessionSubsystem() { }

	COMMONUSER_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	COMMONUSER_API virtual void Deinitialize() override;
	COMMONUSER_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** 创建带默认在线选项的托管请求，调用方可在提交前继续修改。 */
	/** Creates a host session request with default options for online games, this can be modified after creation */
	UFUNCTION(BlueprintCallable, Category = Session)
	COMMONUSER_API virtual UCommonSession_HostSessionRequest* CreateOnlineHostSessionRequest();

	/** 创建带默认在线搜索选项的请求，调用方可在执行搜索前继续修改。 */
	/** Creates a session search object with default options to look for default online games, this can be modified after creation */
	UFUNCTION(BlueprintCallable, Category = Session)
	COMMONUSER_API virtual UCommonSession_SearchSessionRequest* CreateOnlineSearchSessionRequest();

	/** 按请求创建并托管新游戏；成功后执行非无缝地图切换。 */
	/** Creates a new online game using the session request information, if successful this will start a hard map transfer */
	UFUNCTION(BlueprintCallable, Category=Session)
	COMMONUSER_API virtual void HostSession(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* Request);

	/** 启动快速游戏流程：先搜索可加入会话，没有合适结果时改为创建新会话。 */
	/** Starts a process to look for existing sessions or create a new one if no viable sessions are found */
	UFUNCTION(BlueprintCallable, Category=Session)
	COMMONUSER_API virtual void QuickPlaySession(APlayerController* JoiningOrHostingPlayer, UCommonSession_HostSessionRequest* Request);

	/** 开始加入现有会话；成功后连接到该会话指定的服务器。 */
	/** Starts process to join an existing session, if successful this will connect to the specified server */
	UFUNCTION(BlueprintCallable, Category=Session)
	COMMONUSER_API virtual void JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Request);

	/** 查询在线系统，返回符合搜索请求的可加入会话列表。 */
	/** Queries online system for the list of joinable sessions matching the search request */
	UFUNCTION(BlueprintCallable, Category=Session)
	COMMONUSER_API virtual void FindSessions(APlayerController* SearchingPlayer, UCommonSession_SearchSessionRequest* Request);

	/** 清理当前活动或待处理会话，通常在返回主菜单等场景调用。 */
	/** Clean up any active sessions, called from cases like returning to the main menu */
	UFUNCTION(BlueprintCallable, Category=Session)
	COMMONUSER_API virtual void CleanUpSessions();

	//////////////////////////////////////////////////////////////////////
	// 会话生命周期和平台请求事件。
	// Events

	/** 本地用户接受邀请时触发的原生委托。 */
	/** Native Delegate when a local user has accepted an invite */
	FCommonSessionOnUserRequestedSession OnUserRequestedSessionEvent;
	/** 本地用户接受邀请时广播给蓝图的事件。 */
	/** Event broadcast when a local user has accepted an invite */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On User Requested Session"))
	FCommonSessionOnUserRequestedSession_Dynamic K2_OnUserRequestedSessionEvent;

	/** JoinSession 调用完成时触发的原生委托。 */
	/** Native Delegate when a JoinSession call has completed */
	FCommonSessionOnJoinSessionComplete OnJoinSessionCompleteEvent;
	/** JoinSession 调用完成时广播给蓝图的事件。 */
	/** Event broadcast when a JoinSession call has completed */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Join Session Complete"))
	FCommonSessionOnJoinSessionComplete_Dynamic K2_OnJoinSessionCompleteEvent;

	/** CreateSession 调用完成时触发的原生委托。 */
	/** Native Delegate when a CreateSession call has completed */
	FCommonSessionOnCreateSessionComplete OnCreateSessionCompleteEvent;
	/** CreateSession 调用完成时广播给蓝图的事件。 */
	/** Event broadcast when a CreateSession call has completed */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Create Session Complete"))
	FCommonSessionOnCreateSessionComplete_Dynamic K2_OnCreateSessionCompleteEvent;

	/** 可展示的会话状态、模式或地图变化时触发的原生委托。 */
	/** Native Delegate when the presentable session information has changed */
	FCommonSessionOnSessionInformationChanged OnSessionInformationChangedEvent;
	/** 可展示会话信息变化时广播给蓝图的事件。 */
	/** Event broadcast when the presentable session information has changed */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Session Information Changed"))
	FCommonSessionOnSessionInformationChanged_Dynamic K2_OnSessionInformationChangedEvent;

	/** 平台请求销毁或离开会话时触发的原生委托。 */
	/** Native Delegate when a platform session destroy has been requested */
	FCommonSessionOnDestroySessionRequested OnDestroySessionRequestedEvent;
	/** 平台请求销毁或离开会话时广播给蓝图的事件。 */
	/** Event broadcast when a platform session destroy has been requested */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Leave Session Requested"))
	FCommonSessionOnDestroySessionRequested_Dynamic K2_OnDestroySessionRequestedEvent;

	/** 客户端 Travel 前用于修改连接 URL 的原生委托。 */
	/** Native Delegate for modifying the connect URL prior to a client travel */
	FCommonSessionOnPreClientTravel OnPreClientTravelEvent;

	// 可由派生类或配置文件覆写的会话默认设置。
	// Config settings, these can overridden in child classes or config files

	/** 设置搜索和托管请求中 bUseLobbies 的默认值。 */
	/** Sets the default value of bUseLobbies for session search and host requests */
	UPROPERTY(Config)
	bool bUseLobbiesDefault = true;

	/** 设置托管请求中 bUseLobbiesVoiceChat 的默认值。 */
	/** Sets the default value of bUseLobbiesVoiceChat for session host requests */
	UPROPERTY(Config)
	bool bUseLobbiesVoiceChatDefault = false;

	/** 创建或加入会话时，是否在服务器 Travel 前启用 Reservation Beacon 预留流程。 */
	/** Enables reservation beacon flow prior to server travel when creating or joining a game session */ 
	UPROPERTY(Config)
	bool bUseBeacons = true;

protected:
	// 创建或加入会话过程中调用的可覆写函数，用于注入游戏专用行为。
	// Functions called during the process of creating or joining a session, these can be overidden for game-specific behavior

	/** 根据快速游戏托管设置填充搜索请求，可由游戏派生类定制匹配条件。 */
	/** Called to fill in a session request from quick play host settings, can be overridden for game-specific behavior */
	COMMONUSER_API virtual TSharedRef<FCommonOnlineSearchSettings> CreateQuickPlaySearchSettings(UCommonSession_HostSessionRequest* Request, UCommonSession_SearchSessionRequest* QuickPlayRequest);

	/** 快速游戏搜索完成时调用，可由游戏派生类决定加入结果还是转为托管。 */
	/** Called when a quick play search finishes, can be overridden for game-specific behavior */
	COMMONUSER_API virtual void HandleQuickPlaySearchFinished(bool bSucceeded, const FText& ErrorMessage, TWeakObjectPtr<APlayerController> JoiningOrHostingPlayer, TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequest);

	/** 本地会话 Travel 失败时调用。 */
	/** Called when traveling to a session fails */
	COMMONUSER_API virtual void TravelLocalSessionFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ReasonString);

	/** 新会话创建成功或失败后调用。 */
	/** Called when a new session is either created or fails to be created */
	COMMONUSER_API virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	/** 完成会话创建结果通知和后续 Travel 收尾。 */
	/** Called to finalize session creation */
	COMMONUSER_API virtual void FinishSessionCreation(bool bWasSuccessful);

	/** 托管端完成新地图加载后调用。 */
	/** Called after traveling to the new hosted session map */
	COMMONUSER_API virtual void HandlePostLoadMap(UWorld* World);

protected:
	// 初始化在线接口并处理 OSS 返回结果的内部函数。
	// Internal functions for initializing and handling results from the online systems

	COMMONUSER_API void BindOnlineDelegates();
	COMMONUSER_API void CreateOnlineSessionInternal(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request);
	COMMONUSER_API void FindSessionsInternal(APlayerController* SearchingPlayer, const TSharedRef<FCommonOnlineSearchSettings>& InSearchSettings);
	COMMONUSER_API void JoinSessionInternal(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request);
	COMMONUSER_API void InternalTravelToSession(const FName SessionName);
	COMMONUSER_API void NotifyUserRequestedSession(const FPlatformUserId& PlatformUserId, UCommonSession_SearchResult* RequestedSession, const FOnlineResultInformation& RequestedSessionResult);
	COMMONUSER_API void NotifyJoinSessionComplete(const FOnlineResultInformation& Result);
	COMMONUSER_API void NotifyCreateSessionComplete(const FOnlineResultInformation& Result);
	COMMONUSER_API void NotifySessionInformationUpdated(ECommonSessionInformationState SessionStatusStr, const FString& GameMode = FString(), const FString& MapName = FString());
	COMMONUSER_API void NotifyDestroySessionRequested(const FPlatformUserId& PlatformUserId, const FName& SessionName);
	COMMONUSER_API void SetCreateSessionError(const FText& ErrorText);

#if COMMONUSER_OSSV1
	COMMONUSER_API void BindOnlineDelegatesOSSv1();
	COMMONUSER_API void CreateOnlineSessionInternalOSSv1(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request);
	COMMONUSER_API void FindSessionsInternalOSSv1(ULocalPlayer* LocalPlayer);
	COMMONUSER_API void JoinSessionInternalOSSv1(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request);
	COMMONUSER_API TSharedRef<FCommonOnlineSearchSettings> CreateQuickPlaySearchSettingsOSSv1(UCommonSession_HostSessionRequest* Request, UCommonSession_SearchSessionRequest* QuickPlayRequest);
	COMMONUSER_API void CleanUpSessionsOSSv1();

	COMMONUSER_API void HandleSessionFailure(const FUniqueNetId& NetId, ESessionFailure::Type FailureType);
	COMMONUSER_API void HandleSessionUserInviteAccepted(const bool bWasSuccessful, const int32 LocalUserIndex, FUniqueNetIdPtr AcceptingUserId, const FOnlineSessionSearchResult& SearchResult);
	COMMONUSER_API void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	COMMONUSER_API void OnRegisterLocalPlayerComplete_CreateSession(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result);
	COMMONUSER_API void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	COMMONUSER_API void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	COMMONUSER_API void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	COMMONUSER_API void OnDestroySessionRequested(int32 LocalUserNum, FName SessionName);
	COMMONUSER_API void OnFindSessionsComplete(bool bWasSuccessful);
	COMMONUSER_API void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	COMMONUSER_API void OnRegisterJoiningLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result);
	COMMONUSER_API void FinishJoinSession(EOnJoinSessionCompleteResult::Type Result);

#else
	COMMONUSER_API void BindOnlineDelegatesOSSv2();
	COMMONUSER_API void CreateOnlineSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request);
	COMMONUSER_API void FindSessionsInternalOSSv2(ULocalPlayer* LocalPlayer);
	COMMONUSER_API void JoinSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request);
	COMMONUSER_API TSharedRef<FCommonOnlineSearchSettings> CreateQuickPlaySearchSettingsOSSv2(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest);
	COMMONUSER_API void CleanUpSessionsOSSv2();

	/** 处理由在线服务 UI 发起的 Lobby 加入请求。 */
	/** Process a join request originating from the online service */
	COMMONUSER_API void OnLobbyJoinRequested(const UE::Online::FUILobbyJoinRequested& EventParams);

	/** 处理由在线服务 UI 发起的 Session 加入请求。 */
	/** Process a SESSION join request originating from the online service */
	COMMONUSER_API void OnSessionJoinRequested(const UE::Online::FUISessionJoinRequested& EventParams);

	/** 获取指定 PlayerController 对应的本地在线账户标识。 */
	/** Get the local user id for a given controller */
	COMMONUSER_API UE::Online::FAccountId GetAccountId(APlayerController* PlayerController) const;
	/** 获取指定会话名称当前关联的 Lobby 标识。 */
	/** Get the lobby id for a given session name */
	COMMONUSER_API UE::Online::FLobbyId GetLobbyId(const FName SessionName) const;
	/** 在线 UI 请求加入 Lobby 事件的绑定句柄。 */
	/** Event handle for UI lobby join requested */
	UE::Online::FOnlineEventDelegateHandle LobbyJoinRequestedHandle;

	/** 在线 UI 请求加入 Session 事件的绑定句柄。 */
	/** Event handle for UI lobby session requested */
	UE::Online::FOnlineEventDelegateHandle SessionJoinRequestedHandle;

#endif // COMMONUSER_OSSV1

	COMMONUSER_API void CreateHostReservationBeacon();
	COMMONUSER_API void ConnectToHostReservationBeacon();
	COMMONUSER_API void DestroyHostReservationBeacon();

protected:
	/** 会话操作完成后用于地图或客户端 Travel 的待处理 URL。 */
	/** The travel URL that will be used after session operations are complete */
	FString PendingTravelURL;

	/** 最近一次会话创建结果；保存在此以便异步后续阶段继续返回原始错误。 */
	/** Most recent result information for a session creation attempt, stored here to allow storing error codes for later */
	FOnlineResultInformation CreateSessionResult;

	/** 是否要求会话一旦创建完成就立即取消并销毁。 */
	/** True if we want to cancel the session after it is created */
	bool bWantToDestroyPendingSession = false;

	/** 当前是否为专用服务器；专用服务器创建会话不需要 LocalPlayer。 */
	/** True if this is a dedicated server, which doesn't require a LocalPlayer to create a session */
	bool bIsDedicatedServer = false;

	/** 当前正在执行的在线会话搜索设置。 */
	/** Settings for the current search */
	TSharedPtr<FCommonOnlineSearchSettings> SearchSettings;

	/** 用于注册各类 Beacon Host 的通用监听 Actor。 */
	/** General beacon listener for registering beacons with */
	UPROPERTY(Transient)
	TWeakObjectPtr<AOnlineBeaconHost> BeaconHostListener;
	/** Reservation Beacon Host 使用的队伍和预留状态。 */
	/** State of the beacon host */
	UPROPERTY(Transient)
	TObjectPtr<UPartyBeaconState> ReservationBeaconHostState;
	/** 在托管端控制玩家进入当前游戏的 Reservation Beacon。 */
	/** Beacon controlling access to this game. */
	UPROPERTY(Transient)
	TWeakObjectPtr<APartyBeaconHost> ReservationBeaconHost;
	/** 客户端与 Reservation Beacon Host 通信的通用 Beacon Client。 */
	/** Common class object for beacon communication */
	UPROPERTY(Transient)
	TWeakObjectPtr<APartyBeaconClient> ReservationBeaconClient;

	/** Beacon 预留系统使用的队伍数量。 */
	/** Number of teams for beacon reservation */
	UPROPERTY(Config)
	int32 BeaconTeamCount = 2;
	/** Beacon 预留系统中每支队伍的容量。 */
	/** Size of a team for beacon reservation */
	UPROPERTY(Config)
	int32 BeaconTeamSize = 8;
	/** Reservation Beacon 允许的最大预留数量。 */
	/** Max number of beacon reservations */
	UPROPERTY(Config)
	int32 BeaconMaxReservations = 16;
};
