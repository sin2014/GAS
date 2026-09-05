// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraActionWidget.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraActionWidget)

// 优先按增强输入动作的当前实际绑定及输入设备类型取得图标，无法解析时回退到 Common Action Widget 默认图标。
FSlateBrush ULyraActionWidget::GetIcon() const
{
	// 关联 Enhanced Input Action 时查询当前玩家的实际映射，并按当前输入类型和手柄类型取得图标；
	// 这样用户重绑定后显示的是新按键，而不是 Common Input 数据表中的默认图标。
	// If there is an Enhanced Input action associated with this widget, then search for any
	// keys bound to that action and display those instead of the default data table settings.
	// This covers the case of when a player has rebound a key to something else
	if (AssociatedInputAction)
	{
		if (const UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = GetEnhancedInputSubsystem())
		{
			TArray<FKey> BoundKeys = EnhancedInputSubsystem->QueryKeysMappedToAction(AssociatedInputAction);
			FSlateBrush SlateBrush;

			const UCommonInputSubsystem* CommonInputSubsystem = GetInputSubsystem();
			if (!BoundKeys.IsEmpty() && CommonInputSubsystem && UCommonInputPlatformSettings::Get()->TryGetInputBrush(SlateBrush, BoundKeys[0], CommonInputSubsystem->GetCurrentInputType(), CommonInputSubsystem->GetCurrentGamepadName()))
			{
				return SlateBrush;
			}
		}
	}
	
	return Super::GetIcon();
}

// 优先使用当前绑定控件的所属本地玩家取得增强输入子系统，否则回退到本控件所属玩家。
UEnhancedInputLocalPlayerSubsystem* ULyraActionWidget::GetEnhancedInputSubsystem() const
{
	const UWidget* BoundWidget = DisplayedBindingHandle.GetBoundWidget();
	if (const ULocalPlayer* BindingOwner = BoundWidget ? BoundWidget->GetOwningLocalPlayer() : GetOwningLocalPlayer())
	{
		return BindingOwner->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}
	return nullptr;
}
