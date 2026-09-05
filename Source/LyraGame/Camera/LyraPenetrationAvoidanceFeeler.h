// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LyraPenetrationAvoidanceFeeler.generated.h"

/**
 * 定义一条用于第三人称相机穿透规避的球形探针射线。
 */
/**
 * Struct defining a feeler ray used for camera penetration avoidance.
 */
USTRUCT()
struct FLyraPenetrationAvoidanceFeeler
{
	GENERATED_BODY()

	/** 相对主探针方向的旋转偏移。 */
	/** FRotator describing deviance from main ray */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	FRotator AdjustmentRot;

	/** 探针命中世界静态物体时，对最终受阻距离的影响权重。 */
	/** how much this feeler affects the final position if it hits the world */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	float WorldWeight;

	/** 命中 APawn 时的影响权重；设为 0 时不对 Pawn 执行碰撞检测。 */
	/** how much this feeler affects the final position if it hits a APawn (setting to 0 will not attempt to collide with pawns at all) */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	float PawnWeight;

	/** 该探针执行球形扫描时使用的半径。 */
	/** extent to use for collision when tracing this feeler */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	float Extent;

	/** 上次未命中时，该探针两次检测之间至少跳过的帧数。 */
	/** minimum frame interval between traces with this feeler if nothing was hit last frame */
	UPROPERTY(EditAnywhere, Category=PenetrationAvoidanceFeeler)
	int32 TraceInterval;

	/** 距离该探针下次允许检测还需等待的帧数。 */
	/** number of frames since this feeler was used */
	UPROPERTY(transient)
	int32 FramesUntilNextTrace;


	FLyraPenetrationAvoidanceFeeler()
		: AdjustmentRot(ForceInit)
		, WorldWeight(0)
		, PawnWeight(0)
		, Extent(0)
		, TraceInterval(0)
		, FramesUntilNextTrace(0)
	{
	}

	FLyraPenetrationAvoidanceFeeler(const FRotator& InAdjustmentRot,
									const float& InWorldWeight, 
									const float& InPawnWeight, 
									const float& InExtent, 
									const int32& InTraceInterval = 0, 
									const int32& InFramesUntilNextTrace = 0)
		: AdjustmentRot(InAdjustmentRot)
		, WorldWeight(InWorldWeight)
		, PawnWeight(InPawnWeight)
		, Extent(InExtent)
		, TraceInterval(InTraceInterval)
		, FramesUntilNextTrace(InFramesUntilNextTrace)
	{
	}
};
