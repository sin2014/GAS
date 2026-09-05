// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTaggedActor.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTaggedActor)

// 构造提供静态 GameplayTag 的基础 Actor。
ALyraTaggedActor::ALyraTaggedActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 将该 Actor 配置的静态 GameplayTag 完整写入输出容器。
void ALyraTaggedActor::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(StaticGameplayTags);
}

#if WITH_EDITOR
// 编辑器中禁止修改 AActor::Tags，其他属性沿用父类可编辑性判断。
bool ALyraTaggedActor::CanEditChange(const FProperty* InProperty) const
{
	// 禁止编辑 AActor 自带的普通 Tags 属性，避免与下方 GameplayTag 容器混淆。
	// Prevent editing of the other tags property
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Tags))
	{
		return false;
	}

	return Super::CanEditChange(InProperty);
}
#endif

