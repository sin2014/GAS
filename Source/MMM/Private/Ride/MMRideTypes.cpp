// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ride/MMRideTypes.h"

namespace
{
	// RideMap 固定只有 4 个队员槽。
	constexpr int32 RideMemberCount = 4;

	// 判断战车 id 是否位于当前支持范围内；-1 表示未乘坐/未牵引。
	bool IsValidTankId(const int32 TankId, const int32 MaxTankId)
	{
		return TankId == -1 || (TankId >= 0 && TankId <= MaxTankId);
	}
}

// 初始化 4 个队员槽为步行状态。
FMMRideMapState::FMMRideMapState()
{
	MemberTankIds.Init(-1, RideMemberCount);
}

// 只扫描前 4 个成员槽，判断是否有人乘坐战车。
bool FMMRideMapState::IsAnyMemberRiding() const
{
	for (const int32 TankId : MemberTankIds)
	{
		if (TankId >= 0)
		{
			return true;
		}
	}

	return false;
}

// 安全读取成员槽；非法下标按步行处理。
int32 FMMRideMapState::GetMemberTankId(const int32 MemberIndex) const
{
	if (!MemberTankIds.IsValidIndex(MemberIndex))
	{
		return -1;
	}

	return MemberTankIds[MemberIndex];
}

// 安全写入成员槽；如果数组长度异常则先恢复为 4 个步行槽。
void FMMRideMapState::SetMemberTankId(const int32 MemberIndex, const int32 TankId)
{
	if (MemberIndex < 0 || MemberIndex >= RideMemberCount)
	{
		return;
	}

	if (MemberTankIds.Num() != RideMemberCount)
	{
		MemberTankIds.Init(-1, RideMemberCount);
	}

	MemberTankIds[MemberIndex] = TankId;
}

// 校验 RideMap 是否能提交；当前先实现 id 范围、重复占用和牵引冲突。
FMMRideMapValidationResult UMMRideStatics::ValidateRideMap(const FMMRideMapState& RideMap, const int32 MaxTankId)
{
	FMMRideMapValidationResult Result;

	if (RideMap.MemberTankIds.Num() != RideMemberCount)
	{
		Result.bValid = false;
		Result.Reason = FName(TEXT("InvalidMemberCount"));
		return Result;
	}

	TSet<int32> UsedTankIds;
	for (const int32 TankId : RideMap.MemberTankIds)
	{
		if (!IsValidTankId(TankId, MaxTankId))
		{
			Result.bValid = false;
			Result.Reason = FName(TEXT("InvalidMemberTankId"));
			return Result;
		}

		if (TankId >= 0)
		{
			if (UsedTankIds.Contains(TankId))
			{
				Result.bValid = false;
				Result.Reason = FName(TEXT("DuplicateMemberTank"));
				return Result;
			}

			UsedTankIds.Add(TankId);
		}
	}

	if (!IsValidTankId(RideMap.TowTankId, MaxTankId))
	{
		Result.bValid = false;
		Result.Reason = FName(TEXT("InvalidTowTankId"));
		return Result;
	}

	if (RideMap.TowTankId >= 0 && UsedTankIds.Contains(RideMap.TowTankId))
	{
		Result.bValid = false;
		Result.Reason = FName(TEXT("TowTankAlreadyOccupied"));
		return Result;
	}

	return Result;
}

// 生成去重后的成员占用视图；重复 tank id 只保留第一次出现的位置。
TArray<int32> UMMRideStatics::NormalizeRideMap(const FMMRideMapState& RideMap)
{
	TArray<int32> Normalized;
	Normalized.Init(-1, RideMemberCount);

	TSet<int32> UsedTankIds;
	for (int32 MemberIndex = 0; MemberIndex < RideMemberCount; ++MemberIndex)
	{
		const int32 TankId = RideMap.GetMemberTankId(MemberIndex);
		if (TankId < 0 || UsedTankIds.Contains(TankId))
		{
			continue;
		}

		Normalized[MemberIndex] = TankId;
		UsedTankIds.Add(TankId);
	}

	return Normalized;
}

// 构造 RideMap 提交结果；上层可据此决定是否刷新人物/战车地图对象。
FMMRideMapApplyResult UMMRideStatics::BuildApplyRideMapResult(const FMMRideMapState& CurrentRideMap, const FMMRideMapState& CandidateRideMap)
{
	FMMRideMapApplyResult Result;
	Result.NextRideMap = CandidateRideMap;
	Result.NormalizedTankOccupancy = NormalizeRideMap(CandidateRideMap);

	const TArray<int32> CurrentNormalized = NormalizeRideMap(CurrentRideMap);
	Result.bChanged = CurrentRideMap.TowTankId != CandidateRideMap.TowTankId || CurrentNormalized != Result.NormalizedTankOccupancy;

	return Result;
}

// 战车破坏时强制所有成员和牵引槽下车，并持久影响大地图乘降关系。
void UMMRideStatics::ForceGetOffDestroyedTank(FMMRideMapState& RideMap, const int32 DestroyedTankId)
{
	for (int32 MemberIndex = 0; MemberIndex < RideMemberCount; ++MemberIndex)
	{
		if (RideMap.GetMemberTankId(MemberIndex) == DestroyedTankId)
		{
			RideMap.SetMemberTankId(MemberIndex, -1);
		}
	}

	if (RideMap.TowTankId == DestroyedTankId)
	{
		RideMap.TowTankId = -1;
	}
}
