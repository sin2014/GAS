// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FAsyncMixinModule : public IModuleInterface
{
public:
	/** IModuleInterface 生命周期接口。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// 模块加载后进入启动阶段；当前模块不需要额外注册全局服务。
void FAsyncMixinModule::StartupModule()
{
	// 模块进入内存后执行，具体时机由 .uplugin 中该模块的 LoadingPhase 决定。
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

// 模块卸载前执行清理；当前没有动态注册项需要释放。
void FAsyncMixinModule::ShutdownModule()
{
	// 关闭阶段会调用此函数；支持动态重载的模块也会在卸载前经过这里。
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
	
// 将 FAsyncMixinModule 注册为 AsyncMixin 插件模块的实现入口。
IMPLEMENT_MODULE(FAsyncMixinModule, AsyncMixin)
