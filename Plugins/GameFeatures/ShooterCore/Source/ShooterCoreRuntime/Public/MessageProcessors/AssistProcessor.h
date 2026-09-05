// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messages/GameplayMessageProcessor.h"

#include "AssistProcessor.generated.h"

class APlayerState;
class UObject;
struct FGameplayTag;
struct FLyraVerbMessage;
template <typename T> struct TObjectPtr;

// 记录其他玩家对某个受击玩家造成的累计伤害，用于淘汰时结算助攻。
// Tracks the damage done to a player by other players
USTRUCT()
struct FPlayerAssistDamageTracking
{
	GENERATED_BODY()

	// 从伤害来源 PlayerState 映射到其累计造成的伤害值。
	// Map of damager to damage dealt
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, float> AccumulatedDamageByPlayer;
};

// 追踪对目标造成过伤害但未完成最终淘汰的玩家，并在目标被淘汰时广播助攻消息。
// Tracks assists (dealing damage to another player without finishing them)
UCLASS()
class UAssistProcessor : public UGameplayMessageProcessor
{
	GENERATED_BODY()

public:
	virtual void StartListening() override;

private:
	void OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);
	void OnEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);

private:
	// 从受击玩家映射到该玩家本轮生命期间的伤害来源明细。
	// Map of player to damage dealt to them
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, FPlayerAssistDamageTracking> DamageHistory;
};
