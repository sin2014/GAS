// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonPreLoadScreen.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "PreLoadScreenManager.h"

#define LOCTEXT_NAMESPACE "FCommonLoadingScreenModule"

/// <summary>
/// 本模块创建继承自 FPreLoadScreenBase 的 FCommonPreLoadScreen。
/// 该预加载屏幕在引擎启动期间显示一个 Slate 控件。
/// 如果希望改为播放启动影片，需要禁用 CommonStartupLoadingScreen 插件，
/// 因为预加载阶段只能选择显示控件或播放影片其中一种方式。
/// 启动影片可在“项目设置 -> Movies”中配置。
/// </summary>
/// <summary>
/// This module creates a FCommonPreloadScreen which extends from FPreLoadScreenBase
/// The screen shows an animated widget during the startup process of the engine.
/// If you want to show a Movie during the startup of the engine instead, then you have to disable the CommonStartupLoadingScreen plugin
///		This is because either an widget can be displayed during preload or a movie.
/// 
/// You can configure the startup movie in the Project Settings -> Movies 
/// </summary>
class FCommonStartupLoadingScreenModule : public IModuleInterface
{
public:

	/** IModuleInterface 接口实现。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	bool IsGameModule() const override;

private:
	void OnPreLoadScreenManagerCleanUp();

	TSharedPtr<FCommonPreLoadScreen> PreLoadingScreen;
};


// 非专用服务器环境下创建并初始化预加载屏幕；可渲染的非编辑器进程还会将其注册到预加载屏幕管理器。
void FCommonStartupLoadingScreenModule::StartupModule()
{
	// 专用服务器无需加载界面资源，但命令行工具仍需加载，以便烹饪流程发现这些资源。
	// No need to load these assets on dedicated servers.
	// 命令行工具仍会执行此路径，确保烹饪时能够收集相关资源。
	// Still want to load them in commandlets so cook catches them
	if (!IsRunningDedicatedServer())
	{
		PreLoadingScreen = MakeShared<FCommonPreLoadScreen>();
		PreLoadingScreen->Init();

		if (!GIsEditor && FApp::CanEverRender() && FPreLoadScreenManager::Get())
		{
			FPreLoadScreenManager::Get()->RegisterPreLoadScreen(PreLoadingScreen);
			FPreLoadScreenManager::Get()->OnPreLoadScreenManagerCleanUp.AddRaw(this, &FCommonStartupLoadingScreenModule::OnPreLoadScreenManagerCleanUp);
		}
	}
}

// 预加载屏幕管理器开始清理时释放控件资源，并进入模块关闭流程。
void FCommonStartupLoadingScreenModule::OnPreLoadScreenManagerCleanUp()
{
	// 管理器开始清理后，本模块也可以释放所持有的全部预加载资源。
	//Once the PreLoadScreenManager is cleaning up, we can get rid of all our resources too
	PreLoadingScreen.Reset();
	ShutdownModule();
}

// 模块关闭入口；实际预加载资源已由管理器清理回调释放。
void FCommonStartupLoadingScreenModule::ShutdownModule()
{

}

// 将该插件模块标记为游戏模块，使其遵循游戏模块的加载语义。
bool FCommonStartupLoadingScreenModule::IsGameModule() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
	
// 注册 CommonStartupLoadingScreen 模块及其预加载屏幕生命周期实现。
IMPLEMENT_MODULE(FCommonStartupLoadingScreenModule, CommonStartupLoadingScreen)
