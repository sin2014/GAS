// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

// 实现 CommonGame 模块的标准启动与关闭入口。
/**
 * Implements the FCommonGameModule module.
 */
class FCommonGameModule : public IModuleInterface
{
public:
	FCommonGameModule();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

};


// 创建无额外模块状态的 CommonGame 模块实例。
FCommonGameModule::FCommonGameModule()
{
}

// 启动 CommonGame 模块；当前无需注册额外模块级服务。
void FCommonGameModule::StartupModule()
{
}

// 关闭 CommonGame 模块；当前没有需要显式释放的模块级资源。
void FCommonGameModule::ShutdownModule()
{
}

// 向 Unreal 模块管理器注册 CommonGame 模块实现。
IMPLEMENT_MODULE(FCommonGameModule, CommonGame);
