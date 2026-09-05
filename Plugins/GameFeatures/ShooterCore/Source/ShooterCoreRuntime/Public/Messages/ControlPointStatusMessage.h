// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ControlPointStatusMessage.generated.h"

// 表示控制点归属状态发生变化的 Gameplay Message，携带控制点和当前队伍 ID。
// Message indicating the state of a control point is changing
USTRUCT(BlueprintType)
struct FLyraControlPointStatusMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category=Gameplay)
	TObjectPtr<AActor> ControlPoint = nullptr;

	UPROPERTY(BlueprintReadWrite, Category=Gameplay)
	int32 OwnerTeamID = 0;
};
