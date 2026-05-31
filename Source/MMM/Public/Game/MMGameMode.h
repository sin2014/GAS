// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MMGameMode.generated.h"

// 最小游戏模式；指定默认玩家角色和玩家控制器。
UCLASS()
class MMM_API AMMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// 设置默认 Pawn 和 PlayerController。
	AMMGameMode();
};
