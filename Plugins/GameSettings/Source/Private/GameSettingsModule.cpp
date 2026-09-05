// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

// 实现 GameSettings 模块的启动与关闭入口。
/**
 * Implements the FGameSettingsModule module.
 */
class FGameSettingsModule : public IModuleInterface
{
public:
	FGameSettingsModule();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

};


// 创建无额外模块状态的 GameSettings 模块实例。
FGameSettingsModule::FGameSettingsModule()
{
}

// 启动 GameSettings 模块；当前无需注册额外服务，保留标准模块生命周期入口。
void FGameSettingsModule::StartupModule()
{
}

// 关闭 GameSettings 模块；当前没有需要显式释放的模块级资源。
void FGameSettingsModule::ShutdownModule()
{
}

// 向 Unreal 模块管理器注册 GameSettings 模块实现。
IMPLEMENT_MODULE(FGameSettingsModule, GameSettings);
