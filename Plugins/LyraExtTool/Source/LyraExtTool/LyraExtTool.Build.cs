// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LyraExtTool : ModuleRules
{
	public LyraExtTool(ReadOnlyTargetRules Target) : base(Target)
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
				// 在此添加仅供 LyraExtTool 实现使用的头文件搜索路径。
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// 在此添加需要向依赖者公开并静态链接的其他模块。
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// 在此添加仅供 LyraExtTool 内部静态链接的模块依赖。
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此添加由 LyraExtTool 在运行时动态加载的模块。
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
