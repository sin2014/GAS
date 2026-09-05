// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messages/GameplayMessageProcessor.h"

#include "ElimStreakProcessor.generated.h"

class APlayerState;
class UObject;
struct FGameplayTag;
struct FLyraVerbMessage;
template <typename T> struct TObjectPtr;

// 追踪单次生命内的连续淘汰；玩家被淘汰时清除其 Streak 计数。
// Tracks a streak of eliminations (X eliminations without being eliminated)
UCLASS(Abstract)
class UElimStreakProcessor : public UGameplayMessageProcessor
{
	GENERATED_BODY()

public:
	virtual void StartListening() override;

protected:
	// 连杀次数对应的待广播 GameplayTag。
	// The event to rebroadcast when a user gets a streak of a certain length
	UPROPERTY(EditDefaultsOnly)
	TMap<int32, FGameplayTag> ElimStreakTags;

private:
	void OnEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);

private:
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, int32> PlayerStreakHistory;
};
