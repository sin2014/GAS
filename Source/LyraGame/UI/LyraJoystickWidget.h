// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/LyraSimulatedInputWidget.h"

#include "LyraJoystickWidget.generated.h"

#define UE_API LYRAGAME_API

class UImage;
class UObject;
struct FGeometry;
struct FPointerEvent;

// Lyra 虚拟摇杆的 UMG 包装：把触摸位移归一化到 [-1,1]，注入 Enhanced Input，并同步移动前景摇杆图像。
/**
 *  A UMG wrapper for the lyra virtual joystick.
 *
 *  This will calculate a 2D vector clamped between -1 and 1
 *  to input as a key value to the player, simulating a gamepad analog stick.
 *
 *  This is intended for use with and Enhanced Input player.
 */
UCLASS(MinimalAPI, meta=( DisplayName="Lyra Joystick" ))
class ULyraJoystickWidget : public ULyraSimulatedInputWidget
{
	GENERATED_BODY()
	
public:
	
	UE_API ULyraJoystickWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UUserWidget
	UE_API virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	UE_API virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget interface
	
protected:

	// 根据当前触点相对起点的位移计算摇杆向量，注入关联输入，并更新前景图像位置。
	/**
	 * Calculate the delta position of the current touch from the origin.
	 *
	 * Input the associated gamepad key on the player
	 *
	 * Move the foreground joystick image in association with the given input to give the appearance that it
	 * is moving along with the player's finger
	 */
	UE_API void HandleTouchDelta(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent);
	
	/** 注入零向量以释放当前模拟输入，并结束此虚拟摇杆的触摸会话。 */
	/** Flush any player input that has been injected and disable the use of this analog stick. */
	UE_API void StopInputSimulation();

	/** 前景摇杆图像相对中心允许移动的最大像素距离。 */
	/** How far can the inner image of the joystick be moved? */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float StickRange = 50.0f;

	/** 虚拟摇杆底盘图像。 */
	/** Image to be used as the background of the joystick */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> JoystickBackground;

	/** 随触摸移动的摇杆前景图像。 */
	/** Image to be used as the foreground of the joystick */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> JoystickForeground;

	/** 是否反转摇杆 Y 轴；移动摇杆通常需要反转屏幕坐标向下为正的方向。 */
	/** Should we negate the Y-axis value of the joystick? This is common for "movement" sticks */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bNegateYAxis = false;

	/** 本次触摸起点的绝对坐标，由 NativeOnTouchStarted 设置。 */
	/** The origin of the touch. Set on NativeOnTouchStarted */
	UPROPERTY(Transient)
	FVector2D TouchOrigin = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D StickVector = FVector2D::ZeroVector;
};

#undef UE_API
