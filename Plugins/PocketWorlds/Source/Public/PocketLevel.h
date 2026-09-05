// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "PocketLevel.generated.h"

#define UE_API POCKETWORLDS_API

class UObject;
class UWorld;

/** 描述 Pocket Level 使用的关卡资产及实例间防重叠边界。 */
/**
 * 
 */
UCLASS(MinimalAPI)
class UPocketLevel : public UDataAsset
{
	GENERATED_BODY()

public:
	UE_API UPocketLevel();

public:
	// 此 Pocket Level 实例需要动态流送的关卡资产。
	// The level that will be streamed in for this pocket level.
	UPROPERTY(EditAnywhere, Category = "Streaming")
	TSoftObjectPtr<UWorld> Level;
	
	// Pocket 空间的边界尺寸，用于沿 Z 轴排列多个实例并避免彼此重叠。
	// The bounds of the pocket level so that we can create multiple instances without overlapping each other.
	UPROPERTY(EditAnywhere, Category = "Streaming")
	FVector Bounds;	
};

#undef UE_API
