// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "LyraGamePhaseAbility.generated.h"

/**
 * 表示一个活动游戏阶段的 GameplayAbility 基类；技能的激活与结束分别驱动阶段进入和退出。
 */
/**
 * ULyraGamePhaseAbility
 *
 * The base gameplay ability for any ability that is used to change the active game phase.
 */
UCLASS(Abstract, HideCategories = Input)
class ULyraGamePhaseAbility : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	ULyraGamePhaseAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const FGameplayTag& GetGamePhaseTag() const { return GamePhaseTag; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	// 指定该阶段技能代表的 GameplayTag。启动新阶段会结束不在其祖先链上的活动阶段：
	// 例如启动 GamePhase.RoundStart 会结束同级的 GamePhase.WaitingToStart。
	// 阶段也可通过标签层级嵌套；从 GamePhase.Playing.NormalPlay 切换到
	// GamePhase.Playing.SuddenDeath 时，会结束 Playing 下原有子阶段，但保留父阶段 GamePhase.Playing。
	// Defines the game phase that this game phase ability is part of.  So for example,
	// if your game phase is GamePhase.RoundStart, then it will cancel all sibling phases.
	// So if you had a phase such as GamePhase.WaitingToStart that was active, starting
	// the ability part of RoundStart would end WaitingToStart.  However to get nested behaviors
	// you can also nest the phases.  So for example, GamePhase.Playing.NormalPlay, is a sub-phase
	// of the parent GamePhase.Playing, so changing the sub-phase to GamePhase.Playing.SuddenDeath,
	// would stop any ability tied to GamePhase.Playing.*, but wouldn't end any ability 
	// tied to the GamePhase.Playing phase.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Game Phase")
	FGameplayTag GamePhaseTag;
};
