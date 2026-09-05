// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserTypes.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "GameplayTagContainer.h"
#include "CommonUserSubsystem.generated.h"

#if COMMONUSER_OSSV1
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineError.h"
#else
#include "Online/OnlineAsyncOpHandle.h"
#endif

class FNativeGameplayTag;
class IOnlineSubsystem;

/** CommonUserSubsystem 使用的系统消息和平台特性原生标签集合。 */
/** List of tags used by the common user subsystem */
struct FCommonUserTags
{
	// 通用消息严重级别及具体系统消息标签。
	// General severity levels and specific system messages

	// 错误级系统消息的父标签。
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Error;	// SystemMessage.Error
	// 警告级系统消息的父标签。
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Warning; // SystemMessage.Warning
	// 普通展示级系统消息的父标签。
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Display; // SystemMessage.Display

	/** 所有玩家初始化尝试均失败，用户必须处理问题后才能重试。 */
	/** All attempts to initialize a player failed, user has to do something before trying again */
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Error_InitializeLocalPlayerFailed; // SystemMessage.Error.InitializeLocalPlayerFailed


	// 平台特性标签；GameInstance 或其他平台系统应按当前平台调用 SetTraitTags 设置它们。
	// Platform trait tags, it is expected that the game instance or other system calls SetTraitTags with these tags for the appropriate platform

	/** 表示平台严格地将不同 ControllerId 映射到不同系统用户；未设置时同一用户可以拥有多个控制器。 */
	/** This tag means it is a console platform that directly maps controller IDs to different system users. If false, the same user can have multiple controllers */
	static COMMONUSER_API FNativeGameplayTag Platform_Trait_RequiresStrictControllerMapping; // Platform.Trait.RequiresStrictControllerMapping

	/** 表示平台只有一个在线用户，所有本地玩家都使用用户索引 0。 */
	/** This tag means the platform has a single online user and all players use index 0 */
	static COMMONUSER_API FNativeGameplayTag Platform_Trait_SingleOnlineUser; // Platform.Trait.SingleOnlineUser
};

/** 单个逻辑用户的运行时表示；每个已初始化 LocalPlayer 都对应一个实例。 */
/** Logical representation of an individual user, one of these will exist for all initialized local players */
UCLASS(MinimalAPI, BlueprintType)
class UCommonUserInfo : public UObject
{
	GENERATED_BODY()

public:
	/** 用户的主要控制器输入设备；该用户还可以关联其他辅助设备。 */
	/** Primary controller input device for this user, they could also have additional secondary devices */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FInputDeviceId PrimaryInputDevice;

	/** 本地平台上的逻辑用户；访客用户会指向主平台用户。 */
	/** Specifies the logical user on the local platform, guest users will point to the primary user */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FPlatformUserId PlatformUser;
	
	/** 用户分配到 LocalPlayer 后，该值与 GameInstance LocalPlayers 数组中的最终索引一致。 */
	/** If this user is assigned a LocalPlayer, this will match the index in the GameInstance localplayers array once it is fully created */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	int32 LocalPlayerIndex = -1;

	/** 是否允许该用户以访客身份登录。 */
	/** If true, this user is allowed to be a guest */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	bool bCanBeGuest = false;

	/** 是否为附属在主用户 0 下的访客用户。 */
	/** If true, this is a guest user attached to primary user 0 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	bool bIsGuest = false;

	/** 用户本地身份获取、在线登录和权限检查的总体初始化状态。 */
	/** Overall state of the user's initialization process */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	ECommonUserInitializationState InitializationState = ECommonUserInitializationState::Invalid;

	/** 用户已完成在线或仅本地登录时返回 true。 */
	/** Returns true if this user has successfully logged in */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API bool IsLoggedIn() const;

	/** 用户正处于本地初始登录或网络登录阶段时返回 true。 */
	/** Returns true if this user is in the middle of logging in */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API bool IsDoingLogin() const;

	/** 返回指定上下文最近缓存的权限查询结果；从未查询时返回 Unknown。 */
	/** Returns the most recently queries result for a specific privilege, will return unknown if never queried */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API ECommonUserPrivilegeResult GetCachedPrivilegeResult(ECommonUserPrivilege Privilege, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 综合初始化状态和缓存权限结果，判断功能的总体可用性。 */
	/** Ask about the general availability of a feature, this combines cached results with state */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API ECommonUserAvailability GetPrivilegeAvailability(ECommonUserPrivilege Privilege) const;

	/** 返回指定在线上下文缓存的 UniqueNetId。 */
	/** Returns the net id for the given context */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API FUniqueNetIdRepl GetNetId(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回用户可读昵称；该值由 UpdateCachedNetId 或 SetNickname 更新并缓存。 */
	/** Returns the user's human readable nickname, this will return the value that was cached during UpdateCachedNetId or SetNickname */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API FString GetNickname(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 修改缓存昵称，可用于区分多个访客；真实用户的值会在刷新平台身份时被平台昵称覆盖。 */
	/** Modify the user's human readable nickname, this can be used when setting up multiple guests but will get overwritten with the platform nickname for real users */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API void SetNickname(const FString& NewNickname, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game);

	/** 返回描述该用户标识、索引和登录状态的内部调试字符串。 */
	/** Returns an internal debug string for this player */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API FString GetDebugString() const;

	/** 返回平台逻辑用户标识。 */
	/** Accessor for platform user id */
	COMMONUSER_API FPlatformUserId GetPlatformUserId() const;

	/** 为仍接收整数的旧接口返回平台用户索引。 */
	/** Gets the platform user index for older functions expecting an integer */
	COMMONUSER_API int32 GetPlatformUserIndex() const;

	// 仅供在线子系统访问和维护的内部缓存数据。
	// Internal data, only intended to be accessed by online subsystems

	/** 单个在线系统上下文的用户身份与权限缓存。 */
	/** Cached data for each online system */
	struct FCachedData
	{
		/** 当前在线系统缓存的 NetId。 */
		/** Cached net id per system */
		FUniqueNetIdRepl CachedNetId;

		/** 缓存昵称，在 NetId 可能变化时一并刷新。 */
		/** Cached nickanem, updated whenever net ID might change */
		FString CachedNickname;

		/** 各用户权限最近一次查询的缓存结果。 */
		/** Cached values of various user privileges */
		TMap<ECommonUserPrivilege, ECommonUserPrivilegeResult> CachedPrivileges;
	};

	/** 按在线上下文保存缓存；Game 上下文始终存在，其他上下文可能不存在。 */
	/** Per context cache, game will always exist but others may not */
	TMap<ECommonUserOnlineContext, FCachedData> CachedDataMap;
	
	/** 按上下文解析和回退规则查找缓存数据。 */
	/** Looks up cached data using resolution rules */
	COMMONUSER_API FCachedData* GetCachedData(ECommonUserOnlineContext Context);
	COMMONUSER_API const FCachedData* GetCachedData(ECommonUserOnlineContext Context) const;

	/** 更新指定上下文的权限结果，并在需要时同步到聚合 Game 上下文。 */
	/** Updates cached privilege results, will propagate to game if needed */
	COMMONUSER_API void UpdateCachedPrivilegeResult(ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult Result, ECommonUserOnlineContext Context);

	/** 更新指定上下文的 NetId 与昵称缓存，并在需要时同步到 Game 上下文。 */
	/** Updates cached privilege results, will propagate to game if needed */
	COMMONUSER_API void UpdateCachedNetId(const FUniqueNetIdRepl& NewId, ECommonUserOnlineContext Context);

	/** 返回拥有当前用户信息对象的 CommonUserSubsystem。 */
	/** Return the subsystem this is owned by */
	COMMONUSER_API class UCommonUserSubsystem* GetSubsystem() const;
};


/** 用户初始化流程成功或失败时使用的多播和单播动态委托。 */
/** Delegates when initialization processes succeed or fail */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FCommonUserOnInitializeCompleteMulticast, const UCommonUserInfo*, UserInfo, bool, bSuccess, FText, Error, ECommonUserPrivilege, RequestedPrivilege, ECommonUserOnlineContext, OnlineContext);
DECLARE_DYNAMIC_DELEGATE_FiveParams(FCommonUserOnInitializeComplete, const UCommonUserInfo*, UserInfo, bool, bSuccess, FText, Error, ECommonUserPrivilege, RequestedPrivilege, ECommonUserOnlineContext, OnlineContext);

/** 系统消息发送委托；游戏可根据消息类型标签决定如何向用户展示。 */
/** Delegate when a system error message is sent, the game can choose to display it to the user using the type tag */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCommonUserHandleSystemMessageDelegate, FGameplayTag, MessageType, FText, TitleText, FText, BodyText);

/** 权限总体可用性变化委托，可用于监听游戏过程中在线状态等能力变化。 */
/** Delegate when a privilege changes, this can be bound to see if online status/etc changes during gameplay */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FCommonUserAvailabilityChangedDelegate, const UCommonUserInfo*, UserInfo, ECommonUserPrivilege, Privilege, ECommonUserAvailability, OldAvailability, ECommonUserAvailability, NewAvailability);


/** 用户初始化函数参数，通常由异步蓝图节点等包装入口填充。 */
/** Parameter struct for initialize functions, this would normally be filled in by wrapper functions like async nodes */
USTRUCT(BlueprintType)
struct FCommonUserInitializeParams
{
	GENERATED_BODY()
	
	/** 目标 LocalPlayer 索引；允许创建玩家时可指定当前数量之后的下一个索引。 */
	/** What local player index to use, can specify one above current if can create player is enabled */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	int32 LocalPlayerIndex = 0;

	/** 已弃用的 PlatformUser 和输入设备选择方式。 */
	/** Deprecated method of selecting platform user and input device */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	int32 ControllerId = -1;

	/** 用户的主要控制器输入设备；用户还可以关联辅助设备。 */
	/** Primary controller input device for this user, they could also have additional secondary devices */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FInputDeviceId PrimaryInputDevice;

	/** 本地平台上的目标逻辑用户。 */
	/** Specifies the logical user on the local platform */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FPlatformUserId PlatformUser;
	
	/** 初始化必须满足的权限级别，通常为 CanPlay 或 CanPlayOnline。 */
	/** Generally either CanPlay or CanPlayOnline, specifies what level of privilege is required */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	ECommonUserPrivilege RequestedPrivilege = ECommonUserPrivilege::CanPlay;

	/** 要登录的在线上下文；Game 表示登录全部相关上下文并聚合结果。 */
	/** What specific online context to log in to, game means to login to all relevant ones */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	ECommonUserOnlineContext OnlineContext = ECommonUserOnlineContext::Game;

	/** 初始登录时是否允许创建新的 LocalPlayer。 */
	/** True if this is allowed to create a new local player for initial login */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	bool bCanCreateNewLocalPlayer = false;

	/** 玩家是否可以在没有真实在线身份的情况下作为访客用户。 */
	/** True if this player can be a guest user without an actual online presence */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	bool bCanUseGuestLogin = false;

	/** 是否禁止子系统主动显示登录错误；启用后由游戏自行展示。 */
	/** True if we should not show login errors, the game will be responsible for displaying them */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	bool bSuppressLoginErrors = false;

	/** 已绑定时，在登录初始化完成后调用的动态委托。 */
	/** If bound, call this dynamic delegate at completion of login */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	FCommonUserOnInitializeComplete OnUserInitializeComplete;
};

/**
 * 管理用户身份、登录状态和权限查询的 GameInstanceSubsystem。
 * 每个 GameInstance 创建一个实例，可由蓝图或 C++ 访问；存在游戏专用派生子系统时不创建该基类实例。
 */
/**
 * Game subsystem that handles queries and changes to user identity and login status.
 * One subsystem is created for each game instance and can be accessed from blueprints or C++ code.
 * If a game-specific subclass exists, this base subsystem will not be created.
 */
UCLASS(MinimalAPI, BlueprintType, Config=Engine)
class UCommonUserSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCommonUserSubsystem() { }

	COMMONUSER_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	COMMONUSER_API virtual void Deinitialize() override;
	COMMONUSER_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;


	/** 任意用户初始化请求完成时调用的蓝图委托。 */
	/** BP delegate called when any requested initialization request completes */
	UPROPERTY(BlueprintAssignable, Category = CommonUser)
	FCommonUserOnInitializeCompleteMulticast OnUserInitializeComplete;

	/** 系统发送错误、警告或展示消息时调用的蓝图委托。 */
	/** BP delegate called when the system sends an error/warning message */
	UPROPERTY(BlueprintAssignable, Category = CommonUser)
	FCommonUserHandleSystemMessageDelegate OnHandleSystemMessage;

	/** 用户权限总体可用性发生变化时调用的蓝图委托。 */
	/** BP delegate called when privilege availability changes for a user  */
	UPROPERTY(BlueprintAssignable, Category = CommonUser)
	FCommonUserAvailabilityChangedDelegate OnUserPrivilegeChanged;

	/** 通过 OnHandleSystemMessage 广播带类型标签的系统消息。 */
	/** Send a system message via OnHandleSystemMessage */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void SendSystemMessage(FGameplayTag MessageType, FText TitleText, FText BodyText);

	/** 设置允许的最大 LocalPlayer 数量，但不会销毁已经存在的玩家。 */
	/** Sets the maximum number of local players, will not destroy existing ones */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void SetMaxLocalPlayers(int32 InMaxLocalPLayers);

	/** 返回允许的最大 LocalPlayer 数量。 */
	/** Gets the maximum number of local players */
	UFUNCTION(BlueprintPure, Category = CommonUser)
	COMMONUSER_API int32 GetMaxLocalPlayers() const;

	/** 返回当前 LocalPlayer 数量，运行中的游戏至少为 1。 */
	/** Gets the current number of local players, will always be at least 1 */
	UFUNCTION(BlueprintPure, Category = CommonUser)
	COMMONUSER_API int32 GetNumLocalPlayers() const;

	/** 返回指定 LocalPlayer 的用户初始化状态。 */
	/** Returns the state of initializing the specified local player */
	UFUNCTION(BlueprintPure, Category = CommonUser)
	COMMONUSER_API ECommonUserInitializationState GetLocalPlayerInitializationState(int32 LocalPlayerIndex) const;

	/** 按 GameInstance LocalPlayer 索引返回用户信息；运行中的游戏索引 0 始终有效。 */
	/** Returns the user info for a given local player index in game instance, 0 is always valid in a running game */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForLocalPlayerIndex(int32 LocalPlayerIndex) const;

	/** 已弃用；可用时应改用 PlatformUserId。 */
	/** Deprecated, use PlatformUserId when available */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForPlatformUserIndex(int32 PlatformUserIndex) const;

	/** 按 PlatformUserId 返回主用户信息，未初始化时可为空。 */
	/** Returns the primary user info for a given platform user index. Can return null */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForPlatformUser(FPlatformUserId PlatformUser) const;

	/** 按 UniqueNetId 返回用户信息，未找到时可为空。 */
	/** Returns the user info for a unique net id. Can return null */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForUniqueNetId(const FUniqueNetIdRepl& NetId) const;

	/** 已弃用；可用时应改用 InputDeviceId。 */
	/** Deprecated, use InputDeviceId when available */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForControllerId(int32 ControllerId) const;

	/** 按输入设备返回用户信息，设备未映射时可为空。 */
	/** Returns the user info for a given input device. Can return null */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForInputDevice(FInputDeviceId InputDevice) const;

	/**
	 * 尝试创建或更新 LocalPlayer，包括登录用户和创建 PlayerController；完成后广播 OnUserInitializeComplete。
	 *
	 * @param LocalPlayerIndex GameInstance 中目标 LocalPlayer 索引，0 为主玩家，1 及以上用于本地多人。
	 * @param PrimaryInputDevice 映射给用户的物理控制器；无效时使用默认设备。
	 * @param bCanUseGuestLogin 是否允许没有真实 UniqueNetId 的访客登录。
	 * @returns 流程成功启动时返回 true；在安排异步操作前失败则返回 false。
	 */
	/**
	 * Tries to start the process of creating or updating a local player, including logging in and creating a player controller.
	 * When the process has succeeded or failed, it will broadcast the OnUserInitializeComplete delegate.
	 *
	 * @param LocalPlayerIndex	Desired index of LocalPlayer in Game Instance, 0 will be primary player and 1+ for local multiplayer
	 * @param PrimaryInputDevice The physical controller that should be mapped to this user, will use the default device if invalid
	 * @param bCanUseGuestLogin	If true, this player can be a guest without a real Unique Net Id
	 *
	 * @returns true if the process was started, false if it failed before properly starting
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToInitializeForLocalPlay(int32 LocalPlayerIndex, FInputDeviceId PrimaryInputDevice, bool bCanUseGuestLogin);

	/**
	 * 对已完成本地登录的用户执行完整在线登录和账户权限检查；完成后广播 OnUserInitializeComplete。
	 *
	 * @param LocalPlayerIndex GameInstance 中现有 LocalPlayer 的索引。
	 * @returns 流程成功启动时返回 true；在安排异步操作前失败则返回 false。
	 */
	/**
	 * Starts the process of taking a locally logged in user and doing a full online login including account permission checks.
	 * When the process has succeeded or failed, it will broadcast the OnUserInitializeComplete delegate.
	 *
	 * @param LocalPlayerIndex	Index of existing LocalPlayer in Game Instance
	 *
	 * @returns true if the process was started, false if it failed before properly starting
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToLoginForOnlinePlay(int32 LocalPlayerIndex);

	/**
	 * 按 Params 启动通用用户登录和初始化状态机；完成后广播 OnUserInitializeComplete。
	 * AsyncAction_CommonUserInitialize 提供了适合事件图调用的包装入口。
	 *
	 * @returns 流程成功启动时返回 true；在安排异步操作前失败则返回 false。
	 */
	/**
	 * Starts a general user login and initialization process, using the params structure to determine what to log in to.
	 * When the process has succeeded or failed, it will broadcast the OnUserInitializeComplete delegate.
	 * AsyncAction_CommonUserInitialize provides several wrapper functions for using this in an Event graph.
	 *
	 * @returns true if the process was started, false if it failed before properly starting
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToInitializeUser(FCommonUserInitializeParams Params);

	/**
	 * 监听新旧控制器的按键输入并触发用户登录。
	 * 该函数会在活动 GameViewportClient 上安装输入覆盖处理器；再次传入两个空按键数组即可停用。
	 *
	 * @param AnyUserKeys 任意用户均可触发的按键，适用于初始“按键开始”页面；为空时不监听。
	 * @param NewUserKeys 仅未映射新用户可触发的按键，适用于分屏或本地多人；为空时不监听。
	 * @param Params 检测到按键后传给 TryToInitializeUser 的参数。
	 */
	/** 
	 * Starts the process of listening for user input for new and existing controllers and logging them.
	 * This will insert a key input handler on the active GameViewportClient and is turned off by calling again with empty key arrays.
	 *
	 * @param AnyUserKeys		Listen for these keys for any user, even the default user. Set this for an initial press start screen or empty to disable
	 * @param NewUserKeys		Listen for these keys for a new user without a player controller. Set this for splitscreen/local multiplayer or empty to disable
	 * @param Params			Params passed to TryToInitializeUser after detecting key input
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void ListenForLoginKeyInput(TArray<FKey> AnyUserKeys, TArray<FKey> NewUserKeys, FCommonUserInitializeParams Params);

	/** 尝试取消进行中的初始化；平台底层操作可能无法中止，但后续用户回调会被禁用。 */
	/** Attempts to cancel an in-progress initialization attempt, this may not work on all platforms but will disable callbacks */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool CancelUserInitialization(int32 LocalPlayerIndex);

	/** 从所有在线系统注销玩家，并可选择销毁索引非零的 LocalPlayer。 */
	/** Logs a player out of any online systems, and optionally destroys the player entirely if it's not the first one */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToLogOutUser(int32 LocalPlayerIndex, bool bDestroyPlayer = false);

	/** 因错误返回主菜单时，重置用户登录和初始化状态。 */
	/** Resets the login and initialization state when returning to the main menu after an error */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void ResetUserState();

	/** 指定旧式平台用户索引可能代表真实有效身份时返回 true，即使当前未登录。 */
	/** Returns true if this this could be a real platform user with a valid identity (even if not currently logged in)  */
	COMMONUSER_API virtual bool IsRealPlatformUserIndex(int32 PlatformUserIndex) const;

	/** 指定 PlatformUserId 可能代表真实有效身份时返回 true，即使当前未登录。 */
	/** Returns true if this this could be a real platform user with a valid identity (even if not currently logged in) */
	COMMONUSER_API virtual bool IsRealPlatformUser(FPlatformUserId PlatformUser) const;

	/** 将旧式平台用户索引转换为 PlatformUserId。 */
	/** Converts index to id */
	COMMONUSER_API virtual FPlatformUserId GetPlatformUserIdForIndex(int32 PlatformUserIndex) const;

	/** 将 PlatformUserId 转换为旧式平台用户索引。 */
	/** Converts id to index */
	COMMONUSER_API virtual int32 GetPlatformUserIndexForId(FPlatformUserId PlatformUser) const;

	/** 返回当前映射到指定输入设备的平台用户。 */
	/** Gets the user for an input device */
	COMMONUSER_API virtual FPlatformUserId GetPlatformUserIdForInputDevice(FInputDeviceId InputDevice) const;

	/** 返回指定平台用户的主要输入设备。 */
	/** Gets a user's primary input device id */
	COMMONUSER_API virtual FInputDeviceId GetPrimaryInputDeviceForPlatformUser(FPlatformUserId PlatformUser) const;

	/** 平台状态或选项变化时由游戏代码调用，更新缓存的平台特性标签。 */
	/** Call from game code to set the cached trait tags when platform state or options changes */
	COMMONUSER_API virtual void SetTraitTags(const FGameplayTagContainer& InTags);

	/** 返回当前影响功能可用性的特性标签。 */
	/** Gets the current tags that affect feature avialability */
	const FGameplayTagContainer& GetTraitTags() const { return CachedTraitTags; }

	/** 检查指定平台或功能特性标签是否启用。 */
	/** Checks if a specific platform/feature tag is enabled */
	UFUNCTION(BlueprintPure, Category=CommonUser)
	bool HasTraitTag(const FGameplayTag TraitTag) const { return CachedTraitTags.HasTag(TraitTag); }

	/** 判断启动时是否应显示“按键开始”或输入确认页面；游戏也可直接检查特性标签。 */
	/** Checks to see if we should display a press start/input confirmation screen at startup. Games can call this or check the trait tags directly */
	UFUNCTION(BlueprintPure, BlueprintPure, Category=CommonUser)
	COMMONUSER_API virtual bool ShouldWaitForStartInput() const;


	// 访问底层在线系统、身份服务和连接状态的函数。
	// Functions for accessing low-level online system information

#if COMMONUSER_OSSV1
	/** 返回指定上下文的 OSSv1 子系统接口；对应系统不存在时返回空。 */
	/** Returns OSS interface of specific type, will return null if there is no type */
	COMMONUSER_API IOnlineSubsystem* GetOnlineSubsystem(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回指定上下文的 OSSv1 身份接口；对应系统不存在时返回空。 */
	/** Returns identity interface of specific type, will return null if there is no type */
	COMMONUSER_API IOnlineIdentity* GetOnlineIdentity(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回指定 OSSv1 系统的可读名称。 */
	/** Returns human readable name of OSS system */
	COMMONUSER_API FName GetOnlineSubsystemName(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回指定 OSSv1 系统当前的后端连接状态。 */
	/** Returns the current online connection status */
	COMMONUSER_API EOnlineServerConnectionStatus::Type GetConnectionStatus(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
#else
	/** 返回指定上下文的 Online Services 提供方类型；不存在时返回 None。 */
	/** Get the services provider type, or None if there isn't one. */
	COMMONUSER_API UE::Online::EOnlineServices GetOnlineServicesProvider(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
	
	/** 返回指定上下文的 OSSv2 Auth 接口；对应服务不存在时返回空。 */
	/** Returns auth interface of specific type, will return null if there is no type */
	COMMONUSER_API UE::Online::IAuthPtr GetOnlineAuth(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回指定 OSSv2 服务当前的后端连接状态。 */
	/** Returns the current online connection status */
	COMMONUSER_API UE::Online::EOnlineServicesConnectionStatus GetConnectionStatus(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
#endif

	/** 指定在线上下文当前已连接后端服务器时返回 true。 */
	/** Returns true if we are currently connected to backend servers */
	COMMONUSER_API bool HasOnlineConnection(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回真实平台用户在指定在线系统上的当前登录状态。 */
	/** Returns the current login status for a player on the specified online system, only works for real platform users */
	COMMONUSER_API ELoginStatusType GetLocalUserLoginStatus(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回本地平台用户在指定上下文中的 UniqueNetId。 */
	/** Returns the unique net id for a local platform user */
	COMMONUSER_API FUniqueNetIdRepl GetLocalUserNetId(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回本地平台用户在 CommonUserInfo 中缓存的昵称。 */
	/** Returns the nickname for a local platform user, this is cached in common user Info */
	COMMONUSER_API FString GetLocalUserNickname(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 将 PlatformUserId 转换为调试字符串。 */
	/** Convert a user id to a debug string */
	COMMONUSER_API FString PlatformUserIdToString(FPlatformUserId UserId);

	/** 将 CommonUser 在线上下文转换为调试字符串。 */
	/** Convert a context to a debug string */
	COMMONUSER_API FString ECommonUserOnlineContextToString(ECommonUserOnlineContext Context);

	/** 返回权限类型和权限结果的可读本地化描述。 */
	/** Returns human readable string for privilege checks */
	COMMONUSER_API virtual FText GetPrivilegeDescription(ECommonUserPrivilege Privilege) const;
	COMMONUSER_API virtual FText GetPrivilegeResultDescription(ECommonUserPrivilegeResult Result) const;

	/**
	 * 为现有本地用户启动底层登录流程；未能安排完成回调时返回 false。
	 * 该接口只驱动底层状态机，不直接修改 CommonUserInfo 的 InitializationState。
	 */
	/** 
	 * Starts the process of login for an existing local user, will return false if callback was not scheduled 
	 * This activates the low level state machine and does not modify the initialization state on user info
	 */
	DECLARE_DELEGATE_FiveParams(FOnLocalUserLoginCompleteDelegate, const UCommonUserInfo* /*UserInfo*/, ELoginStatusType /*NewStatus*/, FUniqueNetIdRepl /*NetId*/, const TOptional<FOnlineErrorType>& /*Error*/, ECommonUserOnlineContext /*Type*/);
	COMMONUSER_API virtual bool LoginLocalUser(const UCommonUserInfo* UserInfo, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext Context, FOnLocalUserLoginCompleteDelegate OnComplete);

	/** 将 LocalPlayer 绑定到指定逻辑用户，并按需要触发映射变化回调。 */
	/** Assign a local player to a specific local user and call callbacks as needed */
	COMMONUSER_API virtual void SetLocalPlayerUserInfo(ULocalPlayer* LocalPlayer, const UCommonUserInfo* UserInfo);

	/** 将具有聚合或默认语义的上下文解析为具体在线系统上下文。 */
	/** Resolves a context that has default behavior into a specific context */
	COMMONUSER_API ECommonUserOnlineContext ResolveOnlineContext(ECommonUserOnlineContext Context) const;

	/** 平台系统与外部服务使用不同接口时返回 true。 */
	/** True if there is a separate platform and service interface */
	COMMONUSER_API bool HasSeparatePlatformContext() const;

protected:
	/** 缓存每个在线上下文的接口指针、事件句柄和连接状态。 */
	/** Internal structure that caches status and pointers for each online context */
	struct FOnlineContextCache
	{
#if COMMONUSER_OSSV1
		/** OSSv1 基础子系统指针，在 GameInstance 生命周期内保持有效。 */
		/** Pointer to base subsystem, will stay valid as long as game instance does */
		IOnlineSubsystem* OnlineSubsystem = nullptr;

		/** 缓存的 OSSv1 身份接口，完成上下文创建后始终有效。 */
		/** Cached identity system, this will always be valid */
		IOnlineIdentityPtr IdentityInterface;

		/** 最近一次传给 HandleNetworkConnectionStatusChanged 的连接状态。 */
		/** Last connection status that was passed into the HandleNetworkConnectionStatusChanged hander */
		EOnlineServerConnectionStatus::Type	CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;
#else
		/** OSSv2 OnlineServices 聚合接口，用于访问具体服务。 */
		/** Online services, accessor to specific services */
		UE::Online::IOnlineServicesPtr OnlineServices;
		/** 缓存的 OSSv2 Auth 服务。 */
		/** Cached auth service */
		UE::Online::IAuthPtr AuthService;
		/** 登录状态变化事件绑定句柄。 */
		/** Login status changed event handle */
		UE::Online::FOnlineEventDelegateHandle LoginStatusChangedHandle;
		/** 后端连接状态变化事件绑定句柄。 */
		/** Connection status changed event handle */
		UE::Online::FOnlineEventDelegateHandle ConnectionStatusChangedHandle;
		/** 最近一次传给 HandleNetworkConnectionStatusChanged 的连接状态。 */
		/** Last connection status that was passed into the HandleNetworkConnectionStatusChanged hander */
		UE::Online::EOnlineServicesConnectionStatus CurrentConnectionStatus = UE::Online::EOnlineServicesConnectionStatus::NotConnected;
#endif

		/** 重置上下文状态并释放全部共享接口指针。 */
		/** Resets state, important to clear all shared ptrs */
		void Reset()
		{
#if COMMONUSER_OSSV1
			OnlineSubsystem = nullptr;
			IdentityInterface.Reset();
			CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;
#else
			OnlineServices.Reset();
			AuthService.Reset();
			CurrentConnectionStatus = UE::Online::EOnlineServicesConnectionStatus::NotConnected;
#endif
		}
	};

	/** 表示一个进行中用户登录状态机请求的内部结构。 */
	/** Internal structure to represent an in-progress login request */
	struct FUserLoginRequest : public TSharedFromThis<FUserLoginRequest>
	{
		FUserLoginRequest(UCommonUserInfo* InUserInfo, ECommonUserPrivilege InPrivilege, ECommonUserOnlineContext InContext, FOnLocalUserLoginCompleteDelegate&& InDelegate)
			: UserInfo(TWeakObjectPtr<UCommonUserInfo>(InUserInfo))
			, DesiredPrivilege(InPrivilege)
			, DesiredContext(InContext)
			, Delegate(MoveTemp(InDelegate))
			{}

		/** 正在尝试登录的本地用户。 */
		/** Which local user is trying to log on */
		TWeakObjectPtr<UCommonUserInfo> UserInfo;

		/** 登录请求的总体状态，由多个候选登录来源共同推进。 */
		/** Overall state of login request, could come from many sources */
		ECommonUserAsyncTaskState OverallLoginState = ECommonUserAsyncTaskState::NotStarted;

		/** 平台凭据转移尝试状态；OSSv1 不支持该路径，开始后会立即转为 Failed。 */
		/** State of attempt to use platform auth. When started, this immediately transitions to Failed for OSSv1, as we do not support platform auth there. */
		ECommonUserAsyncTaskState TransferPlatformAuthState = ECommonUserAsyncTaskState::NotStarted;

		/** AutoLogin 尝试状态。 */
		/** State of attempt to use AutoLogin */
		ECommonUserAsyncTaskState AutoLoginState = ECommonUserAsyncTaskState::NotStarted;

		/** 外部登录 UI 尝试状态。 */
		/** State of attempt to use external login UI */
		ECommonUserAsyncTaskState LoginUIState = ECommonUserAsyncTaskState::NotStarted;

		/** 本次登录最终必须满足的权限。 */
		/** Final privilege to that is requested */
		ECommonUserPrivilege DesiredPrivilege = ECommonUserPrivilege::Invalid_Count;

		/** 目标权限查询的执行状态。 */
		/** State of attempt to request the relevant privilege */
		ECommonUserAsyncTaskState PrivilegeCheckState = ECommonUserAsyncTaskState::NotStarted;

		/** 请求最终要登录的在线上下文。 */
		/** The final context to log into */
		ECommonUserOnlineContext DesiredContext = ECommonUserOnlineContext::Invalid;

		/** 状态机当前正在处理的具体在线系统上下文。 */
		/** What online system we are currently logging into */
		ECommonUserOnlineContext CurrentContext = ECommonUserOnlineContext::Invalid;

		/** 登录流程结束时执行的调用方回调。 */
		/** User callback for completion */
		FOnLocalUserLoginCompleteDelegate Delegate;

		/** 最近且最适合向用户展示的登录错误。 */
		/** Most recent/relevant error to display to user */
		TOptional<FOnlineErrorType> Error;
	};


	/** 为指定 LocalPlayer 索引创建新的 CommonUserInfo。 */
	/** Create a new user info object */
	COMMONUSER_API virtual UCommonUserInfo* CreateLocalUserInfo(int32 LocalPlayerIndex);

	/** 供内部状态更新使用的去 const 包装器。 */
	/** Deconst wrapper for const getters */
	FORCEINLINE UCommonUserInfo* ModifyInfo(const UCommonUserInfo* Info) { return const_cast<UCommonUserInfo*>(Info); }

	/** 从在线系统刷新用户 NetId、昵称和登录相关缓存。 */
	/** Refresh user info from OSS */
	COMMONUSER_API virtual void RefreshLocalUserInfo(UCommonUserInfo* UserInfo);

	/** 比较权限当前可用性与旧缓存，并在变化时发送通知。 */
	/** Possibly send privilege availability notification, compares current value to cached old value */
	COMMONUSER_API virtual void HandleChangedAvailability(UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserAvailability OldAvailability);

	/** 更新用户缓存权限结果，并在总体可用性变化时通知委托。 */
	/** Updates the cached privilege on a user and notifies delegate */
	COMMONUSER_API virtual void UpdateUserPrivilegeResult(UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult Result, ECommonUserOnlineContext Context);

	/** 返回指定在线系统类型的内部上下文缓存；外部服务不存在时可为空。 */
	/** Gets internal data for a type of online system, can return null for service */
	COMMONUSER_API const FOnlineContextCache* GetContextCache(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
	COMMONUSER_API FOnlineContextCache* GetContextCache(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game);

	/** 在绑定在线事件前创建并配置各在线上下文接口对象。 */
	/** Create and set up system objects before delegates are bound */
	COMMONUSER_API virtual void CreateOnlineContexts();
	COMMONUSER_API virtual void DestroyOnlineContexts();

	/** 为已创建的在线上下文绑定登录、连接和输入设备事件。 */
	/** Bind online delegates */
	COMMONUSER_API virtual void BindOnlineDelegates();

	/** 强制注销并反初始化指定平台用户。 */
	/** Forcibly logs out and deinitializes a single user */
	COMMONUSER_API virtual void LogOutLocalUser(FPlatformUserId PlatformUser);

	/** 推进登录状态机的下一步，可能直接完成请求；请求结束时返回 true。 */
	/** Performs the next step of a login request, which could include completing it. Returns true if it's done */
	COMMONUSER_API virtual void ProcessLoginRequest(TSharedRef<FUserLoginRequest> Request);

	/** 使用平台 OSS 提供的凭据登录目标服务；成功启动凭据转移时返回 true。 */
	/** Call login on OSS, with platform auth from the platform OSS. Return true if AutoLogin started */
	COMMONUSER_API virtual bool TransferPlatformAuth(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 调用 OSS AutoLogin；异步登录成功启动时返回 true。 */
	/** Call AutoLogin on OSS. Return true if AutoLogin started. */
	COMMONUSER_API virtual bool AutoLogin(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 调用 OSS 外部登录 UI；异步 UI 流程成功启动时返回 true。 */
	/** Call ShowLoginUI on OSS. Return true if ShowLoginUI started. */
	COMMONUSER_API virtual bool ShowLoginUI(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 调用 OSS 用户权限查询；异步查询成功启动时返回 true。 */
	/** Call QueryUserPrivilege on OSS. Return true if QueryUserPrivilege started. */
	COMMONUSER_API virtual bool QueryUserPrivilege(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 按 OSSv1 或 OSSv2 分离的权限转换和登录实现。 */
	/** OSS-specific functions */
#if COMMONUSER_OSSV1
	COMMONUSER_API virtual ECommonUserPrivilege ConvertOSSPrivilege(EUserPrivileges::Type Privilege) const;
	COMMONUSER_API virtual EUserPrivileges::Type ConvertOSSPrivilege(ECommonUserPrivilege Privilege) const;
	COMMONUSER_API virtual ECommonUserPrivilegeResult ConvertOSSPrivilegeResult(EUserPrivileges::Type Privilege, uint32 Results) const;

	COMMONUSER_API void BindOnlineDelegatesOSSv1();
	COMMONUSER_API bool AutoLoginOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool ShowLoginUIOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool QueryUserPrivilegeOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
#else
	COMMONUSER_API virtual ECommonUserPrivilege ConvertOnlineServicesPrivilege(UE::Online::EUserPrivileges Privilege) const;
	COMMONUSER_API virtual UE::Online::EUserPrivileges ConvertOnlineServicesPrivilege(ECommonUserPrivilege Privilege) const;
	COMMONUSER_API virtual ECommonUserPrivilegeResult ConvertOnlineServicesPrivilegeResult(UE::Online::EUserPrivileges Privilege, UE::Online::EPrivilegeResults Results) const;

	COMMONUSER_API void BindOnlineDelegatesOSSv2();
	COMMONUSER_API void CacheConnectionStatus(ECommonUserOnlineContext Context);
	COMMONUSER_API bool TransferPlatformAuthOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool AutoLoginOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool ShowLoginUIOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool QueryUserPrivilegeOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API TSharedPtr<UE::Online::FAccountInfo> GetOnlineServiceAccountInfo(UE::Online::IAuthPtr AuthService, FPlatformUserId InUserId) const;
#endif

	/** OSS 登录、连接、权限和外部 UI 操作的完成回调。 */
	/** Callbacks for OSS functions */
#if COMMONUSER_OSSV1
	COMMONUSER_API virtual void HandleIdentityLoginStatusChanged(int32 PlatformUserIndex, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleUserLoginCompleted(int32 PlatformUserIndex, bool bWasSuccessful, const FUniqueNetId& NetId, const FString& Error, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleControllerPairingChanged(int32 PlatformUserIndex, FControllerPairingChangedUserInfo PreviousUser, FControllerPairingChangedUserInfo NewUser);
	COMMONUSER_API virtual void HandleNetworkConnectionStatusChanged(const FString& ServiceName, EOnlineServerConnectionStatus::Type LastConnectionStatus, EOnlineServerConnectionStatus::Type ConnectionStatus, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleOnLoginUIClosed(TSharedPtr<const FUniqueNetId> LoggedInNetId, const int PlatformUserIndex, const FOnlineError& Error, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleCheckPrivilegesComplete(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 PrivilegeResults, ECommonUserPrivilege RequestedPrivilege, TWeakObjectPtr<UCommonUserInfo> CommonUserInfo, ECommonUserOnlineContext Context);
#else
	COMMONUSER_API virtual void HandleAuthLoginStatusChanged(const UE::Online::FAuthLoginStatusChanged& EventParameters, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleUserLoginCompletedV2(const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result, FPlatformUserId PlatformUser, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleOnLoginUIClosedV2(const UE::Online::TOnlineResult<UE::Online::FExternalUIShowLoginUI>& Result, FPlatformUserId PlatformUser, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleNetworkConnectionStatusChanged(const UE::Online::FConnectionStatusChanged& EventParameters, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleCheckPrivilegesComplete(const UE::Online::TOnlineResult<UE::Online::FQueryUserPrivilege>& Result, TWeakObjectPtr<UCommonUserInfo> CommonUserInfo, UE::Online::EUserPrivileges DesiredPrivilege, ECommonUserOnlineContext Context);
#endif

	/**
	 * 输入设备（例如手柄）连接或断开时的回调。
	 */
	/**
	 * Callback for when an input device (i.e. a gamepad) has been connected or disconnected. 
	 */
	COMMONUSER_API virtual void HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId);

	COMMONUSER_API virtual void HandleLoginForUserInitialize(const UCommonUserInfo* UserInfo, ELoginStatusType NewStatus, FUniqueNetIdRepl NetId, const TOptional<FOnlineErrorType>& Error, ECommonUserOnlineContext Context, FCommonUserInitializeParams Params);
	COMMONUSER_API virtual void HandleUserInitializeFailed(FCommonUserInitializeParams Params, FText Error);
	COMMONUSER_API virtual void HandleUserInitializeSucceeded(FCommonUserInitializeParams Params);

	/** 处理“按键开始”和按键触发登录的视口输入覆盖回调。 */
	/** Callback for handling press start/login logic */
	COMMONUSER_API virtual bool OverrideInputKeyForLogin(FInputKeyEventArgs& EventArgs);


	/** 安装登录输入监听前保存的原覆盖处理器，取消时恢复。 */
	/** Previous override handler, will restore on cancel */
	FOverrideInputKeyHandler WrappedInputKeyHandler;

	/** 任意用户均可触发登录的按键列表。 */
	/** List of keys to listen for from any user */
	TArray<FKey> LoginKeysForAnyUser;

	/** 仅未映射新用户可触发登录的按键列表。 */
	/** List of keys to listen for a new unmapped user */
	TArray<FKey> LoginKeysForNewUser;

	/** 检测到登录按键后使用的初始化参数。 */
	/** Params to use for a key-triggered login */
	FCommonUserInitializeParams ParamsForLoginKey;

	/** 当前允许的最大 LocalPlayer 数量。 */
	/** Maximum number of local players */
	int32 MaxNumberOfLocalPlayers = 0;
	
	/** 当前是否为不需要 LocalPlayer 的专用服务器。 */
	/** True if this is a dedicated server, which doesn't require a LocalPlayer */
	bool bIsDedicatedServer = false;

	/** 当前所有进行中的登录状态机请求。 */
	/** List of current in progress login requests */
	TArray<TSharedRef<FUserLoginRequest>> ActiveLoginRequests;

	/** LocalPlayer 索引到用户信息对象的映射。 */
	/** Information about each local user, from local player index to user */
	UPROPERTY()
	TMap<int32, TObjectPtr<UCommonUserInfo>> LocalUserInfos;
	
	/** 缓存的平台和运行模式特性标签。 */
	/** Cached platform/mode trait tags */
	FGameplayTagContainer CachedTraitTags;

	/** 仅在在线上下文初始化和销毁期间访问的内部快捷指针。 */
	/** Do not access this outside of initialization */
	FOnlineContextCache* DefaultContextInternal = nullptr;
	FOnlineContextCache* ServiceContextInternal = nullptr;
	FOnlineContextCache* PlatformContextInternal = nullptr;

	friend UCommonUserInfo;
};
