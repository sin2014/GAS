// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "LyraAbilityCost.generated.h"

class ULyraGameplayAbility;

/**
 * Lyra 技能附加成本的抽象基类，例如弹药、充能次数等资源消耗。
 */
/**
 * ULyraAbilityCost
 *
 * Base class for costs that a LyraGameplayAbility has (e.g., ammo or charges)
 */
UCLASS(MinimalAPI, DefaultToInstanced, EditInlineNew, Abstract)
class ULyraAbilityCost : public UObject
{
	GENERATED_BODY()

public:
	ULyraAbilityCost()
	{
	}

	/**
	 * 判断当前技能是否具备支付此项成本所需的资源。
	 *
	 * 若无法支付，可向非空的 OptionalRelevantTags 添加失败原因标签，供其他系统生成对应的玩家反馈，
	 * 例如武器弹药耗尽时播放空仓提示音。
	 *
	 * 调用时 Ability 与 ActorInfo 保证有效，OptionalRelevantTags 允许为空。
	 *
	 * @return 可以支付并继续激活技能时返回 true，否则返回 false。
	 */
	/**
	 * Checks if we can afford this cost.
	 *
	 * A failure reason tag can be added to OptionalRelevantTags (if non-null), which can be queried
	 * elsewhere to determine how to provide user feedback (e.g., a clicking noise if a weapon is out of ammo)
	 * 
	 * Ability and ActorInfo are guaranteed to be non-null on entry, but OptionalRelevantTags can be nullptr.
	 * 
	 * @return true if we can pay for the ability, false otherwise.
	 */
	virtual bool CheckCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
	{
		return true;
	}

	/**
	 * 实际扣除该技能的这项成本。
	 *
	 * 注意：
	 * - 调用方已根据 ShouldOnlyApplyCostOnHit() 判断是否应扣费，实现无需重复检查。
	 * - 调用时 Ability 与 ActorInfo 保证有效。
	 */
	/**
	 * Applies the ability's cost to the target
	 *
	 * Notes:
	 * - Your implementation don't need to check ShouldOnlyApplyCostOnHit(), the caller does that for you.
 	 * - Ability and ActorInfo are guaranteed to be non-null on entry.
	 */
	virtual void ApplyCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
	{
	}

	/** 该项成本是否仅在技能成功命中目标后扣除。 */
	/** If true, this cost should only be applied if this ability hits successfully */
	bool ShouldOnlyApplyCostOnHit() const { return bOnlyApplyCostOnHit; }

protected:
	/** 为 true 时，仅当技能成功命中目标才实际扣除这项成本。 */
	/** If true, this cost should only be applied if this ability hits successfully */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Costs)
	bool bOnlyApplyCostOnHit = false;
};
