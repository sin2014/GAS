// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameSubtitles : ModuleRules
{
	public GameSubtitles(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// 在此添加需要向依赖者公开并静态链接的其他模块。
				// ... add other public dependencies that you statically link with here ...
				"Overlay",
                "UMG",
				"MediaAssets",
				"MediaUtils",
				"GameplayTags"
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "UMG",
            }
        );
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// 在此添加仅供 GameSubtitles 实现使用的静态依赖。
				// ... add private dependencies that you statically link with here ...
			}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此添加由 GameSubtitles 在运行时动态加载的模块。
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
