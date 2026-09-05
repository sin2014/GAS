// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
// 引入 System.IO 以便构建规则使用 Path 路径 API。
using System.IO; // for Path

public class ModularGameplayActors : ModuleRules
{
	public ModularGameplayActors(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// 在此添加模块对外公开所需的头文件搜索路径。
				// ... add public include paths required here ...
			}
		);


		PrivateIncludePaths.AddRange(
			new string[] {
				// 在此添加仅供模块内部使用的头文件搜索路径。
				// ... add other private include paths required here ...
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ModularGameplay",
				"AIModule",
				// 在此添加需要向依赖者公开并静态链接的其他模块。
				// ... add other public dependencies that you statically link with here ...
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// 在此添加只供当前模块实现使用的静态依赖。
				// ... add private dependencies that you statically link with here ...	
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此添加由当前模块在运行时动态加载的模块。
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
