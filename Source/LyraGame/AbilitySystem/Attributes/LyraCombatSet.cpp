// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCombatSet.h"

#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCombatSet)

class FLifetimeProperty;


// 初始化来源侧基础伤害与基础治疗属性的默认值。
ULyraCombatSet::ULyraCombatSet()
	: BaseDamage(0.0f)
	, BaseHeal(0.0f)
{
}

// 注册 BaseDamage 与 BaseHeal 的网络复制属性。
void ULyraCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ULyraCombatSet, BaseDamage, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULyraCombatSet, BaseHeal, COND_OwnerOnly, REPNOTIFY_Always);
}

// BaseDamage 复制更新时通知 GAS 刷新属性聚合与监听者。
void ULyraCombatSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULyraCombatSet, BaseDamage, OldValue);
}

// BaseHeal 复制更新时通知 GAS 刷新属性聚合与监听者。
void ULyraCombatSet::OnRep_BaseHeal(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULyraCombatSet, BaseHeal, OldValue);
}

