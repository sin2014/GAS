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
            string FceummCorePath = Path.Combine(CoreDir, "fceumm_libretro.dll");
            string DesmumeCorePath = Path.Combine(CoreDir, "desmume_libretro.dll");
            string AzaharCorePath = Path.Combine(CoreDir, "azahar_libretro.dll");
            RuntimeDependencies.Add(FceummCorePath);
            RuntimeDependencies.Add(DesmumeCorePath);
            RuntimeDependencies.Add(AzaharCorePath);
            PublicSystemLibraries.Add("opengl32.lib");
            PublicDelayLoadDLLs.Add("fceumm_libretro.dll");
            PublicDelayLoadDLLs.Add("desmume_libretro.dll");
            PublicDelayLoadDLLs.Add("azahar_libretro.dll");
        }
    }
}
