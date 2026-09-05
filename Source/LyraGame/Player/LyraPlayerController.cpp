// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerController.h"
#include "CommonInputTypeEnum.h"
#include "Components/PrimitiveComponent.h"
#include "LyraLogChannels.h"
#include "LyraCheatManager.h"
#include "LyraPlayerState.h"
#include "Camera/LyraPlayerCameraManager.h"
#include "UI/LyraHUD.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "EngineUtils.h"
#include "LyraGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemGlobals.h"
#include "CommonInputSubsystem.h"
#include "LyraLocalPlayer.h"
#include "GameModes/LyraGameState.h"
#include "Settings/LyraSettingsLocal.h"
#include "Settings/LyraSettingsShared.h"
#include "Replays/LyraReplaySubsystem.h"
#include "ReplaySubsystem.h"
#include "Development/LyraDeveloperSettings.h"
#include "GameMapsSettings.h"
#if WITH_RPC_REGISTRY
#include "Tests/LyraGameplayRpcRegistrationComponent.h"
#include "HttpServerModule.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerController)

namespace Lyra
{
	namespace Input
	{
		// 非零时忽略最近输入设备类型，始终允许播放力反馈。
		static int32 ShouldAlwaysPlayForceFeedback = 0;
		// 将开发者设置和控制台变量绑定到强制力反馈开关。
		static FAutoConsoleVariableRef CVarShouldAlwaysPlayForceFeedback(TEXT("LyraPC.ShouldAlwaysPlayForceFeedback"),
			ShouldAlwaysPlayForceFeedback,
			TEXT("Should force feedback effects be played, even if the last input device was not a gamepad?"));
	}
}

// 配置 LyraPlayerCameraManager、CheatManager 和复制设置，并初始化自动奔跑与视点隐藏状态。
ALyraPlayerController::ALyraPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerCameraManagerClass = ALyraPlayerCameraManager::StaticClass();

#if USING_CHEAT_MANAGER
	CheatClass = ULyraCheatManager::StaticClass();
#endif // #if USING_CHEAT_MANAGER
}

// 在组件初始化前完成 ALyraPlayerController 的早期注册和依赖准备。
void ALyraPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

// 在 BeginPlay 阶段启动 ALyraPlayerController 的运行时监听和初始化流程。
void ALyraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	#if WITH_RPC_REGISTRY
	int32 RpcPort = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("rpcport="), RpcPort))
	{
		ULyraGameplayRpcRegistrationComponent* ObjectInstance = ULyraGameplayRpcRegistrationComponent::GetInstance();
		if (ObjectInstance && ObjectInstance->IsValidLowLevel())
		{
			ObjectInstance->RegisterAlwaysOnHttpCallbacks();
			ObjectInstance->RegisterInMatchHttpCallbacks();
		}
	}
	#endif
	SetActorHiddenInGame(false);
}

// 在 EndPlay 阶段解除 ALyraPlayerController 的委托、状态注册和外部引用。
void ALyraPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// 登记 ALyraPlayerController 需要通过网络复制的属性及复制条件。
void ALyraPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 禁用引擎默认的 TargetViewRotation 复制，因为它不适合回放和客户端观战。
	// 引擎只会在服务器预先知道观战 Pawn 时于 APlayerController::TickActor 设置该值，且仅按 COND_OwnerOnly 复制。
	// 客户端保存回放时 COND_OwnerOnly 永远不成立，录制当下也未必已知目标 Pawn。
	// 因此视角复制改由 PlayerState::ReplicatedViewRotation 承担，并在 PlayerTick 中更新。
	// Disable replicating the PC target view as it doesn't work well for replays or client-side spectating.
	// The engine TargetViewRotation is only set in APlayerController::TickActor if the server knows ahead of time that
	// a specific pawn is being spectated and it only replicates down for COND_OwnerOnly.
	// In client-saved replays, COND_OwnerOnly is never true and the target pawn is not always known at the time of recording.
	// To support client-saved replays, the replication of this was moved to ReplicatedViewRotation and updated in PlayerTick.
	DISABLE_REPLICATED_PROPERTY(APlayerController, TargetViewRotation);
}

// 完成玩家连接接收后的父类初始化，不附加额外状态。
void ALyraPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
}

// 处理自动奔跑输入，并在服务器或本地控制端更新 PlayerState 视角供观战和回放复制。
void ALyraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// 自动奔跑启用时，每帧向当前 Pawn 注入向前移动输入。
	// If we are auto running then add some player input
	if (GetIsAutoRunning())
	{
		if (APawn* CurrentPawn = GetPawn())
		{
			const FRotator MovementRotation(0.0f, GetControlRotation().Yaw, 0.0f);
			const FVector MovementDirection = MovementRotation.RotateVector(FVector::ForwardVector);
			CurrentPawn->AddMovementInput(MovementDirection, 1.0f);	
		}
	}

	ALyraPlayerState* LyraPlayerState = GetLyraPlayerState();

	if (PlayerCameraManager && LyraPlayerState)
	{
		APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();

		if (TargetPawn)
		{
			// 权威端或本地控制端更新 PlayerState 的视角旋转，供服务器复制给观战者。
			// Update view rotation on the server so it replicates
			if (HasAuthority() || TargetPawn->IsLocallyControlled())
			{
				LyraPlayerState->SetReplicatedViewRotation(TargetPawn->GetViewRotation());
			}

			// 非本地控制目标无法直接读取本地相机，因此从复制数据更新观战视角。
			// Update the target view rotation if the pawn isn't locally controlled
			if (!TargetPawn->IsLocallyControlled())
			{
				LyraPlayerState = TargetPawn->GetPlayerState<ALyraPlayerState>();
				if (LyraPlayerState)
				{
					// 从被观战 Pawn 自己的 PlayerState 读取；它可能与当前 Controller 的 PlayerState 不同。
					// Get it from the spectated pawn's player state, which may not be the same as the PC's playerstate
					TargetViewRotation = LyraPlayerState->GetReplicatedViewRotation();
				}
			}
		}
	}
}

// 将当前 PlayerState 转换为 ALyraPlayerState；尚未复制或类型不匹配时返回 nullptr。
ALyraPlayerState* ALyraPlayerController::GetLyraPlayerState() const
{
	return CastChecked<ALyraPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

// 从当前 LyraPlayerState 返回其持有的 ASC；没有 PlayerState 时返回 nullptr。
ULyraAbilitySystemComponent* ALyraPlayerController::GetLyraAbilitySystemComponent() const
{
	const ALyraPlayerState* LyraPS = GetLyraPlayerState();
	return (LyraPS ? LyraPS->GetLyraAbilitySystemComponent() : nullptr);
}

// 将当前 HUD 转换为 ALyraHUD；尚未创建或类型不匹配时返回 nullptr。
ALyraHUD* ALyraPlayerController::GetLyraHUD() const
{
	return CastChecked<ALyraHUD>(GetHUD(), ECastCheckedType::NullAllowed);
}

// 检查录制策略后，由首个本地玩家设置回放记录者并启动客户端 ReplaySubsystem 录制。
bool ALyraPlayerController::TryToRecordClientReplay()
{
	// 根据玩家、地图和设置判断是否应启动客户端回放录制。
	// See if we should record a replay
	if (ShouldRecordClientReplay())
	{
		if (ULyraReplaySubsystem* ReplaySubsystem = GetGameInstance()->GetSubsystem<ULyraReplaySubsystem>())
		{
			APlayerController* FirstLocalPlayerController = GetGameInstance()->GetFirstLocalPlayerController();
			if (FirstLocalPlayerController == this)
			{
				// 首个本地玩家负责设置本地回放的观战记录者，然后开始录制。
				// If this is the first player, update the spectator player for local replays and then record
				if (ALyraGameState* GameState = Cast<ALyraGameState>(GetWorld()->GetGameState()))
				{
					GameState->SetRecorderPlayerState(PlayerState);

					ReplaySubsystem->RecordClientReplay(this);
					return true;
				}
			}
		}
	}
	return false;
}

// 结合网络模式、前端地图、现有录制状态和本地设置判断是否允许自动录制客户端回放。
bool ALyraPlayerController::ShouldRecordClientReplay()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance != nullptr &&
		World != nullptr &&
		!World->IsPlayingReplay() &&
		!World->IsRecordingClientReplay() &&
		NM_DedicatedServer != GetNetMode() &&
		IsLocalPlayerController())
	{
		FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
		FString CurrentMap = World->URL.Map;

#if WITH_EDITOR
		CurrentMap = UWorld::StripPIEPrefixFromPackageName(CurrentMap, World->StreamingLevelsPrefix);
#endif
		if (CurrentMap == DefaultMap)
		{
			// 默认前端地图视为主菜单，禁止录制回放；后续可替换为更明确的菜单状态判断。
			// Never record demos on the default frontend map, this could be replaced with a better check for being in the main menu
			return false;
		}

		if (UReplaySubsystem* ReplaySubsystem = GameInstance->GetSubsystem<UReplaySubsystem>())
		{
			if (ReplaySubsystem->IsRecording() || ReplaySubsystem->IsPlaying())
			{
				// 同一进程一次只允许进行一个回放录制。
				// Only one at a time
				return false;
			}
		}

		// 基础条件满足后，再读取本地玩家设置确认是否启用自动录制。
		// If this is possible, now check the settings
		if (const ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(GetLocalPlayer()))
		{
			if (LyraLocalPlayer->GetLocalSettings()->ShouldAutoRecordReplays())
			{
				return true;
			}
		}
	}
	return false;
}

// 确认通知来自当前 PlayerState 后，把 TeamId 变化转发给 Controller 监听者。
void ALyraPlayerController::OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
}

// PlayerState 设置或清除后的派生类扩展点，基类不附加行为。
void ALyraPlayerController::OnPlayerStateChanged()
{
	// 基类不执行额外操作，派生类可在统一 PlayerState 变更流程中覆盖此扩展点。
	// Empty, place for derived classes to implement without having to hook all the other events
}

// 解除旧 PlayerState 队伍委托、绑定新 PlayerState，并仅在 TeamId 实际变化时广播。
void ALyraPlayerController::BroadcastOnPlayerStateChanged()
{
	OnPlayerStateChanged();

	// 若存在旧 PlayerState，先解除其团队变化委托并记录旧 TeamId。
	// Unbind from the old player state, if any
	FGenericTeamId OldTeamID = FGenericTeamId::NoTeam;
	if (LastSeenPlayerState != nullptr)
	{
		if (ILyraTeamAgentInterface* PlayerStateTeamInterface = Cast<ILyraTeamAgentInterface>(LastSeenPlayerState))
		{
			OldTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
		}
	}

	// 绑定新 PlayerState 的团队变化委托，并读取新 TeamId。
	// Bind to the new player state, if any
	FGenericTeamId NewTeamID = FGenericTeamId::NoTeam;
	if (PlayerState != nullptr)
	{
		if (ILyraTeamAgentInterface* PlayerStateTeamInterface = Cast<ILyraTeamAgentInterface>(PlayerState))
		{
			NewTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnPlayerStateChangedTeam);
		}
	}

	// 仅在 TeamId 实际变化时广播 Controller 的团队变化。
	// Broadcast the team change (if it really has)
	ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);

	LastSeenPlayerState = PlayerState;
}

// 由父类创建 PlayerState 后刷新队伍绑定和技能生成条件。
void ALyraPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	BroadcastOnPlayerStateChanged();
}

// 释放 PlayerState 前解除其队伍委托并广播失去队伍。
void ALyraPlayerController::CleanupPlayerState()
{
	Super::CleanupPlayerState();
	BroadcastOnPlayerStateChanged();
}

// 客户端解析到 PlayerState 后刷新绑定，并补做一次因 Controller 晚复制而错过的生成技能激活。
void ALyraPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BroadcastOnPlayerStateChanged();

	// 远程客户端上 PlayerController 可能晚于 PlayerState 和 ASC 完成复制。
	// TryActivateAbilitiesOnSpawn 需要 PlayerController 已存在，才能判断生成时技能是否应在本地执行。
	// 因此 PlayerController 解析到 PlayerState 后，再补做一次生成技能激活尝试。
	// 其他网络模式不存在该晚到顺序，ASC 自身的尝试即可成功；这里只修正 PlayerState/ASC 先到、
	// 当时误判技能不属于本地玩家的客户端场景。
	// When we're a client connected to a remote server, the player controller may replicate later than the PlayerState and AbilitySystemComponent.
	// However, TryActivateAbilitiesOnSpawn depends on the player controller being replicated in order to check whether on-spawn abilities should
	// execute locally. Therefore once the PlayerController exists and has resolved the PlayerState, try once again to activate on-spawn abilities.
	// On other net modes the PlayerController will never replicate late, so LyraASC's own TryActivateAbilitiesOnSpawn calls will succeed. The handling
	// here is only for when the PlayerState and ASC replicated before the PC and incorrectly thought the abilities were not for the local player.
	if (GetWorld()->IsNetMode(NM_Client))
	{
		if (ALyraPlayerState* LyraPS = GetPlayerState<ALyraPlayerState>())
		{
			if (ULyraAbilitySystemComponent* LyraASC = LyraPS->GetLyraAbilitySystemComponent())
			{
				LyraASC->RefreshAbilityActorInfo();
				LyraASC->TryActivateAbilitiesOnSpawn();
			}
		}
	}
}

// 设置底层 UPlayer 后，为 LyraLocalPlayer 绑定共享设置变化委托并立即应用当前设置。
void ALyraPlayerController::SetPlayer(UPlayer* InPlayer)
{
	Super::SetPlayer(InPlayer);

	if (const ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(InPlayer))
	{
		ULyraSettingsShared* UserSettings = LyraLocalPlayer->GetSharedSettings();
		UserSettings->OnSettingChanged.AddUObject(this, &ThisClass::OnSettingsChanged);

		OnSettingsChanged(UserSettings);
	}
}

// 共享设置变化时把后台音频和力反馈等选项应用到 Controller。
void ALyraPlayerController::OnSettingsChanged(ULyraSettingsShared* InSettings)
{
	bForceFeedbackEnabled = InSettings->GetForceFeedbackEnabled();
}

// 非 Shipping 构建按配置创建 CheatManager；关闭作弊支持时仅调用父类。
void ALyraPlayerController::AddCheats(bool bForce)
{
#if USING_CHEAT_MANAGER
	Super::AddCheats(true);
#else //#if USING_CHEAT_MANAGER
	Super::AddCheats(bForce);
#endif // #else //#if USING_CHEAT_MANAGER
}

// 服务器验证通过后由 CheatManager 为当前玩家执行命令字符串。
void ALyraPlayerController::ServerCheat_Implementation(const FString& Msg)
{
#if USING_CHEAT_MANAGER
	if (CheatManager)
	{
		UE_LOG(LogLyra, Warning, TEXT("ServerCheat: %s"), *Msg);
		ClientMessage(ConsoleCommand(Msg));
	}
#endif // #if USING_CHEAT_MANAGER
}

// 仅在 CheatManager 编译启用时接受服务器作弊 RPC。
bool ALyraPlayerController::ServerCheat_Validate(const FString& Msg)
{
	return true;
}

// 服务器遍历全部 PlayerController，并让每个 Controller 执行同一命令。
void ALyraPlayerController::ServerCheatAll_Implementation(const FString& Msg)
{
#if USING_CHEAT_MANAGER
	if (CheatManager)
	{
		UE_LOG(LogLyra, Warning, TEXT("ServerCheatAll: %s"), *Msg);
		for (TActorIterator<ALyraPlayerController> It(GetWorld()); It; ++It)
		{
			ALyraPlayerController* LyraPC = (*It);
			if (LyraPC)
			{
				LyraPC->ClientMessage(LyraPC->ConsoleCommand(Msg));
			}
		}
	}
#endif // #if USING_CHEAT_MANAGER
}

// 仅在 CheatManager 编译启用时接受全体玩家作弊 RPC。
bool ALyraPlayerController::ServerCheatAll_Validate(const FString& Msg)
{
	return true;
}

// 在玩家输入预处理前调用 ASC 的 ProcessAbilityInput 前置流程。
void ALyraPlayerController::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

// 在玩家输入处理结束后让 ASC 消费本帧按下、保持和释放的技能输入。
void ALyraPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

// 记录下一帧应隐藏 ViewTarget Pawn，避免相机进入角色内部时遮挡画面。
void ALyraPlayerController::OnCameraPenetratingTarget()
{
	bHideViewTargetPawnNextFrame = true;
}

// 接管 Pawn 后执行配置的自动作弊命令，并把当前设置与输入状态应用到新 Pawn。
void ALyraPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

#if WITH_SERVER_CODE && WITH_EDITOR
	if (GIsEditor && (InPawn != nullptr) && (GetPawn() == InPawn))
	{
		for (const FLyraCheatToRun& CheatRow : GetDefault<ULyraDeveloperSettings>()->CheatsToRun)
		{
			if (CheatRow.Phase == ECheatExecutionTime::OnPlayerPawnPossession)
			{
				ConsoleCommand(CheatRow.Cheat, /*bWriteToLog=*/ true);
			}
		}
	}
#endif

	SetIsAutoRunning(false);
}

// 状态变化时写入 Gameplay.AutoRunning 标签，并调用开始或结束自动奔跑事件。
void ALyraPlayerController::SetIsAutoRunning(const bool bEnabled)
{
	const bool bIsAutoRunning = GetIsAutoRunning();
	if (bEnabled != bIsAutoRunning)
	{
		if (!bEnabled)
		{
			OnEndAutoRun();
		}
		else
		{
			OnStartAutoRun();
		}
	}
}

// 从当前 ASC 查询 Gameplay.AutoRunning 标签；没有 ASC 时返回 false。
bool ALyraPlayerController::GetIsAutoRunning() const
{
	bool bIsAutoRunning = false;
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		bIsAutoRunning = LyraASC->GetTagCount(LyraGameplayTags::Status_AutoRunning) > 0;
	}
	return bIsAutoRunning;
}

// 触发蓝图自动奔跑开始事件。
void ALyraPlayerController::OnStartAutoRun()
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_AutoRunning, 1);
		K2_OnStartAutoRun();
	}	
}

// 触发蓝图自动奔跑结束事件。
void ALyraPlayerController::OnEndAutoRun()
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->SetLooseGameplayTagCount(LyraGameplayTags::Status_AutoRunning, 0);
		K2_OnEndAutoRun();
	}
}

// 仅在允许的输入设备条件下更新力反馈；设置可强制非手柄输入也播放效果。
void ALyraPlayerController::UpdateForceFeedback(IInputInterface* InputInterface, const int32 ControllerId)
{
	if (bForceFeedbackEnabled)
	{
		if (const UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(GetLocalPlayer()))
		{
			const ECommonInputType CurrentInputType = CommonInputSubsystem->GetCurrentInputType();
			if (Lyra::Input::ShouldAlwaysPlayForceFeedback || CurrentInputType == ECommonInputType::Gamepad || CurrentInputType == ECommonInputType::Touch)
			{
				InputInterface->SetForceFeedbackChannelValues(ControllerId, ForceFeedbackValues);
				return;
			}
		}
	}
	
	InputInterface->SetForceFeedbackChannelValues(ControllerId, FForceFeedbackValues());
}

// 相机穿入 ViewTarget Pawn 时把其 PrimitiveComponent 及附着子组件加入本帧隐藏集合。
void ALyraPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& OutHiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, OutHiddenComponents);

	if (bHideViewTargetPawnNextFrame)
	{
		AActor* const ViewTargetPawn = PlayerCameraManager ? Cast<AActor>(PlayerCameraManager->GetViewTarget()) : nullptr;
		if (ViewTargetPawn)
		{
			// 将目标 Actor 的所有 PrimitiveComponent 加入本帧隐藏集合。
			// internal helper func to hide all the components
			auto AddToHiddenComponents = [&OutHiddenComponents](const TInlineComponentArray<UPrimitiveComponent*>& InComponents)
			{
				// 隐藏每个组件及其附着的所有子组件。
				// add every component and all attached children
				for (UPrimitiveComponent* Comp : InComponents)
				{
					if (Comp->IsRegistered())
					{
						OutHiddenComponents.Add(Comp->GetPrimitiveSceneId());

						for (USceneComponent* AttachedChild : Comp->GetAttachChildren())
						{
							static FName NAME_NoParentAutoHide(TEXT("NoParentAutoHide"));
							UPrimitiveComponent* AttachChildPC = Cast<UPrimitiveComponent>(AttachedChild);
							if (AttachChildPC && AttachChildPC->IsRegistered() && !AttachChildPC->ComponentTags.Contains(NAME_NoParentAutoHide))
							{
								OutHiddenComponents.Add(AttachChildPC->GetPrimitiveSceneId());
							}
						}
					}
				}
			};

			// TODO：改为通过接口收集需要隐藏的组件，避免依赖具体 Pawn 实现。
			// TODO：直接隐藏体验生硬，部分场景应由设计侧配置近距离淡出效果。
			//TODO Solve with an interface.  Gather hidden components or something.
			//TODO Hiding isn't awesome, sometimes you want the effect of a fade out over a proximity, needs to bubble up to designers.

			// 本帧隐藏 ViewTarget Pawn 的所有可渲染组件。
			// hide pawn's components
			TInlineComponentArray<UPrimitiveComponent*> PawnComponents;
			ViewTargetPawn->GetComponents(PawnComponents);
			AddToHiddenComponents(PawnComponents);

			//// hide weapon too
			//if (ViewTargetPawn->CurrentWeapon)
			//{
			//	TInlineComponentArray<UPrimitiveComponent*> WeaponComponents;
			//	ViewTargetPawn->CurrentWeapon->GetComponents(WeaponComponents);
			//	AddToHiddenComponents(WeaponComponents);
			//}
		}

		// 本帧隐藏请求已消费，复位标记等待下一次相机穿入通知。
		// we consumed it, reset for next frame
		bHideViewTargetPawnNextFrame = false;
	}
}

// PlayerController 不直接拥有可写 TeamId，设置请求记录错误并保持 PlayerState 队伍不变。
void ALyraPlayerController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a player controller (%s); it's driven by the associated player state"), *GetPathNameSafe(this));
}

// 从当前 PlayerState 队伍接口读取 TeamId；没有有效 PlayerState 时返回 NoTeam。
FGenericTeamId ALyraPlayerController::GetGenericTeamId() const
{
	if (const ILyraTeamAgentInterface* PSWithTeamInterface = Cast<ILyraTeamAgentInterface>(PlayerState))
	{
		return PSWithTeamInterface->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

// 返回 PlayerController 自身的队伍变化多播委托地址。
FOnLyraTeamIndexChangedDelegate* ALyraPlayerController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

// 失去 Pawn 前确保其 PawnExtension 解除 PlayerState ASC 的 AvatarActor 关系。
void ALyraPlayerController::OnUnPossess()
{
	// 失去控制前确保该 Pawn 不再残留为 PlayerState ASC 的 AvatarActor。
	// Make sure the pawn that is being unpossessed doesn't remain our ASC's avatar actor
	if (APawn* PawnBeingUnpossessed = GetPawn())
	{
		const APlayerState* ThePlayerState = PlayerState.Get();
		if (IsValid(ThePlayerState))
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ThePlayerState))
			{
				if (ASC->GetAvatarActor() == PawnBeingUnpossessed)
				{
					ASC->SetAvatarActor(nullptr);
				}
			}
		}
	}

	Super::OnUnPossess();
}

//////////////////////////////////////////////////////////////////////
// ALyraReplayPlayerController

// 回放拖动可能使跟随状态失效；持续从 GameState 获取 RecorderPlayerState 并维护 Pawn 变化监听。
void ALyraReplayPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 回放拖动时间轴会重建状态，当前跟随的 PlayerState 可能随时失效。
	// The state may go invalid at any time due to scrubbing during a replay
	if (!IsValid(FollowedPlayerState))
	{
		UWorld* World = GetWorld();

		// 录制和播放阶段都监听 RecorderPlayerState 变化，以维持正确跟随目标。
		// Listen for changes for both recording and playback
		if (ALyraGameState* GameState = Cast<ALyraGameState>(World->GetGameState()))
		{
			if (!GameState->OnRecorderPlayerStateChangedEvent.IsBoundToObject(this))
			{
				GameState->OnRecorderPlayerStateChangedEvent.AddUObject(this, &ThisClass::RecorderPlayerStateUpdated);
			}
			if (APlayerState* RecorderState = GameState->GetRecorderPlayerState())
			{
				RecorderPlayerStateUpdated(RecorderState);
			}
		}
	}
}

// 保持父类观战旋转平滑流程，但使用 PlayerState 复制视角作为 TargetViewRotation 来源。
void ALyraReplayPlayerController::SmoothTargetViewRotation(APawn* TargetPawn, float DeltaSeconds)
{
	// 默认逻辑会插值到 TickActor 设置的 TargetViewRotation，但回放观战效果不够平滑。
	// Default behavior is to interpolate to TargetViewRotation which is set from APlayerController::TickActor but it's not very smooth

	Super::SmoothTargetViewRotation(TargetPawn, DeltaSeconds);
}

// 回放专用 Controller 从不再次启动客户端回放录制。
bool ALyraReplayPlayerController::ShouldRecordClientReplay()
{
	return false;
}

// 切换回放跟随的 PlayerState，解绑旧 PawnSet 委托并立即同步新观察 Pawn。
void ALyraReplayPlayerController::RecorderPlayerStateUpdated(APlayerState* NewRecorderPlayerState)
{
	if (NewRecorderPlayerState)
	{
		FollowedPlayerState = NewRecorderPlayerState;

		// 绑定 Pawn 变化事件，并立即用当前 Pawn 执行一次以同步观察目标。
		// Bind to when pawn changes and call now
		NewRecorderPlayerState->OnPawnSet.AddUniqueDynamic(this, &ALyraReplayPlayerController::OnPlayerStatePawnSet);
		OnPlayerStatePawnSet(NewRecorderPlayerState, NewRecorderPlayerState->GetPawn(), nullptr);
	}
}

// 被跟随 PlayerState 更换 Pawn 时，把回放 ViewTarget 切换到新 Pawn。
void ALyraReplayPlayerController::OnPlayerStatePawnSet(APlayerState* ChangedPlayerState, APawn* NewPlayerPawn, APawn* OldPlayerPawn)
{
	if (ChangedPlayerState == FollowedPlayerState)
	{
		SetViewTarget(NewPlayerPawn);
	}
}

