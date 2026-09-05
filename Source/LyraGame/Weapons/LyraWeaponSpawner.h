// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "LyraWeaponSpawner.generated.h"

#define UE_API LYRAGAME_API

namespace EEndPlayReason { enum Type : int; }

class APawn;
class UCapsuleComponent;
class ULyraInventoryItemDefinition;
class ULyraWeaponPickupDefinition;
class UObject;
class UPrimitiveComponent;
class UStaticMeshComponent;
struct FFrame;
struct FGameplayTag;
struct FHitResult;

UCLASS(MinimalAPI, Blueprintable,BlueprintType)
class ALyraWeaponSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// 创建碰撞区、底座和武器网格等默认组件与初始属性。
	// Sets default values for this actor's properties
	UE_API ALyraWeaponSpawner();

protected:
	// 游戏开始或生成后初始化武器可用状态及重生逻辑。
	// Called when the game starts or when spawned
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// 每帧旋转可见武器网格并更新冷却进度。
	// Called every frame
	UE_API virtual void Tick(float DeltaTime) override;

	UE_API void OnConstruction(const FTransform& Transform) override;

protected:
	// 配置拾取物品、网格和相关表现的武器刷新器数据资产。
	//Data asset used to configure a Weapon Spawner
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Lyra|WeaponPickup")
	TObjectPtr<ULyraWeaponPickupDefinition> WeaponDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, ReplicatedUsing = OnRep_WeaponAvailability, Category = "Lyra|WeaponPickup")
	bool bIsWeaponAvailable;

	// 武器被拾取后到重新可用之间的冷却时间，单位秒。
	//The amount of time between weapon pickup and weapon spawning in seconds
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|WeaponPickup")
	float CoolDownTime;

	// 武器重新可用后延迟检查刷新点内已有 Pawn，给 bIsWeaponAvailable 的 OnRep 和重生特效留出执行时间。
	//Delay between when the weapon is made available and when we check for a pawn standing in the spawner. Used to give the bIsWeaponAvailable OnRep time to fire and play FX. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|WeaponPickup")
	float CheckExistingOverlapDelay;

	// 供 UI 显示武器重生进度的归一化值，范围为 0 到 1。
	//Used to drive weapon respawn time indicators 0-1
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Lyra|WeaponPickup")
	float CoolDownPercentage;

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|WeaponPickup")
	TObjectPtr<UCapsuleComponent> CollisionVolume;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|WeaponPickup")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(BlueprintReadOnly, Category = "Lyra|WeaponPickup")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Lyra|WeaponPickup")
	float WeaponMeshRotationSpeed;

	FTimerHandle CoolDownTimerHandle;

	FTimerHandle CheckOverlapsDelayTimerHandle;

	UFUNCTION()
	UE_API void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	// 武器重生后检查已站在底座碰撞区内的 Pawn，避免必须重新进入才触发拾取。
	//Check for pawns standing on pad when the weapon is spawned. 
	UE_API void CheckForExistingOverlaps();

	UFUNCTION(BlueprintNativeEvent)
	UE_API void AttemptPickUpWeapon(APawn* Pawn);

	UFUNCTION(BlueprintImplementableEvent, Category = "Lyra|WeaponPickup")
	UE_API bool GiveWeapon(TSubclassOf<ULyraInventoryItemDefinition> WeaponItemClass, APawn* ReceivingPawn);

	UE_API void StartCoolDown();

	UFUNCTION(BlueprintCallable, Category = "Lyra|WeaponPickup")
	UE_API void ResetCoolDown();

	UFUNCTION()
	UE_API void OnCoolDownTimerComplete();

	UE_API void SetWeaponPickupVisibility(bool bShouldBeVisible);

	UFUNCTION(BlueprintNativeEvent, Category = "Lyra|WeaponPickup")
	UE_API void PlayPickupEffects();

	UFUNCTION(BlueprintNativeEvent, Category = "Lyra|WeaponPickup")
	UE_API void PlayRespawnEffects();

	UFUNCTION()
	UE_API void OnRep_WeaponAvailability();

	/** 在物品定义的 SetStats Fragment 中查找指定统计标签；不存在时返回 0。 */
	/** Searches an item definition type for a matching stat and returns the value, or 0 if not found */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lyra|WeaponPickup")
	static UE_API int32 GetDefaultStatFromItemDef(const TSubclassOf<ULyraInventoryItemDefinition> WeaponItemClass, FGameplayTag StatTag);
};

#undef UE_API
