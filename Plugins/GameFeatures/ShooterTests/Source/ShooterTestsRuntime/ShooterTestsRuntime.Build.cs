// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShooterTestsRuntime : ModuleRules
{
	// 配置 ShooterTests 的 Lyra、GAS、CQTest、Enhanced Input 和 AsyncMessageSystem 依赖，并在 Editor 构建中加入 PIE 测试模块。
	public ShooterTestsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LyraGame",
				"GameplayTags",
				"GameplayAbilities",
				"ModularGameplay",
				"AsyncMessageSystem"
				// 在此添加需要静态链接并向使用方公开的其他依赖。
				// ... add other public dependencies that you statically link with here ...
			}
		);
		
		//PrivateIncludePathModuleNames.AddRange(new string[]{"AsyncMessageSystem"});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"InputCore",
				"EnhancedInput",
				"CQTest",
				"CQTestEnhancedInput",
				// 在此添加仅供测试实现静态链接的其他私有依赖。
				// ... add private dependencies that you statically link with here ...	
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"EngineSettings",
					"LevelEditor",
					"UnrealEd"
			});
		}
	}
}
