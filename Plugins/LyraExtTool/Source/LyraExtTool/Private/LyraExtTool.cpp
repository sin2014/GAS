// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraExtTool.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FLyraExtToolModule"

// 模块按 .uplugin 配置的加载阶段进入内存后调用；当前没有额外工具服务需要注册。
void FLyraExtToolModule::StartupModule()
{
	// 此入口在模块加载完成后执行，具体时机由 .uplugin 中该模块的 LoadingPhase 决定。
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

// 模块关闭或动态重载卸载前调用；当前没有模块级资源需要释放。
void FLyraExtToolModule::ShutdownModule()
{
	// 此入口用于关闭期间清理模块资源；支持动态重载的模块会在卸载前收到调用。
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
// 注册 LyraExtTool 模块及其自定义生命周期实现。
IMPLEMENT_MODULE(FLyraExtToolModule, LyraExtTool)
