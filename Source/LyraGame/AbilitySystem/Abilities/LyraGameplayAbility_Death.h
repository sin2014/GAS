// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraGameplayAbility.h"

#include "LyraGameplayAbility_Death.generated.h"

class UObject;
struct FFrame;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;


/**
 * 负责驱动角色死亡流程的 GameplayAbility。
 * 该技能由技能触发标签 "GameplayEvent.Death" 对应的 GameplayEvent 自动激活。
 */
/**
 * ULyraGameplayAbility_Death
 *
 *	Gameplay ability used for handling death.
 *	Ability is activated automatically via the "GameplayEvent.Death" ability trigger tag.
 */
UCLASS(Abstract)
class ULyraGameplayAbility_Death : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	ULyraGameplayAbility_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// 将 HealthComponent 推进到死亡开始状态。
	// Starts the death sequence.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	void StartDeath();

	// 将 HealthComponent 推进到死亡完成状态。
	// Finishes the death sequence.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	void FinishDeath();

protected:

	// 启用后，技能激活时自动调用 StartDeath；只要死亡流程已开始，技能结束时始终调用 FinishDeath 收尾。
	// If enabled, the ability will automatically call StartDeath.  FinishDeath is always called when the ability ends if the death was started.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Death")
	bool bAutoStartDeath;
};
