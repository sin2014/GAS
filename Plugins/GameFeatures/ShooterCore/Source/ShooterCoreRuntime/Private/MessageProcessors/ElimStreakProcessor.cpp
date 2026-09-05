// Copyright Epic Games, Inc. All Rights Reserved.

#include "MessageProcessors/ElimStreakProcessor.h"

#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ElimStreakProcessor)

namespace ElimStreak
{
	// 定义单次生命连杀处理器监听的 Gameplay Message 通道。
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Elimination_Message, "Lyra.Elimination.Message");
}

// 注册淘汰消息监听器，按 PlayerState 维护本轮生命的 Streak 计数。
void UElimStreakProcessor::StartListening()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	AddListenerHandle(MessageSubsystem.RegisterListener(ElimStreak::TAG_Lyra_Elimination_Message, this, &ThisClass::OnEliminationMessage));
}

// 非自杀淘汰会增加攻击者 Streak 并按配置广播里程碑；随后清除被淘汰目标的 Streak 历史。
void UElimStreakProcessor::OnEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	// 仅为攻击者追踪连杀，自我淘汰不增加计数。
	// Track elimination streaks for the attacker (except for self-eliminations)
	if (Payload.Instigator != Payload.Target)
	{
		if (APlayerState* InstigatorPS = Cast<APlayerState>(Payload.Instigator))
		{
			int32& StreakCount = PlayerStreakHistory.FindOrAdd(InstigatorPS);
			StreakCount++;

			if (FGameplayTag* pTag = ElimStreakTags.Find(StreakCount))
			{
				FLyraVerbMessage ElimStreakMessage;
				ElimStreakMessage.Verb = *pTag;
				ElimStreakMessage.Instigator = InstigatorPS;
				ElimStreakMessage.InstigatorTags = Payload.InstigatorTags;
				ElimStreakMessage.ContextTags = Payload.ContextTags;
 				ElimStreakMessage.Magnitude = StreakCount;

				UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
				MessageSubsystem.BroadcastMessage(ElimStreakMessage.Verb, ElimStreakMessage);
			}
		}
	}

	// 被淘汰玩家的本轮生命结束，连杀计数随之清除。
	// End the elimination streak for the target
	if (APlayerState* TargetPS = Cast<APlayerState>(Payload.Target))
	{
		PlayerStreakHistory.Remove(TargetPS);
	}
}

