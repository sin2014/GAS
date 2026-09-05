// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "LyraAttributeSet.h"

#include "LyraCombatSet.generated.h"

class UObject;
struct FFrame;


/**
 * 定义伤害与治疗 ExecutionCalculation 所需的来源侧战斗属性。
 * 当前包含基础伤害和基础治疗量，后续也可扩展攻击力、护盾穿透等属性。
 */
/**
 * ULyraCombatSet
 *
 *  Class that defines attributes that are necessary for applying damage or healing.
 *	Attribute examples include: damage, healing, attack power, and shield penetrations.
 */
UCLASS(BlueprintType)
class ULyraCombatSet : public ULyraAttributeSet
{
	GENERATED_BODY()

public:

	ULyraCombatSet();

	ATTRIBUTE_ACCESSORS(ULyraCombatSet, BaseDamage);
	ATTRIBUTE_ACCESSORS(ULyraCombatSet, BaseHeal);

protected:

	UFUNCTION()
	void OnRep_BaseDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BaseHeal(const FGameplayAttributeData& OldValue);

private:

	// 伤害 ExecutionCalculation 读取的基础伤害值。
	// The base amount of damage to apply in the damage execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseDamage, Category = "Lyra|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseDamage;

	// 治疗 ExecutionCalculation 读取的基础治疗值。
	// The base amount of healing to apply in the heal execution.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseHeal, Category = "Lyra|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseHeal;
};
