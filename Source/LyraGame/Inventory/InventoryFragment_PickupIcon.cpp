// Copyright Epic Games, Inc. All Rights Reserved.

#include "InventoryFragment_PickupIcon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryFragment_PickupIcon)

// 将拾取图标底板颜色默认设为绿色，其余显示资源由物品定义资产配置。
UInventoryFragment_PickupIcon::UInventoryFragment_PickupIcon()
{
	PadColor = FLinearColor::Green;
}
