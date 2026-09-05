// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "LyraPickupDefinition.generated.h"

class ULyraInventoryItemDefinition;
class UNiagaraSystem;
class UObject;
class USoundBase;
class UStaticMesh;

/** 拾取物的数据资产，配置获得的背包物品、世界表现、重生间隔及拾取/重生反馈。 */
/**
 * 
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Const, Meta = (DisplayName = "Lyra Pickup Data", ShortTooltip = "Data asset used to configure a pickup."))
class ULyraPickupDefinition : public UDataAsset
{
	GENERATED_BODY()
	
public:

	// 拾取后加入背包的物品定义；该定义进一步决定生成 Actor、授予技能和添加标签等行为。
	//Defines the pickup's actors to spawn, abilities to grant, and tags to add
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup|Equipment")
	TSubclassOf<ULyraInventoryItemDefinition> InventoryItemDefinition;

	// 拾取物在世界中的静态网格体表现。
	//Visual representation of the pickup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup|Mesh")
	TObjectPtr<UStaticMesh> DisplayMesh;

	// 拾取物被取走后到再次生成之间的冷却秒数。
	//Cool down time between pickups in seconds
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup")
	int32 SpawnCoolDownSeconds;

	// 拾取成功时播放的声音。
	//Sound to play when picked up
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup")
	TObjectPtr<USoundBase> PickedUpSound;

	// 拾取物重新生成时播放的声音。
	//Sound to play when pickup is respawned
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup")
	TObjectPtr<USoundBase> RespawnedSound;

	// 拾取成功时播放的 Niagara 特效。
	//Particle FX to play when picked up
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup")
	TObjectPtr<UNiagaraSystem> PickedUpEffect;

	// 拾取物重新生成时播放的 Niagara 特效。
	//Particle FX to play when pickup is respawned
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup")
	TObjectPtr<UNiagaraSystem> RespawnedEffect;
};


UCLASS(MinimalAPI, Blueprintable, BlueprintType, Const, Meta = (DisplayName = "Lyra Weapon Pickup Data", ShortTooltip = "Data asset used to configure a weapon pickup."))
class ULyraWeaponPickupDefinition : public ULyraPickupDefinition
{
	GENERATED_BODY()

public:

	// 显示网格体相对武器生成器的局部位置偏移。
	//Sets the height of the display mesh above the Weapon spawner
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup|Mesh")
	FVector WeaponMeshOffset;

	// 显示网格体相对武器生成器使用的缩放。
	//Sets the height of the display mesh above the Weapon spawner
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Pickup|Mesh")
	FVector WeaponMeshScale = FVector(1.0f, 1.0f, 1.0f);
};
