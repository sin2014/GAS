// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerBotController.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameMode.h"
#include "LyraLogChannels.h"
#include "Perception/AIPerceptionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerBotController)

class UObject;

// 创建 AI 感知组件，并配置 Bot Controller 需要 PlayerState。
ALyraPlayerBotController::ALyraPlayerBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = false;
}

// 确认通知来自当前 PlayerState 后，把队伍变化转发给 Bot Controller。
void ALyraPlayerBotController::OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
}

// PlayerState 设置或清除后的派生类扩展点，基类不附加行为。
void ALyraPlayerBotController::OnPlayerStateChanged()
{
	// 基类不执行额外操作，派生类可在统一 PlayerState 变更流程中覆盖此扩展点。
	// Empty, place for derived classes to implement without having to hook all the other events
}

// 在 Bot PlayerState 更换时迁移队伍委托，并仅在实际变化时广播 TeamId。
void ALyraPlayerBotController::BroadcastOnPlayerStateChanged()
{
	OnPlayerStateChanged();

	// 若存在旧 PlayerState，先解除团队变化委托并记录旧 TeamId。
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

	// 仅在 TeamId 实际变化时广播 Bot Controller 的团队变化。
	// Broadcast the team change (if it really has)
	ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);

	LastSeenPlayerState = PlayerState;
}

// 由父类创建 PlayerState 后迁移队伍委托并广播可能的 TeamId 变化。
void ALyraPlayerBotController::InitPlayerState()
{
	Super::InitPlayerState();
	BroadcastOnPlayerStateChanged();
}

// 清理 PlayerState 前先解除队伍委托，再调用父类释放状态。
void ALyraPlayerBotController::CleanupPlayerState()
{
	Super::CleanupPlayerState();
	BroadcastOnPlayerStateChanged();
}

// PlayerState 复制到客户端后调用父类回调，并迁移 Bot Controller 的队伍委托绑定。
void ALyraPlayerBotController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BroadcastOnPlayerStateChanged();
}

// 将新 TeamId 写入 Bot 的 PlayerState；PlayerState 不存在时无法设置。
void ALyraPlayerBotController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a player bot controller (%s); it's driven by the associated player state"), *GetPathNameSafe(this));
}

// 从 Bot PlayerState 返回 TeamId；没有 PlayerState 或接口时返回 NoTeam。
FGenericTeamId ALyraPlayerBotController::GetGenericTeamId() const
{
	if (ILyraTeamAgentInterface* PSWithTeamInterface = Cast<ILyraTeamAgentInterface>(PlayerState))
	{
		return PSWithTeamInterface->GetGenericTeamId();
	}
	return FGenericTeamId::NoTeam;
}

// 返回 Bot Controller 自身的队伍变化多播委托地址。
FOnLyraTeamIndexChangedDelegate* ALyraPlayerBotController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}


// 仅权威端重启 Bot；先离开旧 Pawn、恢复输入标志，再请求 GameMode 重新生成。
void ALyraPlayerBotController::ServerRestartController()
{
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	ensure((GetPawn() == nullptr) && IsInState(NAME_Inactive));

	if (IsInState(NAME_Inactive) || (IsInState(NAME_Spectating)))
	{
 		ALyraGameMode* const GameMode = GetWorld()->GetAuthGameMode<ALyraGameMode>();

		if ((GameMode == nullptr) || !GameMode->ControllerCanRestart(this))
		{
			return;
		}

		// 若仍控制着旧 Pawn，先解除接管再执行重启。
		// If we're still attached to a Pawn, leave it
		if (GetPawn() != nullptr)
		{
			UnPossess();
		}

		// 像 ClientRestart 一样恢复输入标志，使重生后的 Bot 可再次控制 Pawn。
		// Re-enable input, similar to code in ClientRestart
		ResetIgnoreInputFlags();

		GameMode->RestartPlayer(this);
	}
}

// 优先按 Lyra TeamId 判断目标敌我关系，无法解析队伍时回退到 AIController 默认逻辑。
ETeamAttitude::Type ALyraPlayerBotController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (const APawn* OtherPawn = Cast<APawn>(&Other)) {

		if (const ILyraTeamAgentInterface* TeamAgent = Cast<ILyraTeamAgentInterface>(OtherPawn->GetController()))
		{
			FGenericTeamId OtherTeamID = TeamAgent->GetGenericTeamId();

			// 比较目标 Pawn 与自身 TeamId，以确定敌对或友好态度。
			//Checking Other pawn ID to define Attitude
			if (OtherTeamID.GetId() != GetGenericTeamId().GetId())
			{
				return ETeamAttitude::Hostile;
			}
			else
			{
				return ETeamAttitude::Friendly;
			}
		}
	}

	return ETeamAttitude::Neutral;
}

// 让 AI 感知组件重新处理所有已知目标，使 TeamId 变化立即刷新敌我态度。
void ALyraPlayerBotController::UpdateTeamAttitude(UAIPerceptionComponent* AIPerception)
{
	if (AIPerception)
	{
		AIPerception->RequestStimuliListenerUpdate();
	}
}

// 失去 Pawn 前通过 PawnExtension 解除其 ASC AvatarActor 关系，再调用父类。
void ALyraPlayerBotController::OnUnPossess()
{
	// 失去控制前确保该 Pawn 不再残留为 PlayerState ASC 的 AvatarActor。
	// Make sure the pawn that is being unpossessed doesn't remain our ASC's avatar actor
	if (APawn* PawnBeingUnpossessed = GetPawn())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
		{
			if (ASC->GetAvatarActor() == PawnBeingUnpossessed)
			{
				ASC->SetAvatarActor(nullptr);
			}
		}
	}

	Super::OnUnPossess();
}

