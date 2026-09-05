// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameState.h"

#include "ModularGameState.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

class UObject;

/** 与 ModularGameModeBase 配套使用的模块化 GameStateBase。 */
/** Pair this with a ModularGameModeBase */
UCLASS(MinimalAPI, Blueprintable)
class AModularGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	//~ 开始实现 AActor 接口。
	//~ Begin AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ 结束实现 AActor 接口。
	//~ End AActor interface
};


/** 与 ModularGameState 配套使用的模块化完整 GameState。 */
/** Pair this with a ModularGameState */
UCLASS(MinimalAPI, Blueprintable)
class AModularGameState : public AGameState
{
	GENERATED_BODY()

public:
	//~ 开始实现 AActor 接口。
	//~ Begin AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ 结束实现 AActor 接口。
	//~ End AActor interface

protected:
	//~ 开始实现 AGameState 接口。
	//~ Begin AGameState interface
	UE_API virtual void HandleMatchHasStarted() override;
	//~ 结束实现 AGameState 接口。
	//~ Begin AGameState interface
};

#undef UE_API
