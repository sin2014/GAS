// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameData.h"
#include "LyraAssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameData)

// 构造全局游戏数据资产，具体 GameplayEffect 等核心引用由资产配置提供。
ULyraGameData::ULyraGameData()
{
}

// 通过项目 AssetManager 获取唯一的全局 LyraGameData 实例。
const ULyraGameData& ULyraGameData::ULyraGameData::Get()
{
	return ULyraAssetManager::Get().GetGameData();
}
