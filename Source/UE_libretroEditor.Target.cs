using UnrealBuildTool;
using System.Collections.Generic;

public class UE_libretroEditorTarget : TargetRules
{
    public UE_libretroEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UE_libretro");
    }
}
