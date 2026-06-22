using UnrealBuildTool;
using System.IO;

public class UE_libretro : ModuleRules
{
    public UE_libretro(ReadOnlyTargetRules Target) : base(Target)
    {
        // 使用显式或共享 PCH，保持 UE C++ 模块的常规编译方式。
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 运行时模块需要的 UE 基础、UMG、Slate、渲染和音频依赖。
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Slate",
            "SlateCore",
            "UMG",
            "RenderCore",
            "RHI",
            "Projects",
            "AudioMixer"
        });

        // libretro.h 放在 ThirdParty 下，供 Runner 直接包含 C API。
        string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string LibretroInclude = Path.Combine(ProjectRoot, "ThirdParty", "Libretro", "Include");
        PublicIncludePaths.Add(LibretroInclude);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // 当前示例只打包 Win64 core DLL；其他平台需要换成对应平台的 core。
            string CoreDir = Path.Combine(ProjectRoot, "ThirdParty", "Libretro", "Cores", "Win64");
            string FceummCorePath = Path.Combine(CoreDir, "fceumm_libretro.dll");
            string DesmumeCorePath = Path.Combine(CoreDir, "desmume_libretro.dll");
            string AzaharCorePath = Path.Combine(CoreDir, "azahar_libretro.dll");
            RuntimeDependencies.Add(FceummCorePath);
            RuntimeDependencies.Add(DesmumeCorePath);
            RuntimeDependencies.Add(AzaharCorePath);

            // Azahar 的 libretro 硬件渲染路径使用 OpenGL/WGL。
            PublicSystemLibraries.Add("opengl32.lib");
            PublicDelayLoadDLLs.Add("fceumm_libretro.dll");
            PublicDelayLoadDLLs.Add("desmume_libretro.dll");
            PublicDelayLoadDLLs.Add("azahar_libretro.dll");
        }
    }
}
