// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayAbilityTargetData_SingleTargetHit.h"

#include "LyraGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayAbilityTargetData_SingleTargetHit)

struct FGameplayEffectContextHandle;

//////////////////////////////////////////////////////////////////////

// 先写入基类单目标命中数据，再把 CartridgeID 复制到 Lyra EffectContext。
void FLyraGameplayAbilityTargetData_SingleTargetHit::AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const
{
	FGameplayAbilityTargetData_SingleTargetHit::AddTargetDataToContext(Context, bIncludeActorArray);

	// 将 Lyra 扩展的 CartridgeID 写入 GameplayEffectContext，供后续伤害与命中归组逻辑使用。
	// Add game-specific data
	if (FLyraGameplayEffectContext* TypedContext = FLyraGameplayEffectContext::ExtractEffectContext(Context))
	{
		TypedContext->CartridgeID = CartridgeID;
	}
}

// 序列化基类命中数据与 CartridgeID，并通过 bOutSuccess 汇总网络序列化结果。
bool FLyraGameplayAbilityTargetData_SingleTargetHit::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayAbilityTargetData_SingleTargetHit::NetSerialize(Ar, Map, bOutSuccess);

	Ar << CartridgeID;

	return true;
}

