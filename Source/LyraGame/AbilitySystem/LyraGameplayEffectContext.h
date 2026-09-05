// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectTypes.h"

#include "LyraGameplayEffectContext.generated.h"

class AActor;
class FArchive;
class ILyraAbilitySourceInterface;
class UObject;
class UPhysicalMaterial;

USTRUCT()
struct FLyraGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FLyraGameplayEffectContext()
		: FGameplayEffectContext()
	{
	}

	FLyraGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
		: FGameplayEffectContext(InInstigator, InEffectCauser)
	{
	}

	/** 从句柄中提取 FLyraGameplayEffectContext；上下文不存在或类型不匹配时返回 nullptr。 */
	/** Returns the wrapped FLyraGameplayEffectContext from the handle, or nullptr if it doesn't exist or is the wrong type */
	static LYRAGAME_API FLyraGameplayEffectContext* ExtractEffectContext(struct FGameplayEffectContextHandle Handle);

	/** 设置作为 GameplayEffect 数值计算来源的对象。 */
	/** Sets the object used as the ability source */
	void SetAbilitySource(const ILyraAbilitySourceInterface* InObject, float InSourceLevel);

	/** 返回来源对象实现的 ILyraAbilitySourceInterface；该弱引用当前不复制，仅在权威端有效。 */
	/** Returns the ability source interface associated with the source object. Only valid on the authority. */
	const ILyraAbilitySourceInterface* GetAbilitySource() const;

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FLyraGameplayEffectContext* NewContext = new FLyraGameplayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			// 深拷贝 HitResult，避免复制后的 Context 与原对象共享命中结果内存。
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FLyraGameplayEffectContext::StaticStruct();
	}

	/** 覆盖网络序列化入口，以支持 Lyra 扩展字段。 */
	/** Overridden to serialize new fields */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

	/** 若 Context 包含 HitResult，则返回命中的 PhysicalMaterial。 */
	/** Returns the physical material from the hit result if there is one */
	const UPhysicalMaterial* GetPhysicalMaterial() const;

public:
	/** 标识同一发弹药产生的多枚投射物，例如霰弹枪一次射击中的多颗弹丸。 */
	/** ID to allow the identification of multiple bullets that were part of the same cartridge */
	UPROPERTY()
	int32 CartridgeID = -1;

protected:
	/** 技能效果的来源对象，应实现 ILyraAbilitySourceInterface；当前不会网络复制。 */
	/** Ability Source object (should implement ILyraAbilitySourceInterface). NOT replicated currently */
	UPROPERTY()
	TWeakObjectPtr<const UObject> AbilitySourceObject;
};

template<>
struct TStructOpsTypeTraits<FLyraGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FLyraGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

