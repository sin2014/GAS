// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"

#include "AimAssistTargetManagerComponent.generated.h"

#define UE_API SHOOTERCORERUNTIME_API

enum class ECommonInputType : uint8;

class APlayerController;
class UObject;
struct FAimAssistFilter;
struct FAimAssistOwnerViewData;
struct FAimAssistSettings;
struct FAimAssistTargetOptions;
struct FCollisionQueryParams;
struct FLyraAimAssistTarget;

/**
 * Aim Assist 目标管理器根据本地玩家视图收集候选目标。目标必须实现 IAimAssistTargetInterface，并在 ShooterCoreRuntimeSettings 配置的碰撞通道上可查询。
 */
/**
 * The Aim Assist Target Manager Component is used to gather all aim assist targets that are within
 * a given player's view. Targets must implement the IAimAssistTargetInterface and be on the
 * collision channel that is set in the ShooterCoreRuntimeSettings. 
 */
UCLASS(MinimalAPI, Blueprintable)
class UAimAssistTargetManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:

	/** 根据玩家视图、筛选规则和上一帧缓存，输出当前可见且有效的目标，并限制可见性检测数量。 */
	/** Gets all visible active targets based on the given local player and their ViewTransform */
	UE_API void GetVisibleTargets(const FAimAssistFilter& Filter, const FAimAssistSettings& Settings, const FAimAssistOwnerViewData& OwnerData, const TArray<FLyraAimAssistTarget>& OldTargets, OUT TArray<FLyraAimAssistTarget>& OutNewTargets);

	/** 按玩家当前输入设备计算 FOV 缩放；手柄使用保持缩放手感一致的投影比例。 */
	/** Get a Player Controller's FOV scaled based on their current input type. */
	static UE_API float GetFOVScale(const APlayerController* PC, ECommonInputType InputType);

	/** 返回 ShooterCore 运行时设置中用于发现 Aim Assist 目标的碰撞通道。 */
	/** Get the collision channel that should be used to find targets within the player's view. */
	UE_API ECollisionChannel GetAimAssistChannel() const;
	
protected:

	/** 根据距离、阵营、存活状态、排除类和 GameplayTag 判断目标是否应参与 Aim Assist。 */
	/**
	 * Returns true if the given target passes the filter based on the current player owner data.
	 * False if the given target should be excluded from aim assist calculations 
	 */
	UE_API bool DoesTargetPassFilter(const FAimAssistOwnerViewData& OwnerData, const FAimAssistFilter& Filter, const FAimAssistTargetOptions& Target, const float AcceptableRange) const;

	/** 使用同步或跨帧异步 Trace 更新目标可见性；目标离开外圈后会停止继续发起 Trace。 */
	/** Determine if the given target is visible based on our current view data. */
	UE_API void DetermineTargetVisibility(FLyraAimAssistTarget& Target, const FAimAssistSettings& Settings, const FAimAssistFilter& Filter, const FAimAssistOwnerViewData& OwnerData);
	
	/** 根据 Filter 把请求者、Instigator 及其附加 Actor 加入 Trace 忽略列表，并设置复杂碰撞选项。 */
	/** Setup CollisionQueryParams to ignore a set of actors based on filter settings. Such as Ignoring Requester or Instigator. */
	UE_API void InitTargetSelectionCollisionParams(FCollisionQueryParams& OutParams, const AActor& RequestedBy, const FAimAssistFilter& Filter) const;
};

#undef UE_API
