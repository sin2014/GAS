using UnrealBuildTool;

public class BlueprintReader : ModuleRules
{
    public BlueprintReader(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "BlueprintGraph",
            "UnrealEd",
            "Json",
            "JsonUtilities"
        });
    }
}
