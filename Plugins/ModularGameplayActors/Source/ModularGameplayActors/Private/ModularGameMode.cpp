// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularGameMode.h"

#include "ModularGameState.h"
#include "ModularPawn.h"
#include "ModularPlayerController.h"
#include "ModularPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGameMode)

// 为 GameModeBase 配置与其配套的模块化 GameStateBase、Controller、PlayerState 和 Pawn 默认类。
AModularGameModeBase::AModularGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AModularGameStateBase::StaticClass();
	PlayerControllerClass = AModularPlayerController::StaticClass();
	PlayerStateClass = AModularPlayerState::StaticClass();
	DefaultPawnClass = AModularPawn::StaticClass();
}

// 为完整 AGameMode 配置模块化 GameState、Controller、PlayerState 和 Pawn 默认类。
AModularGameMode::AModularGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AModularGameState::StaticClass();
	PlayerControllerClass = AModularPlayerController::StaticClass();
	PlayerStateClass = AModularPlayerState::StaticClass();
	DefaultPawnClass = AModularPawn::StaticClass();
}

