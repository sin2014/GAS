// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayEffectContext.h"

#include "AbilitySystem/LyraAbilitySourceInterface.h"
#include "Engine/HitResult.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayEffectContext)

class FArchive;

// 从通用句柄安全提取 Lyra EffectContext；脚本结构类型不匹配时返回 nullptr。
FLyraGameplayEffectContext* FLyraGameplayEffectContext::ExtractEffectContext(struct FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* BaseEffectContext = Handle.Get();
	if ((BaseEffectContext != nullptr) && BaseEffectContext->GetScriptStruct()->IsChildOf(FLyraGameplayEffectContext::StaticStruct()))
	{
		return (FLyraGameplayEffectContext*)BaseEffectContext;
	}

	return nullptr;
}

// 复用基类网络序列化当前支持的字段，并通过 bOutSuccess 返回序列化结果。
bool FLyraGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	// CartridgeID 仅供激活阶段识别同一发弹药的多次命中，当前不为激活后的用途进行序列化。
	// Not serialized for post-activation use:
	// CartridgeID

	return true;
}

namespace UE::Net
{
	// Iris 复制暂时转发给 FGameplayEffectContextNetSerializer。
	// 若修改 FLyraGameplayEffectContext::NetSerialize()，必须同步实现自定义 NetSerializer，当前回退方案将不再足够。
	// Forward to FGameplayEffectContextNetSerializer
	// Note: If FLyraGameplayEffectContext::NetSerialize() is modified, a custom NetSerializer must be implemented as the current fallback will no longer be sufficient.
	UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(LyraGameplayEffectContext, FGameplayEffectContextNetSerializer);
}

// 以弱引用保存实现 ILyraAbilitySourceInterface 的效果来源；SourceLevel 当前尚未持久化。
void FLyraGameplayEffectContext::SetAbilitySource(const ILyraAbilitySourceInterface* InObject, float InSourceLevel)
{
	AbilitySourceObject = MakeWeakObjectPtr(Cast<const UObject>(InObject));
	//SourceLevel = InSourceLevel;
}

// 将仍有效的来源对象转换为 ILyraAbilitySourceInterface，失效或类型不符时返回 nullptr。
const ILyraAbilitySourceInterface* FLyraGameplayEffectContext::GetAbilitySource() const
{
	return Cast<ILyraAbilitySourceInterface>(AbilitySourceObject.Get());
}

// 从 HitResult 返回命中的 PhysicalMaterial；没有命中结果时返回 nullptr。
const UPhysicalMaterial* FLyraGameplayEffectContext::GetPhysicalMaterial() const
{
	if (const FHitResult* HitResultPtr = GetHitResult())
	{
		return HitResultPtr->PhysMaterial.Get();
	}
	return nullptr;
}

