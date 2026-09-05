// Copyright Epic Games, Inc. All Rights Reserved.

#include "MessageProcessors/AssistProcessor.h"

#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssistProcessor)

// 定义处理器监听和广播所使用的淘汰、伤害与助攻消息通道。
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Elimination_Message, "Lyra.Elimination.Message");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Damage_Message, "Lyra.Damage.Message");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Assist_Message, "Lyra.Assist.Message");

// 注册淘汰和伤害消息监听器，并由基类保存句柄以便统一注销。
void UAssistProcessor::StartListening()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	AddListenerHandle(MessageSubsystem.RegisterListener(TAG_Lyra_Elimination_Message, this, &ThisClass::OnEliminationMessage));
	AddListenerHandle(MessageSubsystem.RegisterListener(TAG_Lyra_Damage_Message, this, &ThisClass::OnDamageMessage));
}

// 累计非自伤事件中每个 Instigator 对目标 PlayerState 造成的伤害，作为目标淘汰时的助攻贡献值。
void UAssistProcessor::OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	if (Payload.Instigator != Payload.Target)
	{
		if (APlayerState* InstigatorPS = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Payload.Instigator))
		{
			if (APlayerState* TargetPS = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Payload.Target))
			{
				FPlayerAssistDamageTracking& Damage = DamageHistory.FindOrAdd(TargetPS);
				float& DamageTotalFromTarget = Damage.AccumulatedDamageByPlayer.FindOrAdd(InstigatorPS);
				DamageTotalFromTarget += Payload.Magnitude;
			}
		}
	}
}


// 目标被淘汰时为所有造成过伤害但不是最终 Instigator 的玩家广播助攻消息，并清除该目标的伤害历史。
void UAssistProcessor::OnEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload)
{
	if (APlayerState* TargetPS = Cast<APlayerState>(Payload.Target))
	{
		// 为每个伤害过目标但没有完成最终淘汰的玩家授予助攻。
		// Grant an assist to each player who damaged the target but wasn't the instigator
		if (FPlayerAssistDamageTracking* DamageOnTarget = DamageHistory.Find(TargetPS))
		{
			for (const auto& KVP : DamageOnTarget->AccumulatedDamageByPlayer)
			{
				if (APlayerState* AssistPS = KVP.Key)
				{
					if (AssistPS != Payload.Instigator)
					{
						FLyraVerbMessage AssistMessage;
						AssistMessage.Verb = TAG_Lyra_Assist_Message;
						AssistMessage.Instigator = AssistPS;
						// TODO：应从 PlayerState 获取默认标签，或在记录助攻伤害时保存最近一次 InstigatorTags。
						//@TODO: Get default tags from a player state or save off most recent tags during assist damage?
						//AssistMessage.InstigatorTags = ;
						AssistMessage.Target = TargetPS;
						AssistMessage.TargetTags = Payload.TargetTags;
						AssistMessage.ContextTags = Payload.ContextTags;
						AssistMessage.Magnitude = KVP.Value;

						UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
						MessageSubsystem.BroadcastMessage(AssistMessage.Verb, AssistMessage);
					}
				}
			}

			// 淘汰结算完成后清除该玩家本轮生命的伤害记录。
			// Clear the damage log for the eliminated player
			DamageHistory.Remove(TargetPS);
		}
	}
}

