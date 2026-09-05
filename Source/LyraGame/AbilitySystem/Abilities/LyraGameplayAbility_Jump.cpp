// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayAbility_Jump.h"

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Character/LyraCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbility_Jump)

struct FGameplayTagContainer;


// 将跳跃技能配置为本地预测执行，并在输入保持期间持续激活。
ULyraGameplayAbility_Jump::ULyraGameplayAbility_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// 在基类 GAS 条件通过后，进一步要求 Avatar 为可执行 Jump 的 LyraCharacter。
bool ULyraGameplayAbility_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	const ALyraCharacter* LyraCharacter = Cast<ALyraCharacter>(ActorInfo->AvatarActor.Get());
	if (!LyraCharacter || !LyraCharacter->CanJump())
	{
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return true;
}

// 技能结束时先停止角色跳跃，再交给基类完成复制与结束状态清理。
void ULyraGameplayAbility_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 技能结束时兜底停止跳跃，防止技能蓝图遗漏对应调用而持续保持跳跃输入。
	// Stop jumping in case the ability blueprint doesn't call it.
	CharacterJumpStop();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// 让当前 LyraCharacter 开始跳跃；没有有效 Avatar 时不执行。
void ULyraGameplayAbility_Jump::CharacterJumpStart()
{
	if (ALyraCharacter* LyraCharacter = GetLyraCharacterFromActorInfo())
	{
		if (LyraCharacter->IsLocallyControlled() && !LyraCharacter->bPressedJump)
		{
			LyraCharacter->UnCrouch();
			LyraCharacter->Jump();
		}
	}
}

// 让当前 LyraCharacter 停止跳跃输入；没有有效 Avatar 时不执行。
void ULyraGameplayAbility_Jump::CharacterJumpStop()
{
	if (ALyraCharacter* LyraCharacter = GetLyraCharacterFromActorInfo())
	{
		if (LyraCharacter->IsLocallyControlled() && LyraCharacter->bPressedJump)
		{
			LyraCharacter->StopJumping();
		}
	}
}

