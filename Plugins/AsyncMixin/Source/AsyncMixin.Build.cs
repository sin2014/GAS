// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AsyncMixin : ModuleRules
{
	// 声明 AsyncMixin 运行时模块公开依赖的 Core、CoreUObject 与 Engine 模块。
	public AsyncMixin(ReadOnlyTargetRules Target) : base(Target)
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
			}
		);
	}
}
