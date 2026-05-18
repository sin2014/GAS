// ZYZ

using UnrealBuildTool;
using System.Collections.Generic;

public class GASTarget : TargetRules
{
	public GASTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "GAS" } );
	}
}
