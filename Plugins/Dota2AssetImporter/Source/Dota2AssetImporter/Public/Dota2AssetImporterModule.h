// 防止该头文件在同一个编译单元中被重复包含。
#pragma once

// 引入 Unreal 模块系统接口。
// IModuleInterface 定义模块生命周期函数，ModuleManager 负责加载和卸载模块。
#include "Modules/ModuleManager.h"

// Dota2AssetImporter 插件模块类。
// 这个类是当前插件 C++ 模块的入口点，Unreal 会在加载模块时创建它，
// 并在合适的生命周期阶段调用 StartupModule 和 ShutdownModule。
//
// final 表示该类不允许再被其他类继承。
// 当前模块只声明了生命周期函数，还没有公开命令、菜单、导入器或 Commandlet 类型。
class FDota2AssetImporterModule final : public IModuleInterface
{
public:
    // 模块启动回调。
    // 当 Unreal Editor 加载 Dota2AssetImporter 模块时会调用这里。
    // 后续如果要注册菜单、命令、资产导入器、Commandlet 或编辑器扩展，通常会放在这个函数中。
    virtual void StartupModule() override;

    // 模块关闭回调。
    // 当 Unreal Editor 卸载 Dota2AssetImporter 模块或编辑器退出时会调用这里。
    // 后续如果 StartupModule 注册了任何回调、菜单、命令或资源，应在这里反注册和释放。
    virtual void ShutdownModule() override;
};
