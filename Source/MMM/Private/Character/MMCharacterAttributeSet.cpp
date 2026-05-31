// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MMCharacterAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// 同时设置 Attribute 的基础值和当前值，避免构造时只初始化其中一层数值。
	void InitCharacterAttribute(FGameplayAttributeData& Attribute, const float Value)
	{
		Attribute.SetBaseValue(Value);
		Attribute.SetCurrentValue(Value);
	}

	// 将属性当前值限制到合法范围；用于避免直接依赖宏生成 Setter。
	void SetCharacterAttributeCurrentValue(FGameplayAttributeData& Attribute, const float Value)
	{
		Attribute.SetCurrentValue(Value);
	}
}

UMMCharacterAttributeSet::UMMCharacterAttributeSet()
{
	// 默认角色从 1 级开始。
	InitCharacterAttribute(Level, 1.0f);

	// 默认等级上限先使用 99，后续可按具体作品规则调整。
	InitCharacterAttribute(LevelCap, 99.0f);

	// 默认累计经验为 0。
	InitCharacterAttribute(Experience, 0.0f);

	// 默认下一级经验需求为 0，后续由成长规则写入。
	InitCharacterAttribute(ExperienceToNextLevel, 0.0f);

	// 默认增幅点数为 0。
	InitCharacterAttribute(SkillUpgradePoints, 0.0f);

	// 默认增幅点数进度为 0。
	InitCharacterAttribute(SkillUpgradeProgress, 0.0f);

	// 默认当前 HP 为 1，避免新建角色处于 0 HP。
	InitCharacterAttribute(CurrentHP, 1.0f);

	// 默认最大 HP 为 1，避免最大生命值非法。
	InitCharacterAttribute(MaxHP, 1.0f);

	// 默认战斗 LV 为 0。
	InitCharacterAttribute(BattleLevel, 0.0f);

	// 默认驾驶 LV 为 0。
	InitCharacterAttribute(DrivingLevel, 0.0f);

	// 默认腕力为 0。
	InitCharacterAttribute(Strength, 0.0f);

	// 默认体力为 0。
	InitCharacterAttribute(Vitality, 0.0f);

	// 默认敏捷度为 0。
	InitCharacterAttribute(Agility, 0.0f);

	// 默认男子气概为 0。
	InitCharacterAttribute(Manliness, 0.0f);

	// 默认伤痕为 0。
	InitCharacterAttribute(Scars, 0.0f);

	// 默认攻击力为 0。
	InitCharacterAttribute(AttackPower, 0.0f);

	// 默认防御力为 0。
	InitCharacterAttribute(DefensePower, 0.0f);

	// 默认命中率为 100%。
	InitCharacterAttribute(HitRate, 100.0f);

	// 默认回避率为 0%。
	InitCharacterAttribute(EvasionRate, 0.0f);

	// 默认会心率为 0%。
	InitCharacterAttribute(CriticalRate, 0.0f);

	// 默认火焰防御为 0。
	InitCharacterAttribute(FireDefense, 0.0f);

	// 默认冷气防御为 0。
	InitCharacterAttribute(ColdDefense, 0.0f);

	// 默认电气防御为 0。
	InitCharacterAttribute(ElectricDefense, 0.0f);

	// 默认音波防御为 0。
	InitCharacterAttribute(SonicDefense, 0.0f);

	// 默认瓦斯防御为 0。
	InitCharacterAttribute(GasDefense, 0.0f);

	// 默认光束防御为 0。
	InitCharacterAttribute(BeamDefense, 0.0f);
}

void UMMCharacterAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 当前 HP 需要同步给客户端，保证 UI 和战斗反馈能获得变化通知。
	DOREPLIFETIME_CONDITION_NOTIFY(UMMCharacterAttributeSet, CurrentHP, COND_None, REPNOTIFY_Always);

	// 最大 HP 需要同步给客户端，保证 CurrentHP 的上限变化能正确刷新。
	DOREPLIFETIME_CONDITION_NOTIFY(UMMCharacterAttributeSet, MaxHP, COND_None, REPNOTIFY_Always);
}

void UMMCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCurrentHPAttribute())
	{
		// 当前 HP 不允许小于 0，也不允许超过当前最大 HP。
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHP());
	}
	else if (Attribute == GetMaxHPAttribute())
	{
		// 最大 HP 只限制不小于 0；不能像 Demo 那样限制到旧 MaxHP，否则最大 HP 无法提升。
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UMMCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetCurrentHPAttribute())
	{
		// GE 最终写入 CurrentHP 后再次 Clamp，避免伤害或治疗把 HP 写出合法范围。
		SetCharacterAttributeCurrentValue(CurrentHP, FMath::Clamp(GetCurrentHP(), 0.0f, GetMaxHP()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHPAttribute())
	{
		// GE 修改 MaxHP 后确保 MaxHP 非负，并同步修正 CurrentHP 上限。
		const float SafeMaxHP = FMath::Max(0.0f, GetMaxHP());
		SetCharacterAttributeCurrentValue(MaxHP, SafeMaxHP);
		SetCharacterAttributeCurrentValue(CurrentHP, FMath::Clamp(GetCurrentHP(), 0.0f, SafeMaxHP));
	}
}

void UMMCharacterAttributeSet::OnRep_CurrentHP(const FGameplayAttributeData& OldCurrentHP) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMMCharacterAttributeSet, CurrentHP, OldCurrentHP);
}

void UMMCharacterAttributeSet::OnRep_MaxHP(const FGameplayAttributeData& OldMaxHP) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMMCharacterAttributeSet, MaxHP, OldMaxHP);
}
