using UnrealBuildTool;

public class UERingCore : ModuleRules
{
    public UERingCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Json",
                "JsonUtilities"
            }
        );
    }
}

