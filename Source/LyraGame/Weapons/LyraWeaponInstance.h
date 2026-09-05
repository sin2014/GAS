// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cosmetics/LyraCosmeticAnimationTypes.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "GameFramework/InputDevicePropertyHandle.h"

#include "LyraWeaponInstance.generated.h"

#define UE_API LYRAGAME_API

class UAnimInstance;
class UObject;
struct FFrame;
struct FGameplayTagContainer;
class UInputDeviceProperty;

// 装备到 Pawn 上的通用武器实例，负责装备动画层、交互时间和装备期间持续生效的输入设备属性。
/**
 * ULyraWeaponInstance
 *
 * A piece of equipment representing a weapon spawned and applied to a pawn
 */
UCLASS(MinimalAPI)
class ULyraWeaponInstance : public ULyraEquipmentInstance
{
	GENERATED_BODY()

public:
	UE_API ULyraWeaponInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ULyraEquipmentInstance interface
	UE_API virtual void OnEquipped() override;
	UE_API virtual void OnUnequipped() override;
	//~End of ULyraEquipmentInstance interface

	UFUNCTION(BlueprintCallable)
	UE_API void UpdateFiringTime();

	// 返回距最近一次装备或开火经过的时间，取两种交互时间中的较新者。
	// Returns how long it's been since the weapon was interacted with (fired or equipped)
	UFUNCTION(BlueprintPure)
	UE_API float GetTimeSinceLastInteractedWith() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	FLyraAnimLayerSelectionSet EquippedAnimSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	FLyraAnimLayerSelectionSet UneuippedAnimSet;

	// 武器装备期间持续应用的输入设备属性。它们以 Looping 模式激活，直到卸下武器或持有者死亡时手动移除。
	/**
	 * Device properties that should be applied while this weapon is equipped.
	 * These properties will be played in with the "Looping" flag enabled, so they will
	 * play continuously until this weapon is unequipped! 
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Input Devices")
	TArray<TObjectPtr<UInputDeviceProperty>> ApplicableDeviceProperties;
	
	// 根据装备状态和外观 GameplayTag，从对应动画集合中选择最匹配的动画层。
	// Choose the best layer from EquippedAnimSet or UneuippedAnimSet based on the specified gameplay tags
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Animation)
	UE_API TSubclassOf<UAnimInstance> PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticTags) const;

	/** 返回武器所属 Pawn 对应的本地平台用户 ID；没有本地玩家时返回无效 ID。 */
	/** Returns the owning Pawn's Platform User ID */
	UFUNCTION(BlueprintCallable)
	UE_API const FPlatformUserId GetOwningUserId() const;

	/** 武器所属 Pawn 开始死亡时的回调，用于移除全部已激活输入设备属性。 */
	/** Callback for when the owning pawn of this weapon dies. Removes all spawned device properties. */
	UFUNCTION()
	UE_API void OnDeathStarted(AActor* OwningActor);

	// 将 ApplicableDeviceProperties 以循环模式应用到所属玩家的主输入设备，并保存句柄以便之后精确移除。
	/**
	 * Apply the ApplicableDeviceProperties to the owning pawn of this weapon.
	 * Populate the DevicePropertyHandles so that they can be removed later. This will
	 * Play the device properties in Looping mode so that they will share the lifetime of the
	 * weapon being Equipped.
	 */
	UE_API void ApplyDeviceProperties();

	/** 移除 ApplyDeviceProperties 激活的全部输入设备属性并清空句柄集合。 */
	/** Remove any device proeprties that were activated in ApplyDeviceProperties. */
	UE_API void RemoveDeviceProperties();

private:

	/** 当前由此武器激活的输入设备属性句柄集合。 */
	/** Set of device properties activated by this weapon. Populated by ApplyDeviceProperties */
	UPROPERTY(Transient)
	TSet<FInputDevicePropertyHandle> DevicePropertyHandles;

	double TimeLastEquipped = 0.0;
	double TimeLastFired = 0.0;
};

#undef UE_API
