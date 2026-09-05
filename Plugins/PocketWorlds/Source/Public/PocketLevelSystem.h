// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "PocketLevelSystem.generated.h"

#define UE_API POCKETWORLDS_API

class ULocalPlayer;
class UObject;
class UPocketLevel;
class UPocketLevelInstance;

/** 按世界维护 Pocket Level 实例，并为同一本地玩家和数据资产复用已有实例。 */
/**
 *
 */
UCLASS(MinimalAPI)
class UPocketLevelSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 获取或创建指定玩家的 Pocket Level；新实例会沿 Z 轴避开现有 Pocket 空间，配置失败时返回空。 */
	/**
	 * 
	 */
	UE_API UPocketLevelInstance* GetOrCreatePocketLevelFor(ULocalPlayer* LocalPlayer, UPocketLevel* PocketLevel, FVector DesiredSpawnPoint);

private:
	UPROPERTY()
	TArray<TObjectPtr<UPocketLevelInstance>> PocketInstances;
};

#undef UE_API
