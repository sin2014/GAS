// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameMode.h"

#include "ModularGameMode.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

class UObject;

/** 与 ModularGameStateBase 配套使用的模块化 GameModeBase。 */
/** Pair this with a ModularGameStateBase */
UCLASS(MinimalAPI, Blueprintable)
class AModularGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	UE_API AModularGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** 与 ModularGameState 配套使用的模块化完整 GameMode。 */
/** Pair this with a ModularGameState */
UCLASS(MinimalAPI, Blueprintable)
class AModularGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	UE_API AModularGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

#undef UE_API
