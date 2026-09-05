// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UIExtension : ModuleRules
{
	public UIExtension(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"SlateCore",
				"Slate",
				"UMG",
				"CommonUI",
				"CommonGame",
				"GameplayTags"
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
            }
        );
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// 在此添加需要静态链接的私有依赖模块。
				// ... add private dependencies that you statically link with here ...
			}
		);
	}
}
