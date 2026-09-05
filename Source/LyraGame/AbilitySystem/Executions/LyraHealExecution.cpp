// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHealExecution.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHealExecution)


struct FHealStatics
{
	FGameplayEffectAttributeCaptureDefinition BaseHealDef;

	// 构造并固定来源侧 BaseHeal 的属性捕获定义，使用创建 Spec 时的快照值。
	FHealStatics()
	{
		BaseHealDef = FGameplayEffectAttributeCaptureDefinition(ULyraCombatSet::GetBaseHealAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
	}
};

// 返回进程内唯一的治疗属性捕获定义集合，避免每次执行重复构造。
static FHealStatics& HealStatics()
{
	static FHealStatics Statics;
	return Statics;
}


// 注册治疗执行计算需要捕获的来源侧 BaseHeal 属性定义。
ULyraHealExecution::ULyraHealExecution()
{
	RelevantAttributesToCapture.Add(HealStatics().BaseHealDef);
}

// 计算非负治疗量，并向执行输出写入目标 Healing 元属性 Modifier。
void ULyraHealExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = SourceTags;
	EvaluateParameters.TargetTags = TargetTags;

	float BaseHeal = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealStatics().BaseHealDef, EvaluateParameters, BaseHeal);

	const float HealingDone = FMath::Max(0.0f, BaseHeal);

	if (HealingDone > 0.0f)
	{
		// 将结果累加到目标的 Healing 元属性，随后由 ULyraHealthSet 转换为 Health 增量。
		// Apply a healing modifier, this gets turned into + health on the target
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(ULyraHealthSet::GetHealingAttribute(), EGameplayModOp::Additive, HealingDone));
	}
#endif // #if WITH_SERVER_CODE
}

