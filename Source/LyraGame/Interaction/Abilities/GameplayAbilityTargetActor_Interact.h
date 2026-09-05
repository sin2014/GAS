// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetActor_Trace.h"

#include "GameplayAbilityTargetActor_Interact.generated.h"

class AActor;
class UObject;


/** 所有交互 GameplayAbilityTargetActor 的中间基类。 */
/** Intermediate base class for all interaction target actors. */
UCLASS(Blueprintable)
class AGameplayAbilityTargetActor_Interact : public AGameplayAbilityTargetActor_Trace
{
	GENERATED_BODY()

public:
	AGameplayAbilityTargetActor_Interact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

protected:
};
