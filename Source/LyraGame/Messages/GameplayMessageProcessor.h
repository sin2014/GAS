// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "GameplayMessageProcessor.generated.h"

#define UE_API LYRAGAME_API

namespace EEndPlayReason { enum Type : int; }

class UObject;

// 用于监听 GameplayMessageSubsystem 中的消息并按需派生新消息，例如检测连锁事件或连击。
// 处理器仅在服务器生成一份，而不是每位玩家一份；只关心部分玩家时必须自行过滤消息。
/**
 * UGameplayMessageProcessor
 * 
 * Base class for any message processor which observes other gameplay messages
 * and potentially re-emits updates (e.g., when a chain or combo is detected)
 * 
 * Note that these processors are spawned on the server once (not per player)
 * and should do their own internal filtering if only relevant for some players.
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class UGameplayMessageProcessor : public UActorComponent
{
	GENERATED_BODY()

public:
	//~UActorComponent interface
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	UE_API virtual void StartListening();
	UE_API virtual void StopListening();

protected:
	UE_API void AddListenerHandle(FGameplayMessageListenerHandle&& Handle);
	UE_API double GetServerTime() const;

private:
	TArray<FGameplayMessageListenerHandle> ListenerHandles;
};

#undef UE_API
