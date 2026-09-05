// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LyraJoystickWidget.h"

#include "CommonHardwareVisibilityBorder.h"
#include "Components/Image.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraJoystickWidget)

#define LOCTEXT_NAMESPACE "LyraJoystick"

// 构造会消费指针输入的虚拟摇杆控件。
ULyraJoystickWidget::ULyraJoystickWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetConsumePointerInput(true);
}

// 记录触摸起点、消费事件，并在当前指针尚未捕获时捕获鼠标以持续接收移动。
FReply ULyraJoystickWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	
	TouchOrigin = InGestureEvent.GetScreenSpacePosition();

	FReply Reply = FReply::Handled();
	if (!HasMouseCaptureByUser(InGestureEvent.GetUserIndex(), InGestureEvent.GetPointerIndex()))
	{
		Reply.CaptureMouse(GetCachedWidget().ToSharedRef());
	}
	return Reply;
}

// 根据触点更新归一化摇杆向量，消费事件并确保当前指针保持捕获。
FReply ULyraJoystickWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
	HandleTouchDelta(InGeometry, InGestureEvent);

	FReply Reply = FReply::Handled();
	if (!HasMouseCaptureByUser(InGestureEvent.GetUserIndex(), InGestureEvent.GetPointerIndex()))
	{
		Reply.CaptureMouse(GetCachedWidget().ToSharedRef());
	}
	return Reply;
}

// 清零模拟摇杆状态，消费触摸结束并释放鼠标捕获。
FReply ULyraJoystickWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	StopInputSimulation();
	return FReply::Handled().ReleaseMouseCapture();
}

// 指针离开控件时停止模拟输入，避免残留方向值。
void ULyraJoystickWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	StopInputSimulation();
}

// 可见时按摇杆向量移动前景图像，并每帧向增强输入系统注入二维值。
void ULyraJoystickWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CommonVisibilityBorder || CommonVisibilityBorder->IsVisible())
	{
		// 按归一化摇杆向量和最大位移更新前景图像的 Canvas Slot 位置。
		// Move the inner stick icon around with the vector
		if (JoystickForeground && JoystickBackground)
		{
			JoystickForeground->SetRenderTranslation(
				(bNegateYAxis ? FVector2D(1.0f, -1.0f) : FVector2D(1.0f)) *
				StickVector *
				(JoystickBackground->GetDesiredSize() * 0.5f)
			);
		}
		InputKeyValue2D(StickVector);
	}
}

// 把屏幕触点转换为控件局部中心偏移，按配置翻转 Y 轴、限制半径并归一化为摇杆向量。
void ULyraJoystickWidget::HandleTouchDelta(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const FVector2D& ScreenSpacePos = InGestureEvent.GetScreenSpacePosition();
	
	// 局部几何中心等于控件尺寸的一半。
	// The center of the geo locally is just half its size
	FVector2D LocalStickCenter = InGeometry.GetAbsoluteSize() * 0.5f;

	FVector2D ScreenSpaceStickCenter = InGeometry.LocalToAbsolute(LocalStickCenter);
	// 将当前触点转为局部坐标，并计算相对触摸起点的位移。
	// Get the offset from the origin
	FVector2D MoveStickOffset = (ScreenSpacePos - ScreenSpaceStickCenter);
	if (bNegateYAxis)
	{
		MoveStickOffset *= FVector2D(1.0f, -1.0f);
	}
	
	FVector2D MoveStickDir = FVector2D::ZeroVector;
	float MoveStickLength = 0.0f;
	MoveStickOffset.ToDirectionAndLength(MoveStickDir, MoveStickLength);

	MoveStickLength = FMath::Min(MoveStickLength, StickRange);
	MoveStickOffset = MoveStickDir * MoveStickLength;

	StickVector = MoveStickOffset / StickRange;
}

// 清空触摸起点和摇杆向量，使后续 Tick 注入零输入。
void ULyraJoystickWidget::StopInputSimulation()
{
	TouchOrigin = FVector2D::ZeroVector;
	StickVector = FVector2D::ZeroVector;
}

#undef LOCTEXT_NAMESPACE

