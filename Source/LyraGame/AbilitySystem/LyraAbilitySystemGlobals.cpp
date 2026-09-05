// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAbilitySystemGlobals.h"

#include "LyraGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAbilitySystemGlobals)

struct FGameplayEffectContext;

// 构造项目 AbilitySystemGlobals 实例。
ULyraAbilitySystemGlobals::ULyraAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 为 GAS 分配 Lyra 扩展的 GameplayEffectContext，使效果上下文携带项目字段。
FGameplayEffectContext* ULyraAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FLyraGameplayEffectContext();
}

