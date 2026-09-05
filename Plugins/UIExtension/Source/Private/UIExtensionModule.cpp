// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FUIExtensionModule : public IModuleInterface
{
public:
	/** IModuleInterface 接口实现。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

// 模块加载时的启动入口；当前不需要额外初始化工作。
void FUIExtensionModule::StartupModule()
{
	// 此代码在模块载入内存后执行，具体时机由 .uplugin 中的模块配置决定。
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

// 模块关闭或动态卸载前的清理入口；当前没有需要释放的模块级资源。
void FUIExtensionModule::ShutdownModule()
{
	// 引擎关闭时可能调用此函数；支持动态重载的模块也会在卸载前调用它。
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// 对于支持动态重载的模块，会在卸载模块前执行该函数。
	// we call this function before unloading the module.
}
	
// 注册 UIExtension 运行时模块及其生命周期实现。
IMPLEMENT_MODULE(FUIExtensionModule, UIExtension)
