// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraCameraMode.h"
#include "Curves/CurveFloat.h"
#include "LyraPenetrationAvoidanceFeeler.h"
#include "DrawDebugHelpers.h"
#include "LyraCameraMode_ThirdPerson.generated.h"

class UCurveVector;

/**
 * 基础第三人称相机模式，根据俯仰角计算跟随偏移，并通过探针避免相机穿入场景。
 */
/**
 * ULyraCameraMode_ThirdPerson
 *
 *	A basic third person camera mode.
 */
UCLASS(Abstract, Blueprintable)
class ULyraCameraMode_ThirdPerson : public ULyraCameraMode
{
	GENERATED_BODY()

public:

	ULyraCameraMode_ThirdPerson();

protected:

	virtual void UpdateView(float DeltaTime) override;

	void UpdateForTarget(float DeltaTime);
	void UpdatePreventPenetration(float DeltaTime);
	void PreventCameraPenetration(class AActor const& ViewTarget, FVector const& SafeLoc, FVector& CameraLoc, float const& DeltaTime, float& DistBlockedPct, bool bSingleRayOnly);

	virtual void DrawDebug(UCanvas* Canvas) const override;

protected:

	// 根据视角俯仰角采样，定义相对目标的局部空间相机偏移。
	// Curve that defines local-space offsets from the target using the view pitch to evaluate the curve.
	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "!bUseRuntimeFloatCurves"))
	TObjectPtr<const UCurveVector> TargetOffsetCurve;

	// UE-103986：PIE 期间无法像曲线资产一样实时编辑 RuntimeFloatCurve。
	// 该问题解决后将默认使用运行时曲线，并移除 TargetOffsetCurve。
	// UE-103986: Live editing of RuntimeFloatCurves during PIE does not work (unlike curve assets).
	// Once that is resolved this will become the default and TargetOffsetCurve will be removed.
	UPROPERTY(EditDefaultsOnly, Category = "Third Person")
	bool bUseRuntimeFloatCurves;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "bUseRuntimeFloatCurves"))
	FRuntimeFloatCurve TargetOffsetX;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "bUseRuntimeFloatCurves"))
	FRuntimeFloatCurve TargetOffsetY;

	UPROPERTY(EditDefaultsOnly, Category = "Third Person", Meta = (EditCondition = "bUseRuntimeFloatCurves"))
	FRuntimeFloatCurve TargetOffsetZ;

	// 控制蹲伏相机偏移淡入和淡出的插值速度。
	// Alters the speed that a crouch offset is blended in or out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Third Person")
	float CrouchOffsetBlendMultiplier = 5.0f;

	// 相机穿透防护配置。
	// Penetration prevention
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float PenetrationBlendInTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float PenetrationBlendOutTime = 0.15f;

	/** 为 true 时执行碰撞检测，防止相机进入场景几何体。 */
	/** If true, does collision checks to keep the camera out of the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bPreventPenetration = true;

	/** 为 true 时使用附加探针预判附近墙体并提前拉近相机，以减少突然跳变。 */
	/** If true, try to detect nearby walls and move the camera in anticipation.  Helps prevent popping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	bool bDoPredictiveAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionPushOutDistance = 2.f;

	/** 相机因遮挡被压缩到完整臂长的该比例以内时，向 CameraAssist 报告穿入目标。 */
	/** When the camera's distance is pushed into this percentage of its full distance due to penetration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float ReportPenetrationPercent = 0.f;

	/**
	 * 用于确定相机可放置位置的一组探针射线。
	 * 索引 0 是始终执行的主探针，用于实际防碰撞；索引 1 及以后仅在 bDoPredictiveAvoidance 为 true 时使用，
	 * 它们扫描玩家可能转向的位置，使相机在撞上遮挡物前提前拉近。
	 */
	/**
	 * These are the feeler rays that are used to find where to place the camera.
	 * Index: 0  : This is the normal feeler we use to prevent collisions.
	 * Index: 1+ : These feelers are used if you bDoPredictiveAvoidance=true, to scan for potential impacts if the player
	 *             were to rotate towards that direction and primitively collide the camera so that it pulls in before
	 *             impacting the occluder.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	TArray<FLyraPenetrationAvoidanceFeeler> PenetrationAvoidanceFeelers;

	UPROPERTY(Transient)
	float AimLineToDesiredPosBlockedPct;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const AActor>> DebugActorsHitDuringCameraPenetration;

#if ENABLE_DRAW_DEBUG
	mutable float LastDrawDebugTime = -MAX_FLT;
#endif

protected:
	
	void SetTargetCrouchOffset(FVector NewTargetOffset);
	void UpdateCrouchOffset(float DeltaTime);

	FVector InitialCrouchOffset = FVector::ZeroVector;
	FVector TargetCrouchOffset = FVector::ZeroVector;
	float CrouchOffsetBlendPct = 1.0f;
	FVector CurrentCrouchOffset = FVector::ZeroVector;
	
};
