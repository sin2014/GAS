using UnrealBuildTool;

public class StormHeroesAssetImporter : ModuleRules
{
    public StormHeroesAssetImporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "AssetTools",
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd"
        });
    }
}
