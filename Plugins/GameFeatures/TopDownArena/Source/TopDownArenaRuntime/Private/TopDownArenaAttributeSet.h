// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "AbilitySystemComponent.h"

#include "TopDownArenaAttributeSet.generated.h"

class UObject;
struct FFrame;

/**
 * 定义 TopDownArena 模式专用的炸弹数量、容量、爆炸范围和移动速度属性，并负责复制通知与合法范围约束。
 */
/**
 * UTopDownArenaAttributeSet
 *
 *	Class that defines attributes specific to the top-down arena gameplay mode.
 */
UCLASS(BlueprintType)
class UTopDownArenaAttributeSet : public ULyraAttributeSet
{
	GENERATED_BODY()

public:
	UTopDownArenaAttributeSet();

	ATTRIBUTE_ACCESSORS(ThisClass, BombsRemaining);
	ATTRIBUTE_ACCESSORS(ThisClass, BombCapacity);
	ATTRIBUTE_ACCESSORS(ThisClass, BombRange);
	ATTRIBUTE_ACCESSORS(ThisClass, MovementSpeed);

	// UAttributeSet 接口开始。
	//~UAttributeSet interface
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	// UAttributeSet 接口结束。
	//~End of UAttributeSet interface

protected:

	UFUNCTION()
	void OnRep_BombsRemaining(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BombCapacity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BombRange(const FGameplayAttributeData& OldValue);
	
	UFUNCTION()
	void OnRep_MovementSpeed(const FGameplayAttributeData& OldValue);

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	// 当前剩余、可放置的炸弹数量。
	// The number of bombs remaining
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BombsRemaining, Category="TopDownArenaGame", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BombsRemaining;

	// 同时允许放置的炸弹数量上限。
	// The maximum number of bombs that can be placed at once
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BombCapacity, Category="TopDownArenaGame", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BombCapacity;

	// 炸弹爆炸影响的范围或半径。
	// The range/radius of bomb blasts
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BombRange, Category="TopDownArenaGame", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData BombRange;

	// 角色步行时由移动组件读取的移动速度。
	// The range/radius of bomb blasts
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MovementSpeed, Category="TopDownArenaGame", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MovementSpeed;
};
