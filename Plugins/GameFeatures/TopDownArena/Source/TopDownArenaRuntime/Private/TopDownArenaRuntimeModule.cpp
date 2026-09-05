// Copyright Epic Games, Inc. All Rights Reserved.

#include "TopDownArenaRuntimeModule.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FTopDownArenaRuntimeModule"

// 模块加载后进入启动阶段；当前运行时类型依靠反射注册，无需额外全局初始化。
void FTopDownArenaRuntimeModule::StartupModule()
{
	// 模块进入内存后执行，具体时机由 .uplugin 中该模块的 LoadingPhase 决定。
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

// 模块卸载前执行清理；当前没有动态注册项需要释放。
void FTopDownArenaRuntimeModule::ShutdownModule()
{
	// 关闭阶段会调用此函数；支持动态重载的模块也会在卸载前经过这里。
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
// 将 FTopDownArenaRuntimeModule 注册为 TopDownArenaRuntime 模块的实现入口。
IMPLEMENT_MODULE(FTopDownArenaRuntimeModule, TopDownArenaRuntime)
