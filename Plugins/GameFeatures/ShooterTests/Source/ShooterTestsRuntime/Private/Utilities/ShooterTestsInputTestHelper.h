// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/InputTestActions.h"
#include "Misc/DateTime.h"

/** 表示 Lyra 蹲伏按钮测试输入，向 IA_Crouch 注入布尔 true。 */
/**
 * Input action to handle the Lyra player's crouch action.
 * 
 * @note Crouching is handled by a button press which is a boolean value.
 */
struct FToggleCrouchTestAction : public FTestAction
{
	FToggleCrouchTestAction()
	{
		InputActionName = TEXT("IA_Crouch");
		InputActionValue = FInputActionValue(true);
	}
};

/** 表示 Lyra 近战按钮测试输入，向 IA_Melee 注入布尔 true。 */
/**
 * Input action to handle the Lyra player's melee action.
 *
 * @note Melee is handled by a button press which is a boolean value.
 */
struct FMeleeTestAction : public FTestAction
{
	FMeleeTestAction()
	{
		InputActionName = TEXT("IA_Melee");
		InputActionValue = FInputActionValue(true);
	}
};

/** 表示 Lyra 跳跃按钮测试输入，向 IA_Jump 注入布尔 true。 */
/**
 * Input action to handle the Lyra player's jump action.
 *
 * @note Jump is handled by a button press which is a boolean value.
 */
struct FJumpTestAction : public FTestAction
{
	FJumpTestAction()
	{
		InputActionName = TEXT("IA_Jump");
		InputActionValue = FInputActionValue(true);
	}
};

/** Lyra 二维移动测试输入基类：X 轴控制横移，Y 轴控制前后移动，派生类型提供具体方向值。 */
/**
 * Base input action to handle the Lyra player's movement.
 *
 * @note Movement is setup as a 2D axis with the X-axis handling strafing and the Y-axis handling moving forward/backward.
 * @note Derived objects below will handle movement direction along a particular axis
 */
struct FMoveTestAction : public FTestAction
{
	FMoveTestAction(const FInputActionValue& InInputActionValue)
	{
		InputActionName = TEXT("IA_Move");
		InputActionValue = InInputActionValue;
	}
};

/** Lyra 二维观察旋转测试输入基类，派生类型沿指定轴向 IA_Look_Mouse 注入方向值。 */
/**
 * Base input action to handle the Lyra player's look (rotation).
 *
 * @note Rotation is setup as a 2D axis
 * @note Derived objects below will handle rotation along a particular axis
 */
struct FLookTestAction : public FTestAction
{
	FLookTestAction(const FInputActionValue& InInputActionValue)
	{
		InputActionName = TEXT("IA_Look_Mouse");
		InputActionValue = InInputActionValue;
	}
};

/** 向 Lyra 玩家注入向前移动输入。 */
/** Movement input action to move the Lyra player forward. */
struct FMoveForwardTestAction : public FMoveTestAction
{
	FMoveForwardTestAction() : FMoveTestAction(FVector2D(0.0f, 1.0f))
	{
	}
};

/** 向 Lyra 玩家注入向后移动输入。 */
/** Movement input action to move the Lyra player backward. */
struct FMoveBackwardTestAction : public FMoveTestAction
{
	FMoveBackwardTestAction() : FMoveTestAction(FVector2D(0.0f, -1.0f))
	{
	}
};

/** 向 Lyra 玩家注入向左横移输入。 */
/** Movement input action to strafe the Lyra player to the left. */
struct FStrafeLeftTestAction : public FMoveTestAction
{
	FStrafeLeftTestAction() : FMoveTestAction(FVector2D(-1.0f, 0.0f))
	{
	}
};

/** 向 Lyra 玩家注入向右横移输入。 */
/** Movement input action to strafe the Lyra player to the right. */
struct FStrafeRightTestAction : public FMoveTestAction
{
	FStrafeRightTestAction() : FMoveTestAction(FVector2D(1.0f, 0.0f))
	{
	}
};

/** 向 Lyra 玩家注入向左观察旋转输入。 */
/** Rotation input action to rotate the Lyra player to the left. */
struct FRotateLeftTestAction : public FLookTestAction
{
	FRotateLeftTestAction() : FLookTestAction(FVector2D(-1.0f, 0.0f))
	{
	}
};

/** 向 Lyra 玩家注入向右观察旋转输入。 */
/** Rotation input action to rotate the Lyra player to the right. */
struct FRotateRightTestAction : public FLookTestAction
{
	FRotateRightTestAction() : FLookTestAction(FVector2D(1.0f, 0.0f))
	{
	}
};

/** 封装 CQTest 的 FInputTestActions，为 Lyra Pawn 提供按钮与持续轴输入测试接口。 */
/**
 * Inherited InputTestAction used for testing our button and axis interactions for the Lyra player.
 * 
 * @see FInputTestActions
 */
class FShooterTestsPawnTestActions : public FInputTestActions
{
public:
	/** 为指定 Pawn 创建输入动作代理，后续模拟输入都会施加到该 Pawn。 */
	/**
	 * Construct our Input actions object.
	 * 
	 * @param Pawn - Pawn which will have the input actions applied against.
	 */
	explicit FShooterTestsPawnTestActions(APawn* Pawn) : FInputTestActions(Pawn)
	{
	}

	/** 模拟一次蹲伏按钮按下。 */
	/** Simulate a button press for our crouch action. */
	void ToggleCrouch();

	/** 模拟一次近战按钮按下。 */
	/** Simulate a button press for our melee action. */
	void PerformMelee();

	/** 模拟一次跳跃按钮按下。 */
	/** Simulate a button press for our jump action. */
	void PerformJump();

	/** 模拟前进或后退移动轴输入。 */
	/** Simulates player movement input actions. */
	void MoveForward();
	void MoveBackward();

	/** 模拟向左或向右横移轴输入。 */
	/** Simulates player strafing input actions. */
	void StrafeLeft();
	void StrafeRight();

	/** 模拟向左或向右观察旋转轴输入。 */
	/** Simulates player look/rotation input actions. */
	void RotateLeft();
	void RotateRight();

private:
	/** 连续执行指定轴输入，直到经过 5 秒后由完成谓词结束。 */
	/** Method to perform any of our axis based actions over the span of 5 seconds. */
	void PerformAxisAction(TFunction<void(const APawn* Pawn)> Action);
};
