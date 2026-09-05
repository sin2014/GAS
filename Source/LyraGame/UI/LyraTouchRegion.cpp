// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LyraTouchRegion.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTouchRegion)

struct FGeometry;
struct FPointerEvent;

// 触摸开始时启用持续输入模拟，并保留基类事件处理结果。
FReply ULyraTouchRegion::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	bShouldSimulateInput = true;
	return Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
}

// 触摸在区域内移动时保持持续输入模拟状态。
FReply ULyraTouchRegion::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	// 触摸仍在控件范围内时持续注入关联输入。
	// Input our associatied key as long as the player is touching within our bounds
	//InputKeyValue(FVector::OneVector);
	bShouldSimulateInput = true;
	// 按配置决定是否每帧重复触发输入，而不是只在触摸开始时触发一次。
	// Continuously trigger the input if we should
	return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
}

// 触摸结束时停止持续输入模拟，并交由基类处理事件。
FReply ULyraTouchRegion::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	bShouldSimulateInput = false;
	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

// 持续触摸期间每帧向关联 Action 或后备按键注入单位输入。
void ULyraTouchRegion::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if(bShouldSimulateInput)
	{
		InputKeyValue(FVector::OneVector);
	}
}

