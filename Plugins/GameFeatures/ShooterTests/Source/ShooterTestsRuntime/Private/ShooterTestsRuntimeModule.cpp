// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FShooterTestsRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface 生命周期接口；当前测试模块无需额外启动或关闭注册。 */
	/** IModuleInterface implementation */
	// 模块启动时无需额外注册，测试类型由编译宏和自动化框架发现。
	virtual void StartupModule() override {}
	// 模块关闭时没有持久注册项需要清理。
	virtual void ShutdownModule() override {}
};

// 将 FShooterTestsRuntimeModule 注册为 ShooterTestsRuntime 模块的实现入口。
IMPLEMENT_MODULE(FShooterTestsRuntimeModule, ShooterTestsRuntime)
