// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAttributeSet.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAttributeSet)

class UWorld;


// 构造 Lyra AttributeSet 基类实例，不额外初始化具体 GameplayAttribute。
ULyraAttributeSet::ULyraAttributeSet()
{
}

// 通过所属 ASC 的 OwnerActor 返回当前 World；缺少有效归属时返回 nullptr。
UWorld* ULyraAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);

	return Outer->GetWorld();
}

// 将 owning ASC 转换为项目使用的 ULyraAbilitySystemComponent。
ULyraAbilitySystemComponent* ULyraAttributeSet::GetLyraAbilitySystemComponent() const
{
	return Cast<ULyraAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

