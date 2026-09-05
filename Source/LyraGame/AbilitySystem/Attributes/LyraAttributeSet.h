// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"

#include "LyraAttributeSet.generated.h"

#define UE_API LYRAGAME_API

class AActor;
class ULyraAbilitySystemComponent;
class UObject;
class UWorld;
struct FGameplayEffectSpec;


/**
 * 为指定 GameplayAttribute 生成属性对象访问器、数值读写器和初始化函数。
 * 例如 ATTRIBUTE_ACCESSORS(ULyraHealthSet, Health) 会生成 GetHealthAttribute、GetHealth、SetHealth 和 InitHealth。
 */
/**
 * This macro defines a set of helper functions for accessing and initializing attributes.
 *
 * The following example of the macro:
 *		ATTRIBUTE_ACCESSORS(ULyraHealthSet, Health)
 * will create the following functions:
 *		static FGameplayAttribute GetHealthAttribute();
 *		float GetHealth() const;
 *		void SetHealth(float NewVal);
 *		void InitHealth(float NewVal);
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 属性变化事件委托；客户端上部分上下文参数可能为空。
 * @param EffectInstigator 最初发起本次效果的 Actor。
 * @param EffectCauser 实际造成属性变化的物理 Actor。
 * @param EffectSpec 导致本次变化的完整 GameplayEffectSpec。
 * @param EffectMagnitude 钳制前的原始效果幅值。
 * @param OldValue 属性变化前的值。
 * @param NewValue 属性变化后的值。
 */
/** 
 * Delegate used to broadcast attribute events, some of these parameters may be null on clients: 
 * @param EffectInstigator	The original instigating actor for this event
 * @param EffectCauser		The physical actor that caused the change
 * @param EffectSpec		The full effect spec for this change
 * @param EffectMagnitude	The raw magnitude, this is before clamping
 * @param OldValue			The value of the attribute before it was changed
 * @param NewValue			The value after it was changed
*/
DECLARE_MULTICAST_DELEGATE_SixParams(FLyraAttributeEvent, AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/, const FGameplayEffectSpec* /*EffectSpec*/, float /*EffectMagnitude*/, float /*OldValue*/, float /*NewValue*/);

/**
 * 项目中所有 AttributeSet 的公共基类。
 */
/**
 * ULyraAttributeSet
 *
 *	Base attribute set class for the project.
 */
UCLASS(MinimalAPI)
class ULyraAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UE_API ULyraAttributeSet();

	UE_API UWorld* GetWorld() const override;

	UE_API ULyraAbilitySystemComponent* GetLyraAbilitySystemComponent() const;
};

#undef UE_API
