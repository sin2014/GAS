// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MMRideTypes.generated.h"

// 大地图乘降关系；复刻 RideMap5 的 4 名队员乘坐关系和 1 个牵引槽。
USTRUCT(BlueprintType)
struct MMM_API FMMRideMapState
{
	GENERATED_BODY()

	// 初始化 4 个队员槽为 -1，表示步行。
	FMMRideMapState();

	// 队员槽当前乘坐的战车 id；-1 表示步行。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	TArray<int32> MemberTankIds;

	// 牵引战车 id；-1 表示没有牵引。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	int32 TowTankId = -1;

	// 检查 4 个队员槽中是否至少有一名队员乘车。
	bool IsAnyMemberRiding() const;
	// 读取指定队员槽的战车 id；非法下标返回 -1。
	int32 GetMemberTankId(int32 MemberIndex) const;
	// 写入指定队员槽的战车 id；非法下标会被忽略。
	void SetMemberTankId(int32 MemberIndex, int32 TankId);
};

// RideMap 校验结果；Reason 使用稳定英文 key，UI 层再决定本地化文本。
USTRUCT(BlueprintType)
struct MMM_API FMMRideMapValidationResult
{
	GENERATED_BODY()

	// 是否通过校验。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	bool bValid = true;

	// 失败原因 key；成功时为 None。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	FName Reason = NAME_None;
};

// RideMap 提交结果；用于 UI 或上层系统决定是否刷新地图对象。
USTRUCT(BlueprintType)
struct MMM_API FMMRideMapApplyResult
{
	GENERATED_BODY()

	// 候选 RideMap 相比当前 RideMap 是否产生有效变化。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	bool bChanged = false;

	// 校验/提交后的下一份 RideMap。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	FMMRideMapState NextRideMap;

	// 去重后的占用视图；第一版按 4 个队员槽返回。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MM|Ride")
	TArray<int32> NormalizedTankOccupancy;
};

// 乘降系统的纯函数集合；第一版不直接操作场景 Actor，只处理数据规则。
UCLASS()
class MMM_API UMMRideStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 校验 RideMap 是否可提交；检查槽位数量、战车 id 范围、重复占用和牵引冲突。
	UFUNCTION(BlueprintPure, Category = "MM|Ride")
	static FMMRideMapValidationResult ValidateRideMap(const FMMRideMapState& RideMap, int32 MaxTankId = 14);

	// 生成去重后的乘坐占用视图；重复战车只保留第一次出现的队员槽。
	UFUNCTION(BlueprintPure, Category = "MM|Ride")
	static TArray<int32> NormalizeRideMap(const FMMRideMapState& RideMap);

	// 根据当前 RideMap 和候选 RideMap 构造提交结果。
	UFUNCTION(BlueprintPure, Category = "MM|Ride")
	static FMMRideMapApplyResult BuildApplyRideMapResult(const FMMRideMapState& CurrentRideMap, const FMMRideMapState& CandidateRideMap);

	// 战车破坏强制下车：从所有队员槽和牵引槽中清除指定战车 id。
	UFUNCTION(BlueprintCallable, Category = "MM|Ride")
	static void ForceGetOffDestroyedTank(UPARAM(ref) FMMRideMapState& RideMap, int32 DestroyedTankId);
};
