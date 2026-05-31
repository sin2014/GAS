// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/MMGameMode.h"

#include "Character/MMCharacterBase.h"
#include "Player/MMPlayerController.h"

AMMGameMode::AMMGameMode()
{
	// 默认生成最小 GAS 角色。
	DefaultPawnClass = AMMCharacterBase::StaticClass();

	// 默认使用四向移动玩家控制器。
	PlayerControllerClass = AMMPlayerController::StaticClass();
}
