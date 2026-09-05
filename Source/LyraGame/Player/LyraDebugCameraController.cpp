// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraDebugCameraController.h"
#include "LyraCheatManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraDebugCameraController)


// 将调试相机的 CheatClass 设置为 LyraCheatManager，使进入自由相机后仍可执行 Lyra 作弊命令。
ALyraDebugCameraController::ALyraDebugCameraController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 与 LyraPlayerController 共用 CheatManager，使调试相机中仍可通过作弊命令切换相机。
	// Use the same cheat class as LyraPlayerController to allow toggling the debug camera through cheats.
	CheatClass = ULyraCheatManager::StaticClass();
}

// 始终为调试相机创建 CheatManager，确保玩家可通过命令退出调试相机。
void ALyraDebugCameraController::AddCheats(bool bForce)
{
	// 与 LyraPlayerController::AddCheats() 保持一致，确保可再次执行命令退出调试相机。
	// Mirrors LyraPlayerController's AddCheats() to avoid the player becoming stuck in the debug camera.
#if USING_CHEAT_MANAGER
	Super::AddCheats(true);
#else //#if USING_CHEAT_MANAGER
	Super::AddCheats(bForce);
#endif // #else //#if USING_CHEAT_MANAGER
}

