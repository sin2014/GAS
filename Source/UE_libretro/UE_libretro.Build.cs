using UnrealBuildTool;
using System.IO;

public class UE_libretro : ModuleRules
{
    public UE_libretro(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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

        string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string LibretroInclude = Path.Combine(ProjectRoot, "ThirdParty", "Libretro", "Include");
        PublicIncludePaths.Add(LibretroInclude);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string CoreDir = Path.Combine(ProjectRoot, "ThirdParty", "Libretro", "Cores", "Win64");
            string NesCorePath = Path.Combine(CoreDir, "fceumm_libretro.dll");
            string NdsCorePath = Path.Combine(CoreDir, "desmume_libretro.dll");
            string ThreeDsAzaharCorePath = Path.Combine(CoreDir, "azahar_libretro.dll");
            string ThreeDsCitraCorePath = Path.Combine(CoreDir, "citra_libretro.dll");
            string ThreeDsPandaCorePath = Path.Combine(CoreDir, "panda3ds_libretro.dll");
            RuntimeDependencies.Add(NesCorePath);
            RuntimeDependencies.Add(NdsCorePath);
            RuntimeDependencies.Add(ThreeDsAzaharCorePath);
            RuntimeDependencies.Add(ThreeDsCitraCorePath);
            RuntimeDependencies.Add(ThreeDsPandaCorePath);
            PublicSystemLibraries.Add("opengl32.lib");
            PublicDelayLoadDLLs.Add("fceumm_libretro.dll");
            PublicDelayLoadDLLs.Add("desmume_libretro.dll");
            PublicDelayLoadDLLs.Add("azahar_libretro.dll");
            PublicDelayLoadDLLs.Add("citra_libretro.dll");
            PublicDelayLoadDLLs.Add("panda3ds_libretro.dll");
        }
    }
}
