// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PocketWorlds : ModuleRules
{
	// 声明 PocketWorlds 运行时模块的公开依赖；场景捕获和关卡流送均由 Engine 提供。
	public PocketWorlds(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
            }
        );
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// 在此添加需要静态链接的私有模块依赖；当前无需额外依赖。
				// ... add private dependencies that you statically link with here ...
			}
		);
	}
}
