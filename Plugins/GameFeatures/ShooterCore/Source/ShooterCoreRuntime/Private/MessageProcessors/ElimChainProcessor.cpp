// Copyright Epic Games, Inc. All Rights Reserved.

#include "MessageProcessors/ElimChainProcessor.h"

#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ElimChainProcessor)

namespace ElimChain
{
	// 定义限时连续淘汰处理器监听的 Gameplay Message 通道。
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Elimination_Message, "Lyra.Elimination.Message");
}

// 注册淘汰消息监听器，用服务器时间维护玩家的限时 Chain 状态。
void UElimChainProcessor::StartListening()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	AddListenerHandle(MessageSubsystem.RegisterListener(ElimChain::TAG_Lyra_Elimination_Message, this, &ThisClass::OnEliminationMessage));
}

// 对非自杀淘汰检查与上次淘汰的时间间隔，超时则重置为 1，否则递增并在命中配置档位时广播对应消息。
void UElimChainProcessor::OnEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	// 仅为攻击者追踪连续淘汰，自我淘汰不计入 Chain。
	// Track elimination chains for the attacker (except for self-eliminations)
	if (Payload.Instigator != Payload.Target)
	{
		if (APlayerState* InstigatorPS = Cast<APlayerState>(Payload.Instigator))
		{
			const double CurrentTime = GetServerTime();

			FPlayerElimChainInfo& History = PlayerChainHistory.FindOrAdd(InstigatorPS);
			const bool bStreakReset = (History.LastEliminationTime == 0.0) || (History.LastEliminationTime + ChainTimeLimit < CurrentTime);

			History.LastEliminationTime = CurrentTime;
			if (bStreakReset)
			{
				History.ChainCounter = 1;
			}
			else
			{
				++History.ChainCounter;

				if (FGameplayTag* pTag = ElimChainTags.Find(History.ChainCounter))
				{
					FLyraVerbMessage ElimChainMessage;
					ElimChainMessage.Verb = *pTag;
					ElimChainMessage.Instigator = InstigatorPS;
					ElimChainMessage.InstigatorTags = Payload.InstigatorTags;
					ElimChainMessage.ContextTags = Payload.ContextTags;
					ElimChainMessage.Magnitude = History.ChainCounter;
					
					UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
					MessageSubsystem.BroadcastMessage(ElimChainMessage.Verb, ElimChainMessage);
				}
			}
		}
	}
}

