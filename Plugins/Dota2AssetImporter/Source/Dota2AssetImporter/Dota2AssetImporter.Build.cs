// 引入 Unreal Build Tool 命名空间。
// ModuleRules、ReadOnlyTargetRules、PCHUsageMode 等构建规则类型都定义在这里。
using UnrealBuildTool;

// Dota2AssetImporter 模块的构建规则类。
// 类名必须与文件名 Dota2AssetImporter.Build.cs 中的模块名保持一致，
// Unreal Build Tool 会通过这个类决定该模块如何编译、依赖哪些引擎模块。
public class Dota2AssetImporter : ModuleRules
{
    // 构造函数会在 Unreal Build Tool 生成工程文件或编译模块时被调用。
    // Target 描述当前构建目标，例如 Editor、Game、平台、配置等。
    public Dota2AssetImporter(ReadOnlyTargetRules Target) : base(Target)
    {
        // 使用显式或共享 PCH。
        // 这是 UE 模块常用设置，可以减少编译时间，同时避免依赖旧式隐式 PCH。
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 声明本模块私有实现代码需要链接的依赖模块。
        // 这些模块只暴露给 Dota2AssetImporter 自己的 Private 源文件使用，
        // 不会作为公共依赖传播给其他模块。
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // 编辑器动画工具库，通常用于批量创建、修改或查询 Animation Sequence 等动画资产。
            "AnimationBlueprintLibrary",

            // 资产注册表模块，用于查询、扫描和刷新 Content Browser 中的资产信息。
            "AssetRegistry",

            // 编辑器资产工具模块，用于创建资产、导入资产、重命名资产和处理资产工厂。
            "AssetTools",

            // UE 最基础的核心模块，提供容器、字符串、日志、断言、平台抽象等基础能力。
            "Core",

            // UObject 反射系统模块，提供 UObject、UClass、UPackage、序列化和垃圾回收等能力。
            "CoreUObject",

            // 引擎运行时核心模块，提供材质、纹理、骨骼网格体、动画、物理资产等常用引擎类型。
            "Engine",

            // JSON 基础模块，用于读取或写入导入配置、元数据清单等 JSON 数据。
            "Json",

            // JSON 辅助序列化模块，用于在 UStruct/C++ 结构体和 JSON 之间转换。
            "JsonUtilities",

            // 材质编辑器模块，通常用于编辑器侧材质处理、材质表达式或材质相关工具。
            "MaterialEditor",

            // 项目管理模块，可用于读取插件路径、项目路径和项目/插件描述信息。
            "Projects",

            // Kismet/蓝图相关工具模块，可用于编辑器侧蓝图或脚本工具集成。
            "Kismet",

            // Unreal Editor 核心编辑器模块，提供导入工厂、编辑器工具、Commandlet 等编辑器专用 API。
            "UnrealEd"
        });
    }
}
