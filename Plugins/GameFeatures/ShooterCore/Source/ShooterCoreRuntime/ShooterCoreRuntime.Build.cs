// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShooterCoreRuntime : ModuleRules
{
	// 配置 ShooterCore 运行时模块所需的 Lyra、GAS、CommonUI、输入、消息和数据注册表依赖。
	public ShooterCoreRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// 在此添加额外的公开头文件搜索路径；当前使用模块默认路径。
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// 在此添加额外的私有头文件搜索路径；当前使用模块默认路径。
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LyraGame",
				"ModularGameplay",
				"CommonGame",
				// 在此添加需要静态链接并向使用方公开的其他依赖。
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
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"GameplayMessageRuntime",
				"CommonUI",
				"UMG",
				"DataRegistry",
				"AsyncMixin",
				"EnhancedInput",
				"GameSubtitles",
				"DeveloperSettings",
				"AIModule"
				// 在此添加仅供模块实现静态链接的其他私有依赖。
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此添加运行时动态加载的模块；当前没有此类依赖。
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
