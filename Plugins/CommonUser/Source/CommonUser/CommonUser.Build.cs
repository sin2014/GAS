// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CommonUser : ModuleRules
{
	public CommonUser(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		bool bUseOnlineSubsystemV1 = true;

		PublicIncludePaths.AddRange(
			new string[] {
				// 在此添加 CommonUser 对外公开所需的头文件搜索路径。
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// 在此添加仅供 CommonUser 实现使用的头文件搜索路径。
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreOnline",
				"GameplayTags",
				"OnlineSubsystemUtils",
				// 在此添加需要向 CommonUser 依赖者公开并静态链接的其他模块。
				// ... add other public dependencies that you statically link with here ...
			}
			);

		if (bUseOnlineSubsystemV1)
		{
			PublicDependencyModuleNames.Add("OnlineSubsystem");
		}
		else
		{
			PublicDependencyModuleNames.Add("OnlineServicesInterface");
		}

		PublicDefinitions.Add("COMMONUSER_OSSV1=" + (bUseOnlineSubsystemV1 ? "1" : "0"));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreOnline",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ApplicationCore",
				"InputCore",
				// 在此添加仅供 CommonUser 内部静态链接的模块依赖。
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// 在此添加由 CommonUser 在运行时动态加载的模块。
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
