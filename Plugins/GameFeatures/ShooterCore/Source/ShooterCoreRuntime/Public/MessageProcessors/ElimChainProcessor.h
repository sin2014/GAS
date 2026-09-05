// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Messages/GameplayMessageProcessor.h"

#include "ElimChainProcessor.generated.h"

class APlayerState;
class UObject;
struct FGameplayTag;
struct FLyraVerbMessage;
template <typename T> struct TObjectPtr;

USTRUCT()
struct FPlayerElimChainInfo
{
	GENERATED_BODY()

	double LastEliminationTime = 0.0;

	int32 ChainCounter = 1;
};

// 追踪限时连续淘汰：相邻两次淘汰间隔不超过配置时限时累加 Chain，并在达到映射档位时广播消息。
// Tracks a chain of eliminations (X eliminations without more than Y seconds passing between each one)
UCLASS(Abstract)
class UElimChainProcessor : public UGameplayMessageProcessor
{
	GENERATED_BODY()

public:
	virtual void StartListening() override;

protected:
	UPROPERTY(EditDefaultsOnly)
	float ChainTimeLimit = 4.5f;

	// 连续淘汰次数对应的待广播 GameplayTag。
	// The event to rebroadcast when a user gets a chain of a certain length
	UPROPERTY(EditDefaultsOnly)
	TMap<int32, FGameplayTag> ElimChainTags;

private:
	void OnEliminationMessage(FGameplayTag Channel, const FLyraVerbMessage& Payload);

private:
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerState>, FPlayerElimChainInfo> PlayerChainHistory;
};
