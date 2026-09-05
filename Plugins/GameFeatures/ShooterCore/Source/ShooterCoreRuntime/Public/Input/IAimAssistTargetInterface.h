// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/AimAssistInputModifier.h"
#include "UObject/Interface.h"
#include "IAimAssistTargetInterface.generated.h"

USTRUCT(BlueprintType)
struct FAimAssistTargetOptions
{
	GENERATED_BODY()
	
	FAimAssistTargetOptions()
		: bIsActive(true)
	{}

	/** Aim Assist 计算该目标屏幕边界和可见性时使用的 ShapeComponent。 */
	/** The shape component that should be used when considering this target's hitbox */
	TWeakObjectPtr<UShapeComponent> TargetShapeComponent;

	/** 与目标关联的 GameplayTag；玩家 Filter 命中任一排除标签时忽略该目标。 */
	/**
	 * Gameplay tags that are associated with this target that can be used to filter it out.
	 *
	 * If the player's aim assist settings have any tags that match these, it will be excluded.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTagContainer AssociatedTags;

	/** 目标当前是否激活；为 false 时完全不参与 Aim Assist 候选计算。 */
	/** Whether or not this target is currently active. If false, it will not be considered for aim assist */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	uint8 bIsActive : 1;
};


UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAimAssistTaget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 定义 Aim Assist 目标使用的形状和关联标签。目标进入玩家外圈准星后，管理器通过此接口提取筛选与命中数据。
 */
/**
 * Used to define the shape of an aim assist target as well as let the aim assist manager know
 * about any associated gameplay tags.
 * 
 * The target will be considered when it is within the view of a player's outer reticle
 *
 * @see UAimAssistTargetComponent for an example
 */
class IAimAssistTaget
{
	GENERATED_BODY()

public:
	/** 当目标进入玩家视野候选范围时填充形状、标签和激活状态。 */
	/** Populate the given target data with this interface. This will be called when a target is within view of the player */
	virtual void GatherTargetOptions(OUT FAimAssistTargetOptions& TargetData) = 0;
};
