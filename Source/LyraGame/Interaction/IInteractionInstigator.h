// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "InteractionOption.h"
#include "IInteractionInstigator.generated.h"

struct FInteractionQuery;

/**  */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractionInstigator : public UInterface
{
	GENERATED_BODY()
};

/**
 * 为交互发起者提供多 Option 仲裁能力，例如弹出菜单让玩家选择实际执行的交互。
 * 当 ULyraGameplayAbility_Interact 派生类产生多个候选时，可通过该接口选择最佳项。
 */
/**
 * Implementing this interface allows you to add an arbitrator to the interaction process.  For example,
 * some games present the user with a menu to pick which interaction they want to perform.  This will allow you
 * to take the multiple matches (Assuming your ULyraGameplayAbility_Interact subclass generates more than one option).
 */
class IInteractionInstigator
{
	GENERATED_BODY()

public:
	/** 存在多个候选 InteractionOption 时调用，返回最终选中的一项。 */
	/** Will be called if there are more than one InteractOptions that need to be decided on. */
	virtual FInteractionOption ChooseBestInteractionOption(const FInteractionQuery& InteractQuery, const TArray<FInteractionOption>& InteractOptions) = 0;
};
