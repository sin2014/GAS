// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FPocketWorldsModule : public IModuleInterface
{
public:
	/** IModuleInterface 生命周期接口。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// 模块加载后进入启动阶段；PocketWorlds 的对象和子系统由 Unreal 反射与世界生命周期自行创建。
void FPocketWorldsModule::StartupModule()
{
	// 模块进入内存后执行，具体时机由 .uplugin 中该模块的 LoadingPhase 决定。
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

// 模块卸载前执行清理；当前没有额外注册的全局资源需要释放。
void FPocketWorldsModule::ShutdownModule()
{
	// 关闭阶段会调用此函数；支持动态重载的模块也会在卸载前经过这里。
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
	
// 将 FPocketWorldsModule 注册为 PocketWorlds 插件模块的实现入口。
IMPLEMENT_MODULE(FPocketWorldsModule, PocketWorlds)
