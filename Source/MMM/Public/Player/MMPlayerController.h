// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MMPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// 最小玩家控制器；负责通过 Enhanced Input 驱动严格四向移动。
UCLASS()
class MMM_API AMMPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 设置鼠标显示等基础玩家控制器状态。
	AMMPlayerController();

protected:
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

private:
	// 运行时创建的移动输入动作；当前只负责键盘四向移动。
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	// 运行时创建的键盘移动映射；包含 WASD 和方向键。
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MovementMappingContext;

	// 确保默认输入动作和映射已创建。
	void EnsureDefaultInputAssets();

	// Enhanced Input 的 Move 回调；把 2D 输入值转成四向移动。
	void HandleMoveInput(const FInputActionValue& Value);

	// 根据 Enhanced Input 的 2D Move 值计算四向移动方向；同时按横向和纵向时优先纵向。
	FVector GetFourWayMovementDirection(const FVector2D& MovementValue) const;
};
