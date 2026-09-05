// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"

#include "LyraGameplayAbilityTargetData_SingleTargetHit.generated.h"

class FArchive;
struct FGameplayEffectContextHandle;


/** 在 GAS 的单目标命中数据上扩展 Lyra 所需的同发弹药标识。 */
/** Game-specific additions to SingleTargetHit tracking */
USTRUCT()
struct FLyraGameplayAbilityTargetData_SingleTargetHit : public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	FLyraGameplayAbilityTargetData_SingleTargetHit()
		: CartridgeID(-1)
	{ }

	virtual void AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const override;

	/** 标识同一发弹药产生的多枚投射物，例如一次霰弹射击中的多颗弹丸。 */
	/** ID to allow the identification of multiple bullets that were part of the same cartridge */
	UPROPERTY()
	int32 CartridgeID;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FLyraGameplayAbilityTargetData_SingleTargetHit::StaticStruct();
	}
};

template<>
struct TStructOpsTypeTraits<FLyraGameplayAbilityTargetData_SingleTargetHit> : public TStructOpsTypeTraitsBase2<FLyraGameplayAbilityTargetData_SingleTargetHit>
{
	enum
	{
		WithNetSerializer = true	/* 当前必须启用，否则 FGameplayAbilityTargetDataHandle 无法完成网络序列化。 */ // For now this is REQUIRED for FGameplayAbilityTargetDataHandle net serialization to work
	};
};

