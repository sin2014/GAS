// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAbilityCost_InventoryItem.h"
#include "GameplayAbilitySpec.h"
#include "GameplayAbilitySpecHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAbilityCost_InventoryItem)

// 将背包物品成本默认配置为仅在技能成功命中后扣除。
ULyraAbilityCost_InventoryItem::ULyraAbilityCost_InventoryItem()
{
	Quantity.SetValue(1.0f);
}

// 按技能等级计算所需数量，并检查关联 Controller 的背包是否拥有足够的指定物品。
bool ULyraAbilityCost_InventoryItem::CheckCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
#if 0
	if (AController* PC = Ability->GetControllerFromActorInfo())
	{
		if (ULyraInventoryManagerComponent* InventoryComponent = PC->GetComponentByClass<ULyraInventoryManagerComponent>())
		{
			const int32 AbilityLevel = Ability->GetAbilityLevel(Handle, ActorInfo);

			const float NumItemsToConsumeReal = Quantity.GetValueAtLevel(AbilityLevel);
			const int32 NumItemsToConsume = FMath::TruncToInt(NumItemsToConsumeReal);

			return InventoryComponent->GetTotalItemCountByDefinition(ItemDefinition) >= NumItemsToConsume;
		}
	}
#endif
	return false;
}

// 在调用方确认应扣费后，从权威端背包消耗按技能等级计算出的物品数量。
void ULyraAbilityCost_InventoryItem::ApplyCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
#if 0
	if (ActorInfo->IsNetAuthority())
	{
		if (AController* PC = Ability->GetControllerFromActorInfo())
		{
			if (ULyraInventoryManagerComponent* InventoryComponent = PC->GetComponentByClass<ULyraInventoryManagerComponent>())
			{
				const int32 AbilityLevel = Ability->GetAbilityLevel(Handle, ActorInfo);

				const float NumItemsToConsumeReal = Quantity.GetValueAtLevel(AbilityLevel);
				const int32 NumItemsToConsume = FMath::TruncToInt(NumItemsToConsumeReal);

				InventoryComponent->ConsumeItemsByDefinition(ItemDefinition, NumItemsToConsume);
			}
		}
	}
#endif
}

