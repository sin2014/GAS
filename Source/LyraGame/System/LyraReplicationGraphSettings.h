// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "LyraReplicationGraphTypes.h"
#include "LyraReplicationGraphSettings.generated.h"

/**
 * Lyra ReplicationGraph 的项目默认配置，包括 FastShared 带宽、空间网格和动态 Actor 分桶频率。
 */
/**
 * Default settings for the Lyra replication graph
 */
UCLASS(config=Game, MinimalAPI)
class ULyraReplicationGraphSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	ULyraReplicationGraphSettings();

public:

	UPROPERTY(config, EditAnywhere, Category = ReplicationGraph)
	bool bDisableReplicationGraph = true;

	UPROPERTY(config, EditAnywhere, Category = ReplicationGraph, meta = (MetaClass = "/Script/LyraGame.LyraReplicationGraph"))
	FSoftClassPath DefaultReplicationGraphClass;

	UPROPERTY(EditAnywhere, Category = FastSharedPath, meta = (ConsoleVariable = "Lyra.RepGraph.EnableFastSharedPath"))
	bool bEnableFastSharedPath = true;

	// FastShared 移动更新可使用的目标带宽，独立于 NetDriver 的常规目标带宽统计。
	// How much bandwidth to use for FastShared movement updates. This is counted independently of the NetDriver's target bandwidth.
	UPROPERTY(EditAnywhere, Category = FastSharedPath, meta = (ForceUnits=Kilobytes, ConsoleVariable = "Lyra.RepGraph.TargetKBytesSecFastSharedPath"))
	int32 TargetKBytesSecFastSharedPath = 10;

	UPROPERTY(EditAnywhere, Category = FastSharedPath, meta = (ConsoleVariable = "Lyra.RepGraph.FastSharedPathCullDistPct"))
	float FastSharedPathCullDistPct = 0.80f;

	UPROPERTY(EditAnywhere, Category = DestructionInfo, meta = (ForceUnits = cm, ConsoleVariable = "Lyra.RepGraph.DestructInfo.MaxDist"))
	float DestructionInfoMaxDist = 30000.f;

	UPROPERTY(EditAnywhere, Category=SpatialGrid, meta=(ForceUnits=cm, ConsoleVariable = "Lyra.RepGraph.CellSize"))
	float SpatialGridCellSize = 10000.0f;

	// 空间复制网格的初始最小 X 偏移；Actor 超出范围时系统会自动重建调整。
	// Essentially "Min X" for replication. This is just an initial value. The system will reset itself if actors appears outside of this.
	UPROPERTY(EditAnywhere, Category=SpatialGrid, meta=(ForceUnits=cm, ConsoleVariable = "Lyra.RepGraph.SpatialBiasX"))
	float SpatialBiasX = -200000.0f;

	// 空间复制网格的初始最小 Y 偏移；Actor 超出范围时系统会自动重建调整。
	// Essentially "Min Y" for replication. This is just an initial value. The system will reset itself if actors appears outside of this.
	UPROPERTY(EditAnywhere, Category=SpatialGrid, meta=(ForceUnits=cm, ConsoleVariable = "Lyra.RepGraph.SpatialBiasY"))
	float SpatialBiasY = -200000.0f;

	UPROPERTY(EditAnywhere, Category=SpatialGrid, meta = (ConsoleVariable = "Lyra.RepGraph.DisableSpatialRebuilds"))
	bool bDisableSpatialRebuilds = true;

	// 动态空间化 Actor 分摊到的频率桶数量；桶越多，实际进入复制收集的频率越低。
	// 该分桶发生在每个 Actor 自身 NetUpdateFrequency 检查之前。
	// How many buckets to spread dynamic, spatialized actors across.
	// High number = more buckets = smaller effective replication frequency.
	// This happens before individual actors do their own NetUpdateFrequency check.
	UPROPERTY(EditAnywhere, Category = DynamicSpatialFrequency, meta = (ConsoleVariable = "Lyra.RepGraph.DynamicActorFrequencyBuckets"))
	int32 DynamicActorFrequencyBuckets = 3;

	// 针对特定 Actor Class 的自定义 ReplicationGraph 设置。
	// Array of Custom Settings for Specific Classes 
	UPROPERTY(config, EditAnywhere, Category = ReplicationGraph)
	TArray<FRepGraphActorClassSettings> ClassSettings;
};
