// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DebugCameraController.h"

#include "LyraDebugCameraController.generated.h"

class UObject;


/**
 * 由 CheatManager 启用调试相机后负责自由相机控制，并复用 Lyra 的作弊命令。
 */
/**
 * ALyraDebugCameraController
 *
 *	Used for controlling the debug camera when it is enabled via the cheat manager.
 */
UCLASS()
class ALyraDebugCameraController : public ADebugCameraController
{
	GENERATED_BODY()

public:

	ALyraDebugCameraController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	virtual void AddCheats(bool bForce) override;
};
