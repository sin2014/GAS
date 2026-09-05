// Copyright Epic Games, Inc. All Rights Reserved.

#include "TopDownArenaAttributeSet.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TopDownArenaAttributeSet)

class FLifetimeProperty;

// 为炸弹库存、容量、范围和移动速度设置新角色的初始基础值。
UTopDownArenaAttributeSet::UTopDownArenaAttributeSet()
	: BombsRemaining(1.0f)
	, BombCapacity(1.0f)
	, BombRange(2.0f)
	, MovementSpeed(400.0f)
{
}

// 将全部 TopDownArena 属性注册为无条件复制并始终触发 RepNotify，以便 GAS 正确更新客户端聚合器和委托。
void UTopDownArenaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, BombsRemaining, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, BombCapacity, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, BombRange, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MovementSpeed, COND_None, REPNOTIFY_Always);
}

// 收到炸弹余量复制值后，把新旧值交给 GAS 的属性复制通知机制。
void UTopDownArenaAttributeSet::OnRep_BombsRemaining(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, BombsRemaining, OldValue);
}

// 收到炸弹容量复制值后，把新旧值交给 GAS 的属性复制通知机制。
void UTopDownArenaAttributeSet::OnRep_BombCapacity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, BombCapacity, OldValue);
}

// 收到爆炸范围复制值后，把新旧值交给 GAS 的属性复制通知机制。
void UTopDownArenaAttributeSet::OnRep_BombRange(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, BombRange, OldValue);
}

// 收到移动速度复制值后，把新旧值交给 GAS 的属性复制通知机制。
void UTopDownArenaAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MovementSpeed, OldValue);
}

// 在基础值写入前约束输入，保证永久修改也不会越过该属性的玩法边界。
void UTopDownArenaAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

// 在当前值因 GameplayEffect 等因素变化前约束输入，保证运行时聚合结果处于合法范围。
void UTopDownArenaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

// 按属性实施模式规则：余量不超过容量，容量和范围至少为 1，移动速度限制在 200 到 800。
void UTopDownArenaAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetBombsRemainingAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetBombCapacity());
	}
	else if (Attribute == GetBombCapacityAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetBombRangeAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetMovementSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 200.0f, 800.0f);
	}
}

