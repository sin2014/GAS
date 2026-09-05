// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWorldCollectable.h"

#include "Async/TaskGraphInterfaces.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWorldCollectable)

struct FInteractionQuery;

// 构造可由蓝图配置交互选项和静态库存内容的世界拾取物。
ALyraWorldCollectable::ALyraWorldCollectable()
{
}

// 将该拾取物配置的唯一交互选项加入查询结果，供交互扫描器展示和触发。
void ALyraWorldCollectable::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	InteractionBuilder.AddInteractionOption(Option);
}

// 按值返回拾取物预配置的库存条目和数量，供 IPickupable 消费方发放物品。
FInventoryPickup ALyraWorldCollectable::GetPickupInventory() const
{
	return StaticInventory;
}
