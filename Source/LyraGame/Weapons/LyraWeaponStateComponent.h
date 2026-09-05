// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "LyraWeaponStateComponent.generated.h"

class UObject;
struct FFrame;
struct FGameplayAbilityTargetDataHandle;
struct FGameplayEffectContextHandle;
struct FHitResult;

// 记录远程武器命中在视口中的准星反馈位置；对敌方造成伤害的命中显示为成功样式。
// Hit markers are shown for ranged weapon impacts in the reticle
// A 'successful' hit marker is shown for impacts that damaged an enemy
struct FLyraScreenSpaceHitLocation
{
	/** 命中点投影到视口后的屏幕空间坐标。 */
	/** Hit location in viewport screenspace */
	FVector2D Location;	
	FGameplayTag HitZone;
	bool bShowAsSuccess = false;
};

struct FLyraServerSideHitMarkerBatch
{
	FLyraServerSideHitMarkerBatch() { }

	FLyraServerSideHitMarkerBatch(uint8 InUniqueId) :
		UniqueId(InUniqueId)
	{ }

	TArray<FLyraScreenSpaceHitLocation> Markers;

	uint8 UniqueId = 0;
};

// 玩家控制器组件：跟踪待服务器确认的预测命中，以及最近已确认伤害的屏幕空间命中标记。
// Tracks weapon state and recent confirmed hit markers to display on screen
UCLASS()
class ULyraWeaponStateComponent : public UControllerComponent
{
	GENERATED_BODY()

public:

	ULyraWeaponStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(Client, Reliable)
	void ClientConfirmTargetData(uint16 UniqueId, bool bSuccess, const TArray<uint8>& HitReplaces);

	void AddUnconfirmedServerSideHitMarkers(const FGameplayAbilityTargetDataHandle& InTargetData, const TArray<FHitResult>& FoundHits);

	/** 从效果上下文提取命中结果并更新该玩家最近一次造成武器伤害的时间与屏幕位置。 */
	/** Updates this player's last damage instigated time */
	void UpdateDamageInstigatedTime(const FGameplayEffectContextHandle& EffectContext);

	/** 取得该玩家最近造成伤害的已确认命中点屏幕坐标。 */
	/** Gets the array of most recent locations this player instigated damage, in screen-space */
	void GetLastWeaponDamageScreenLocations(TArray<FLyraScreenSpaceHitLocation>& WeaponDamageScreenLocations)
	{
		WeaponDamageScreenLocations = LastWeaponDamageScreenLocations;
	}

	/** 返回距最近一次对外造成伤害通知经过的世界时间。 */
	/** Returns the elapsed time since the last (outgoing) damage hit notification occurred */
	double GetTimeSinceLastHitNotification() const;

	int32 GetUnconfirmedServerSideHitMarkerCount() const
	{
		return UnconfirmedServerSideHitMarkers.Num();
	}

protected:
	// 判断命中标记是否采用成功样式。默认仅当目标是团队 Actor 且与所属控制器 Pawn 队伍不同才视为成功。
	// This is called to filter hit results to determine whether they should be considered as a successful hit or not
	// The default behavior is to treat it as a success if being done to a team actor that belongs to a different team
	// to the owning controller's pawn
	virtual bool ShouldShowHitAsSuccess(const FHitResult& Hit) const;

	virtual bool ShouldUpdateDamageInstigatedTime(const FGameplayEffectContextHandle& EffectContext) const;

	void ActuallyUpdateDamageInstigatedTime();

private:
	/** 此控制器最近一次造成武器伤害的世界时间。 */
	/** Last time this controller instigated weapon damage */
	double LastWeaponDamageInstigatedTime = 0.0;

	/** 最近一次已确认伤害对应的屏幕空间命中位置。 */
	/** Screen-space locations of our most recently instigated weapon damage (the confirmed hits) */
	TArray<FLyraScreenSpaceHitLocation> LastWeaponDamageScreenLocations;

	/** 按目标数据唯一 ID 保存、等待服务器确认的预测命中批次。 */
	/** The unconfirmed hits */
	TArray<FLyraServerSideHitMarkerBatch> UnconfirmedServerSideHitMarkers;
};
