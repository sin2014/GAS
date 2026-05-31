// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MMCharacterAttributeSet.generated.h"

// 为角色 AttributeSet 声明标准 GAS 访问器。
#define MM_CHARACTER_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
		GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// 角色 GAS 属性集合；当前只定义角色数值属性，不包含装备、技能、存档或其它系统数据。
UCLASS()
class MMM_API UMMCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	// 初始化角色属性默认值。
	UMMCharacterAttributeSet();

	// 注册需要网络同步的角色属性。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 属性变化前的保护入口，用于限制属性合法范围。
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// GameplayEffect 修改属性后的保护入口，用于处理最终写入后的属性范围。
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	// 当前 HP，表示角色现在剩余的生命值。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHP, Category = "MM|角色|生命")
	FGameplayAttributeData CurrentHP;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, CurrentHP)

	// 最大 HP，表示角色生命值上限。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHP, Category = "MM|角色|生命")
	FGameplayAttributeData MaxHP;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, MaxHP)

	// 当前 HP 复制通知，用于让 GAS 正确广播属性变化。
	UFUNCTION()
	void OnRep_CurrentHP(const FGameplayAttributeData& OldCurrentHP) const;

	// 最大 HP 复制通知，用于让 GAS 正确广播属性变化。
	UFUNCTION()
	void OnRep_MaxHP(const FGameplayAttributeData& OldMaxHP) const;

	// 当前等级，用于角色成长、经验阈值和技能学习判断。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|成长")
	FGameplayAttributeData Level;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Level)

	// 等级上限，用于限制角色成长的最高等级。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|成长")
	FGameplayAttributeData LevelCap;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, LevelCap)

	// 累计经验值，用于升级判断和角色成长显示。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|成长")
	FGameplayAttributeData Experience;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Experience)

	// 距离下一级所需经验，用于菜单显示和升级提示。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|成长")
	FGameplayAttributeData ExperienceToNextLevel;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, ExperienceToNextLevel)

	// 增幅点数或技能强化点数，用于后续技能强化和角色成长系统。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|成长")
	FGameplayAttributeData SkillUpgradePoints;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, SkillUpgradePoints)

	// 增幅点数进度，用于累计获得下一点增幅点数的进度。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|成长")
	FGameplayAttributeData SkillUpgradeProgress;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, SkillUpgradeProgress)

	// 战斗 LV，表示角色人身战斗熟练度。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData BattleLevel;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, BattleLevel)

	// 驾驶 LV，表示角色驾驶战车和载具战斗的熟练度。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData DrivingLevel;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, DrivingLevel)

	// 腕力，表示角色力量相关能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData Strength;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Strength)

	// 体力，表示角色生命力和耐久相关能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData Vitality;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Vitality)

	// 敏捷度，表示角色行动顺序、回避相关计算和部分战斗判定使用的基础能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData Agility;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Agility)

	// 男子气概，表示原作中的个性化特殊属性。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData Manliness;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Manliness)

	// 伤痕，角色每次死亡之后伤痕+1。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|核心属性")
	FGameplayAttributeData Scars;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, Scars)

	// 攻击力，表示角色当前用于伤害计算的综合攻击能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|战斗")
	FGameplayAttributeData AttackPower;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, AttackPower)
	
	// 会心率，100.0 表示 100%。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|战斗")
	FGameplayAttributeData CriticalRate;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, CriticalRate)
	
	// 命中率，100.0 表示 100%。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|战斗")
	FGameplayAttributeData HitRate;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, HitRate)

	// 回避率，100.0 表示 100%。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|战斗")
	FGameplayAttributeData EvasionRate;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, EvasionRate)

	// 防御力，表示角色当前承受伤害时使用的综合防御能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|战斗")
	FGameplayAttributeData DefensePower;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, DefensePower)

	// 火焰防御，表示角色抵抗火焰属性攻击的能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|属性防御")
	FGameplayAttributeData FireDefense;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, FireDefense)

	// 冷气防御，表示角色抵抗冷气属性攻击的能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|属性防御")
	FGameplayAttributeData ColdDefense;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, ColdDefense)

	// 电气防御，表示角色抵抗电气属性攻击的能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|属性防御")
	FGameplayAttributeData ElectricDefense;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, ElectricDefense)

	// 音波防御，表示角色抵抗音波属性攻击的能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|属性防御")
	FGameplayAttributeData SonicDefense;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, SonicDefense)

	// 瓦斯防御，表示角色抵抗瓦斯属性攻击的能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|属性防御")
	FGameplayAttributeData GasDefense;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, GasDefense)

	// 光束防御，表示角色抵抗光束属性攻击的能力。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MM|角色|属性防御")
	FGameplayAttributeData BeamDefense;
	MM_CHARACTER_ATTRIBUTE_ACCESSORS(UMMCharacterAttributeSet, BeamDefense)
};

#undef MM_CHARACTER_ATTRIBUTE_ACCESSORS
