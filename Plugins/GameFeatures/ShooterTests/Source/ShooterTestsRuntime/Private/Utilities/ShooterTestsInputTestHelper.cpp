// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestsInputTestHelper.h"

// 向被测 Pawn 注入一次 IA_Crouch 按钮动作。
void FShooterTestsPawnTestActions::ToggleCrouch()
{
	PerformAction(FToggleCrouchTestAction{});
}

// 向被测 Pawn 注入一次 IA_Melee 按钮动作。
void FShooterTestsPawnTestActions::PerformMelee()
{
	PerformAction(FMeleeTestAction{});
}

// 向被测 Pawn 注入一次 IA_Jump 按钮动作。
void FShooterTestsPawnTestActions::PerformJump()
{
	PerformAction(FJumpTestAction{});
}

// 持续五秒向被测 Pawn 注入前进轴输入。
void FShooterTestsPawnTestActions::MoveForward()
{
	PerformAxisAction(FMoveForwardTestAction{});
}

// 持续五秒向被测 Pawn 注入后退轴输入。
void FShooterTestsPawnTestActions::MoveBackward()
{
	PerformAxisAction(FMoveBackwardTestAction{});
}

// 持续五秒向被测 Pawn 注入左横移轴输入。
void FShooterTestsPawnTestActions::StrafeLeft()
{
	PerformAxisAction(FStrafeLeftTestAction{});
}

// 持续五秒向被测 Pawn 注入右横移轴输入。
void FShooterTestsPawnTestActions::StrafeRight()
{
	PerformAxisAction(FStrafeRightTestAction{});
}

// 持续五秒向被测 Pawn 注入左转观察轴输入。
void FShooterTestsPawnTestActions::RotateLeft()
{
	PerformAxisAction(FRotateLeftTestAction{});
}

// 持续五秒向被测 Pawn 注入右转观察轴输入。
void FShooterTestsPawnTestActions::RotateRight()
{
	PerformAxisAction(FRotateRightTestAction{});
}

// 执行给定轴动作，并通过以 UTC 时间计算的谓词在五秒后结束持续输入。
void FShooterTestsPawnTestActions::PerformAxisAction(TFunction<void(const APawn* Pawn)> Action)
{
	FDateTime StartTime{ 0 };

	// 连续执行轴向动作五秒，以便移动或转向动画有足够时间进入稳定状态。
	// Perform move actions over the duration of 5 seconds
	PerformAction(Action, [this, StartTime]() mutable -> bool {
		if (StartTime.GetTicks() == 0)
		{
			StartTime = FDateTime::UtcNow();
		}

		FTimespan Elapsed = FDateTime::UtcNow() - StartTime;
		return Elapsed >= FTimespan::FromSeconds(5.0);
	});
}
