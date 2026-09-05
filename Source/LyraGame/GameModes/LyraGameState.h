// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "ModularGameState.h"

#include "LyraGameState.generated.h"

#define UE_API LYRAGAME_API

struct FLyraVerbMessage;

class APlayerState;
class UAbilitySystemComponent;
class ULyraAbilitySystemComponent;
class ULyraExperienceManagerComponent;
class UObject;
struct FFrame;

/**
 * 项目的 GameState 基类，承载当前 Experience、全局 ASC、消息广播和 Replay 录制者状态。
 */
/**
 * ALyraGameState
 *
 *	The base game state class used by this project.
 */
UCLASS(MinimalAPI, Config = Game)
class ALyraGameState : public AModularGameStateBase, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	UE_API ALyraGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void PostInitializeComponents() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void Tick(float DeltaSeconds) override;
	//~End of AActor interface

	//~AGameStateBase interface
	UE_API virtual void AddPlayerState(APlayerState* PlayerState) override;
	UE_API virtual void RemovePlayerState(APlayerState* PlayerState) override;
	UE_API virtual void SeamlessTravelTransitionCheckpoint(bool bToTransitionMap) override;
	//~End of AGameStateBase interface

	//~IAbilitySystemInterface
	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	// 返回用于全局 GameplayEffect、GameplayAbility 和 GameplayCue 的 ASC。
	// Gets the ability system component used for game wide things
	UFUNCTION(BlueprintCallable, Category = "Lyra|GameState")
	ULyraAbilitySystemComponent* GetLyraAbilitySystemComponent() const { return AbilitySystemComponent; }

	// 通过不可靠 Multicast 向所有客户端广播消息。
	// 仅用于淘汰、玩家加入等允许丢失的客户端通知。
	// Send a message that all clients will (probably) get
	// (use only for client notifications like eliminations, server join messages, etc... that can handle being lost)
	UFUNCTION(NetMulticast, Unreliable, BlueprintCallable, Category = "Lyra|GameState")
	UE_API void MulticastMessageToClients(const FLyraVerbMessage Message);

	// 通过可靠 Multicast 向所有客户端广播消息。
	// 仅用于不能容忍丢失的客户端通知。
	// Send a message that all clients will be guaranteed to get
	// (use only for client notifications that cannot handle being lost)
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "Lyra|GameState")
	UE_API void MulticastReliableMessageToClients(const FLyraVerbMessage Message);

	// 返回复制到客户端的服务器帧率。
	// Gets the server's FPS, replicated to clients
	UE_API float GetServerFPS() const;

	// 设置当前负责录制 Replay 的本地 PlayerState。
	// Indicate the local player state is recording a replay
	UE_API void SetRecorderPlayerState(APlayerState* NewPlayerState);

	// 返回录制该 Replay 的 PlayerState；无有效记录时为空。
	// Gets the player state that recorded the replay, if valid
	UE_API APlayerState* GetRecorderPlayerState() const;

	// Replay 录制者 PlayerState 变化时广播。
	// Delegate called when the replay player state changes
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRecorderPlayerStateChanged, APlayerState*);
	FOnRecorderPlayerStateChanged OnRecorderPlayerStateChangedEvent;

private:
	// 负责当前 Gameplay Experience 的加载、激活和卸载。
	// Handles loading and managing the current gameplay experience
	UPROPERTY()
	TObjectPtr<ULyraExperienceManagerComponent> ExperienceManagerComponent;

	// GameState 的全局 ASC 子对象，主要用于全局 GameplayCue 等跨玩家能力系统功能。
	// The ability system component subobject for game-wide things (primarily gameplay cues)
	UPROPERTY(VisibleAnywhere, Category = "Lyra|GameState")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

protected:
	UPROPERTY(Replicated)
	float ServerFPS;

	// Replay 流中记录实际录制者，用于选择正确的 Pawn 进行跟随。
	// 该字段只在 Replay 数据中设置，正常网络游戏不会复制。
	// The player state that recorded a replay, it is used to select the right pawn to follow
	// This is only set in replay streams and is not replicated normally
	UPROPERTY(Transient, ReplicatedUsing = OnRep_RecorderPlayerState)
	TObjectPtr<APlayerState> RecorderPlayerState;

	UFUNCTION()
	UE_API void OnRep_RecorderPlayerState();

};

#undef UE_API
