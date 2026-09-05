// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraNumberPopComponent.h"

#include "LyraNumberPopComponent_MeshText.generated.h"

class ULyraDamagePopStyle;
class UMaterialInstanceDynamic;
class UObject;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT()
struct FPooledNumberPopComponentList
{
	GENERATED_BODY()

	UPROPERTY(transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Components;
};

USTRUCT()
struct FLiveNumberPopEntry
{
	GENERATED_BODY()

	/** 当前正在播放数字动画的 StaticMeshComponent。 */
	/** The component that is currently live */
	UPROPERTY(transient)
	TObjectPtr<UStaticMeshComponent> Component = nullptr;

	/** 动画结束后该组件要归还的对象池；为空时直接销毁。 */
	/** The pool this component will go into when released */
	FPooledNumberPopComponentList* Pool = nullptr;

	/** 该组件应结束显示并归还对象池的世界时间。 */
	/** The world time that this component will be released to the pool */
	float ReleaseTime = 0.0f;

	FLiveNumberPopEntry()
	{}

	FLiveNumberPopEntry(UStaticMeshComponent* InComponent, FPooledNumberPopComponentList* InPool, float InReleaseTime)
		: Component(InComponent), Pool(InPool), ReleaseTime(InReleaseTime)
	{}
};

/** 保存一条待显示伤害数字的解析结果、样式和运行时组件。 */
/** Struct that holds the info for a new damage number */
struct FTempNumberPopInfo
{
	UStaticMeshComponent* StaticMeshComponent = nullptr;

	TArray<UMaterialInstanceDynamic*> MeshMIDs;

	TArray<int32> DamageNumberArray;
};




UCLASS(Blueprintable)
class ULyraNumberPopComponent_MeshText : public ULyraNumberPopComponent
{
	GENERATED_BODY()

public:

	ULyraNumberPopComponent_MeshText(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ULyraNumberPopComponent interface
	virtual void AddNumberPop(const FLyraNumberPopRequest& NewRequest) override;
	//~End of ULyraNumberPopComponent interface

protected:
	void SetMaterialParameters(const FLyraNumberPopRequest& Request, FTempNumberPopInfo& NewDamageNumberInfo, const FTransform& CameraTransform, const FVector& NumberLocation);

	FLinearColor DetermineColor(const FLyraNumberPopRequest& Request) const;
	UStaticMesh* DetermineStaticMesh(const FLyraNumberPopRequest& Request) const;


	/** 将已超过显示寿命的组件归还对象池或销毁，并安排下一次释放定时器。 */
	/** Releases components back to the pool that have exceeded their lifespan */
	void ReleaseNextComponents();

	/** 按顺序匹配传入数字请求的网格材质样式。 */
	/** Style patterns to attempt to apply to the incoming number pops */
	UPROPERTY(EditDefaultsOnly, Category="Number Pop|Style")
	TArray<TObjectPtr<ULyraDamagePopStyle>> Styles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Number Pop|Style")
	float ComponentLifespan;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	float DistanceFromCameraBeforeDoublingSize;
	
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	float CriticalHitSizeMultiplier;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Font")
	float FontXSize;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Font")
	float FontYSize;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Font")
	float SpacingPercentageForOnes;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	float NumberOfNumberRotations;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName SignDigitParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName ColorParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName AnimationLifespanParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName IsCriticalHitParameterName;

	/** 材质默认把伤害数字深度拉近相机以避免遮挡；此参数控制是否启用，值应为 0 或 1。 */
	/** Damage numbers by default are given a depth close to the camera in the material to make sure they are never occluded. This can be toggled off here, should only be 0/1. */
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	FName MoveToCameraParameterName;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	TArray<FName> PositionParameterNames;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	TArray<FName> ScaleRotationAngleParameterNames;

	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Material Bindings")
	TArray<FName> DurationParameterNames;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, FPooledNumberPopComponentList> PooledComponentMap;

	UPROPERTY(transient)
	TArray<FLiveNumberPopEntry> LiveComponents;

	FTimerHandle ReleaseTimerHandle;
};
