// Copyright Epic Games, Inc. All Rights Reserved.

#include "TopDownArenaMovementComponent.h"

#include "AbilitySystemGlobals.h"
#include "TopDownArenaAttributeSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TopDownArenaMovementComponent)

// 使用 Lyra 角色移动组件的默认初始化流程构造 TopDownArena 移动组件。
UTopDownArenaMovementComponent::UTopDownArenaMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 步行时优先服从 MovementStopped GameplayTag，其次读取 GAS 的 MovementSpeed 属性；其他移动模式或无有效属性时回退父类速度。
float UTopDownArenaMovementComponent::GetMaxSpeed() const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (MovementMode == MOVE_Walking)
		{
			if (ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped))
			{
				return 0;
			}

			const float MaxSpeedFromAttribute = ASC->GetNumericAttribute(UTopDownArenaAttributeSet::GetMovementSpeedAttribute());
			if (MaxSpeedFromAttribute > 0.0f)
			{
				return MaxSpeedFromAttribute;
			}
		}
	}

	return Super::GetMaxSpeed();
}
