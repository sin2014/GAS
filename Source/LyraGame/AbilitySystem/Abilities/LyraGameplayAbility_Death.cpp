// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayAbility_Death.h"

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "LyraGameplayTags.h"
#include "LyraLogChannels.h"
#include "Trace/Trace.inl"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_Death)

// 配置死亡技能的服务器执行、阻塞型排他组和 GameplayEvent.Death 默认触发器。
ULyraGameplayAbility_Death::ULyraGameplayAbility_Death(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	bAutoStartDeath = true;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// 在 CDO 上注册默认触发器，使 GameplayEvent.Death 事件自动激活此技能。
		// Add the ability trigger tag as default to the CDO.
		FAbilityTriggerData TriggerData;
		TriggerData.TriggerTag = LyraGameplayTags::GameplayEvent_Death;
		TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(TriggerData);
	}
}

// 激活时取消不具备 SurvivesDeath 标签的技能、禁止自身取消，并按配置启动死亡流程。
void ULyraGameplayAbility_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	ULyraAbilitySystemComponent* LyraASC = CastChecked<ULyraAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());

	FGameplayTagContainer AbilityTypesToIgnore;
	AbilityTypesToIgnore.AddTag(LyraGameplayTags::Ability_Behavior_SurvivesDeath);

	// 取消除 Ability.Behavior.SurvivesDeath 外的技能，并通过阻塞型排他组禁止其他排他技能启动。
	// Cancel all abilities and block others from starting.
	LyraASC->CancelAbilities(nullptr, &AbilityTypesToIgnore, this);

	SetCanBeCanceled(false);

	if (!ChangeActivationGroup(ELyraAbilityActivationGroup::Exclusive_Blocking))
	{
		UE_LOG(LogLyraAbilitySystem, Error, TEXT("ULyraGameplayAbility_Death::ActivateAbility: Ability [%s] failed to change activation group to blocking."), *GetName());
	}

	if (bAutoStartDeath)
	{
		StartDeath();
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

// 结束时兜底完成已开始的死亡流程，再执行基类技能结束。
void ULyraGameplayAbility_Death::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	check(ActorInfo);

	// 技能结束时兜底完成死亡流程，避免蓝图路径遗漏 FinishDeath。
	// 若死亡流程尚未开始，FinishDeath 不会产生状态变化。
	// Always try to finish the death when the ability ends in case the ability doesn't.
	// This won't do anything if the death hasn't been started.
	FinishDeath();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 将 Avatar 的 HealthComponent 从未死亡状态推进到 DeathStarted。
void ULyraGameplayAbility_Death::StartDeath()
{
	if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo()))
	{
		if (HealthComponent->GetDeathState() == ELyraDeathState::NotDead)
		{
			HealthComponent->StartDeath();
		}
	}
}

// 将已开始的死亡流程推进到 DeathFinished，未开始时保持不变。
void ULyraGameplayAbility_Death::FinishDeath()
{
	if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(GetAvatarActorFromActorInfo()))
	{
		if (HealthComponent->GetDeathState() == ELyraDeathState::DeathStarted)
		{
			HealthComponent->FinishDeath();
		}
	}
}

