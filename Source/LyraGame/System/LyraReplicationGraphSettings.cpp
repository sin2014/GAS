// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraReplicationGraphSettings.h"
#include "Misc/App.h"
#include "System/LyraReplicationGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraReplicationGraphSettings)

// 将复制图设置归入 Game 分类，并默认使用 LyraReplicationGraph 实现类。
ULyraReplicationGraphSettings::ULyraReplicationGraphSettings()
{
	CategoryName = TEXT("Game");
	DefaultReplicationGraphClass = ULyraReplicationGraph::StaticClass();
}
