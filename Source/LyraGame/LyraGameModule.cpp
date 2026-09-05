// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"


/**
 * LyraGame 运行时模块入口，使用默认游戏模块生命周期实现。
 */
/**
 * FLyraGameModule
 */
class FLyraGameModule : public FDefaultGameModuleImpl
{
	// LyraGame 运行时模块启动入口；当前无需在默认模块加载流程之外注册额外服务。
	virtual void StartupModule() override
	{
	}

	// LyraGame 运行时模块关闭入口；当前没有模块级资源需要显式释放。
	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FLyraGameModule, LyraGame, "LyraGame");
