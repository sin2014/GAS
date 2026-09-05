// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraCharacterMovementComponent.h"

#include "TopDownArenaMovementComponent.generated.h"

class UObject;

UCLASS()
class UTopDownArenaMovementComponent : public ULyraCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UTopDownArenaMovementComponent(const FObjectInitializer& ObjectInitializer);

	// UMovementComponent 接口开始。
	//~UMovementComponent interface
	virtual float GetMaxSpeed() const override;
	// UMovementComponent 接口结束。
	//~End of UMovementComponent interface

};
