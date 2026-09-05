// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/LyraCameraMode.h"
#include "Curves/CurveFloat.h"

#include "LyraCameraMode_TopDownArenaCamera.generated.h"

class UObject;


/**
 * TopDownArena 的固定俯视相机模式，根据竞技场边界尺寸通过曲线计算相机抬升距离，从而完整覆盖场地。
 */
/**
 * ULyraCameraMode_TopDownArenaCamera
 *
 *	A basic third person camera mode that looks down at a fixed arena.
 */
UCLASS(Abstract, Blueprintable)
class ULyraCameraMode_TopDownArenaCamera : public ULyraCameraMode
{
	GENERATED_BODY()

public:

	ULyraCameraMode_TopDownArenaCamera();

protected:

	// ULyraCameraMode 接口开始。
	//~ULyraCameraMode interface
	virtual void UpdateView(float DeltaTime) override;
	// ULyraCameraMode 接口结束。
	//~End of ULyraCameraMode interface

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	float ArenaWidth;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	float ArenaHeight;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	FRotator DefaultPivotRotation;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	FRuntimeFloatCurve BoundsSizeToDistance;
};
