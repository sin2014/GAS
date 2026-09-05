// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameState.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Async/TaskGraphInterfaces.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "Messages/LyraVerbMessage.h"
#include "Player/LyraPlayerState.h"
#include "LyraLogChannels.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameState)

class APlayerState;
class FLifetimeProperty;

extern ENGINE_API float GAverageFPS;


// 创建 ExperienceManagerComponent 与全局 ASC 子对象，并启用服务器帧率更新 Tick。
ALyraGameState::ALyraGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	ExperienceManagerComponent = CreateDefaultSubobject<ULyraExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));

	ServerFPS = 0.0f;
}

// 组件预初始化阶段执行父类逻辑，保留 GameState 扩展入口。
void ALyraGameState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

// 组件初始化后将全局 ASC 的 Owner 与 Avatar 都绑定到当前 GameState。
void ALyraGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(/*Owner=*/ this, /*Avatar=*/ this);
}

// 通过 IAbilitySystemInterface 返回 GameState 持有的全局 ASC。
UAbilitySystemComponent* ALyraGameState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// GameState 结束时执行父类清理，子组件负责各自卸载生命周期。
void ALyraGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// 将有效非重复 PlayerState 加入 GameState 玩家数组。
void ALyraGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
}

// 从玩家数组移除 PlayerState；当前 AGameModeBase 路径可能不会调用该重载。
void ALyraGameState::RemovePlayerState(APlayerState* PlayerState)
{
	//@TODO：当前 AGameModeBase 不会调用此路径，只有完整 AGameMode 才会调用；需记录引擎行为并评估迁移逻辑。
	//@TODO: This isn't getting called right now (only the 'rich' AGameMode uses it, not AGameModeBase)
	// Need to at least comment the engine code, and possibly move things around
	Super::RemovePlayerState(PlayerState);
}

// 无缝 Travel 检查点中清除非活动 PlayerState 与 Bot，避免跨地图保留无效项。
void ALyraGameState::SeamlessTravelTransitionCheckpoint(bool bToTransitionMap)
{
	// Seamless Travel 检查点中移除非活动 PlayerState 和 Bot，避免带入目标地图。
	// Remove inactive and bots
	for (int32 i = PlayerArray.Num() - 1; i >= 0; i--)
	{
		APlayerState* PlayerState = PlayerArray[i];
		if (PlayerState && (PlayerState->IsABot() || PlayerState->IsInactive()))
		{
			RemovePlayerState(PlayerState);
		}
	}
}

// 注册 ServerFPS 与 Replay RecorderPlayerState 的网络复制属性。
void ALyraGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ServerFPS);
	DOREPLIFETIME_CONDITION(ThisClass, RecorderPlayerState, COND_ReplayOnly);
}

// 权威端每帧采样当前平均 FPS，并更新复制给客户端的 ServerFPS。
void ALyraGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetLocalRole() == ROLE_Authority)
	{
		ServerFPS = GAverageFPS;
	}
}

// 通过不可靠 Multicast 在各客户端的 GameplayMessageSubsystem 广播 VerbMessage。
void ALyraGameState::MulticastMessageToClients_Implementation(const FLyraVerbMessage Message)
{
	if (GetNetMode() == NM_Client)
	{
		UGameplayMessageSubsystem::Get(this).BroadcastMessage(Message.Verb, Message);
	}
}

// 通过可靠 Multicast 在各客户端广播不能容忍丢失的 VerbMessage。
void ALyraGameState::MulticastReliableMessageToClients_Implementation(const FLyraVerbMessage Message)
{
	MulticastMessageToClients_Implementation(Message);
}

// 返回最近由服务器更新并复制的帧率值。
float ALyraGameState::GetServerFPS() const
{
	return ServerFPS;
}

// 仅权威端更新 Replay 录制者，并主动执行 RepNotify 完成录制端初始化。
void ALyraGameState::SetRecorderPlayerState(APlayerState* NewPlayerState)
{
	if (RecorderPlayerState == nullptr)
	{
		// 服务器本地设置后主动调用 OnRep，使录制时也执行与复制接收端一致的初始化。
		// Set it and call the rep callback so it can do any record-time setup
		RecorderPlayerState = NewPlayerState;
		OnRep_RecorderPlayerState();
	}
	else
	{
		UE_LOG(LogLyra, Warning, TEXT("SetRecorderPlayerState was called on %s but should only be called once per game on the primary user"), *GetName());
	}
}

// 返回当前 Replay 录制者 PlayerState。
APlayerState* ALyraGameState::GetRecorderPlayerState() const
{
	// TODO：RecorderPlayerState 为空时可考虑自动选择合适的 Replay 跟随目标。
	// TODO: Maybe auto select it if null?

	return RecorderPlayerState;
}

// RecorderPlayerState 更新后广播变化委托，供 Replay 跟随逻辑选择正确 Pawn。
void ALyraGameState::OnRep_RecorderPlayerState()
{
	OnRecorderPlayerStateChangedEvent.Broadcast(RecorderPlayerState);
}
