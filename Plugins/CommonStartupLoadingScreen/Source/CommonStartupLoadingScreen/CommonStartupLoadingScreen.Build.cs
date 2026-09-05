// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CommonStartupLoadingScreen : ModuleRules
{
	public CommonStartupLoadingScreen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// 在此添加本模块所需的公共头文件搜索路径。
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// 在此添加本模块所需的其他私有头文件搜索路径。
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// 在此添加需要静态链接的其他公共依赖模块。
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
				"MoviePlayer",
				"PreLoadScreen",
				"DeveloperSettings"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此添加由本模块动态加载的模块。
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
