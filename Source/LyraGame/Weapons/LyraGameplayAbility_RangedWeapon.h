// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Equipment/LyraGameplayAbility_FromEquipment.h"

#include "LyraGameplayAbility_RangedWeapon.generated.h"

enum ECollisionChannel : int;

class APawn;
class ULyraRangedWeaponInstance;
class UObject;
struct FCollisionQueryParams;
struct FFrame;
struct FGameplayAbilityActorInfo;
struct FGameplayEventData;
struct FGameplayTag;
struct FGameplayTagContainer;

/** 定义远程武器查询的起点以及无散布时的朝向。 */
/** Defines where an ability starts its trace from and where it should face */
UENUM(BlueprintType)
enum class ELyraAbilityTargetingSource : uint8
{
	// 从玩家相机位置朝相机焦点查询。
	// From the player's camera towards camera focus
	CameraTowardsFocus,
	// 从 Pawn 中心沿 Pawn 正前方查询。
	// From the pawn's center, in the pawn's orientation
	PawnForward,
	// 从 Pawn 中心朝相机焦点查询。
	// From the pawn's center, oriented towards camera focus
	PawnTowardsFocus,
	// 从武器枪口或武器位置沿 Pawn 正前方查询。
	// From the weapon's muzzle or location, in the pawn's orientation
	WeaponForward,
	// 从武器枪口或武器位置朝相机焦点查询。
	// From the weapon's muzzle or location, towards camera focus
	WeaponTowardsFocus,
	// 使用蓝图提供的自定义查询变换。
	// Custom blueprint-specified source location
	Custom
};



// 由远程武器实例授予并与该实例绑定的 Gameplay Ability，负责本地瞄准查询、目标数据提交及确认命中反馈。
/**
 * ULyraGameplayAbility_RangedWeapon
 *
 * An ability granted by and associated with a ranged weapon instance
 */
UCLASS()
class ULyraGameplayAbility_RangedWeapon : public ULyraGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:

	ULyraGameplayAbility_RangedWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="Lyra|Ability")
	ULyraRangedWeaponInstance* GetWeaponInstance() const;

	//~UGameplayAbility interface
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~End of UGameplayAbility interface

protected:
	struct FRangedWeaponFiringInput
	{
		// 本次弹道查询的起点。
		// Start of the trace
		FVector StartTrace;

		// 完全无散布时瞄准射线的终点。
		// End of the trace if aim were perfect
		FVector EndAim;

		// 完全无散布时的单位瞄准方向。
		// The direction of the trace if aim were perfect
		FVector AimDir;

		// 提供弹丸数量、射程、查询半径和散布参数的武器实例。
		// The weapon instance / source of weapon data
		ULyraRangedWeaponInstance* WeaponData = nullptr;

		// 本次查询命中时是否允许播放弹丸命中特效。
		// Can we play bullet FX for hits during this trace
		bool bCanPlayBulletFX = false;

		FRangedWeaponFiringInput()
			: StartTrace(ForceInitToZero)
			, EndAim(ForceInitToZero)
			, AimDir(ForceInitToZero)
		{
		}
	};

protected:
	static int32 FindFirstPawnHitResult(const TArray<FHitResult>& HitResults);

	// 执行一次武器碰撞查询；SweepRadius 大于 0 时使用球形 Sweep，否则使用射线查询。
	// Does a single weapon trace, either sweeping or ray depending on if SweepRadius is above zero
	FHitResult WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHitResults) const;

	// 先做精确射线查询；没有命中且 SweepRadius 大于 0 时，再用球形 Sweep 扩大容错范围，并处理遮挡优先级。
	// Wrapper around WeaponTrace to handle trying to do a ray trace before falling back to a sweep trace if there were no hits and SweepRadius is above zero 
	FHitResult DoSingleBulletTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, OUT TArray<FHitResult>& OutHits) const;

	// 按单发弹药中的弹丸数量和当前散布，为每颗弹丸分别执行查询并汇总命中。
	// Traces all of the bullets in a single cartridge
	void TraceBulletsInCartridge(const FRangedWeaponFiringInput& InputData, OUT TArray<FHitResult>& OutHits);

	virtual void AddAdditionalTraceIgnoreActors(FCollisionQueryParams& TraceParams) const;

	// 根据本地预测或模拟状态选择武器查询通道，并可补充查询参数。
	// Determine the trace channel to use for the weapon trace(s)
	virtual ECollisionChannel DetermineTraceChannel(FCollisionQueryParams& TraceParams, bool bIsSimulated) const;

	void PerformLocalTargeting(OUT TArray<FHitResult>& OutHits);

	FVector GetWeaponTargetingSourceLocation() const;
	FTransform GetTargetingTransform(APawn* SourcePawn, ELyraAbilityTargetingSource Source) const;

	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	UFUNCTION(BlueprintCallable)
	void StartRangedWeaponTargeting();

	// 目标数据经过本地预测或服务器确认后交给蓝图处理伤害、特效等玩法逻辑。
	// Called when target data is ready
	UFUNCTION(BlueprintImplementableEvent)
	void OnRangedWeaponTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

private:
	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
};
