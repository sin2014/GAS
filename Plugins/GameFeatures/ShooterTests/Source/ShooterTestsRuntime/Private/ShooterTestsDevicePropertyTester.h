// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/InputDevicePropertyHandle.h"
#include "ShooterTestsDevicePropertyTester.generated.h"

class UInputDeviceProperty;
class UCapsuleComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
struct FHitResult;

/** 玩家 Pawn 进入碰撞体时为其 PlatformUser 激活输入设备属性，离开时移除本 Actor 激活的全部句柄。 */
/** This tester will apply device properties to a Player Controller on overlap, and remove them once overlap ends. */
UCLASS(Blueprintable, BlueprintType)
class AShooterTestsDevicePropertyTester : public AActor
{
	GENERATED_BODY()

public:

	AShooterTestsDevicePropertyTester();

	/** 玩家重叠时需要激活的 InputDeviceProperty 类列表。 */
	/** Device properties to apply on overlap with a player controller. */
	UPROPERTY(EditAnywhere, Category = "Device Property")
	TArray<TSubclassOf<UInputDeviceProperty>> DeviceProperties;

	/** 触发设备属性添加与移除的胶囊碰撞体。 */
	/** The volume that will trigger device properties to be added and removed on overlap */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Device Property")
	TObjectPtr<UCapsuleComponent> CollisionVolume;

	/** 用于在关卡中可视化碰撞区域的小型平台网格。 */
	/** A little mesh to make this collision volume visible */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Device Property")
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	UFUNCTION(BlueprintCallable, Category = "Device Property")
	void ApplyDeviceProperties(const FPlatformUserId UserId);

	UFUNCTION(BlueprintCallable, Category = "Device Property")
	void RemoveDeviceProperties();

private:

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(Transient)
	TSet<FInputDevicePropertyHandle> ActivePropertyHandles;
};
