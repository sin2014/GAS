// 引入本模块的公共头文件，获得 FDota2AssetImporterModule 类声明。
#include "Dota2AssetImporterModule.h"

// 引入 Unreal 模块管理系统。
// IMPLEMENT_MODULE 宏依赖这里提供的模块注册机制。
#include "Modules/ModuleManager.h"

// 将 FDota2AssetImporterModule 注册为名为 Dota2AssetImporter 的 Unreal 模块。
// 这个模块名必须与 .uplugin 的 Modules.Name、Source 子目录名和 Build.cs 模块名保持一致。
// Unreal Editor 加载插件时，会根据这个注册信息创建模块实例并调用生命周期函数。
IMPLEMENT_MODULE(FDota2AssetImporterModule, Dota2AssetImporter)

// 模块启动函数。
// 当前函数体为空，说明插件目前只完成了模块骨架和依赖声明，
// 还没有真正注册导入命令、菜单入口、Commandlet、资产工厂或其他编辑器扩展。
void FDota2AssetImporterModule::StartupModule()
{
}

// 模块关闭函数。
// 当前函数体为空，因为 StartupModule 还没有注册任何需要清理的资源。
// 如果未来添加了委托绑定、菜单扩展、命令注册或单例对象，应在这里成对释放。
void FDota2AssetImporterModule::ShutdownModule()
{
}
