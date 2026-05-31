// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMRuntimeTypes.generated.h"

// 人物装备槽；Legs 是 MMM 自定义扩展槽，不是当前 MM4 逆向确认的 PcRuntime 原作装备槽。
UENUM(BlueprintType)
enum class EMMPlayerEquipSlot : uint8
{
	// 武器 1。
	Weapon1,
	// 武器 2。
	Weapon2,
	// 武器 3。
	Weapon3,
	// 头部装备。
	Head,
	// 身体装备。
	Body,
	// 手部装备。
	Hands,
	// 腿部装备；MMM 自定义扩展槽。
	Legs,
	// 脚部装备。
	Feet,
	// 饰品装备。
	Accessory,
	// 槽位数量哨兵，不作为真实装备槽使用。
	Count UMETA(Hidden)
};

// 战车 9 个主部件槽；部分正式中文槽名仍待逆向资料继续确认。
UENUM(BlueprintType)
enum class EMMTankPartSlot : uint8
{
	// 槽 0，当前按车体/底盘来源处理。
	Slot0_Body,
	// 槽 1，正式业务名待确认。
	Slot1,
	// 槽 2，当前逆向显示会参与承载容量来源。
	Slot2_CapacitySource,
	// 槽 3，正式业务名待确认。
	Slot3,
	// 槽 4，正式业务名待确认。
	Slot4,
	// 槽 5，正式业务名待确认。
	Slot5,
	// 槽 6，正式业务名待确认。
	Slot6,
	// 槽 7，正式业务名待确认。
	Slot7,
	// 槽 8，特定条件下可能参与承载容量来源。
	Slot8_OptionalCapacitySource,
	// 槽位数量哨兵，不作为真实战车部件槽使用。
	Count UMETA(Hidden)
};

// 战斗中的人物乘降状态；对应 BattleActor+0x0C 的第一版复刻语义。
UENUM(BlueprintType)
enum class EMMBattleRideState : uint8
{
	// 人身状态。
	OnFoot = 0,
	// 乘车状态类型 1；正式业务名待确认。
	RidingType1 = 1,
	// 乘车状态类型 2；正式业务名待确认。
	RidingType2 = 2
};

// 战斗 actor 类型；用于区分人物、战车和敌方快照。
UENUM(BlueprintType)
enum class EMMBattleActorType : uint8
{
	// 人物 actor。
	Human,
	// 战车 actor。
	Tank,
	// 敌方 actor。
	Enemy
};

// 人物技能槽运行时数据；当前只保留 action 和次数字段，后续再接完整技能系统。
USTRUCT(BlueprintType)
struct MMM_API FMMPlayerSkillSlotData
{
	GENERATED_BODY()

	// 技能槽解析出的 action id；0 表示空或未解析。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 ActionId = 0;

	// 当前剩余次数。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	uint8 CurrentCount = 0;

	// 最大/初始次数。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	uint8 MaxCount = 0;
};

// 人物长期运行时数据；对应 PcRuntime 风格字段，不等同于 GAS AttributeSet。
USTRUCT(BlueprintType)
struct MMM_API FMMPlayerRuntimeData
{
	GENERATED_BODY()

	// 初始化装备槽和技能槽数组长度。
	FMMPlayerRuntimeData();

	// 角色类型或职业类型；对应 PcRuntime+0x10 一类字段。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	uint8 CharacterType = 0;

	// 外观或形态字段；变身/外观系统后续会使用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	uint8 FormOrAvatar = 0;

	// 人物低位状态 flags；不作为普通 GAS 数值属性。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Flags = 0;

	// 当前等级。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Level = 1;

	// 等级上限。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 LevelCap = 99;

	// 累计经验值。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Experience = 0;

	// 当前 HP，战斗结束或恢复流程会写回。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 CurrentHP = 1;

	// 战斗等级，属于升级直接增长的基础属性之一。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 BattleLevel = 0;

	// 驾驶等级，影响战车/载具相关战斗能力。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 DrivingLevel = 0;

	// 腕力。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Strength = 0;

	// 体力。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Vitality = 0;

	// 敏捷度。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Agility = 0;

	// 男子气概。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Manliness = 0;

	// 伤痕/伤疤计数。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 Scars = 0;

	// 技能强化点数池。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 SkillUpgradePoints = 0;

	// 技能强化点数进度。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 SkillUpgradeProgress = 0;

	// 派生最大 HP；由成长/装备/技能等规则重算。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 DerivedMaxHP = 1;

	// 派生行动顺序值；入战时会进入 BattleActor 快照。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	int32 DerivedActionOrder = 0;

	// 人物装备 item id 数组；包含 MMM 自定义 Legs 槽，不直接等同原作 u16[8]。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	TArray<int32> EquipmentItemIds;

	// 人物技能槽；第一版固定 27 个槽。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Player")
	TArray<FMMPlayerSkillSlotData> SkillSlots;

	// 读取指定人物装备槽的 item id；非法槽返回 0。
	int32 GetEquipmentItemId(EMMPlayerEquipSlot Slot) const;
	// 写入指定人物装备槽的 item id；非法槽会被忽略。
	void SetEquipmentItemId(EMMPlayerEquipSlot Slot, int32 ItemId);
};

// 战车主部件或额外部件的运行时数据；对应 TankRuntime parts 槽的第一版字段。
USTRUCT(BlueprintType)
struct MMM_API FMMTankPartRuntimeData
{
	GENERATED_BODY()

	// 装备或部件 item id。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 ItemId = 0;

	// 部件运行时菜单参数；部分车体会影响乘车时的人类道具菜单权限。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	uint8 RuntimeMenuParam = 0;

	// 部件损伤值；20/60/100 阈值语义后续接修理系统。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 Damage = 0;

	// 弹药或计数字段。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 AmmoOrCount = 0;

	// 战斗参数字段；具体业务名随部件类型解释。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 BattleParam = 0;

	// 战车武器威力或部件当前攻击力。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 Power = 0;

	// 弹药或计数上限。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 AmmoCapacity = 0;

	// 部件重量；用于战车总重量和最大 SP 计算。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 Weight = 0;
};

// 战车长期运行时数据；对应 TankRuntime 风格字段，不等同于战斗 actor 快照。
USTRUCT(BlueprintType)
struct MMM_API FMMTankRuntimeData
{
	GENERATED_BODY()

	// 初始化 9 个主部件槽和 18 个额外槽。
	FMMTankRuntimeData();

	// 当前 SP/装甲片类主值。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 CurrentSP = 0;

	// 第一版承载容量基值；完整容量来源后续接战车部件/特殊效果。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 BaseCapacity = 0;

	// 9 个主部件槽。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	TArray<FMMTankPartRuntimeData> Parts;

	// 18 个额外/载货槽；正式业务名待逆向继续确认。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	TArray<FMMTankPartRuntimeData> ExtraParts;

	// 战车特殊 item 字段。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 SpecialItemId = 0;

	// 战车持久 flags；战斗结束会写回部分状态。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 PersistentFlags = 0;

	// 计算已装备主部件和额外槽的总重量。
	int32 CalculateTotalWeight() const;
	// 计算最大 SP；第一版为承载容量减总重量。
	int32 CalculateMaxSP() const;
	// 计算剩余承载空间；第一版为最大 SP 减当前 SP。
	int32 CalculateRemainingCapacity() const;
};

// 战车大地图停放记录；对应 TankMapRuntime 的第一版字段。
USTRUCT(BlueprintType)
struct MMM_API FMMTankMapRuntimeData
{
	GENERATED_BODY()

	// 停放地图 id。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 MapId = 0;

	// 停放/显示 flags。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	int32 Flags = 0;

	// 停放位置。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	FVector Location = FVector::ZeroVector;

	// 停放朝向。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Tank")
	float Yaw = 0.0f;
};

// 战斗 actor 快照；入战后由人物/战车长期数据构造，战斗结束再按规则回写。
USTRUCT(BlueprintType)
struct MMM_API FMMBattleActorState
{
	GENERATED_BODY()

	// 战斗 actor 类型。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	EMMBattleActorType ActorType = EMMBattleActorType::Human;

	// 人物 id、战车 id 或敌人 id，取决于 ActorType。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	int32 ActorId = -1;

	// 是否处于 active/可参与状态。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	bool bActive = false;

	// 战斗状态 flags。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	int32 Flags = 0;

	// 战斗乘降状态。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	EMMBattleRideState RideState = EMMBattleRideState::OnFoot;

	// 关联战车 actor 下标；-1 表示没有关联战车。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	int32 LinkedTankActorIndex = -1;

	// actor 子类型；战车 actor 会用它决定上车后的 ride state 类型。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	uint8 ActorSubtype = 0;

	// 当前 HP/SP/主耐久值。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	int32 CurrentValue = 0;

	// 最大 HP/SP/主耐久上限。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	int32 MaxValue = 0;

	// 伤害/恢复写回前的最终百分比修正；人物默认 100，战车默认可用 20。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Battle")
	int32 FinalAmountPercent = 100;
};
