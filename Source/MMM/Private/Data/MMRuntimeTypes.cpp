// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/MMRuntimeTypes.h"

namespace
{
	// 人物装备槽数量；包含 MMM 自定义 Legs 槽。
	constexpr int32 PlayerEquipSlotCount = static_cast<int32>(EMMPlayerEquipSlot::Count);
	// 人物技能槽数量；当前按逆向资料中的 27 个运行时槽预留。
	constexpr int32 PlayerSkillSlotCount = 27;
	// 战车主部件槽数量。
	constexpr int32 TankPartSlotCount = static_cast<int32>(EMMTankPartSlot::Count);
	// 战车额外/载货槽数量；正式业务名待逆向确认。
	constexpr int32 TankExtraPartSlotCount = 18;
}

// 初始化人物长期运行时数组，保证新建数据即可安全读写固定槽位。
FMMPlayerRuntimeData::FMMPlayerRuntimeData()
{
	EquipmentItemIds.Init(0, PlayerEquipSlotCount);
	SkillSlots.SetNum(PlayerSkillSlotCount);
}

// 按装备槽读取 item id；非法槽或数组未初始化时返回空 item。
int32 FMMPlayerRuntimeData::GetEquipmentItemId(const EMMPlayerEquipSlot Slot) const
{
	const int32 SlotIndex = static_cast<int32>(Slot);
	if (SlotIndex < 0 || SlotIndex >= PlayerEquipSlotCount || !EquipmentItemIds.IsValidIndex(SlotIndex))
	{
		return 0;
	}

	return EquipmentItemIds[SlotIndex];
}

// 按装备槽写入 item id；非法槽会被忽略，避免 Count 哨兵被误用。
void FMMPlayerRuntimeData::SetEquipmentItemId(const EMMPlayerEquipSlot Slot, const int32 ItemId)
{
	const int32 SlotIndex = static_cast<int32>(Slot);
	if (SlotIndex < 0 || SlotIndex >= PlayerEquipSlotCount)
	{
		return;
	}

	if (!EquipmentItemIds.IsValidIndex(SlotIndex))
	{
		EquipmentItemIds.SetNum(PlayerEquipSlotCount);
	}

	EquipmentItemIds[SlotIndex] = ItemId;
}

// 初始化战车 9 个主部件槽和 18 个额外槽。
FMMTankRuntimeData::FMMTankRuntimeData()
{
	Parts.SetNum(TankPartSlotCount);
	ExtraParts.SetNum(TankExtraPartSlotCount);
}

// 汇总所有已装备主部件和额外槽的重量。
int32 FMMTankRuntimeData::CalculateTotalWeight() const
{
	int32 TotalWeight = 0;

	for (const FMMTankPartRuntimeData& Part : Parts)
	{
		if (Part.ItemId != 0)
		{
			TotalWeight += Part.Weight;
		}
	}

	for (const FMMTankPartRuntimeData& Part : ExtraParts)
	{
		if (Part.ItemId != 0)
		{
			TotalWeight += Part.Weight;
		}
	}

	return TotalWeight;
}

// 计算最大 SP；第一版只实现容量减总重量，特殊容量加成后续接入。
int32 FMMTankRuntimeData::CalculateMaxSP() const
{
	return FMath::Max(0, BaseCapacity - CalculateTotalWeight());
}

// 计算剩余容量；当前 SP 会占用最大 SP 空间。
int32 FMMTankRuntimeData::CalculateRemainingCapacity() const
{
	return FMath::Max(0, CalculateMaxSP() - CurrentSP);
}
