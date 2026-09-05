using UnrealBuildTool;

public class UERingEditor : ModuleRules
{
    public UERingEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UERingCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AssetRegistry",
                "BlueprintGraph",
                "ContentBrowser",
                "ControlRig",
                "ControlRigDeveloper",
                "DeveloperSettings",
                "Json",
                "JsonUtilities",
                "EnhancedInput",
                "LevelSequence",
                "MovieScene",
                "MovieSceneTracks",
                "ModelContextProtocol",
                "Paper2D",
                "PhysicsCore",
                "PlatformCryptoContext",
                "Projects",
                "RigVM",
                "RigVMDeveloper",
                "Slate",
                "SlateCore",
                "SQLiteCore",
                "ToolMenus",
                "UMG",
                "UMGEditor",
                "UnrealEd"
            }
        );
    }
}

