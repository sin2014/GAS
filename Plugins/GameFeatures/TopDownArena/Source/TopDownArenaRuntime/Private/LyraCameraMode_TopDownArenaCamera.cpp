// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCameraMode_TopDownArenaCamera.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCameraMode_TopDownArenaCamera)

// 设置默认半宽和半高，未由蓝图覆盖时形成 2000×2000 的竞技场取景边界。
ULyraCameraMode_TopDownArenaCamera::ULyraCameraMode_TopDownArenaCamera()
{
	ArenaWidth = 1000.0f;
	ArenaHeight = 1000.0f;
}

// 每帧根据竞技场最大边长查询距离曲线，沿默认观察方向反向抬升相机，并输出固定旋转、控制旋转和视野角。
void ULyraCameraMode_TopDownArenaCamera::UpdateView(float DeltaTime)
{
	FBox ArenaBounds(FVector(-ArenaWidth, -ArenaHeight, 0.0f), FVector(ArenaWidth, ArenaHeight, 100.0f));

	const double BoundsMaxComponent = ArenaBounds.GetSize().GetMax();

	const double CameraLoftDistance = BoundsSizeToDistance.GetRichCurveConst()->Eval(BoundsMaxComponent);
	
	FVector PivotLocation = ArenaBounds.GetCenter() - DefaultPivotRotation.Vector() * CameraLoftDistance;
	
	FRotator PivotRotation = DefaultPivotRotation;

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;
}

