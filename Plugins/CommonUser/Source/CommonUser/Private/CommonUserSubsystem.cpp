// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonUserSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputKeyEventArgs.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUserSubsystem)

#if COMMONUSER_OSSV1
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#else
#include "Online/Auth.h"
#include "Online/ExternalUI.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesEngineUtils.h"
#include "Online/Privileges.h"

using namespace UE::Online;
#endif

// CommonUser 身份、登录状态机、权限和设备映射流程使用的日志分类。
DECLARE_LOG_CATEGORY_EXTERN(LogCommonUser, Log, All);
DEFINE_LOG_CATEGORY(LogCommonUser);

// 定义系统错误、警告和普通展示消息的层级标签。
UE_DEFINE_GAMEPLAY_TAG(FCommonUserTags::SystemMessage_Error, "SystemMessage.Error");
UE_DEFINE_GAMEPLAY_TAG(FCommonUserTags::SystemMessage_Warning, "SystemMessage.Warning");
UE_DEFINE_GAMEPLAY_TAG(FCommonUserTags::SystemMessage_Display, "SystemMessage.Display");
// 定义本地玩家初始化彻底失败时使用的具体错误标签。
UE_DEFINE_GAMEPLAY_TAG(FCommonUserTags::SystemMessage_Error_InitializeLocalPlayerFailed, "SystemMessage.Error.InitializeLocalPlayerFailed");

// 定义影响控制器与系统用户映射方式的平台特性标签。
UE_DEFINE_GAMEPLAY_TAG(FCommonUserTags::Platform_Trait_RequiresStrictControllerMapping, "Platform.Trait.RequiresStrictControllerMapping");
UE_DEFINE_GAMEPLAY_TAG(FCommonUserTags::Platform_Trait_SingleOnlineUser, "Platform.Trait.SingleOnlineUser");


//////////////////////////////////////////////////////////////////////
// UCommonUserInfo

// 优先查找请求上下文的独立缓存；没有直接条目时按子系统规则解析为具体后端上下文再查找。
UCommonUserInfo::FCachedData* UCommonUserInfo::GetCachedData(ECommonUserOnlineContext Context)
{
	// Game 使用独立聚合缓存，因此先按传入上下文直接查找。
	// Look up directly, game has a separate cache than default
	FCachedData* FoundData = CachedDataMap.Find(Context);
	if (FoundData)
	{
		return FoundData;
	}

	// 没有直接缓存时，使用子系统的 Default、Service 或 Platform 回退规则解析上下文。
	// Now try system resolution
	UCommonUserSubsystem* Subsystem = GetSubsystem();

	ECommonUserOnlineContext ResolvedContext = Subsystem->ResolveOnlineContext(Context);
	return CachedDataMap.Find(ResolvedContext);
}

// const 查询复用可修改版本的上下文解析逻辑，只以只读指针返回结果。
const UCommonUserInfo::FCachedData* UCommonUserInfo::GetCachedData(ECommonUserOnlineContext Context) const
{
	return const_cast<UCommonUserInfo*>(this)->GetCachedData(Context);
}

// 更新具体后端权限缓存，并将 Game 聚合结果设为相关上下文中更严格的结果。
void UCommonUserInfo::UpdateCachedPrivilegeResult(ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult Result, ECommonUserOnlineContext Context)
{
	// 调用方必须传入已经解析且有效的具体在线上下文。
	// This should only be called with a resolved and valid type
	FCachedData* GameCache = GetCachedData(ECommonUserOnlineContext::Game);
	FCachedData* ContextCache = GetCachedData(Context);

	if (!ensure(GameCache && ContextCache))
	{
		// Game 与目标上下文缓存按初始化约定应始终存在。
		// Should always be valid
		return;
	}

	// 先更新本次查询对应的具体上下文缓存。
	// Update direct cache first
	ContextCache->CachedPrivileges.Add(Privilege, Result);

	if (GameCache != ContextCache)
	{
		// 查找另一个具体上下文，将较差权限结果合并到 Game 聚合缓存。
		// Look for another context to merge into game
		ECommonUserPrivilegeResult GameContextResult = Result;
		ECommonUserPrivilegeResult OtherContextResult = ECommonUserPrivilegeResult::Available;
		for (TPair<ECommonUserOnlineContext, FCachedData>& Pair : CachedDataMap)
		{
			if (&Pair.Value != ContextCache && &Pair.Value != GameCache)
			{
				ECommonUserPrivilegeResult* FoundResult = Pair.Value.CachedPrivileges.Find(Privilege);
				if (FoundResult)
				{
					OtherContextResult = *FoundResult;
				}
				else
				{
					OtherContextResult = ECommonUserPrivilegeResult::Unknown;
				}
				break;
			}
		}

		if (GameContextResult == ECommonUserPrivilegeResult::Available && OtherContextResult != ECommonUserPrivilegeResult::Available)
		{
			// 另一上下文限制更严格时，以其结果作为 Game 聚合结果。
			// Other context is worse, use that
			GameContextResult = OtherContextResult;
		}

		GameCache->CachedPrivileges.Add(Privilege, GameContextResult);
	}
}

// 更新指定上下文的 NetId，并为访客生成默认昵称或用在线系统昵称覆盖真实用户缓存。
void UCommonUserInfo::UpdateCachedNetId(const FUniqueNetIdRepl& NewId, ECommonUserOnlineContext Context)
{
	FCachedData* ContextCache = GetCachedData(Context);

	if (ensure(ContextCache))
	{
		ContextCache->CachedNetId = NewId;

		// NetId 变化时同步刷新该上下文昵称。
		// Update the nickname
		const UCommonUserSubsystem* Subsystem = GetSubsystem();
		if (ensure(Subsystem))
		{
			if (bIsGuest)
			{
				if (ContextCache->CachedNickname.IsEmpty())
				{
					// 访客昵称为空时设置默认值，之后仍可通过 SetNickname 修改。
					// Set a default guest name if it is empty, can be changed with SetNickname
					ContextCache->CachedNickname = NSLOCTEXT("CommonUser", "GuestNickname", "Guest").ToString();
				}
			}
			else
			{
				// 真实用户使用平台或服务昵称刷新，并覆盖手动 SetNickname 值。
				// Refresh with the system nickname, overrides SetNickname
				ContextCache->CachedNickname = Subsystem->GetLocalUserNickname(GetPlatformUserId(), Context);
			}
		}
	}

	// 访客会共享主 PlatformUser，因此不同上下文的 NetId 不合并到 Game 缓存。
	// We don't merge the ids because of how guests work
}

// 从 Outer 取得拥有当前用户信息的 CommonUserSubsystem。
class UCommonUserSubsystem* UCommonUserInfo::GetSubsystem() const
{
	return Cast<UCommonUserSubsystem>(GetOuter());
}

// 用户处于仅本地登录或完整在线登录状态时返回 true。
bool UCommonUserInfo::IsLoggedIn() const
{
	return (InitializationState == ECommonUserInitializationState::LoggedInLocalOnly || InitializationState == ECommonUserInitializationState::LoggedInOnline);
}

// 用户处于初始本地登录或网络登录阶段时返回 true。
bool UCommonUserInfo::IsDoingLogin() const
{
	return (InitializationState == ECommonUserInitializationState::DoingInitialLogin || InitializationState == ECommonUserInitializationState::DoingNetworkLogin);
}

// 返回指定上下文缓存的权限结果；缺少上下文或从未查询时返回 Unknown。
ECommonUserPrivilegeResult UCommonUserInfo::GetCachedPrivilegeResult(ECommonUserPrivilege Privilege, ECommonUserOnlineContext Context) const
{
	const FCachedData* FoundCached = GetCachedData(Context);

	if (FoundCached)
	{
		const ECommonUserPrivilegeResult* FoundResult = FoundCached->CachedPrivileges.Find(Privilege);
		if (FoundResult)
		{
			return *FoundResult;
		}
	}
	return ECommonUserPrivilegeResult::Unknown;
}

// 综合硬性权限失败、访客限制、网络连接、初始化阶段和缓存结果，计算功能当前或未来可用性。
ECommonUserAvailability UCommonUserInfo::GetPrivilegeAvailability(ECommonUserPrivilege Privilege) const
{
	// 无效权限或无效用户状态不能参与可用性计算。
	// Bad feature or user
	if ((int32)Privilege < 0 || (int32)Privilege >= (int32)ECommonUserPrivilege::Invalid_Count || InitializationState == ECommonUserInitializationState::Invalid)
	{
		return ECommonUserAvailability::Invalid;
	}

	ECommonUserPrivilegeResult CachedResult = GetCachedPrivilegeResult(Privilege, ECommonUserOnlineContext::Game);

	// 先将明确失败原因分类为永久不可用或当前不可用。
	// First handle explicit failures
	switch (CachedResult)
	{
	case ECommonUserPrivilegeResult::LicenseInvalid:
	case ECommonUserPrivilegeResult::VersionOutdated:
	case ECommonUserPrivilegeResult::AgeRestricted:
		return ECommonUserAvailability::AlwaysUnavailable;

	case ECommonUserPrivilegeResult::NetworkConnectionUnavailable:
	case ECommonUserPrivilegeResult::AccountTypeRestricted:
	case ECommonUserPrivilegeResult::AccountUseRestricted:
	case ECommonUserPrivilegeResult::PlatformFailure:
		return ECommonUserAvailability::CurrentlyUnavailable;

	default:
		break;
	}

	if (bIsGuest)
	{
		// 访客只能进行本地游戏，不能使用在线能力。
		// Guests can only play, cannot use online features
		if (Privilege == ECommonUserPrivilege::CanPlay)
		{
			return ECommonUserAvailability::NowAvailable;
		}
		else
		{
			return ECommonUserAvailability::AlwaysUnavailable;
		}
	}

	// 依赖在线连接的权限在后端断开时暂时不可用。
	// Check network status
	if (Privilege == ECommonUserPrivilege::CanPlayOnline ||
		Privilege == ECommonUserPrivilege::CanUseCrossPlay ||
		Privilege == ECommonUserPrivilege::CanCommunicateViaTextOnline ||
		Privilege == ECommonUserPrivilege::CanCommunicateViaVoiceOnline)
	{
		UCommonUserSubsystem* Subsystem = GetSubsystem();
		if (ensure(Subsystem) && !Subsystem->HasOnlineConnection(ECommonUserOnlineContext::Game))
		{
			return ECommonUserAvailability::CurrentlyUnavailable;
		}
	}

	if (InitializationState == ECommonUserInitializationState::FailedtoLogin)
	{
		// 先前登录失败，功能当前不可用但后续可以重试。
		// Failed a prior login attempt
		return ECommonUserAvailability::CurrentlyUnavailable;
	}
	else if (InitializationState == ECommonUserInitializationState::Unknown || InitializationState == ECommonUserInitializationState::DoingInitialLogin)
	{
		// 尚未完成本地登录，正常登录后可能可用。
		// Haven't logged in yet
		return ECommonUserAvailability::PossiblyAvailable;
	}
	else if (InitializationState == ECommonUserInitializationState::LoggedInLocalOnly || InitializationState == ECommonUserInitializationState::DoingNetworkLogin)
	{
		// 本地登录已成功，因此 CanPlay 的缓存结果已经有效。
		// Local login succeeded so play checks are valid
		if (Privilege == ECommonUserPrivilege::CanPlay && CachedResult == ECommonUserPrivilegeResult::Available)
		{
			return ECommonUserAvailability::NowAvailable;
		}

		// 在线登录尚未完成，在线权限仍属于可能可用。
		// Haven't logged in online yet
		return ECommonUserAvailability::PossiblyAvailable;
	}
	else if (InitializationState == ECommonUserInitializationState::LoggedInOnline)
	{
		// 完整在线登录后直接依据缓存权限结果判断。
		// Fully logged in
		if (CachedResult == ECommonUserPrivilegeResult::Available)
		{
			return ECommonUserAvailability::NowAvailable;
		}

		// 在线登录完成但权限未通过，功能当前不可用。
		// Failed for other reason
		return ECommonUserAvailability::CurrentlyUnavailable;
	}

	return ECommonUserAvailability::Unknown;
}

// 返回指定上下文缓存的 NetId，无法解析缓存时返回无效标识。
FUniqueNetIdRepl UCommonUserInfo::GetNetId(ECommonUserOnlineContext Context) const
{
	const FCachedData* FoundCached = GetCachedData(Context);

	if (FoundCached)
	{
		return FoundCached->CachedNetId;
	}

	return FUniqueNetIdRepl();
}

// 返回指定上下文缓存的昵称；没有缓存时当前返回空字符串。
FString UCommonUserInfo::GetNickname(ECommonUserOnlineContext Context) const
{
	const FCachedData* FoundCached = GetCachedData(Context);

	if (FoundCached)
	{
		return FoundCached->CachedNickname;
	}

	// TODO：评估无缓存时是否应返回“未知用户”等可读占位文本。
	// TODO maybe return unknown user here?
	return FString();
}

// 直接修改指定上下文的缓存昵称；真实用户下次刷新 NetId 时可能被在线昵称覆盖。
void UCommonUserInfo::SetNickname(const FString& NewNickname, ECommonUserOnlineContext Context)
{
	FCachedData* ContextCache = GetCachedData(Context);

	if (ensure(ContextCache))
	{
		ContextCache->CachedNickname = NewNickname;
	}
}

// 返回 Game 上下文 NetId 的调试字符串。
FString UCommonUserInfo::GetDebugString() const
{
	FUniqueNetIdRepl NetId = GetNetId();
	return NetId.ToDebugString();
}

// 返回该逻辑用户关联的平台用户标识。
FPlatformUserId UCommonUserInfo::GetPlatformUserId() const
{
	return PlatformUser;
}

// 通过拥有者子系统把 PlatformUserId 转换为旧式整数索引，失败时返回 INDEX_NONE。
int32 UCommonUserInfo::GetPlatformUserIndex() const
{
	// 将平台用户标识转换为旧接口使用的索引。
	// Convert our platform id to index
	const UCommonUserSubsystem* Subsystem = GetSubsystem();

	if (ensure(Subsystem))
	{
		return Subsystem->GetPlatformUserIndexForId(PlatformUser);
	}

	return INDEX_NONE;
}


//////////////////////////////////////////////////////////////////////
// UCommonUserSubsystem

// 创建在线上下文、绑定在线和设备事件、设置引擎默认本地玩家上限，并初始化主用户状态。
void UCommonUserSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 创建 Default、Platform 和可选 Service 的 OSS 上下文包装器。
	// Create our OSS wrappers
	CreateOnlineContexts();

	BindOnlineDelegates();
	
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceMapper.GetOnInputDeviceConnectionChange().AddUObject(this, &ThisClass::HandleInputDeviceConnectionChanged);

	// 使用与引擎一致的默认最大本地玩家数。
	// Matches the engine default
	SetMaxLocalPlayers(4);

	ResetUserState();

	UGameInstance* GameInstance = GetGameInstance();
	bIsDedicatedServer = GameInstance->IsDedicatedServerInstance();
}

// 创建 Default 在线上下文，并在平台后端独立时额外创建 Platform 上下文；Service 留给项目后续配置。
void UCommonUserSubsystem::CreateOnlineContexts()
{
	// 首先初始化始终存在的默认在线系统上下文。
	// First initialize default
	DefaultContextInternal = new FOnlineContextCache();
#if COMMONUSER_OSSV1
	DefaultContextInternal->OnlineSubsystem = Online::GetSubsystem(GetWorld());
	check(DefaultContextInternal->OnlineSubsystem);
	DefaultContextInternal->IdentityInterface = DefaultContextInternal->OnlineSubsystem->GetIdentityInterface();
	check(DefaultContextInternal->IdentityInterface.IsValid());

	IOnlineSubsystem* PlatformSub = IOnlineSubsystem::GetByPlatform();

	if (PlatformSub && DefaultContextInternal->OnlineSubsystem != PlatformSub)
	{
		// 平台系统与默认系统不同时创建可选 Platform 上下文。
		// Set up the optional platform service if it exists
		PlatformContextInternal = new FOnlineContextCache();
		PlatformContextInternal->OnlineSubsystem = PlatformSub;
		PlatformContextInternal->IdentityInterface = PlatformSub->GetIdentityInterface();
		check(PlatformContextInternal->IdentityInterface.IsValid());
	}
#else
	DefaultContextInternal->OnlineServices = GetServices(GetWorld(), EOnlineServices::Default);
	check(DefaultContextInternal->OnlineServices);
	DefaultContextInternal->AuthService = DefaultContextInternal->OnlineServices->GetAuthInterface();
	check(DefaultContextInternal->AuthService);

	UE::Online::IOnlineServicesPtr PlatformServices = GetServices(GetWorld(), EOnlineServices::Platform);
	if (PlatformServices && DefaultContextInternal->OnlineServices != PlatformServices)
	{
		PlatformContextInternal = new FOnlineContextCache();
		PlatformContextInternal->OnlineServices = PlatformServices;
		PlatformContextInternal->AuthService = PlatformContextInternal->OnlineServices->GetAuthInterface();
		check(PlatformContextInternal->AuthService);
	}
#endif

	// 如项目需要独立外部服务，可在此后创建 Service 上下文。
	// Explicit external services can be set up after if needed
}

// 销毁在线上下文、解除输入设备事件并释放用户与活动登录请求后反初始化子系统。
void UCommonUserSubsystem::Deinitialize()
{
	DestroyOnlineContexts();

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceMapper.GetOnInputDeviceConnectionChange().RemoveAll(this);

	LocalUserInfos.Reset();
	ActiveLoginRequests.Reset();

	Super::Deinitialize();
}

// 删除各独立在线上下文并清空内部快捷指针，确保其中共享接口引用在子系统关闭时释放。
void UCommonUserSubsystem::DestroyOnlineContexts()
{
	// 所有缓存共享指针都必须在这里随上下文销毁。
	// All cached shared ptrs must be cleared here
	if (ServiceContextInternal && ServiceContextInternal != DefaultContextInternal)
	{
		delete ServiceContextInternal;
	}
	if (PlatformContextInternal && PlatformContextInternal != DefaultContextInternal)
	{
		delete PlatformContextInternal;
	}
	if (DefaultContextInternal)
	{
		delete DefaultContextInternal;
	}

	ServiceContextInternal = PlatformContextInternal = DefaultContextInternal = nullptr;
}

// 为未占用的 LocalPlayer 索引创建用户信息及 Game、Default 和可选 Platform 缓存，并加入索引表。
UCommonUserInfo* UCommonUserSubsystem::CreateLocalUserInfo(int32 LocalPlayerIndex)
{
	UCommonUserInfo* NewUser = nullptr;
	if (ensure(!LocalUserInfos.Contains(LocalPlayerIndex)))
	{
		NewUser = NewObject<UCommonUserInfo>(this);
		NewUser->LocalPlayerIndex = LocalPlayerIndex;
		NewUser->InitializationState = ECommonUserInitializationState::Unknown;

		// Game 聚合缓存和 Default 具体缓存始终存在。
		// Always create game and default cache
		NewUser->CachedDataMap.Add(ECommonUserOnlineContext::Game, UCommonUserInfo::FCachedData());
		NewUser->CachedDataMap.Add(ECommonUserOnlineContext::Default, UCommonUserInfo::FCachedData());

		// 平台后端独立时增加 Platform 具体缓存。
		// Add platform if needed
		if (HasSeparatePlatformContext())
		{
			NewUser->CachedDataMap.Add(ECommonUserOnlineContext::Platform, UCommonUserInfo::FCachedData());
		}

		LocalUserInfos.Add(LocalPlayerIndex, NewUser);
	}
	return NewUser;
}

// 仅在不存在游戏专用派生类时创建基础 CommonUserSubsystem。
bool UCommonUserSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// 游戏专用派生子系统存在时由其替代基础实例。
	// Only create an instance if there is not a game-specific subclass
	return ChildClasses.Num() == 0;
}

// 按编译时 OSS 版本绑定登录、连接和用户身份事件。
void UCommonUserSubsystem::BindOnlineDelegates()
{
#if COMMONUSER_OSSV1
	return BindOnlineDelegatesOSSv1();
#else
	return BindOnlineDelegatesOSSv2();
#endif
}

// 对已完成登录且不在登录中的用户，将初始化状态标记为失败并广播 CanPlay 可用性变化；不直接调用 OSS Logout。
void UCommonUserSubsystem::LogOutLocalUser(FPlatformUserId PlatformUser)
{
	UCommonUserInfo* UserInfo = ModifyInfo(GetUserInfoForPlatformUser(PlatformUser));

	// 从未完成登录或仍在登录中的用户不执行此状态降级。
	// Don't need to do anything if the user has never logged in fully or is in the process of logging in
	if (UserInfo && (UserInfo->InitializationState == ECommonUserInitializationState::LoggedInLocalOnly || UserInfo->InitializationState == ECommonUserInitializationState::LoggedInOnline))
	{
		ECommonUserAvailability OldAvailablity = UserInfo->GetPrivilegeAvailability(ECommonUserPrivilege::CanPlay);

		UserInfo->InitializationState = ECommonUserInitializationState::FailedtoLogin;

		// 可用性变化处理会广播游戏层权限委托。
		// This will broadcast the game delegate
		HandleChangedAvailability(UserInfo, ECommonUserPrivilege::CanPlay, OldAvailablity);
	}
}

// 尝试把平台身份凭据转移给目标在线服务；OSSv1 不支持，OSSv2 分派到对应实现。
bool UCommonUserSubsystem::TransferPlatformAuth(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
#if COMMONUSER_OSSV1
	// OSSv1 路径不支持平台凭据转移。
	// Not supported in V1 path
	return false;
#else
	return TransferPlatformAuthOSSv2(System, Request, PlatformUser);
#endif
}

// 记录请求并按 OSS 版本尝试无需 UI 的 AutoLogin，返回是否成功安排异步登录。
bool UCommonUserSubsystem::AutoLogin(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	UE_LOG(LogCommonUser, Log, TEXT("Player AutoLogin requested - UserIdx:%d, Privilege:%d, Context:%d"),
		PlatformUser.GetInternalId(),
		(int32)Request->DesiredPrivilege,
		(int32)Request->DesiredContext);

#if COMMONUSER_OSSV1
	return AutoLoginOSSv1(System, Request, PlatformUser);
#else
	return AutoLoginOSSv2(System, Request, PlatformUser);
#endif
}

// 记录请求并按 OSS 版本显示外部登录 UI，返回是否成功安排异步 UI 流程。
bool UCommonUserSubsystem::ShowLoginUI(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	UE_LOG(LogCommonUser, Log, TEXT("Player LoginUI requested - UserIdx:%d, Privilege:%d, Context:%d"),
		PlatformUser.GetInternalId(),
		(int32)Request->DesiredPrivilege,
		(int32)Request->DesiredContext);

#if COMMONUSER_OSSV1
	return ShowLoginUIOSSv1(System, Request, PlatformUser);
#else
	return ShowLoginUIOSSv2(System, Request, PlatformUser);
#endif
}

// 按 OSS 版本查询登录请求要求的最终用户权限，返回是否成功安排异步查询。
bool UCommonUserSubsystem::QueryUserPrivilege(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
#if COMMONUSER_OSSV1
	return QueryUserPrivilegeOSSv1(System, Request, PlatformUser);
#else
	return QueryUserPrivilegeOSSv2(System, Request, PlatformUser);
#endif
}


#if COMMONUSER_OSSV1
// OSSv1 下返回解析后上下文的底层 OnlineSubsystem，不存在时返回空。
IOnlineSubsystem* UCommonUserSubsystem::GetOnlineSubsystem(ECommonUserOnlineContext Context) const
{
	const FOnlineContextCache* System = GetContextCache(Context);

	if (System)
	{
		return System->OnlineSubsystem;
	}

	return nullptr;
}

// OSSv1 下返回解析后上下文的 IdentityInterface 裸指针，不存在时返回空。
IOnlineIdentity* UCommonUserSubsystem::GetOnlineIdentity(ECommonUserOnlineContext Context) const
{
	const FOnlineContextCache* System = GetContextCache(Context);
	if (System)
	{
		return System->IdentityInterface.Get();
	}

	return nullptr;
}

// OSSv1 下返回在线子系统名称，无法解析时返回 NAME_None。
FName UCommonUserSubsystem::GetOnlineSubsystemName(ECommonUserOnlineContext Context) const
{
	IOnlineSubsystem* SubSystem = GetOnlineSubsystem(Context);
	if (SubSystem)
	{
		return SubSystem->GetSubsystemName();
	}

	return NAME_None;
}

// OSSv1 下返回缓存的后端连接状态，无上下文时按 ServiceUnavailable 处理。
EOnlineServerConnectionStatus::Type UCommonUserSubsystem::GetConnectionStatus(ECommonUserOnlineContext Context) const
{
	const FOnlineContextCache* System = GetContextCache(Context);
	if (System)
	{
		return System->CurrentConnectionStatus;
	}

	return EOnlineServerConnectionStatus::ServiceUnavailable;
}

// OSSv1 下为 Service 和可选独立 Platform 系统绑定连接、登录状态、登录完成及平台控制器配对事件。
void UCommonUserSubsystem::BindOnlineDelegatesOSSv1()
{
	ECommonUserOnlineContext ServiceType = ResolveOnlineContext(ECommonUserOnlineContext::ServiceOrDefault);
	ECommonUserOnlineContext PlatformType = ResolveOnlineContext(ECommonUserOnlineContext::PlatformOrDefault);
	FOnlineContextCache* ServiceContext = GetContextCache(ServiceType);
	FOnlineContextCache* PlatformContext = GetContextCache(PlatformType);
	check(ServiceContext && ServiceContext->OnlineSubsystem && PlatformContext && PlatformContext->OnlineSubsystem);
	// 连接状态委托必须同时监听 Service 与独立 Platform 系统。
	// Connection delegates need to listen for both systems

	ServiceContext->OnlineSubsystem->AddOnConnectionStatusChangedDelegate_Handle(FOnConnectionStatusChangedDelegate::CreateUObject(this, &ThisClass::HandleNetworkConnectionStatusChanged, ServiceType));
	ServiceContext->CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;

	for (int32 PlayerIdx = 0; PlayerIdx < MAX_LOCAL_PLAYERS; PlayerIdx++)
	{
		ServiceContext->IdentityInterface->AddOnLoginStatusChangedDelegate_Handle(PlayerIdx, FOnLoginStatusChangedDelegate::CreateUObject(this, &ThisClass::HandleIdentityLoginStatusChanged, ServiceType));
		ServiceContext->IdentityInterface->AddOnLoginCompleteDelegate_Handle(PlayerIdx, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::HandleUserLoginCompleted, ServiceType));
	}

	if (ServiceType != PlatformType)
	{
		PlatformContext->OnlineSubsystem->AddOnConnectionStatusChangedDelegate_Handle(FOnConnectionStatusChangedDelegate::CreateUObject(this, &ThisClass::HandleNetworkConnectionStatusChanged, PlatformType));
		PlatformContext->CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;

		for (int32 PlayerIdx = 0; PlayerIdx < MAX_LOCAL_PLAYERS; PlayerIdx++)
		{
			PlatformContext->IdentityInterface->AddOnLoginStatusChangedDelegate_Handle(PlayerIdx, FOnLoginStatusChangedDelegate::CreateUObject(this, &ThisClass::HandleIdentityLoginStatusChanged, PlatformType));
			PlatformContext->IdentityInterface->AddOnLoginCompleteDelegate_Handle(PlayerIdx, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::HandleUserLoginCompleted, PlatformType));
		}
	}

	// 控制器与系统用户配对变化只来自平台身份系统。
	// Hardware change delegates only listen to platform
	PlatformContext->IdentityInterface->AddOnControllerPairingChangedDelegate_Handle(FOnControllerPairingChangedDelegate::CreateUObject(this, &ThisClass::HandleControllerPairingChanged));
}

// OSSv1 下按旧式平台用户索引调用 IdentityInterface AutoLogin，并返回是否成功启动。
bool UCommonUserSubsystem::AutoLoginOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	return System->IdentityInterface->AutoLogin(GetPlatformUserIndexForId(PlatformUser));
}

// OSSv1 下显示外部登录 UI，并把关闭结果绑定回当前登录上下文；接口不可用时返回 false。
bool UCommonUserSubsystem::ShowLoginUIOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	IOnlineExternalUIPtr ExternalUI = System->OnlineSubsystem->GetExternalUIInterface();
	if (ExternalUI.IsValid())
	{
		// TODO：确认 ShowLoginUI 的 OnlineOnly 和 SkipButton 等布尔标志应如何按平台配置。
		// TODO Unclear which flags should be set
		return ExternalUI->ShowLoginUI(GetPlatformUserIndexForId(PlatformUser), false, false, FOnLoginUIClosedDelegate::CreateUObject(this, &ThisClass::HandleOnLoginUIClosed, Request->CurrentContext));
	}
	return false;
}

// OSSv1 下将 CommonUser 权限转换为 OSS 权限并发起查询；回调可能同步重入状态机。
bool UCommonUserSubsystem::QueryUserPrivilegeOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	// 仅在缓存未知或先前失败时由状态机启动新的权限查询。
	// Start query on unknown or failure
	EUserPrivileges::Type OSSPrivilege = ConvertOSSPrivilege(Request->DesiredPrivilege);

	FUniqueNetIdRepl CurrentId = GetLocalUserNetId(PlatformUser, Request->CurrentContext);
	check(CurrentId.IsValid());
	IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate Delegate = IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateUObject(this, &UCommonUserSubsystem::HandleCheckPrivilegesComplete, Request->DesiredPrivilege, Request->UserInfo, Request->CurrentContext);
	System->IdentityInterface->GetUserPrivilege(*CurrentId, OSSPrivilege, Delegate);

	// GetUserPrivilege 可能同步完成并重入状态机，因此安排后立即返回。
	// This may immediately succeed and reenter this function, so we have to return
	return true;
}

#else

// OSSv2 下返回解析后上下文的服务提供方类型，无上下文时返回 None。
UE::Online::EOnlineServices UCommonUserSubsystem::GetOnlineServicesProvider(ECommonUserOnlineContext Context) const
{
	if (const FOnlineContextCache* System = GetContextCache(Context))
	{
		return System->OnlineServices->GetServicesProvider();
	}
	return UE::Online::EOnlineServices::None;
}

// OSSv2 下返回解析后上下文缓存的 Auth 服务共享指针。
UE::Online::IAuthPtr UCommonUserSubsystem::GetOnlineAuth(ECommonUserOnlineContext Context) const
{
	if (const FOnlineContextCache* System = GetContextCache(Context))
	{
		return System->AuthService;
	}
	return nullptr;
}

// OSSv2 下返回缓存连接状态，无上下文时按 NotConnected 处理。
UE::Online::EOnlineServicesConnectionStatus UCommonUserSubsystem::GetConnectionStatus(ECommonUserOnlineContext Context) const
{
	if (const FOnlineContextCache* System = GetContextCache(Context))
	{
		return System->CurrentConnectionStatus;
	}
	return UE::Online::EOnlineServicesConnectionStatus::NotConnected;
}

// OSSv2 下为 Service 和可选 Platform 上下文绑定登录及连接状态事件，并立即缓存当前连接状态。
void UCommonUserSubsystem::BindOnlineDelegatesOSSv2()
{
	ECommonUserOnlineContext ServiceType = ResolveOnlineContext(ECommonUserOnlineContext::ServiceOrDefault);
	ECommonUserOnlineContext PlatformType = ResolveOnlineContext(ECommonUserOnlineContext::PlatformOrDefault);
	FOnlineContextCache* ServiceContext = GetContextCache(ServiceType);
	FOnlineContextCache* PlatformContext = GetContextCache(PlatformType);
	check(ServiceContext && ServiceContext->OnlineServices && PlatformContext && PlatformContext->OnlineServices);

	ServiceContext->LoginStatusChangedHandle = ServiceContext->AuthService->OnLoginStatusChanged().Add(this, &ThisClass::HandleAuthLoginStatusChanged, ServiceType);
	if (IConnectivityPtr ConnectivityInterface = ServiceContext->OnlineServices->GetConnectivityInterface())
	{
		ServiceContext->ConnectionStatusChangedHandle = ConnectivityInterface->OnConnectionStatusChanged().Add(this, &ThisClass::HandleNetworkConnectionStatusChanged, ServiceType);
	}
	CacheConnectionStatus(ServiceType);

	if (ServiceType != PlatformType)
	{
		PlatformContext->LoginStatusChangedHandle = PlatformContext->AuthService->OnLoginStatusChanged().Add(this, &ThisClass::HandleAuthLoginStatusChanged, PlatformType);
		if (IConnectivityPtr ConnectivityInterface = PlatformContext->OnlineServices->GetConnectivityInterface())
		{
			PlatformContext->ConnectionStatusChangedHandle = ConnectivityInterface->OnConnectionStatusChanged().Add(this, &ThisClass::HandleNetworkConnectionStatusChanged, PlatformType);
		}
		CacheConnectionStatus(PlatformType);
	}
	// TODO：控制器配对变化应移出 OSS，改为直接监听 Core 输入设备委托。
	// TODO:  Controller Pairing Changed - move out of OSS and listen to CoreDelegate directly?
}

// 查询 OSSv2 Connectivity 当前状态；无接口时视为已连接，并通过统一回调更新缓存和用户权限可用性。
void UCommonUserSubsystem::CacheConnectionStatus(ECommonUserOnlineContext Context)
{
	FOnlineContextCache* ContextCache = GetContextCache(Context);
	check(ContextCache);

	EOnlineServicesConnectionStatus ConnectionStatus = EOnlineServicesConnectionStatus::NotConnected;
	if (IConnectivityPtr ConnectivityInterface = ContextCache->OnlineServices->GetConnectivityInterface())
	{
		const TOnlineResult<FGetConnectionStatus> Result = ConnectivityInterface->GetConnectionStatus(FGetConnectionStatus::Params());
		if (Result.IsOk())
		{
			ConnectionStatus = Result.GetOkValue().Status;
		}
	}
	else
	{
		ConnectionStatus = EOnlineServicesConnectionStatus::Connected;
	}

	UE::Online::FConnectionStatusChanged EventParams;
	EventParams.PreviousStatus = ContextCache->CurrentConnectionStatus;
	EventParams.CurrentStatus = ConnectionStatus;
	HandleNetworkConnectionStatusChanged(EventParams, Context);
}

// 从 Platform Auth 获取外部凭据登录当前非平台服务，更新请求状态与错误后继续推进登录状态机。
bool UCommonUserSubsystem::TransferPlatformAuthOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	IAuthPtr PlatformAuthInterface = GetOnlineAuth(ECommonUserOnlineContext::Platform);
	if (Request->CurrentContext != ECommonUserOnlineContext::Platform
		&& PlatformAuthInterface)
	{
		FAuthQueryExternalAuthToken::Params Params;
		Params.LocalAccountId = GetLocalUserNetId(PlatformUser, ECommonUserOnlineContext::Platform).GetV2();

		PlatformAuthInterface->QueryExternalAuthToken(MoveTemp(Params))
		.OnComplete(this, [this, Request](const TOnlineResult<FAuthQueryExternalAuthToken>& Result)
		{
			UCommonUserInfo* UserInfo = Request->UserInfo.Get();
			if (!UserInfo)
			{
				// 用户信息已销毁，移除请求且不再执行完成回调。
				// User is gone, just delete this request
				ActiveLoginRequests.Remove(Request);
				return;
			}

			if (Result.IsOk())
			{
				const FAuthQueryExternalAuthToken::Result& GenerateAuthTokenResult = Result.GetOkValue();
				FAuthLogin::Params Params;
				Params.PlatformUserId = UserInfo->GetPlatformUserId();
				Params.CredentialsType = LoginCredentialsType::ExternalAuth;
				Params.CredentialsToken.Emplace<FExternalAuthToken>(GenerateAuthTokenResult.ExternalAuthToken);

				IAuthPtr PrimaryAuthInterface = GetOnlineAuth(Request->CurrentContext);
				PrimaryAuthInterface->Login(MoveTemp(Params))
				.OnComplete(this, [this, Request](const TOnlineResult<FAuthLogin>& Result)
				{
					UCommonUserInfo* UserInfo = Request->UserInfo.Get();
					if (!UserInfo)
					{
						// 用户信息已销毁，移除请求且不再执行完成回调。
						// User is gone, just delete this request
						ActiveLoginRequests.Remove(Request);
						return;
					}

					if (Result.IsOk())
					{
						Request->TransferPlatformAuthState = ECommonUserAsyncTaskState::Done;
						Request->Error.Reset();
					}
					else
					{
						Request->TransferPlatformAuthState = ECommonUserAsyncTaskState::Failed;
						Request->Error = Result.GetErrorValue();
					}
					ProcessLoginRequest(Request);
				});
			}
			else
			{
				Request->TransferPlatformAuthState = ECommonUserAsyncTaskState::Failed;
				Request->Error = Result.GetErrorValue();
				ProcessLoginRequest(Request);
			}
		});
		return true;
	}
	return false;
}

// OSSv2 下使用 Auto 凭据类型发起 Auth Login，并把完成结果绑定到用户和当前上下文。
bool UCommonUserSubsystem::AutoLoginOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	FAuthLogin::Params LoginParameters;
	LoginParameters.PlatformUserId = PlatformUser;
	LoginParameters.CredentialsType = LoginCredentialsType::Auto;
	// 保持其他参数默认，让具体在线服务自行选择自动登录方式。
	// Leave other LoginParameters as default to allow the online service to determine how to try to automatically log in the user
	TOnlineAsyncOpHandle<FAuthLogin> LoginHandle = System->AuthService->Login(MoveTemp(LoginParameters));
	LoginHandle.OnComplete(this, &ThisClass::HandleUserLoginCompletedV2, PlatformUser, Request->CurrentContext);
	return true;
}

// OSSv2 下显示外部登录 UI 并绑定完成回调；服务未提供 ExternalUI 时返回 false。
bool UCommonUserSubsystem::ShowLoginUIOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	IExternalUIPtr ExternalUI = System->OnlineServices->GetExternalUIInterface();
	if (ExternalUI.IsValid())
	{
		FExternalUIShowLoginUI::Params ShowLoginUIParameters;
		ShowLoginUIParameters.PlatformUserId = PlatformUser;
		TOnlineAsyncOpHandle<FExternalUIShowLoginUI> LoginHandle = ExternalUI->ShowLoginUI(MoveTemp(ShowLoginUIParameters));
		LoginHandle.OnComplete(this, &ThisClass::HandleOnLoginUIClosedV2, PlatformUser, Request->CurrentContext);
		return true;
	}
	return false;
}

// OSSv2 下查询目标权限；后端没有 Privileges 接口时按可用缓存该权限并返回未启动异步操作。
bool UCommonUserSubsystem::QueryUserPrivilegeOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser)
{
	UCommonUserInfo* UserInfo = Request->UserInfo.Get();

	if (IPrivilegesPtr PrivilegesInterface = System->OnlineServices->GetPrivilegesInterface())
	{
		const EUserPrivileges DesiredPrivilege = ConvertOnlineServicesPrivilege(Request->DesiredPrivilege);

		FQueryUserPrivilege::Params Params;
		Params.LocalAccountId = GetLocalUserNetId(PlatformUser, Request->CurrentContext).GetV2();
		Params.Privilege = DesiredPrivilege;
		TOnlineAsyncOpHandle<FQueryUserPrivilege> QueryHandle = PrivilegesInterface->QueryUserPrivilege(MoveTemp(Params));
		QueryHandle.OnComplete(this, &ThisClass::HandleCheckPrivilegesComplete, Request->UserInfo, DesiredPrivilege, Request->CurrentContext);
		return true;
	}
	else
	{
		UpdateUserPrivilegeResult(UserInfo, Request->DesiredPrivilege, ECommonUserPrivilegeResult::Available, Request->CurrentContext);
	}
	return false;
}

// 通过 OSSv2 Auth 按 PlatformUserId 查询本地账户信息，查询失败时返回空共享指针。
TSharedPtr<FAccountInfo> UCommonUserSubsystem::GetOnlineServiceAccountInfo(IAuthPtr AuthService, FPlatformUserId InUserId) const
{
	TSharedPtr<FAccountInfo> AccountInfo;
	FAuthGetLocalOnlineUserByPlatformUserId::Params GetAccountParams = { InUserId };
	TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> GetAccountResult = AuthService->GetLocalOnlineUserByPlatformUserId(MoveTemp(GetAccountParams));
	if (GetAccountResult.IsOk())
	{
		AccountInfo = GetAccountResult.GetOkValue().AccountInfo;
	}
	return AccountInfo;
}

#endif

// 按 OSS 版本判断指定上下文缓存状态是否代表已连接后端。
bool UCommonUserSubsystem::HasOnlineConnection(ECommonUserOnlineContext Context) const
{
#if COMMONUSER_OSSV1
	EOnlineServerConnectionStatus::Type ConnectionType = GetConnectionStatus(Context);

	if (ConnectionType == EOnlineServerConnectionStatus::Normal || ConnectionType == EOnlineServerConnectionStatus::Connected)
	{
		return true;
	}

	return false;
#else
	return GetConnectionStatus(Context) == UE::Online::EOnlineServicesConnectionStatus::Connected;
#endif
}

// 对真实平台用户查询指定上下文的 OSS 登录状态；用户、上下文或账户无效时返回 NotLoggedIn。
ELoginStatusType UCommonUserSubsystem::GetLocalUserLoginStatus(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context) const
{
	if (!IsRealPlatformUser(PlatformUser))
	{
		return ELoginStatusType::NotLoggedIn;
	}

	const FOnlineContextCache* System = GetContextCache(Context);
	if (System)
	{
#if COMMONUSER_OSSV1
		return System->IdentityInterface->GetLoginStatus(GetPlatformUserIndexForId(PlatformUser));
#else
		if (TSharedPtr<FAccountInfo> AccountInfo = GetOnlineServiceAccountInfo(System->AuthService, PlatformUser))
		{
			return AccountInfo->LoginStatus;
		}
#endif
	}
	return ELoginStatusType::NotLoggedIn;
}

// 对真实平台用户返回 OSSv1 UniqueNetId 或包装 OSSv2 AccountId；无法查询时返回无效 NetId。
FUniqueNetIdRepl UCommonUserSubsystem::GetLocalUserNetId(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context) const
{
	if (!IsRealPlatformUser(PlatformUser))
	{
		return FUniqueNetIdRepl();
	}

	const FOnlineContextCache* System = GetContextCache(Context);
	if (System)
	{
#if COMMONUSER_OSSV1
		return FUniqueNetIdRepl(System->IdentityInterface->GetUniquePlayerId(GetPlatformUserIndexForId(PlatformUser)));
#else
		// TODO：OSSv2 FAccountId 到 FUniqueNetIdRepl 的包装支持仍在完善中。
		// TODO:  OSSv2 FUniqueNetIdRepl wrapping FAccountId is in progress
		if (TSharedPtr<FAccountInfo> AccountInfo = GetOnlineServiceAccountInfo(System->AuthService, PlatformUser))
		{
			return FUniqueNetIdRepl(AccountInfo->AccountId);
		}
#endif
	}

	return FUniqueNetIdRepl();
}

// 从 OSSv1 Identity 或 OSSv2 AccountInfo DisplayName 读取本地用户昵称，缺失时返回空字符串。
FString UCommonUserSubsystem::GetLocalUserNickname(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context) const
{
#if COMMONUSER_OSSV1
	IOnlineIdentity* Identity = GetOnlineIdentity(Context);
	if (ensure(Identity))
	{
		return Identity->GetPlayerNickname(GetPlatformUserIndexForId(PlatformUser));
	}
#else
	if (IAuthPtr AuthService = GetOnlineAuth(Context))
	{
		if (TSharedPtr<FAccountInfo> AccountInfo = GetOnlineServiceAccountInfo(AuthService, PlatformUser))
		{
			if (const FSchemaVariant* DisplayName = AccountInfo->Attributes.Find(AccountAttributeData::DisplayName))
			{
				return DisplayName->GetString();
			}
		}
	}
#endif // COMMONUSER_OSSV1

	return FString();
}

// 将类型标签、标题和正文广播给游戏层系统消息处理委托。
void UCommonUserSubsystem::SendSystemMessage(FGameplayTag MessageType, FText TitleText, FText BodyText)
{
	OnHandleSystemMessage.Broadcast(MessageType, TitleText, BodyText);
}

// 接受至少一个本地玩家的上限，并同步 GameViewportClient 分屏容量；超出 MAX_LOCAL_PLAYERS 的玩家按访客处理。
void UCommonUserSubsystem::SetMaxLocalPlayers(int32 InMaxLocalPlayers)
{
	if (ensure(InMaxLocalPlayers >= 1))
	{
		// 可允许超过 MAX_LOCAL_PLAYERS 的本地玩家，超出平台真实用户槽位的部分视为访客。
		// We can have more local players than MAX_LOCAL_PLAYERS, the rest are treated as guests
		MaxNumberOfLocalPlayers = InMaxLocalPlayers;

		UGameInstance* GameInstance = GetGameInstance();
		UGameViewportClient* ViewportClient = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;

		if (ViewportClient)
		{
			ViewportClient->MaxSplitscreenPlayers = MaxNumberOfLocalPlayers;
		}
	}
}

// 返回当前配置的最大本地玩家数量。
int32 UCommonUserSubsystem::GetMaxLocalPlayers() const
{
	return MaxNumberOfLocalPlayers;
}

// 返回 GameInstance 当前 LocalPlayer 数量；GameInstance 异常时保守返回 1。
int32 UCommonUserSubsystem::GetNumLocalPlayers() const
{
	UGameInstance* GameInstance = GetGameInstance();
	if (ensure(GameInstance))
	{
		return GameInstance->GetNumLocalPlayers();
	}
	return 1;
}

// 返回现有用户状态；索引超出允许范围时为 Invalid，范围内尚未创建用户时为 Unknown。
ECommonUserInitializationState UCommonUserSubsystem::GetLocalPlayerInitializationState(int32 LocalPlayerIndex) const
{
	const UCommonUserInfo* UserInfo = GetUserInfoForLocalPlayerIndex(LocalPlayerIndex);
	if (UserInfo)
	{
		return UserInfo->InitializationState;
	}

	if (LocalPlayerIndex < 0 || LocalPlayerIndex >= GetMaxLocalPlayers())
	{
		return ECommonUserInitializationState::Invalid;
	}

	return ECommonUserInitializationState::Unknown;
}

// 为本地游戏构造 CanPlay 初始化参数，补齐默认设备并允许创建 LocalPlayer 和可选访客。
bool UCommonUserSubsystem::TryToInitializeForLocalPlay(int32 LocalPlayerIndex, FInputDeviceId PrimaryInputDevice, bool bCanUseGuestLogin)
{
	if (!PrimaryInputDevice.IsValid())
	{
		// 未提供有效设备时使用平台输入映射器的默认输入设备。
		// Set to default device
		PrimaryInputDevice = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
	}

	FCommonUserInitializeParams Params;
	Params.LocalPlayerIndex = LocalPlayerIndex;
	Params.PrimaryInputDevice = PrimaryInputDevice;
	Params.bCanUseGuestLogin = bCanUseGuestLogin;
	Params.bCanCreateNewLocalPlayer = true;
	Params.RequestedPrivilege = ECommonUserPrivilege::CanPlay;

	return TryToInitializeUser(Params);
}

// 为现有 LocalPlayer 构造 CanPlayOnline 初始化参数，并禁止创建新玩家。
bool UCommonUserSubsystem::TryToLoginForOnlinePlay(int32 LocalPlayerIndex)
{
	FCommonUserInitializeParams Params;
	Params.LocalPlayerIndex = LocalPlayerIndex;
	Params.bCanCreateNewLocalPlayer = false;
	Params.RequestedPrivilege = ECommonUserPrivilege::CanPlayOnline;

	return TryToInitializeUser(Params);
}

// 校验索引、设备和用户占用，创建或复用 UserInfo，锁定登录参数并启动本地或在线权限登录流程。
bool UCommonUserSubsystem::TryToInitializeUser(FCommonUserInitializeParams Params)
{
	if (Params.LocalPlayerIndex < 0 || (!Params.bCanCreateNewLocalPlayer && Params.LocalPlayerIndex >= GetNumLocalPlayers()))
	{
		if (!bIsDedicatedServer)
		{
			UE_LOG(LogCommonUser, Error, TEXT("TryToInitializeUser %d failed with current %d and max %d, invalid index"), 
				Params.LocalPlayerIndex, GetNumLocalPlayers(), GetMaxLocalPlayers());
			return false;
		}
	}

	if (Params.LocalPlayerIndex > GetNumLocalPlayers() || Params.LocalPlayerIndex >= GetMaxLocalPlayers())
	{
		UE_LOG(LogCommonUser, Error, TEXT("TryToInitializeUser %d failed with current %d and max %d, can only create in order up to max players"), 
			Params.LocalPlayerIndex, GetNumLocalPlayers(), GetMaxLocalPlayers());
		return false;
	}

	// 根据旧 ControllerId、InputDeviceId 或 PlatformUserId 双向补齐用户和设备映射。
	// Fill in platform user and input device if needed
	if (Params.ControllerId != INDEX_NONE && (!Params.PrimaryInputDevice.IsValid() || !Params.PlatformUser.IsValid()))
	{
		IPlatformInputDeviceMapper::Get().RemapControllerIdToPlatformUserAndDevice(Params.ControllerId, Params.PlatformUser, Params.PrimaryInputDevice);
	}

	if (Params.PrimaryInputDevice.IsValid() && !Params.PlatformUser.IsValid())
	{
		Params.PlatformUser = GetPlatformUserIdForInputDevice(Params.PrimaryInputDevice);
	}
	else if (Params.PlatformUser.IsValid() && !Params.PrimaryInputDevice.IsValid())
	{
		Params.PrimaryInputDevice = GetPrimaryInputDeviceForPlatformUser(Params.PlatformUser);
	}

	UCommonUserInfo* LocalUserInfo = ModifyInfo(GetUserInfoForLocalPlayerIndex(Params.LocalPlayerIndex));
	UCommonUserInfo* LocalUserInfoForController = ModifyInfo(GetUserInfoForInputDevice(Params.PrimaryInputDevice));

	if (LocalUserInfoForController && LocalUserInfo && LocalUserInfoForController != LocalUserInfo)
	{
		UE_LOG(LogCommonUser, Error, TEXT("TryToInitializeUser %d failed because controller %d is already assigned to player %d"),
			Params.LocalPlayerIndex, Params.PrimaryInputDevice.GetId(), LocalUserInfoForController->LocalPlayerIndex);
		return false;
	}

	if (Params.LocalPlayerIndex == 0 && Params.bCanUseGuestLogin)
	{
		UE_LOG(LogCommonUser, Error, TEXT("TryToInitializeUser failed because player 0 cannot be a guest"));
		return false;
	}

	if (!LocalUserInfo)
	{
		LocalUserInfo = CreateLocalUserInfo(Params.LocalPlayerIndex);
	}
	else
	{
		// 调用方未指定设备或平台用户时，从已有 UserInfo 复制当前值。
		// Copy from existing user info
		if (!Params.PrimaryInputDevice.IsValid())
		{
			Params.PrimaryInputDevice = LocalUserInfo->PrimaryInputDevice;
		}

		if (!Params.PlatformUser.IsValid())
		{
			Params.PlatformUser = LocalUserInfo->PlatformUser;
		}
	}
	
	if (LocalUserInfo->InitializationState != ECommonUserInitializationState::Unknown && LocalUserInfo->InitializationState != ECommonUserInitializationState::FailedtoLogin)
	{
		// 登录开始后不允许更换主要设备、平台用户或访客许可。
		// Not allowed to change parameters during login
		if (LocalUserInfo->PrimaryInputDevice != Params.PrimaryInputDevice || LocalUserInfo->PlatformUser != Params.PlatformUser || LocalUserInfo->bCanBeGuest != Params.bCanUseGuestLogin)
		{
			UE_LOG(LogCommonUser, Error, TEXT("TryToInitializeUser failed because player %d has already started the login process with diffrent settings!"), Params.LocalPlayerIndex);
			return false;
		}
	}

	// 立即保存目标设备和平台用户，使后续创建 LocalPlayer 时能使用正确控制器映射。
	// Set desired index now so if it creates a player it knows what controller to use
	LocalUserInfo->PrimaryInputDevice = Params.PrimaryInputDevice;
	LocalUserInfo->PlatformUser = Params.PlatformUser;
	LocalUserInfo->bCanBeGuest = Params.bCanUseGuestLogin;
	RefreshLocalUserInfo(LocalUserInfo);

	// 已具备本地 CanPlay 且请求在线权限时进入网络登录，否则属于初始登录。
	// Either doing an initial or network login
	if (LocalUserInfo->GetPrivilegeAvailability(ECommonUserPrivilege::CanPlay) == ECommonUserAvailability::NowAvailable && Params.RequestedPrivilege == ECommonUserPrivilege::CanPlayOnline)
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::DoingNetworkLogin;
	}
	else
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::DoingInitialLogin;
	}

	LoginLocalUser(LocalUserInfo, Params.RequestedPrivilege, Params.OnlineContext, FOnLocalUserLoginCompleteDelegate::CreateUObject(this, &ThisClass::HandleLoginForUserInitialize, Params));

	return true;
}

// 根据按键数组安装或撤销 GameViewport 输入覆盖，并保存触发登录时使用的初始化参数。
void UCommonUserSubsystem::ListenForLoginKeyInput(TArray<FKey> AnyUserKeys, TArray<FKey> NewUserKeys, FCommonUserInitializeParams Params)
{
	UGameViewportClient* ViewportClient = GetGameInstance()->GetGameViewportClient();
	if (ensure(ViewportClient))
	{
		const bool bIsMapped = LoginKeysForAnyUser.Num() > 0 || LoginKeysForNewUser.Num() > 0;
		const bool bShouldBeMapped = AnyUserKeys.Num() > 0 || NewUserKeys.Num() > 0;

		if (bIsMapped && !bShouldBeMapped)
		{
			// 停止监听时恢复此前的输入覆盖处理器。
			// Set it back to wrapped handler
			ViewportClient->OnOverrideInputKey() = WrappedInputKeyHandler;
			WrappedInputKeyHandler.Unbind();
		}
		else if (!bIsMapped && bShouldBeMapped)
		{
			// 开始监听时保存原处理器，并安装 CommonUser 登录输入包装器。
			// Set up a wrapped handler
			WrappedInputKeyHandler = ViewportClient->OnOverrideInputKey();
			ViewportClient->OnOverrideInputKey().BindUObject(this, &UCommonUserSubsystem::OverrideInputKeyForLogin);
		}

		LoginKeysForAnyUser = AnyUserKeys;
		LoginKeysForNewUser = NewUserKeys;

		if (bShouldBeMapped)
		{
			ParamsForLoginKey = Params;
		}
		else
		{
			ParamsForLoginKey = FCommonUserInitializeParams();
		}
	}
}

// 移除指定玩家的活动登录请求以禁用回调，并按登录阶段恢复为 LocalOnly 或 Failed；底层平台操作可能继续。
bool UCommonUserSubsystem::CancelUserInitialization(int32 LocalPlayerIndex)
{
	UCommonUserInfo* LocalUserInfo = ModifyInfo(GetUserInfoForLocalPlayerIndex(LocalPlayerIndex));
	if (!LocalUserInfo)
	{
		return false;
	}

	if (!LocalUserInfo->IsDoingLogin())
	{
		return false;
	}

	// 从活动登录队列移除该用户的全部请求。
	// Remove from login queue
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		if (Request->UserInfo.IsValid() && Request->UserInfo->LocalPlayerIndex == LocalPlayerIndex)
		{
			ActiveLoginRequests.Remove(Request);
		}
	}

	// 根据取消前阶段推断最合理的剩余初始化状态。
	// Set state with best guess
	if (LocalUserInfo->InitializationState == ECommonUserInitializationState::DoingNetworkLogin)
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::LoggedInLocalOnly;
	}
	else
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::FailedtoLogin;
	}

	return true;
}

// 取消初始化后降级真实用户状态或删除访客信息，并可移除非主 LocalPlayer；不会调用平台级 Logout。
bool UCommonUserSubsystem::TryToLogOutUser(int32 LocalPlayerIndex, bool bDestroyPlayer)
{
	UGameInstance* GameInstance = GetGameInstance();
	
	if (!ensure(GameInstance))
	{
		return false;
	}

	if (LocalPlayerIndex == 0 && bDestroyPlayer)
	{
		UE_LOG(LogCommonUser, Error, TEXT("TryToLogOutUser cannot destroy player 0"));
		return false;
	}

	CancelUserInitialization(LocalPlayerIndex);
	
	UCommonUserInfo* LocalUserInfo = ModifyInfo(GetUserInfoForLocalPlayerIndex(LocalPlayerIndex));
	if (!LocalUserInfo)
	{
		UE_LOG(LogCommonUser, Warning, TEXT("TryToLogOutUser failed to log out user %i because they are not logged in"), LocalPlayerIndex);
		return false;
	}

	FPlatformUserId UserId = LocalUserInfo->PlatformUser;
	if (IsRealPlatformUser(UserId))
	{
		// 为支持立即重新登录，当前不会真正注销平台账户，只重置 CommonUser 状态。
		// Currently this does not do platform logout in case they want to log back in immediately after
		UE_LOG(LogCommonUser, Log, TEXT("TryToLogOutUser succeeded for real platform user %d"), UserId.GetInternalId());

		LogOutLocalUser(UserId);
	}
	else if (ensure(LocalUserInfo->bIsGuest))
	{
		// 访客没有平台身份，直接删除其 UserInfo。
		// For guest users just delete it
		UE_LOG(LogCommonUser, Log, TEXT("TryToLogOutUser succeeded for guest player index %d"), LocalPlayerIndex);

		LocalUserInfos.Remove(LocalPlayerIndex);
	}

	if (bDestroyPlayer)
	{
		ULocalPlayer* ExistingPlayer = GameInstance->FindLocalPlayerFromPlatformUserId(UserId);

		if (ExistingPlayer)
		{
			GameInstance->RemoveLocalPlayer(ExistingPlayer);
		}
	}

	return true;
}

// 废弃全部现有 UserInfo、取消活动登录，并按平台主用户和主输入设备重新创建索引 0 用户。
void UCommonUserSubsystem::ResetUserState()
{
	// 手动把旧 UserInfo 标记为垃圾，避免重置后仍被误用。
	// Manually purge existing info objects
	for (TPair<int32, UCommonUserInfo*> Pair : LocalUserInfos)
	{
		if (Pair.Value)
		{
			Pair.Value->MarkAsGarbage();
		}
	}

	LocalUserInfos.Reset();

	// 清空请求数组以取消进行中登录的后续回调。
	// Cancel in-progress logins
	ActiveLoginRequests.Reset();

	// 为主本地玩家索引 0 创建新的用户信息。
	// Create player info for id 0
	UCommonUserInfo* FirstUser = CreateLocalUserInfo(0);

	FirstUser->PlatformUser = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
	FirstUser->PrimaryInputDevice = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(FirstUser->PlatformUser);

	// TODO：评估是否应将主玩家身份刷新延后一帧，等待在线接口完全就绪。
	// TODO: Schedule a refresh of player 0 for next frame?
	RefreshLocalUserInfo(FirstUser);
}

// 在按键按下时选择现有未登录用户或下一个空 LocalPlayer 槽，并按 AnyUser/NewUser 键集合触发对应初始化。
bool UCommonUserSubsystem::OverrideInputKeyForLogin(FInputKeyEventArgs& EventArgs)
{
	int32 NextLocalPlayerIndex = INDEX_NONE;

	const UCommonUserInfo* MappedUser = GetUserInfoForInputDevice(EventArgs.InputDevice);
	if (EventArgs.Event == IE_Pressed)
	{
		if (MappedUser == nullptr || !MappedUser->IsLoggedIn())
		{
			if (MappedUser)
			{
				NextLocalPlayerIndex = MappedUser->LocalPlayerIndex;
			}
			else
			{
				// 输入设备未映射时查找下一个尚未初始化的本地玩家槽位。
				// Find next player
				for (int32 i = 0; i < MaxNumberOfLocalPlayers; i++)
				{
					if (GetLocalPlayerInitializationState(i) == ECommonUserInitializationState::Unknown)
					{
						NextLocalPlayerIndex = i;
						break;
					}
				}
			}

			if (NextLocalPlayerIndex != INDEX_NONE)
			{
				if (LoginKeysForAnyUser.Contains(EventArgs.Key))
				{
					// 已在登录时消费该按键，避免平台专用输入继续处理并重复触发。
					// If we're in the middle of logging in just return true to ignore platform-specific input
					if (MappedUser && MappedUser->IsDoingLogin())
					{
						return true;
					}

					// AnyUser 键用于“按键开始”页面，可初始化已有或新用户。
					// Press start screen
					FCommonUserInitializeParams NewParams = ParamsForLoginKey;
					NewParams.LocalPlayerIndex = NextLocalPlayerIndex;
					NewParams.PrimaryInputDevice = EventArgs.InputDevice;

					return TryToInitializeUser(NewParams);
				}

				// 再次确认该输入设备是否已经映射到某个本地用户。
				// See if this controller id is mapped
				MappedUser = GetUserInfoForInputDevice(EventArgs.InputDevice);

				if (!MappedUser || MappedUser->LocalPlayerIndex == INDEX_NONE)
				{
					if (LoginKeysForNewUser.Contains(EventArgs.Key))
					{
						// 已在登录时消费该按键，避免平台输入层重复响应。
						// If we're in the middle of logging in just return true to ignore platform-specific input
						if (MappedUser && MappedUser->IsDoingLogin())
						{
							return true;
						}

						// NewUser 键用于为本地多人创建并初始化下一个玩家。
						// Local multiplayer
						FCommonUserInitializeParams NewParams = ParamsForLoginKey;
						NewParams.LocalPlayerIndex = NextLocalPlayerIndex;
						NewParams.PrimaryInputDevice = EventArgs.InputDevice;

						return TryToInitializeUser(NewParams);
					}
				}
			}
		}
	}

	if (WrappedInputKeyHandler.IsBound())
	{
		return WrappedInputKeyHandler.Execute(EventArgs);
	}

	return false;
}

// 从 OSSv1 或 OSSv2 错误类型提取统一的本地化显示文本。
static inline FText GetErrorText(const FOnlineErrorType& InOnlineError)
{
#if COMMONUSER_OSSV1
	return InOnlineError.GetErrorMessage();
#else
	return InOnlineError.GetText();
#endif
}

// 将底层登录结果转为访客或真实用户，按需创建 LocalPlayer、更新用户映射，并延后一帧发送成功或失败完成回调。
void UCommonUserSubsystem::HandleLoginForUserInitialize(const UCommonUserInfo* UserInfo, ELoginStatusType NewStatus, FUniqueNetIdRepl NetId, const TOptional<FOnlineErrorType>& InError, ECommonUserOnlineContext Context, FCommonUserInitializeParams Params)
{
	UGameInstance* GameInstance = GetGameInstance();
	check(GameInstance);
	FTimerManager& TimerManager = GameInstance->GetTimerManager();
	// 复制错误以便在访客回退等已处理场景中清除它。
	TOptional<FOnlineErrorType> Error = InError; // Copy so we can reset on handled errors

	UCommonUserInfo* LocalUserInfo = ModifyInfo(UserInfo);
	UCommonUserInfo* FirstUserInfo = ModifyInfo(GetUserInfoForLocalPlayerIndex(0));

	if (!ensure(LocalUserInfo && FirstUserInfo))
	{
		return;
	}

	// 先刷新平台和服务的实际 NetId 与昵称缓存。
	// Check the hard platform/service ids
	RefreshLocalUserInfo(LocalUserInfo);

	FUniqueNetIdRepl FirstPlayerId = FirstUserInfo->GetNetId(ECommonUserOnlineContext::PlatformOrDefault);

	// 次要玩家允许访客时，登录失败或复用主玩家 NetId 都转为访客，后者视为平台未提供独立用户。
	// Check to see if we should make a guest after a login failure. Some platforms return success but reuse the first player's id, count this as a failure
	if (LocalUserInfo != FirstUserInfo && LocalUserInfo->bCanBeGuest && (NewStatus == ELoginStatusType::NotLoggedIn || NetId == FirstPlayerId))
	{
#if COMMONUSER_OSSV1
		NetId = (FUniqueNetIdRef)FUniqueNetIdString::Create(FString::Printf(TEXT("GuestPlayer%d"), LocalUserInfo->LocalPlayerIndex), NULL_SUBSYSTEM);
#else
		// TODO：OSSv2 FAccountId 到 FUniqueNetIdRepl 的包装支持仍在完善中。
		// TODO:  OSSv2 FUniqueNetIdRepl wrapping FAccountId is in progress
		// TODO：确定 OSSv2 访客账户应如何生成和表示。
		// TODO:  OSSv2 - How to handle guest accounts?
#endif
		LocalUserInfo->bIsGuest = true;
		NewStatus = ELoginStatusType::UsingLocalProfile;
		Error.Reset();
		UE_LOG(LogCommonUser, Log, TEXT("HandleLoginForUserInitialize created guest id %s for local player %d"), *NetId.ToString(), LocalUserInfo->LocalPlayerIndex);
	}
	else
	{
		LocalUserInfo->bIsGuest = false;
	}

	ensure(LocalUserInfo->IsDoingLogin());

	if (Error.IsSet())
	{
		FText ErrorText = GetErrorText(Error.GetValue());
		TimerManager.SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCommonUserSubsystem::HandleUserInitializeFailed, Params, ErrorText));
		return;
	}

	if (Context == ECommonUserOnlineContext::Game)
	{
		LocalUserInfo->UpdateCachedNetId(NetId, ECommonUserOnlineContext::Game);
	}
		
	ULocalPlayer* CurrentPlayer = GameInstance->GetLocalPlayerByIndex(LocalUserInfo->LocalPlayerIndex);
	if (!CurrentPlayer && Params.bCanCreateNewLocalPlayer)
	{
		FString ErrorString;
		CurrentPlayer = GameInstance->CreateLocalPlayer(LocalUserInfo->PlatformUser, ErrorString, true);

		if (!CurrentPlayer)
		{
			TimerManager.SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCommonUserSubsystem::HandleUserInitializeFailed, Params, FText::AsCultureInvariant(ErrorString)));
			return;
		}
		ensure(GameInstance->GetLocalPlayerByIndex(LocalUserInfo->LocalPlayerIndex) == CurrentPlayer);
	}

	// 将 LocalPlayer 的平台用户、输入设备和首选 NetId 同步到最终 UserInfo。
	// Updates controller and net id if needed
	SetLocalPlayerUserInfo(CurrentPlayer, LocalUserInfo);

	// 延后一帧完成成功回调，避免在登录调用栈内重入游戏逻辑。
	// Set a delayed callback
	TimerManager.SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UCommonUserSubsystem::HandleUserInitializeSucceeded, Params));
}

// 处理延迟失败：根据仍保留的登录状态选择 Failed 或 LocalOnly，按配置发送系统消息并广播完成。
void UCommonUserSubsystem::HandleUserInitializeFailed(FCommonUserInitializeParams Params, FText Error)
{
	UCommonUserInfo* LocalUserInfo = ModifyInfo(GetUserInfoForLocalPlayerIndex(Params.LocalPlayerIndex));

	if (!LocalUserInfo)
	{
		// 回调排队后用户状态已被重置，无需再通知旧请求。
		// The user info was reset since this was scheduled
		return;
	}

	UE_LOG(LogCommonUser, Warning, TEXT("TryToInitializeUser %d failed with error %s"), LocalUserInfo->LocalPlayerIndex, *Error.ToString());

	// 状态已不在登录中说明请求可能被取消，终止旧回调。
	// If state is wrong, abort as we might have gotten canceled
	if (!ensure(LocalUserInfo->IsDoingLogin()))
	{
		return;
	}

	// 初始登录失败或最终完全未登录时标记 Failed，否则保留仅本地登录状态。
	// If initial login failed or we ended up totally logged out, set to complete failure
	ELoginStatusType NewStatus = GetLocalUserLoginStatus(Params.PlatformUser, Params.OnlineContext);
	if (NewStatus == ELoginStatusType::NotLoggedIn || LocalUserInfo->InitializationState == ECommonUserInitializationState::DoingInitialLogin)
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::FailedtoLogin;
	}
	else
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::LoggedInLocalOnly;
	}

	FText TitleText = NSLOCTEXT("CommonUser", "LoginFailedTitle", "Login Failure");

	if (!Params.bSuppressLoginErrors)
	{
		SendSystemMessage(FCommonUserTags::SystemMessage_Error_InitializeLocalPlayerFailed, TitleText, Error);
	}
	
	// 依次执行请求专用委托和全局初始化完成广播。
	// Call callbacks
	Params.OnUserInitializeComplete.ExecuteIfBound(LocalUserInfo, false, Error, Params.RequestedPrivilege, Params.OnlineContext);
	OnUserInitializeComplete.Broadcast(LocalUserInfo, false, Error, Params.RequestedPrivilege, Params.OnlineContext);
}

// 处理延迟成功：确认请求未取消，设置 Online 或 LocalOnly 最终状态，并广播请求和全局完成委托。
void UCommonUserSubsystem::HandleUserInitializeSucceeded(FCommonUserInitializeParams Params)
{
	UCommonUserInfo* LocalUserInfo = ModifyInfo(GetUserInfoForLocalPlayerIndex(Params.LocalPlayerIndex));

	if (!LocalUserInfo)
	{
		// 回调排队后用户信息已重置，无需通知旧请求。
		// The user info was reset since this was scheduled
		return;
	}

	// 状态已不在登录中说明请求可能被取消，终止旧回调。
	// If state is wrong, abort as we might have gotten cancelled
	if (!ensure(LocalUserInfo->IsDoingLogin()))
	{
		return;
	}

	// 根据请求权限确定最终是完整在线登录还是仅本地登录。
	// Fix up state
	if (Params.RequestedPrivilege == ECommonUserPrivilege::CanPlayOnline)
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::LoggedInOnline;
	}
	else
	{
		LocalUserInfo->InitializationState = ECommonUserInitializationState::LoggedInLocalOnly;
	}

	ensure(LocalUserInfo->GetPrivilegeAvailability(Params.RequestedPrivilege) == ECommonUserAvailability::NowAvailable);

	// 依次执行请求专用委托和全局初始化完成广播。
	// Call callbacks
	Params.OnUserInitializeComplete.ExecuteIfBound(LocalUserInfo, true, FText(), Params.RequestedPrivilege, Params.OnlineContext);
	OnUserInitializeComplete.Broadcast(LocalUserInfo, true, FText(), Params.RequestedPrivilege, Params.OnlineContext);
}

// 创建并登记登录状态机请求，保存用户、目标权限、上下文与完成委托后立即推进第一步。
bool UCommonUserSubsystem::LoginLocalUser(const UCommonUserInfo* UserInfo, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext Context, FOnLocalUserLoginCompleteDelegate OnComplete)
{
	UCommonUserInfo* LocalUserInfo = ModifyInfo(UserInfo);
	if (!ensure(UserInfo))
	{
		return false;
	}

	TSharedRef<FUserLoginRequest> NewRequest = MakeShared<FUserLoginRequest>(LocalUserInfo, RequestedPrivilege, Context, MoveTemp(OnComplete));
	ActiveLoginRequests.Add(NewRequest);

	// 状态机可能同步完成并执行回调，也可能启动异步登录步骤。
	// This will execute callback or start login process
	ProcessLoginRequest(NewRequest);

	return true;
}

// 推进多上下文登录状态机：依次尝试平台凭据、AutoLogin、登录 UI 和权限查询，并在需要时切换到 Service 上下文。
void UCommonUserSubsystem::ProcessLoginRequest(TSharedRef<FUserLoginRequest> Request)
{
	// 首先检查请求是否已经满足目标登录和权限状态。
	// First, see if we've fully logged in
	UCommonUserInfo* UserInfo = Request->UserInfo.Get();

	if (!UserInfo)
	{
		// 用户信息已销毁，移除请求且不再执行完成回调。
		// User is gone, just delete this request
		ActiveLoginRequests.Remove(Request);

		return;
	}

	const FPlatformUserId PlatformUser = UserInfo->GetPlatformUserId();

	// 访客没有真实 PlatformUserId，底层在线登录直接以 InvalidUser 失败完成。
	// If the platform user id is invalid because this is a guest, skip right to failure
	if (!IsRealPlatformUser(PlatformUser))
	{
#if COMMONUSER_OSSV1
		Request->Error = FOnlineError(NSLOCTEXT("CommonUser", "InvalidPlatformUser", "Invalid Platform User"));
#else
		Request->Error = UE::Online::Errors::InvalidUser();
#endif
		// 从活动请求数组移除，防止后续异步回调再次完成。
		// Remove from active array
		ActiveLoginRequests.Remove(Request);

		// 以未登录状态和无效 NetId 执行请求完成委托。
		// Execute delegate if bound
		Request->Delegate.ExecuteIfBound(UserInfo, ELoginStatusType::NotLoggedIn, FUniqueNetIdRepl(), Request->Error, Request->DesiredContext);

		return;
	}

	// 决定状态机首先处理的平台或目标在线上下文。
	// Figure out what context to process first
	if (Request->CurrentContext == ECommonUserOnlineContext::Invalid)
	{
		// Game 聚合登录先处理 Platform，再切换到最终 Service 或 Default。
		// First start with platform context if this is a game login
		if (Request->DesiredContext == ECommonUserOnlineContext::Game)
		{
			Request->CurrentContext = ResolveOnlineContext(ECommonUserOnlineContext::PlatformOrDefault);
		}
		else
		{
			Request->CurrentContext = ResolveOnlineContext(Request->DesiredContext);
		}
	}

	ELoginStatusType CurrentStatus = GetLocalUserLoginStatus(PlatformUser, Request->CurrentContext);
	FUniqueNetIdRepl CurrentId = GetLocalUserNetId(PlatformUser, Request->CurrentContext);
	FOnlineContextCache* System = GetContextCache(Request->CurrentContext);

	if (!ensure(System))
	{
		return;
	}

	// 首次进入时将整个登录请求标记为进行中，后续异步回调会沿用该状态继续推进。
	// Starting a new request
	if (Request->OverallLoginState == ECommonUserAsyncTaskState::NotStarted)
	{
		Request->OverallLoginState = ECommonUserAsyncTaskState::InProgress;
	}

	bool bHasRequiredStatus = (CurrentStatus == ELoginStatusType::LoggedIn);
	if (Request->DesiredPrivilege == ECommonUserPrivilege::CanPlay)
	{
		// 仅要求本地游玩时，UsingLocalProfile 已满足登录要求，不必强制建立在线会话。
		// If this is not an online required login, allow local profile to count as fully logged in
		bHasRequiredStatus |= (CurrentStatus == ELoginStatusType::UsingLocalProfile);
	}

	// 只有登录状态满足要求且取得有效 NetId，当前上下文的登录阶段才算成功。
	// Check for overall success
	if (bHasRequiredStatus && CurrentId.IsValid())
	{
		// 登录 UI 尚未关闭时不能提前完成，等待 UI 回调提供最终账户或错误。
		// Stall if we're waiting for the login UI to close
		if (Request->LoginUIState == ECommonUserAsyncTaskState::InProgress)
		{
			return;
		}

		Request->OverallLoginState = ECommonUserAsyncTaskState::Done;
	}
	else
	{
		// 第一优先级是把平台账户凭据转交给当前在线服务进行登录。
		// Try using platform auth to login
		if (Request->TransferPlatformAuthState == ECommonUserAsyncTaskState::NotStarted)
		{
			Request->TransferPlatformAuthState = ECommonUserAsyncTaskState::InProgress;

			if (TransferPlatformAuth(System, Request, PlatformUser))
			{
				return;
			}
			// 底层未能启动凭据转移请求，将该子步骤记为失败并继续尝试其他登录方式。
			// We didn't start a login attempt, so set failure
			Request->TransferPlatformAuthState = ECommonUserAsyncTaskState::Failed;
		}

		// 平台凭据路径结束后，第二优先级是使用默认凭据执行 AutoLogin。
		// Next check AutoLogin
		if (Request->AutoLoginState == ECommonUserAsyncTaskState::NotStarted)
		{
			if (Request->TransferPlatformAuthState == ECommonUserAsyncTaskState::Done || Request->TransferPlatformAuthState == ECommonUserAsyncTaskState::Failed)
			{
				Request->AutoLoginState = ECommonUserAsyncTaskState::InProgress;

				// 默认凭据在多数平台可直接完成静默登录，成功启动后等待异步回调。
				// Try an auto login with default credentials, this will work on many platforms
				if (AutoLogin(System, Request, PlatformUser))
				{
					return;
				}
				// AutoLogin 未能启动时记录子步骤失败，使流程可以继续尝试交互式 UI。
				// We didn't start an autologin attempt, so set failure
				Request->AutoLoginState = ECommonUserAsyncTaskState::Failed;
			}
		}

		// 前两种静默登录方式都已结束后，最后尝试显示交互式登录界面。
		// Next check login UI
		if (Request->LoginUIState == ECommonUserAsyncTaskState::NotStarted)
		{
			if ((Request->TransferPlatformAuthState == ECommonUserAsyncTaskState::Done || Request->TransferPlatformAuthState == ECommonUserAsyncTaskState::Failed)
				&& (Request->AutoLoginState == ECommonUserAsyncTaskState::Done || Request->AutoLoginState == ECommonUserAsyncTaskState::Failed))
			{
				Request->LoginUIState = ECommonUserAsyncTaskState::InProgress;

				if (ShowLoginUI(System, Request, PlatformUser))
				{
					return;
				}
				// 底层未显示登录界面，标记 UI 子步骤失败，避免状态机永久等待。
				// We didn't show a UI, so set failure
				Request->LoginUIState = ECommonUserAsyncTaskState::Failed;
			}
		}
	}

	// 三种登录方式全部失败，或已经没有进行中的子步骤但仍未登录时，整体请求失败。
	// Check for overall failure
	if (Request->LoginUIState == ECommonUserAsyncTaskState::Failed &&
		Request->AutoLoginState == ECommonUserAsyncTaskState::Failed &&
		Request->TransferPlatformAuthState == ECommonUserAsyncTaskState::Failed)
	{
		Request->OverallLoginState = ECommonUserAsyncTaskState::Failed;
	}
	else if (Request->OverallLoginState == ECommonUserAsyncTaskState::InProgress &&
		Request->LoginUIState != ECommonUserAsyncTaskState::InProgress &&
		Request->AutoLoginState != ECommonUserAsyncTaskState::InProgress &&
		Request->TransferPlatformAuthState != ECommonUserAsyncTaskState::InProgress)
	{
		// 没有子步骤仍在运行却未取得成功状态时主动失败，防止请求永久滞留。
		// If none of the substates are still in progress but we haven't successfully logged in, mark this as a failure to avoid stalling forever
		Request->OverallLoginState = ECommonUserAsyncTaskState::Failed;
	}

	if (Request->OverallLoginState == ECommonUserAsyncTaskState::Done)
	{
		// 登录成功后检查目标权限；有效缓存可直接完成，否则发起异步平台查询。
		// Do the permissions check if needed
		if (Request->PrivilegeCheckState == ECommonUserAsyncTaskState::NotStarted)
		{
			Request->PrivilegeCheckState = ECommonUserAsyncTaskState::InProgress;

			ECommonUserPrivilegeResult CachedResult = UserInfo->GetCachedPrivilegeResult(Request->DesiredPrivilege, Request->CurrentContext);
			if (CachedResult == ECommonUserPrivilegeResult::Available)
			{
				// 已缓存为可用时无需再次访问在线服务。
				// Use cached success value
				Request->PrivilegeCheckState = ECommonUserAsyncTaskState::Done;
			}
			else
			{
				if (QueryUserPrivilege(System, Request, PlatformUser))
				{
					return;
				}
				else
				{
#if !COMMONUSER_OSSV1
					// OSSv2 尚未实现权限查询期间临时按可用处理，避免阻断登录流程。
					// Temp while OSSv2 gets privileges implemented
					CachedResult = ECommonUserPrivilegeResult::Available;
					Request->PrivilegeCheckState = ECommonUserAsyncTaskState::Done;
#endif
				}
			}
		}

		if (Request->PrivilegeCheckState == ECommonUserAsyncTaskState::Failed)
		{
			// 目标权限不可用意味着本次登录目标未达成，因此把权限失败提升为整体登录失败。
			// Count a privilege failure as a login failure
			Request->OverallLoginState = ECommonUserAsyncTaskState::Failed;
		}
		else if (Request->PrivilegeCheckState == ECommonUserAsyncTaskState::Done)
		{
			// Game 登录先完成 Platform；若最终上下文是独立 Service，则重置子状态并开始第二阶段。
			// If platform context done but still need to do service context, do that next
			ECommonUserOnlineContext ResolvedDesiredContext = ResolveOnlineContext(Request->DesiredContext);

			if (Request->OverallLoginState == ECommonUserAsyncTaskState::Done && Request->CurrentContext != ResolvedDesiredContext)
			{
				Request->CurrentContext = ResolvedDesiredContext;
				Request->OverallLoginState = ECommonUserAsyncTaskState::NotStarted;
				Request->PrivilegeCheckState = ECommonUserAsyncTaskState::NotStarted;
				Request->TransferPlatformAuthState = ECommonUserAsyncTaskState::NotStarted;
				Request->AutoLoginState = ECommonUserAsyncTaskState::NotStarted;
				Request->LoginUIState = ECommonUserAsyncTaskState::NotStarted;

				// 立即递归推进新上下文，并返回以避免旧上下文继续进入登录完成分支。
				// Reprocess and immediately return
				ProcessLoginRequest(Request);
				return;
			}
		}
	}

	if (Request->PrivilegeCheckState == ECommonUserAsyncTaskState::InProgress)
	{
		// 权限查询仍在异步执行，等待回调更新结果后再次推进状态机。
		// Stall to wait for it to finish
		return;
	}

	// 请求进入终态后从活动集合移除，并只执行一次调用方完成委托。
	// If done, remove and do callback
	if (Request->OverallLoginState == ECommonUserAsyncTaskState::Done || Request->OverallLoginState == ECommonUserAsyncTaskState::Failed)
	{
		// 递归推进可能已经完成并移除请求，仅对仍在活动集合中的请求收尾。
		// Skip if this already happened in a nested function
		if (ActiveLoginRequests.Contains(Request))
		{
			// 失败路径没有底层错误时补充通用请求失败，保证调用方总能收到可诊断结果。
			// Add a generic error if none is set
			if (Request->OverallLoginState == ECommonUserAsyncTaskState::Failed && !Request->Error.IsSet())
			{
	#if COMMONUSER_OSSV1
				Request->Error = FOnlineError(NSLOCTEXT("CommonUser", "FailedToRequest", "Failed to Request Login"));
	#else
				Request->Error = UE::Online::Errors::RequestFailure();
	#endif
			}

			// 先移除活动请求，防止委托执行过程中重入导致重复完成。
			// Remove from active array
			ActiveLoginRequests.Remove(Request);

			// 返回最终登录状态、NetId、错误和调用方请求的逻辑上下文。
			// Execute delegate if bound
			Request->Delegate.ExecuteIfBound(UserInfo, CurrentStatus, CurrentId, Request->Error, Request->DesiredContext);
		}
	}
}

#if COMMONUSER_OSSV1
// 处理 OSSv1 登录完成事件：更新匹配请求的 AutoLogin 子状态和错误，然后重新推进登录状态机。
void UCommonUserSubsystem::HandleUserLoginCompleted(int32 PlatformUserIndex, bool bWasSuccessful, const FUniqueNetId& NetId, const FString& ErrorString, ECommonUserOnlineContext Context)
{
	FPlatformUserId PlatformUser = GetPlatformUserIdForIndex(PlatformUserIndex);
	ELoginStatusType NewStatus = GetLocalUserLoginStatus(PlatformUser, Context);
	FUniqueNetIdRepl NewId = FUniqueNetIdRepl(NetId);
	UE_LOG(LogCommonUser, Log, TEXT("Player login Completed - System:%s, UserIdx:%d, Successful:%d, NewStatus:%s, NewId:%s, ErrorIfAny:%s"),
		*GetOnlineSubsystemName(Context).ToString(),
		PlatformUserIndex,
		(int32)bWasSuccessful,
		ELoginStatus::ToString(NewStatus),
		*NewId.ToString(),
		*ErrorString);

	// 使用副本遍历，允许 ProcessLoginRequest 在回调期间从原数组移除已完成请求。
	// Update any waiting login requests
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		UCommonUserInfo* UserInfo = Request->UserInfo.Get();

		if (!UserInfo)
		{
			// 用户对象已销毁，丢弃无法再交付结果的请求。
			// User is gone, just delete this request
			ActiveLoginRequests.Remove(Request);

			continue;
		}

		if (UserInfo->PlatformUser == PlatformUser && Request->CurrentContext == Context)
		{
			// 某些平台的登录 UI 失败也会触发此事件；仅在 AutoLogin 正等待时更新该子状态。
			// On some platforms this gets called from the login UI with a failure
			if (Request->AutoLoginState == ECommonUserAsyncTaskState::InProgress)
			{
				Request->AutoLoginState = bWasSuccessful ? ECommonUserAsyncTaskState::Done : ECommonUserAsyncTaskState::Failed;
			}

			if (!bWasSuccessful)
			{
				Request->Error = FOnlineError(FText::FromString(ErrorString));
			}

			ProcessLoginRequest(Request);
		}
	}
}

// 处理 OSSv1 登录界面关闭事件：确定实际登录用户，记录 UI 成败，并恢复对应上下文的登录状态机。
void UCommonUserSubsystem::HandleOnLoginUIClosed(TSharedPtr<const FUniqueNetId> LoggedInNetId, const int PlatformUserIndex, const FOnlineError& Error, ECommonUserOnlineContext Context)
{
	FPlatformUserId PlatformUser = GetPlatformUserIdForIndex(PlatformUserIndex);

	// 在活动请求副本中寻找正在等待此上下文登录 UI 的请求。
	// Update any waiting login requests
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		UCommonUserInfo* UserInfo = Request->UserInfo.Get();

		if (!UserInfo)
		{
			// 用户对象已销毁，移除对应请求且不再回调。
			// User is gone, just delete this request
			ActiveLoginRequests.Remove(Request);

			continue;
		}

		// 登录 UI 一次只对应此上下文中首个等待交互登录的请求。
		// Look for first user trying to log in on this context
		if (Request->CurrentContext == Context && Request->LoginUIState == ECommonUserAsyncTaskState::InProgress)
		{
			if (LoggedInNetId.IsValid() && LoggedInNetId->IsValid() && Error.WasSuccessful())
			{
				// UI 中最终选中的平台账户可能与发起 UI 的账户不同；返回值有效时以实际账户为准。
				// The platform user id that actually logged in may not be the same one who requested the UI,
				// so swap it if the returned id is actually valid
				if (UserInfo->PlatformUser != PlatformUser && PlatformUser != PLATFORMUSERID_NONE)
				{
					UserInfo->PlatformUser = PlatformUser;
				}

				Request->LoginUIState = ECommonUserAsyncTaskState::Done;
				Request->Error.Reset();
			}
			else
			{
				Request->LoginUIState = ECommonUserAsyncTaskState::Failed;
				Request->Error = Error;
			}

			ProcessLoginRequest(Request);
		}
	}
}

// 处理 OSSv1 权限查询结果：转换平台位标志、更新用户缓存与连接状态，并完成等待该权限的登录请求。
void UCommonUserSubsystem::HandleCheckPrivilegesComplete(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 PrivilegeResults, ECommonUserPrivilege UserPrivilege, TWeakObjectPtr<UCommonUserInfo> CommonUserInfo, ECommonUserOnlineContext Context)
{
	// 异步查询完成前用户可能已被释放，此时结果已无接收者。
	// Only handle if user still exists
	UCommonUserInfo* UserInfo = CommonUserInfo.Get();

	if (!UserInfo)
	{
		return;
	}

	ECommonUserPrivilegeResult UserResult = ConvertOSSPrivilegeResult(Privilege, PrivilegeResults);

	// 写入上下文权限缓存，并在可用性变化时广播通知。
	// Update the user cached value
	UpdateUserPrivilegeResult(UserInfo, UserPrivilege, UserResult, Context);

	FOnlineContextCache* ContextCache = GetContextCache(Context);
	check(ContextCache);

	// 网络不可用结果同步到上下文连接状态；在线权限恢复时清除此前的断网状态。
	// If this returns disconnected, update the connection status
	if (UserResult == ECommonUserPrivilegeResult::NetworkConnectionUnavailable)
	{
		ContextCache->CurrentConnectionStatus = EOnlineServerConnectionStatus::NoNetworkConnection;
	}
	else if (UserResult == ECommonUserPrivilegeResult::Available && UserPrivilege == ECommonUserPrivilege::CanPlayOnline)
	{
		if (ContextCache->CurrentConnectionStatus == EOnlineServerConnectionStatus::NoNetworkConnection)
		{
			ContextCache->CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;
		}
	}
		
	// 唤醒正在等待同一用户、上下文和权限的登录请求。
	// See if a login request is waiting on this
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		if (Request->UserInfo.Get() == UserInfo && Request->CurrentContext == Context && Request->DesiredPrivilege == UserPrivilege && Request->PrivilegeCheckState == ECommonUserAsyncTaskState::InProgress)
		{
			if (UserResult == ECommonUserPrivilegeResult::Available)
			{
				Request->PrivilegeCheckState = ECommonUserAsyncTaskState::Done;
			}
			else
			{
				Request->PrivilegeCheckState = ECommonUserAsyncTaskState::Failed;

				// 将“权限结果原因”和“目标操作”组合成英文错误句，例如“不允许该用户进行在线游玩”。
				// Forms strings in english like "(The user is not allowed) to (play the game)"
				Request->Error = FOnlineError(FText::Format(NSLOCTEXT("CommonUser", "PrivilegeFailureFormat", "{0} to {1}"), GetPrivilegeResultDescription(UserResult), GetPrivilegeDescription(UserPrivilege)));
			}

			ProcessLoginRequest(Request);
		}
	}
}
#else

// 处理 OSSv2 Auth 登录结果：提取账户标识，更新匹配请求的 AutoLogin 状态和在线错误，再继续状态机。
void UCommonUserSubsystem::HandleUserLoginCompletedV2(const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result, FPlatformUserId PlatformUser, ECommonUserOnlineContext Context)
{
	const bool bWasSuccessful = Result.IsOk();
	FAccountId NewId;
	if (bWasSuccessful)
	{
		NewId = Result.GetOkValue().AccountInfo->AccountId;
	}
	
	ELoginStatusType NewStatus = GetLocalUserLoginStatus(PlatformUser, Context);
	UE_LOG(LogCommonUser, Log, TEXT("Player login Completed - System:%d, UserIdx:%d, Successful:%d, NewId:%s, ErrorIfAny:%s"),
		(int32)Context,
		PlatformUser.GetInternalId(),
		(int32)Result.IsOk(),
		*ToLogString(NewId),
		Result.IsError() ? *Result.GetErrorValue().GetLogString() : TEXT(""));

	// 使用活动请求副本遍历，使状态机可以在回调中安全移除原数组元素。
	// Update any waiting login requests
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		UCommonUserInfo* UserInfo = Request->UserInfo.Get();

		if (!UserInfo)
		{
			// 用户对象已经销毁，移除无法再完成的请求。
			// User is gone, just delete this request
			ActiveLoginRequests.Remove(Request);

			continue;
		}

		if (UserInfo->PlatformUser == PlatformUser && Request->CurrentContext == Context)
		{
			// 平台可能从登录 UI 失败路径触发同一 Auth 事件；只更新正在等待 AutoLogin 的请求。
			// On some platforms this gets called from the login UI with a failure
			if (Request->AutoLoginState == ECommonUserAsyncTaskState::InProgress)
			{
				Request->AutoLoginState = bWasSuccessful ? ECommonUserAsyncTaskState::Done : ECommonUserAsyncTaskState::Failed;
			}

			if (bWasSuccessful)
			{
				Request->Error.Reset();
			}
			else
			{
				Request->Error = Result.GetErrorValue();
			}

			ProcessLoginRequest(Request);
		}
	}
}

// 处理 OSSv2 外部登录 UI 关闭事件：接收实际平台用户和在线错误，并恢复等待 UI 的请求。
void UCommonUserSubsystem::HandleOnLoginUIClosedV2(const UE::Online::TOnlineResult<UE::Online::FExternalUIShowLoginUI>& Result, FPlatformUserId PlatformUser, ECommonUserOnlineContext Context)
{
	// 在活动请求副本中查找等待该上下文外部 UI 的请求。
	// Update any waiting login requests
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		UCommonUserInfo* UserInfo = Request->UserInfo.Get();

		if (!UserInfo)
		{
			// 用户对象已经销毁，直接移除失去接收者的请求。
			// User is gone, just delete this request
			ActiveLoginRequests.Remove(Request);

			continue;
		}

		// 处理此上下文中首个处于 UI 等待状态的请求。
		// Look for first user trying to log in on this context
		if (Request->CurrentContext == Context && Request->LoginUIState == ECommonUserAsyncTaskState::InProgress)
		{
			if (Result.IsOk())
			{
				// UI 中实际选中的平台账户可能不同于发起者；返回账户有效时更新用户归属。
				// The platform user id that actually logged in may not be the same one who requested the UI,
				// so swap it if the returned id is actually valid
				if (UserInfo->PlatformUser != PlatformUser && PlatformUser != PLATFORMUSERID_NONE)
				{
					UserInfo->PlatformUser = PlatformUser;
				}

				Request->LoginUIState = ECommonUserAsyncTaskState::Done;
				Request->Error.Reset();
			}
			else
			{
				Request->LoginUIState = ECommonUserAsyncTaskState::Failed;
				Request->Error = Result.GetErrorValue();
			}

			ProcessLoginRequest(Request);
		}
	}
}

// 处理 OSSv2 权限查询：将服务结果转换为 CommonUser 枚举、更新缓存，并结束等待该权限的登录步骤。
void UCommonUserSubsystem::HandleCheckPrivilegesComplete(const UE::Online::TOnlineResult<UE::Online::FQueryUserPrivilege>& Result, TWeakObjectPtr<UCommonUserInfo> CommonUserInfo, EUserPrivileges DesiredPrivilege, ECommonUserOnlineContext Context)
{
	// 异步完成时用户可能已释放，此时忽略过期结果。
	// Only handle if user still exists
	UCommonUserInfo* UserInfo = CommonUserInfo.Get();
	if (!UserInfo)
	{
		return;
	}

	ECommonUserPrivilege UserPrivilege = ConvertOnlineServicesPrivilege(DesiredPrivilege);
	ECommonUserPrivilegeResult UserResult = ECommonUserPrivilegeResult::PlatformFailure;
	if (const FQueryUserPrivilege::Result* OkResult = Result.TryGetOkValue())
	{
		UserResult = ConvertOnlineServicesPrivilegeResult(DesiredPrivilege, OkResult->PrivilegeResult);
	}
	else
	{
		UE_LOG(LogCommonUser, Warning, TEXT("QueryUserPrivilege failed: %s"), *Result.GetErrorValue().GetLogString());
	}

	// 保存转换后的权限结果，供后续登录和可用性查询复用。
	// Update the user cached value
	UserInfo->UpdateCachedPrivilegeResult(UserPrivilege, UserResult, Context);

	// 查找与用户、上下文和权限完全匹配的等待请求，并以查询结果恢复状态机。
	// See if a login request is waiting on this
	TArray<TSharedRef<FUserLoginRequest>> RequestsCopy = ActiveLoginRequests;
	for (TSharedRef<FUserLoginRequest>& Request : RequestsCopy)
	{
		if (Request->UserInfo.Get() == UserInfo && Request->CurrentContext == Context && Request->DesiredPrivilege == UserPrivilege && Request->PrivilegeCheckState == ECommonUserAsyncTaskState::InProgress)
		{
			if (UserResult == ECommonUserPrivilegeResult::Available)
			{
				Request->PrivilegeCheckState = ECommonUserAsyncTaskState::Done;
			}
			else
			{
				Request->PrivilegeCheckState = ECommonUserAsyncTaskState::Failed;
				Request->Error = Result.IsError() ? Result.GetErrorValue() : UE::Online::Errors::Unknown();
			}

			ProcessLoginRequest(Request);
		}
	}
}
#endif // COMMONUSER_OSSV1

// 刷新用户在 Default 以及独立 Platform 上下文中的 NetId 和显示名缓存。
void UCommonUserSubsystem::RefreshLocalUserInfo(UCommonUserInfo* UserInfo)
{
	if (ensure(UserInfo))
	{
		// Default 是所有配置都存在的基础上下文，始终刷新。
		// Always update default
		UserInfo->UpdateCachedNetId(GetLocalUserNetId(UserInfo->PlatformUser, ECommonUserOnlineContext::Default), ECommonUserOnlineContext::Default);

		if (HasSeparatePlatformContext())
		{
			// 平台服务与默认服务分离时，还需独立刷新 Platform 缓存。
			// Also update platform
			UserInfo->UpdateCachedNetId(GetLocalUserNetId(UserInfo->PlatformUser, ECommonUserOnlineContext::Platform), ECommonUserOnlineContext::Platform);
		}
	}
}

// 比较权限的旧、新可用性，仅在外部可观察状态实际变化时广播通知。
void UCommonUserSubsystem::HandleChangedAvailability(UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserAvailability OldAvailability)
{
	ECommonUserAvailability NewAvailability = UserInfo->GetPrivilegeAvailability(Privilege);

	if (OldAvailability != NewAvailability)
	{
		OnUserPrivilegeChanged.Broadcast(UserInfo, Privilege, OldAvailability, NewAvailability);
	}
}

// 更新指定在线上下文的权限缓存，并将由此产生的聚合可用性变化通知监听方。
void UCommonUserSubsystem::UpdateUserPrivilegeResult(UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult Result, ECommonUserOnlineContext Context)
{
	check(UserInfo);
	
	ECommonUserAvailability OldAvailability = UserInfo->GetPrivilegeAvailability(Privilege);

	UserInfo->UpdateCachedPrivilegeResult(Privilege, Result, Context);

	HandleChangedAvailability(UserInfo, Privilege, OldAvailability);
}

#if COMMONUSER_OSSV1
// 将 OSSv1 权限类型映射为 CommonUser 权限；旧接口无法区分文本和语音通信权限。
ECommonUserPrivilege UCommonUserSubsystem::ConvertOSSPrivilege(EUserPrivileges::Type Privilege) const
{
	switch (Privilege)
	{
	case EUserPrivileges::CanPlay:
		return ECommonUserPrivilege::CanPlay;
	case EUserPrivileges::CanPlayOnline:
		return ECommonUserPrivilege::CanPlayOnline;
	case EUserPrivileges::CanCommunicateOnline:
		// OSSv1 只有统一的在线通信权限，这里选择映射到文本通信。
		return ECommonUserPrivilege::CanCommunicateViaTextOnline; // No good thing to do here, just mapping to text.
	case EUserPrivileges::CanUseUserGeneratedContent:
		return ECommonUserPrivilege::CanUseUserGeneratedContent;
	case EUserPrivileges::CanUserCrossPlay:
		return ECommonUserPrivilege::CanUseCrossPlay;
	default:
		return ECommonUserPrivilege::Invalid_Count;
	}
}

// 将 CommonUser 权限映射到 OSSv1；文本和语音通信会合并为同一个 CanCommunicateOnline 查询。
EUserPrivileges::Type UCommonUserSubsystem::ConvertOSSPrivilege(ECommonUserPrivilege Privilege) const
{
	switch (Privilege)
	{
	case ECommonUserPrivilege::CanPlay:
		return EUserPrivileges::CanPlay;
	case ECommonUserPrivilege::CanPlayOnline:
		return EUserPrivileges::CanPlayOnline;
	case ECommonUserPrivilege::CanCommunicateViaTextOnline:
	case ECommonUserPrivilege::CanCommunicateViaVoiceOnline:
		return EUserPrivileges::CanCommunicateOnline;
	case ECommonUserPrivilege::CanUseUserGeneratedContent:
		return EUserPrivileges::CanUseUserGeneratedContent;
	case ECommonUserPrivilege::CanUseCrossPlay:
		return EUserPrivileges::CanUserCrossPlay;
	default:
		// OSSv1 没有无效权限枚举，未知输入回退到最基础的 CanPlay。
		// No failure type, return CanPlay
		return EUserPrivileges::CanPlay;
	}
}

// 按优先级解释 OSSv1 权限失败位标志，并归一化为 CommonUser 可展示的单一结果。
ECommonUserPrivilegeResult UCommonUserSubsystem::ConvertOSSPrivilegeResult(EUserPrivileges::Type Privilege, uint32 Results) const
{
	// OSSv1 使用位标志返回多个原因，各平台组合方式并不完全一致，因此按明确优先级归类。
	// The V1 results enum is a bitfield where each platform behaves a bit differently
	if (Results == (uint32)IOnlineIdentity::EPrivilegeResults::NoFailures)
	{
		return ECommonUserPrivilegeResult::Available;
	}
	if ((Results & (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound) || (Results & (uint32)IOnlineIdentity::EPrivilegeResults::UserNotLoggedIn))
	{
		return ECommonUserPrivilegeResult::UserNotLoggedIn;
	}
	if ((Results & (uint32)IOnlineIdentity::EPrivilegeResults::RequiredPatchAvailable) || (Results & (uint32)IOnlineIdentity::EPrivilegeResults::RequiredSystemUpdate))
	{
		return ECommonUserPrivilegeResult::VersionOutdated;
	}
	if (Results & (uint32)IOnlineIdentity::EPrivilegeResults::AgeRestrictionFailure)
	{
		return ECommonUserPrivilegeResult::AgeRestricted;
	}
	if (Results & (uint32)IOnlineIdentity::EPrivilegeResults::AccountTypeFailure)
	{
		return ECommonUserPrivilegeResult::AccountTypeRestricted;
	}
	if (Results & (uint32)IOnlineIdentity::EPrivilegeResults::NetworkConnectionUnavailable)
	{
		return ECommonUserPrivilegeResult::NetworkConnectionUnavailable;
	}

	// 在线游玩、UGC 和聊天限制统一归为账户使用受限。
	// Bucket other account failures together
	uint32 AccountUseFailures = (uint32)IOnlineIdentity::EPrivilegeResults::OnlinePlayRestricted 
		| (uint32)IOnlineIdentity::EPrivilegeResults::UGCRestriction 
		| (uint32)IOnlineIdentity::EPrivilegeResults::ChatRestriction;

	if (Results & AccountUseFailures)
	{
		return ECommonUserPrivilegeResult::AccountUseRestricted;
	}

	// 最基础的 CanPlay 仍失败时，将剩余原因解释为游戏许可无效。
	// If you can't play at all, this is a license failure
	if (Privilege == EUserPrivileges::CanPlay)
	{
		return ECommonUserPrivilegeResult::LicenseInvalid;
	}

	// 无法识别的位组合保留为平台失败，避免误报为具体账户限制。
	// Unknown reason
	return ECommonUserPrivilegeResult::PlatformFailure;
}
#else
// 将 OSSv2 权限枚举逐项映射为 CommonUser 权限，保留文本与语音通信的区分。
ECommonUserPrivilege UCommonUserSubsystem::ConvertOnlineServicesPrivilege(EUserPrivileges Privilege) const
{
	switch (Privilege)
	{
	case EUserPrivileges::CanPlay:
		return ECommonUserPrivilege::CanPlay;
	case EUserPrivileges::CanPlayOnline:
		return ECommonUserPrivilege::CanPlayOnline;
	case EUserPrivileges::CanCommunicateViaTextOnline:
		return ECommonUserPrivilege::CanCommunicateViaTextOnline;
	case EUserPrivileges::CanCommunicateViaVoiceOnline:
		return ECommonUserPrivilege::CanCommunicateViaVoiceOnline;
	case EUserPrivileges::CanUseUserGeneratedContent:
		return ECommonUserPrivilege::CanUseUserGeneratedContent;
	case EUserPrivileges::CanCrossPlay:
		return ECommonUserPrivilege::CanUseCrossPlay;
	default:
		return ECommonUserPrivilege::Invalid_Count;
	}
}

// 将 CommonUser 权限映射到 OSSv2 查询枚举；无效输入回退到基础 CanPlay。
EUserPrivileges UCommonUserSubsystem::ConvertOnlineServicesPrivilege(ECommonUserPrivilege Privilege) const
{
	switch (Privilege)
	{
	case ECommonUserPrivilege::CanPlay:
		return EUserPrivileges::CanPlay;
	case ECommonUserPrivilege::CanPlayOnline:
		return EUserPrivileges::CanPlayOnline;
	case ECommonUserPrivilege::CanCommunicateViaTextOnline:
		return EUserPrivileges::CanCommunicateViaTextOnline;
	case ECommonUserPrivilege::CanCommunicateViaVoiceOnline:
		return EUserPrivileges::CanCommunicateViaVoiceOnline;
	case ECommonUserPrivilege::CanUseUserGeneratedContent:
		return EUserPrivileges::CanUseUserGeneratedContent;
	case ECommonUserPrivilege::CanUseCrossPlay:
		return EUserPrivileges::CanCrossPlay;
	default:
		// OSSv2 没有专用失败类型，未知权限只能回退到 CanPlay。
		// No failure type, return CanPlay
		return EUserPrivileges::CanPlay;
	}
}

// 解释 OSSv2 权限结果标志，并按用户可处理的原因归一化为 CommonUser 结果。
ECommonUserPrivilegeResult UCommonUserSubsystem::ConvertOnlineServicesPrivilegeResult(EUserPrivileges Privilege, EPrivilegeResults Results) const
{
	// OSSv2 同样可能组合多个失败标志，按稳定优先级选择最有行动意义的结果。
	// The V1 results enum is a bitfield where each platform behaves a bit differently
	if (Results == EPrivilegeResults::NoFailures)
	{
		return ECommonUserPrivilegeResult::Available;
	}
	if (EnumHasAnyFlags(Results, EPrivilegeResults::UserNotFound | EPrivilegeResults::UserNotLoggedIn))
	{
		return ECommonUserPrivilegeResult::UserNotLoggedIn;
	}
	if (EnumHasAnyFlags(Results, EPrivilegeResults::RequiredPatchAvailable | EPrivilegeResults::RequiredSystemUpdate))
	{
		return ECommonUserPrivilegeResult::VersionOutdated;
	}
	if (EnumHasAnyFlags(Results, EPrivilegeResults::AgeRestrictionFailure))
	{
		return ECommonUserPrivilegeResult::AgeRestricted;
	}
	if (EnumHasAnyFlags(Results, EPrivilegeResults::AccountTypeFailure))
	{
		return ECommonUserPrivilegeResult::AccountTypeRestricted;
	}
	if (EnumHasAnyFlags(Results, EPrivilegeResults::NetworkConnectionUnavailable))
	{
		return ECommonUserPrivilegeResult::NetworkConnectionUnavailable;
	}

	// 在线游玩、UGC 和聊天限制统一归为账户使用受限。
	// Bucket other account failures together
	const EPrivilegeResults AccountUseFailures = EPrivilegeResults::OnlinePlayRestricted
		| EPrivilegeResults::UGCRestriction
		| EPrivilegeResults::ChatRestriction;

	if (EnumHasAnyFlags(Results, AccountUseFailures))
	{
		return ECommonUserPrivilegeResult::AccountUseRestricted;
	}

	// 基础 CanPlay 权限失败且无更具体原因时，归类为许可无效。
	// If you can't play at all, this is a license failure
	if (Privilege == EUserPrivileges::CanPlay)
	{
		return ECommonUserPrivilegeResult::LicenseInvalid;
	}

	// 未识别的标志组合保留为平台失败。
	// Unknown reason
	return ECommonUserPrivilegeResult::PlatformFailure;
}
#endif // COMMONUSER_OSSV1

// 将 PlatformUserId 转为日志文本，对无效用户使用明确的 None 标记。
FString UCommonUserSubsystem::PlatformUserIdToString(FPlatformUserId UserId)
{
	if (UserId == PLATFORMUSERID_NONE)
	{
		return TEXT("None");
	}
	else
	{
		return FString::Printf(TEXT("%d"), UserId.GetInternalId());
	}
}

// 将逻辑在线上下文枚举转为稳定的调试文本，包括带回退语义的上下文类型。
FString UCommonUserSubsystem::ECommonUserOnlineContextToString(ECommonUserOnlineContext Context)
{
	switch (Context)
	{
	case ECommonUserOnlineContext::Game:
		return TEXT("Game");
	case ECommonUserOnlineContext::Default:
		return TEXT("Default");
	case ECommonUserOnlineContext::Service:
		return TEXT("Service");
	case ECommonUserOnlineContext::ServiceOrDefault:
		return TEXT("Service/Default");
	case ECommonUserOnlineContext::Platform:
		return TEXT("Platform");
	case ECommonUserOnlineContext::PlatformOrDefault:
		return TEXT("Platform/Default");
	default:
		return TEXT("Invalid");
	}
}

// 返回权限动作的本地化短语，用于与失败原因组合成面向用户的错误信息。
FText UCommonUserSubsystem::GetPrivilegeDescription(ECommonUserPrivilege Privilege) const
{
	switch (Privilege)
	{
	case ECommonUserPrivilege::CanPlay:
		return NSLOCTEXT("CommonUser", "PrivilegeCanPlay", "play the game");
	case ECommonUserPrivilege::CanPlayOnline:
		return NSLOCTEXT("CommonUser", "PrivilegeCanPlayOnline", "play online");
	case ECommonUserPrivilege::CanCommunicateViaTextOnline:
		return NSLOCTEXT("CommonUser", "PrivilegeCanCommunicateViaTextOnline", "communicate with text");
	case ECommonUserPrivilege::CanCommunicateViaVoiceOnline:
		return NSLOCTEXT("CommonUser", "PrivilegeCanCommunicateViaVoiceOnline", "communicate with voice");
	case ECommonUserPrivilege::CanUseUserGeneratedContent:
		return NSLOCTEXT("CommonUser", "PrivilegeCanUseUserGeneratedContent", "access user content");
	case ECommonUserPrivilege::CanUseCrossPlay:
		return NSLOCTEXT("CommonUser", "PrivilegeCanUseCrossPlay", "play with other platforms");
	default:
		return NSLOCTEXT("CommonUser", "PrivilegeInvalid", "Invalid");
	}
}

// 将权限结果转换为面向用户的本地化原因文本；平台认证要求可能需要项目按主机覆盖。
FText UCommonUserSubsystem::GetPrivilegeResultDescription(ECommonUserPrivilegeResult Result) const
{
	// TODO：这些提示可能受主机认证规范约束，项目可能需要针对不同平台覆盖。
	// TODO these strings might have cert requirements we need to override per console
	switch (Result)
	{
	case ECommonUserPrivilegeResult::Unknown:
		return NSLOCTEXT("CommonUser", "ResultUnknown", "Unknown if the user is allowed");
	case ECommonUserPrivilegeResult::Available:
		return NSLOCTEXT("CommonUser", "ResultAvailable", "The user is allowed");
	case ECommonUserPrivilegeResult::UserNotLoggedIn:
		return NSLOCTEXT("CommonUser", "ResultUserNotLoggedIn", "The user must login");
	case ECommonUserPrivilegeResult::LicenseInvalid:
		return NSLOCTEXT("CommonUser", "ResultLicenseInvalid", "A valid game license is required");
	case ECommonUserPrivilegeResult::VersionOutdated:
		return NSLOCTEXT("CommonUser", "VersionOutdated", "The game or hardware needs to be updated");
	case ECommonUserPrivilegeResult::NetworkConnectionUnavailable:
		return NSLOCTEXT("CommonUser", "ResultNetworkConnectionUnavailable", "A network connection is required");
	case ECommonUserPrivilegeResult::AgeRestricted:
		return NSLOCTEXT("CommonUser", "ResultAgeRestricted", "This age restricted account is not allowed");
	case ECommonUserPrivilegeResult::AccountTypeRestricted:
		return NSLOCTEXT("CommonUser", "ResultAccountTypeRestricted", "This account type does not have access");
	case ECommonUserPrivilegeResult::AccountUseRestricted:
		return NSLOCTEXT("CommonUser", "ResultAccountUseRestricted", "This account is not allowed");
	case ECommonUserPrivilegeResult::PlatformFailure:
		return NSLOCTEXT("CommonUser", "ResultPlatformFailure", "Not allowed");
	default:
		return NSLOCTEXT("CommonUser", "ResultInvalid", "Invalid");

	}
}

// 只读版本复用可变上下文查找逻辑，并保持返回缓存不可修改。
const UCommonUserSubsystem::FOnlineContextCache* UCommonUserSubsystem::GetContextCache(ECommonUserOnlineContext Context) const
{
	return const_cast<UCommonUserSubsystem*>(this)->GetContextCache(Context);
}

// 按精确或回退上下文选择 Default、Service、Platform 缓存；不可满足的精确上下文返回空。
UCommonUserSubsystem::FOnlineContextCache* UCommonUserSubsystem::GetContextCache(ECommonUserOnlineContext Context)
{
	switch (Context)
	{
	case ECommonUserOnlineContext::Game:
	case ECommonUserOnlineContext::Default:
		return DefaultContextInternal;

	case ECommonUserOnlineContext::Service:
		return ServiceContextInternal;
	case ECommonUserOnlineContext::ServiceOrDefault:
		return ServiceContextInternal ? ServiceContextInternal : DefaultContextInternal;

	case ECommonUserOnlineContext::Platform:
		return PlatformContextInternal;
	case ECommonUserOnlineContext::PlatformOrDefault:
		return PlatformContextInternal ? PlatformContextInternal : DefaultContextInternal;
	}

	return nullptr;
}

// 将 Game 和带 OrDefault 的逻辑上下文解析为当前配置中实际存在的具体在线上下文。
ECommonUserOnlineContext UCommonUserSubsystem::ResolveOnlineContext(ECommonUserOnlineContext Context) const
{
	switch (Context)
	{
	case ECommonUserOnlineContext::Game:
	case ECommonUserOnlineContext::Default:
		return ECommonUserOnlineContext::Default;

	case ECommonUserOnlineContext::Service:
		return ServiceContextInternal ? ECommonUserOnlineContext::Service : ECommonUserOnlineContext::Invalid;
	case ECommonUserOnlineContext::ServiceOrDefault:
		return ServiceContextInternal ? ECommonUserOnlineContext::Service : ECommonUserOnlineContext::Default;

	case ECommonUserOnlineContext::Platform:
		return PlatformContextInternal ? ECommonUserOnlineContext::Platform : ECommonUserOnlineContext::Invalid;
	case ECommonUserOnlineContext::PlatformOrDefault:
		return PlatformContextInternal ? ECommonUserOnlineContext::Platform : ECommonUserOnlineContext::Default;
	}

	return  ECommonUserOnlineContext::Invalid;
}

// 判断平台身份服务与游戏默认服务是否解析到不同上下文，从而决定是否维护两套用户缓存。
bool UCommonUserSubsystem::HasSeparatePlatformContext() const
{
	ECommonUserOnlineContext ServiceType = ResolveOnlineContext(ECommonUserOnlineContext::ServiceOrDefault);
	ECommonUserOnlineContext PlatformType = ResolveOnlineContext(ECommonUserOnlineContext::PlatformOrDefault);

	if (ServiceType != PlatformType)
	{
		return true;
	}
	return false;
}

// 把 CommonUser 身份同步到 LocalPlayer，并在可用时同步 PlayerState 的网络唯一标识。
void UCommonUserSubsystem::SetLocalPlayerUserInfo(ULocalPlayer* LocalPlayer, const UCommonUserInfo* UserInfo)
{
	if (!bIsDedicatedServer && ensure(LocalPlayer && UserInfo))
	{
		LocalPlayer->SetPlatformUserId(UserInfo->GetPlatformUserId());

		FUniqueNetIdRepl NetId = UserInfo->GetNetId(ECommonUserOnlineContext::Game);
		LocalPlayer->SetCachedUniqueNetId(NetId);

		// PlayerState 已创建时同步 NetId，确保复制层和玩法代码看到同一身份。
		// Also update player state if possible
		APlayerController* PlayerController = LocalPlayer->GetPlayerController(nullptr);
		if (PlayerController && PlayerController->PlayerState)
		{
			PlayerController->PlayerState->SetUniqueId(NetId);
		}
	}
}

// 按本地玩家槽位查找已登记的 CommonUserInfo，未初始化的槽位返回空。
const UCommonUserInfo* UCommonUserSubsystem::GetUserInfoForLocalPlayerIndex(int32 LocalPlayerIndex) const
{
	TObjectPtr<UCommonUserInfo> const* Found = LocalUserInfos.Find(LocalPlayerIndex);
	if (Found)
	{
		return *Found;
	}
	return nullptr;
}

// 先把旧式平台用户索引转换为 PlatformUserId，再查找对应的非访客用户信息。
const UCommonUserInfo* UCommonUserSubsystem::GetUserInfoForPlatformUserIndex(int32 PlatformUserIndex) const
{
	FPlatformUserId PlatformUser = GetPlatformUserIdForIndex(PlatformUserIndex);
	return GetUserInfoForPlatformUser(PlatformUser);
}

// 按 PlatformUserId 查找真实且非访客的本地用户，避免把共享平台身份错误匹配给访客。
const UCommonUserInfo* UCommonUserSubsystem::GetUserInfoForPlatformUser(FPlatformUserId PlatformUser) const
{
	if (!IsRealPlatformUser(PlatformUser))
	{
		return nullptr;
	}

	for (TPair<int32, UCommonUserInfo*> Pair : LocalUserInfos)
	{
		// 访客可能复用平台身份，不参与按 PlatformUserId 的唯一匹配。
		// Don't include guest users in this check
		if (ensure(Pair.Value) && Pair.Value->PlatformUser == PlatformUser && !Pair.Value->bIsGuest)
		{
			return Pair.Value;
		}
	}

	return nullptr;
}

// 在所有用户的各在线上下文缓存中查找匹配 NetId 的用户；无效 NetId 不参与匹配。
const UCommonUserInfo* UCommonUserSubsystem::GetUserInfoForUniqueNetId(const FUniqueNetIdRepl& NetId) const
{
	if (!NetId.IsValid())
	{
		// TODO：移动平台登录前 NetId 可能无效，后续需确认是否需要支持该阶段的身份查找。
		// TODO do we need to handle pre-login case on mobile platforms where netID is invalid?
		return nullptr;
	}

	for (TPair<int32, UCommonUserInfo*> UserPair : LocalUserInfos)
	{
		if (ensure(UserPair.Value))
		{
			for (const TPair<ECommonUserOnlineContext, UCommonUserInfo::FCachedData>& CachedPair : UserPair.Value->CachedDataMap)
			{
				if (NetId == CachedPair.Value.CachedNetId)
				{
					return UserPair.Value;
				}
			}
		}
	}

	return nullptr;
}

// 通过输入设备映射器把旧式 ControllerId 转为平台用户，并返回其 CommonUserInfo。
const UCommonUserInfo* UCommonUserSubsystem::GetUserInfoForControllerId(int32 ControllerId) const
{
	FPlatformUserId PlatformUser;
	FInputDeviceId IgnoreDevice;

	IPlatformInputDeviceMapper::Get().RemapControllerIdToPlatformUserAndDevice(ControllerId, PlatformUser, IgnoreDevice);

	return GetUserInfoForPlatformUser(PlatformUser);
}

// 将输入设备解析为平台用户，再查找该用户对应的非访客 CommonUserInfo。
const UCommonUserInfo* UCommonUserSubsystem::GetUserInfoForInputDevice(FInputDeviceId InputDevice) const
{
	FPlatformUserId PlatformUser = GetPlatformUserIdForInputDevice(InputDevice);
	return GetUserInfoForPlatformUser(PlatformUser);
}

// 验证旧式平台用户索引是否可分配：拒绝负值、超出 OSSv1 上限以及单用户平台的次要槽位。
bool UCommonUserSubsystem::IsRealPlatformUserIndex(int32 PlatformUserIndex) const
{
	if (PlatformUserIndex < 0)
	{
		return false;
	}

#if COMMONUSER_OSSV1
	if (PlatformUserIndex >= MAX_LOCAL_PLAYERS)
	{
		// OSSv1 的本地用户索引不能超过引擎支持的本地玩家数量。
		// Check against OSS count
		return false;
	}
#else
	// TODO：确认 OSSv2 是否提供等价的本地玩家数量上限。
	// TODO:  OSSv2 define MAX_LOCAL_PLAYERS?
#endif

	if (PlatformUserIndex > 0 && GetTraitTags().HasTag(FCommonUserTags::Platform_Trait_SingleOnlineUser))
	{
		return false;
	}

	return true;
}

// 验证类型化 PlatformUserId，并在单在线用户平台上仅接受输入映射器的主用户。
bool UCommonUserSubsystem::IsRealPlatformUser(FPlatformUserId PlatformUser) const
{
	// PlatformUserId 在分配和转换时已完成范围验证，此处只检查类型有效性。
	// Validation is done at conversion/allocation time so trust the type
	if (!PlatformUser.IsValid())
	{
		return false;
	}

	// TODO：后续可增加与在线服务或输入映射器状态的一致性校验。
	// TODO: Validate against OSS or input mapper somehow

	if (GetTraitTags().HasTag(FCommonUserTags::Platform_Trait_SingleOnlineUser))
	{
		// 单在线用户平台只有主平台用户具备在线功能。
		// Only the default user is supports online functionality 
		if (PlatformUser != IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser())
		{
			return false;
		}
	}

	return true;
}

// 使用平台输入设备映射器把顺序用户索引转换为稳定的 PlatformUserId。
FPlatformUserId UCommonUserSubsystem::GetPlatformUserIdForIndex(int32 PlatformUserIndex) const
{
	return IPlatformInputDeviceMapper::Get().GetPlatformUserForUserIndex(PlatformUserIndex);
}

// 使用平台输入设备映射器获取 PlatformUserId 对应的顺序用户索引。
int32 UCommonUserSubsystem::GetPlatformUserIndexForId(FPlatformUserId PlatformUser) const
{
	return IPlatformInputDeviceMapper::Get().GetUserIndexForPlatformUser(PlatformUser);
}

// 查询当前拥有指定输入设备的平台用户。
FPlatformUserId UCommonUserSubsystem::GetPlatformUserIdForInputDevice(FInputDeviceId InputDevice) const
{
	return IPlatformInputDeviceMapper::Get().GetUserForInputDevice(InputDevice);
}

// 查询平台用户当前的主输入设备，用于控制器归属和登录输入处理。
FInputDeviceId UCommonUserSubsystem::GetPrimaryInputDeviceForPlatformUser(FPlatformUserId PlatformUser) const
{
	return IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(PlatformUser);
}

// 替换缓存的平台特性标签，后续用户有效性和启动输入策略会据此判断。
void UCommonUserSubsystem::SetTraitTags(const FGameplayTagContainer& InTags)
{
	CachedTraitTags = InTags;
}

// 多用户平台等待玩家按键选择身份；单用户平台可直接使用唯一主用户而无需等待输入。
bool UCommonUserSubsystem::ShouldWaitForStartInput() const
{
	// 单用户平台默认不等待开始输入，以便直接进入主用户初始化流程。
	// By default, don't wait for input if this is a single user platform
	return !HasTraitTag(FCommonUserTags::Platform_Trait_SingleOnlineUser.GetTag());
}

#if COMMONUSER_OSSV1
// 处理 OSSv1 身份登录状态变化；检测到已登录用户退出时，重置该平台用户的本地登录状态。
void UCommonUserSubsystem::HandleIdentityLoginStatusChanged(int32 PlatformUserIndex, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId, ECommonUserOnlineContext Context)
{
	UE_LOG(LogCommonUser, Log, TEXT("Player login status changed - System:%s, UserIdx:%d, OldStatus:%s, NewStatus:%s, NewId:%s"),
		*GetOnlineSubsystemName(Context).ToString(),
		PlatformUserIndex,
		ELoginStatus::ToString(OldStatus),
		ELoginStatus::ToString(NewStatus),
		*NewId.ToString());

	if (NewStatus == ELoginStatus::NotLoggedIn && OldStatus != ELoginStatus::NotLoggedIn)
	{
		FPlatformUserId PlatformUser = GetPlatformUserIdForIndex(PlatformUserIndex);
		LogOutLocalUser(PlatformUser);
	}
}

// 处理控制器账户配对变化：识别平台界面主动登出，并避免把旧账户继续保留在本地玩家上。
void UCommonUserSubsystem::HandleControllerPairingChanged(int32 PlatformUserIndex, FControllerPairingChangedUserInfo PreviousUser, FControllerPairingChangedUserInfo NewUser)
{
	UE_LOG(LogCommonUser, Log, TEXT("Player controller pairing changed - UserIdx:%d, PreviousUser:%s, NewUser:%s"),
		PlatformUserIndex,
		*ToDebugString(PreviousUser),
		*ToDebugString(NewUser));

	UGameInstance* GameInstance = GetGameInstance();
	FPlatformUserId PlatformUser = GetPlatformUserIdForIndex(PlatformUserIndex);
	ULocalPlayer* ControlledLocalPlayer = GameInstance->FindLocalPlayerFromPlatformUserId(PlatformUser);
	ULocalPlayer* NewLocalPlayer = GameInstance->FindLocalPlayerFromUniqueNetId(NewUser.User);
	const UCommonUserInfo* NewUserInfo = GetUserInfoForUniqueNetId(FUniqueNetIdRepl(NewUser.User));
	const UCommonUserInfo* PreviousUserInfo = GetUserInfoForUniqueNetId(FUniqueNetIdRepl(PreviousUser.User));

	// 检查失去全部控制器的旧账户是否仍绑定到现有本地玩家。
	// See if we think this is already bound to an existing player	
	if (PreviousUser.ControllersRemaining == 0 && PreviousUserInfo && PreviousUserInfo != NewUserInfo)
	{
		// 旧账户与新账户不同且不再拥有控制器，视为用户通过平台界面主动登出。
		// This means that the user deliberately logged out using a platform interface
		if (IsRealPlatformUser(PlatformUser))
		{
			LogOutLocalUser(PlatformUser);
		}
	}

	if (ControlledLocalPlayer && ControlledLocalPlayer != NewLocalPlayer)
	{
		// TODO：当前触发该委托的平台尚不能可靠处理 ControllerId 交换，因此暂不重绑本地玩家。
		// TODO Currently the platforms that call this delegate do not really handle swapping controller IDs
		// SetLocalPlayerUserIndex(ControlledLocalPlayer, -1);
	}
}

// 处理 OSSv1 网络连接状态变化：先保存用户旧可用性，更新上下文连接缓存，再广播在线权限变化。
void UCommonUserSubsystem::HandleNetworkConnectionStatusChanged(const FString& ServiceName, EOnlineServerConnectionStatus::Type LastConnectionStatus, EOnlineServerConnectionStatus::Type ConnectionStatus, ECommonUserOnlineContext Context)
{
	UE_LOG(LogCommonUser, Log, TEXT("HandleNetworkConnectionStatusChanged(ServiceName: %s, LastStatus: %s, ConnectionStatus: %s)"),
		*ServiceName,
		EOnlineServerConnectionStatus::ToString(LastConnectionStatus),
		EOnlineServerConnectionStatus::ToString(ConnectionStatus));

	// 在修改连接状态前保存每个用户的在线权限可用性，用于精确检测变化。
	// Cache old availablity for current users
	TMap<UCommonUserInfo*, ECommonUserAvailability> AvailabilityMap;

	for (TPair<int32, UCommonUserInfo*> Pair : LocalUserInfos)
	{
		AvailabilityMap.Add(Pair.Value, Pair.Value->GetPrivilegeAvailability(ECommonUserPrivilege::CanPlayOnline));
	}

	FOnlineContextCache* System = GetContextCache(Context);
	if (ensure(System))
	{
		// ServiceName 通常等于 OSS 名称，但部分平台会报告不同服务名，因此按传入上下文更新缓存。
		// Service name is normally the same as the OSS name, but not necessarily on all platforms
		System->CurrentConnectionStatus = ConnectionStatus;
	}

	for (TPair<UCommonUserInfo*, ECommonUserAvailability> Pair : AvailabilityMap)
	{
		// 仅当连接变化确实改变用户在线权限可用性时通知其他系统。
		// Notify other systems when someone goes online/offline
		HandleChangedAvailability(Pair.Key, ECommonUserPrivilege::CanPlayOnline, Pair.Value);
	}

}
#else
// 记录 OSSv2 Auth 登录状态事件；当前实现只提供诊断日志，不直接修改本地用户状态。
void UCommonUserSubsystem::HandleAuthLoginStatusChanged(const UE::Online::FAuthLoginStatusChanged& EventParameters, ECommonUserOnlineContext Context)
{
	UE_LOG(LogCommonUser, Log, TEXT("Player login status changed - System:%d, UserId:%s, NewStatus:%s"),
		(int)Context,
		*ToLogString(EventParameters.AccountInfo->AccountId),
		LexToString(EventParameters.LoginStatus));
}

// 处理 OSSv2 连接状态事件：更新上下文缓存，并按变化前后的 CanPlayOnline 可用性广播通知。
void UCommonUserSubsystem::HandleNetworkConnectionStatusChanged(const UE::Online::FConnectionStatusChanged& EventParameters, ECommonUserOnlineContext Context)
{
	UE_LOG(LogCommonUser, Log, TEXT("HandleNetworkConnectionStatusChanged(Context:%d, ServiceName:%s, OldStatus:%s, NewStatus:%s)"),
		(int)Context,
		*EventParameters.ServiceName,
		LexToString(EventParameters.PreviousStatus),
		LexToString(EventParameters.CurrentStatus));

	// 先保存用户旧的在线权限可用性，避免连接缓存更新后丢失比较基准。
	// Cache old availablity for current users
	TMap<UCommonUserInfo*, ECommonUserAvailability> AvailabilityMap;

	for (TPair<int32, UCommonUserInfo*> Pair : LocalUserInfos)
	{
		AvailabilityMap.Add(Pair.Value, Pair.Value->GetPrivilegeAvailability(ECommonUserPrivilege::CanPlayOnline));
	}

	FOnlineContextCache* System = GetContextCache(Context);
	if (ensure(System))
	{
		// 服务名不一定与上下文配置名一致，因此以委托绑定时携带的 Context 定位缓存。
		// Service name is normally the same as the OSS name, but not necessarily on all platforms
		System->CurrentConnectionStatus = EventParameters.CurrentStatus;
	}

	for (TPair<UCommonUserInfo*, ECommonUserAvailability> Pair : AvailabilityMap)
	{
		// 对每个本地用户比较新旧聚合可用性，仅广播实际发生的上线或离线变化。
		// Notify other systems when someone goes online/offline
		HandleChangedAvailability(Pair.Key, ECommonUserPrivilege::CanPlayOnline, Pair.Value);
	}
}
#endif // COMMONUSER_OSSV1

// 记录输入设备连接变化；当前仅保留诊断信息，平台专用断连处理尚待实现。
void UCommonUserSubsystem::HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId)
{
	FString InputDeviceIDString = FString::Printf(TEXT("%d"), InputDeviceId.GetId());
	const bool bIsConnected = NewConnectionState == EInputDeviceConnectionState::Connected;
	UE_LOG(LogCommonUser, Log, TEXT("Controller connection changed - UserIdx:%s, UserID:%s, Connected:%d"), *InputDeviceIDString, *PlatformUserIdToString(PlatformUserId), bIsConnected ? 1 : 0);

	// TODO：为支持设备热插拔语义的平台实现用户状态和输入归属更新。
	// TODO Implement for platforms that support this
}

