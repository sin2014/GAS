// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

class FGameSubtitlesModule : public IModuleInterface
{
public:
	/** IModuleInterface 生命周期实现。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// 模块启动时把插件自带的 Config/Tags 目录加入原生 GameplayTag INI 搜索路径。
void FGameSubtitlesModule::StartupModule()
{
	UGameplayTagsManager::Get().AddTagIniSearchPath(FPaths::ProjectPluginsDir() / TEXT("GameSubtitles/Config/Tags"));
}

// 模块关闭入口当前没有额外资源需要释放，GameplayTag 搜索路径由引擎生命周期管理。
void FGameSubtitlesModule::ShutdownModule()
{
}
	
// 注册 GameSubtitles 运行时模块及其自定义启动生命周期。
IMPLEMENT_MODULE(FGameSubtitlesModule, GameSubtitles)
