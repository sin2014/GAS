// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"

#include "LyraHealExecution.generated.h"

class UObject;


/**
 * GameplayEffect 的治疗执行计算：读取来源侧 BaseHeal，输出目标 Healing 元属性增量。
 */
/**
 * ULyraHealExecution
 *
 *	Execution used by gameplay effects to apply healing to the health attributes.
 */
UCLASS()
class ULyraHealExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:

	ULyraHealExecution();

protected:

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
