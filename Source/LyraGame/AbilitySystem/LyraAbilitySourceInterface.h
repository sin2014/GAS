// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "LyraAbilitySourceInterface.generated.h"

class UObject;
class UPhysicalMaterial;
struct FGameplayTagContainer;

/** 供武器、装备等对象作为 GameplayEffect 数值计算来源时实现的基础接口。 */
/** Base interface for anything acting as a ability calculation source */
UINTERFACE()
class ULyraAbilitySourceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ILyraAbilitySourceInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * 计算 GameplayEffect 数值随来源到目标距离衰减时使用的倍率。
	 *
	 * @param Distance 来源到目标的计算距离，例如子弹实际飞行距离。
	 * @param SourceTags 来源聚合后的 GameplayTag。
	 * @param TargetTags 目标当前聚合后的 GameplayTag。
	 *
	 * @return 应乘到基础属性数值上的距离衰减倍率。
	 */
	/**
	 * Compute the multiplier for effect falloff with distance
	 * 
	 * @param Distance			Distance from source to target for ability calculations (distance bullet traveled for a gun, etc...)
	 * @param SourceTags		Aggregated Tags from the source
	 * @param TargetTags		Aggregated Tags currently on the target
	 * 
	 * @return Multiplier to apply to the base attribute value due to distance
	 */
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;

	virtual float GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;
};
