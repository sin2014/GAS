// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAbilityCost_PlayerTagStack.h"

#include "GameFramework/Controller.h"
#include "LyraGameplayAbility.h"
#include "Player/LyraPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAbilityCost_PlayerTagStack)

// 初始化从 PlayerState 标签堆叠容器扣费的技能成本。
ULyraAbilityCost_PlayerTagStack::ULyraAbilityCost_PlayerTagStack()
{
	Quantity.SetValue(1.0f);
}

// 检查技能 Owner 的 LyraPlayerState 是否拥有足量指定标签堆叠。
bool ULyraAbilityCost_PlayerTagStack::CheckCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (AController* PC = Ability->GetControllerFromActorInfo())
	{
		if (ALyraPlayerState* PS = Cast<ALyraPlayerState>(PC->PlayerState))
		{
			const int32 AbilityLevel = Ability->GetAbilityLevel(Handle, ActorInfo);

			const float NumStacksReal = Quantity.GetValueAtLevel(AbilityLevel);
			const int32 NumStacks = FMath::TruncToInt(NumStacksReal);

			return PS->GetStatTagStackCount(Tag) >= NumStacks;
		}
	}
	return false;
}

// 从权威 PlayerState 扣除按技能等级计算出的标签堆叠数。
void ULyraAbilityCost_PlayerTagStack::ApplyCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (ActorInfo->IsNetAuthority())
	{
		if (AController* PC = Ability->GetControllerFromActorInfo())
		{
			if (ALyraPlayerState* PS = Cast<ALyraPlayerState>(PC->PlayerState))
			{
				const int32 AbilityLevel = Ability->GetAbilityLevel(Handle, ActorInfo);

				const float NumStacksReal = Quantity.GetValueAtLevel(AbilityLevel);
				const int32 NumStacks = FMath::TruncToInt(NumStacksReal);

				PS->RemoveStatTagStack(Tag, NumStacks);
			}
		}
	}
}

