// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterCoreRuntimeModule.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "ShooterCoreRuntime"

// ShooterCore 运行时模块启动入口；当前类型和设置依靠反射注册，无额外初始化工作。
void FShooterCoreRuntimeModule::StartupModule()
{
}

// ShooterCore 运行时模块关闭入口；当前没有动态注册资源需要释放。
void FShooterCoreRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
// 将 FShooterCoreRuntimeModule 注册为 ShooterCoreRuntime 模块的实现入口。
IMPLEMENT_MODULE(FShooterCoreRuntimeModule, ShooterCoreRuntime)
