// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreenManager.h"

#include "HAL/ThreadHeartBeat.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#include "LoadingProcessInterface.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#include "PreLoadScreen.h"
#include "PreLoadScreenManager.h"

#include "ShaderPipelineCache.h"
#include "CommonLoadingScreenSettings.h"

//@TODO: 当前在错误路径中直接使用该控件作为占位加载画面，后续应增加至少能够居中显示等基础布局的包装控件。
//@TODO: Used as the placeholder widget in error cases, should probably create a wrapper that at least centers it/etc...
#include "Widgets/Images/SThrobber.h"
#include "Blueprint/UserWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LoadingScreenManager)

// 声明并定义加载画面系统使用的日志分类。
DECLARE_LOG_CATEGORY_EXTERN(LogLoadingScreen, Log, All);
DEFINE_LOG_CATEGORY(LogLoadingScreen);

//@TODO: 确认 GetLocalPlayers() 是否确实可能包含空条目，并据此简化或保留防御性检查。
//@TODO: 在 PIE 模拟模式下测试，并确定是否以及应在多大程度上启用加载画面流程。
//@TODO: 允许 GameState、PlayerController 及其组件之外的 ILoadingProcessInterface 实现主动注册为加载观察者。
//@TODO: 在此处或通过 LoadingScreenVisibilityChanged 委托切换音乐设置。
//@TODO: 接入工作室分析事件，例如 PIE 完成加载和 PIE 启动耗时回归跟踪。
//@TODO: Why can GetLocalPlayers() have nullptr entries?  Can it really?
//@TODO: Test with PIE mode set to simulate and decide how much (if any) loading screen action should occur
//@TODO: Allow other things implementing ILoadingProcessInterface besides GameState/PlayerController (and owned components) to register as interested parties
//@TODO: ChangeMusicSettings (either here or using the LoadingScreenVisibilityChanged delegate)
//@TODO: Studio analytics (FireEvent_PIEFinishedLoading / tracking PIE startup time for regressions, either here or using the LoadingScreenVisibilityChanged delegate)

// 加载画面流程使用的 CSV 性能分析分类。
// Profiling category for loading screens
CSV_DEFINE_CATEGORY(LoadingScreen, true);

//////////////////////////////////////////////////////////////////////

// 若对象实现加载流程接口，则询问其是否要求显示加载画面，并强制要求返回非空诊断原因。
bool ILoadingProcessInterface::ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason)
{
	if (TestObject != nullptr)
	{
		if (ILoadingProcessInterface* LoadObserver = Cast<ILoadingProcessInterface>(TestObject))
		{
			FString ObserverReason;
			if (LoadObserver->ShouldShowLoadingScreen(/*out*/ ObserverReason))
			{
				if (ensureMsgf(!ObserverReason.IsEmpty(), TEXT("%s failed to set a reason why it wants to show the loading screen"), *GetPathNameSafe(TestObject)))
				{
					OutReason = ObserverReason;
				}
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////

namespace LoadingScreenCVars
{
	// 控制加载画面额外保持时间、逐帧原因日志和强制显示行为的控制台变量。
	// CVars
	static float HoldLoadingScreenAdditionalSecs = 2.0f;
	static FAutoConsoleVariableRef CVarHoldLoadingScreenUpAtLeastThisLongInSecs(
		TEXT("CommonLoadingScreen.HoldLoadingScreenAdditionalSecs"),
		HoldLoadingScreenAdditionalSecs,
		TEXT("How long to hold the loading screen up after other loading finishes (in seconds) to try to give texture streaming a chance to avoid blurriness"),
		ECVF_Default | ECVF_Preview);

	static bool LogLoadingScreenReasonEveryFrame = false;
	static FAutoConsoleVariableRef CVarLogLoadingScreenReasonEveryFrame(
		TEXT("CommonLoadingScreen.LogLoadingScreenReasonEveryFrame"),
		LogLoadingScreenReasonEveryFrame,
		TEXT("When true, the reason the loading screen is shown or hidden will be printed to the log every frame."),
		ECVF_Default);

	static bool ForceLoadingScreenVisible = false;
	static FAutoConsoleVariableRef CVarForceLoadingScreenVisible(
		TEXT("CommonLoadingScreen.AlwaysShow"),
		ForceLoadingScreenVisible,
		TEXT("Force the loading screen to show."),
		ECVF_Default);
}

//////////////////////////////////////////////////////////////////////
// FLoadingScreenInputPreProcessor

// 加载画面显示期间注册的输入预处理器。
// 它会截获所有输入，防止加载画面下方仍处于活动状态的菜单响应操作。
// Input processor to throw in when loading screen is shown
// This will capture any inputs, so active menus under the loading screen will not interact
class FLoadingScreenInputPreProcessor : public IInputProcessor
{
public:
	// 创建无额外状态的加载画面输入预处理器。
	FLoadingScreenInputPreProcessor() { }
	// 销毁输入预处理器；注册关系由加载管理器负责解除。
	virtual ~FLoadingScreenInputPreProcessor() { }

	// 编辑器中保留输入以便调试，游戏进程中则吞掉输入。
	bool CanEatInput() const
	{
		return !GIsEditor;
	}

	//~IInputProcess 接口
	//~IInputProcess interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override { }

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return CanEatInput(); }
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override { return CanEatInput(); }
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return CanEatInput(); }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override { return CanEatInput(); }
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) override { return CanEatInput(); }
	//~IInputProcess 接口结束
	//~End of IInputProcess interface
};

//////////////////////////////////////////////////////////////////////
// ULoadingScreenManager

// 绑定地图加载前后委托，使管理器能够立即响应所属 GameInstance 的地图切换状态。
void ULoadingScreenManager::Initialize(FSubsystemCollectionBase& Collection)
{
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	const UGameInstance* LocalGameInstance = GetGameInstance();
	check(LocalGameInstance);
}

// 停止输入阻断、移除加载控件、解除地图委托，并永久停止该子系统 Tick。
void ULoadingScreenManager::Deinitialize()
{
	StopBlockingInput();

	RemoveWidgetFromViewport();

	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	// 清理已经完成，不再允许该对象进入 Tick。
	// We are done, so do not attempt to tick us again
	SetTickableTickType(ETickableTickType::Never);
}

// 仅为客户端 GameInstance 创建加载画面子系统，专用服务器无需界面流程。
bool ULoadingScreenManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// 只有客户端需要加载画面。
	// Only clients have loading screens
	const UGameInstance* GameInstance = CastChecked<UGameInstance>(Outer);
	const bool bIsServerWorld = GameInstance->IsDedicatedServerInstance();	
	return !bIsServerWorld;
}

// 每帧更新加载画面状态，并递减下一次心跳原因日志的计时器。
void ULoadingScreenManager::Tick(float DeltaTime)
{
	UpdateLoadingScreen();

	TimeUntilNextLogHeartbeatSeconds = FMath::Max(TimeUntilNextLogHeartbeatSeconds - DeltaTime, 0.0);
}

// 模板对象永不 Tick，实例则根据 IsTickable 的运行条件决定。
ETickableTickType ULoadingScreenManager::GetTickableTickType() const
{
	if (IsTemplate())
	{
		return ETickableTickType::Never;
	}
	return ETickableTickType::Conditional;
}

// 仅当所属 GameInstance 存在有效游戏视口客户端时参与 Tick。
bool ULoadingScreenManager::IsTickable() const
{
	// 没有游戏视口客户端时不执行 Tick，这可覆盖 ShouldCreateSubsystem 未能排除的情况。
	// Don't tick if we don't have a game viewport client, this catches cases that ShouldCreateSubsystem does not
	UGameInstance* GameInstance = GetGameInstance();
	return (GameInstance && GameInstance->GetGameViewportClient());
}

// 返回用于 Tick 性能统计的循环计数标识。
TStatId ULoadingScreenManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULoadingScreenManager, STATGROUP_Tickables);
}

// 返回所属 GameInstance 当前关联的 World，供 Tick 系统确定世界上下文。
UWorld* ULoadingScreenManager::GetTickableGameObjectWorld() const
{
	return GetGameInstance()->GetWorld();
}

// 以弱接口引用记录外部加载处理器，使其可以请求保持加载画面而不会被管理器强持有。
void ULoadingScreenManager::RegisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface)
{
	ExternalLoadingProcessors.Add(Interface.GetObject());
}

// 从外部加载处理器列表移除指定接口对象。
void ULoadingScreenManager::UnregisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface)
{
	ExternalLoadingProcessors.Remove(Interface.GetObject());
}

// 所属 GameInstance 开始加载地图时设置加载标志，并在引擎已初始化后立即刷新加载画面。
void ULoadingScreenManager::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance == GetGameInstance())
	{
		bCurrentlyInLoadMap = true;

		// 引擎已完成初始化时立即刷新加载画面，避免等待下一帧 Tick。
		// Update the loading screen immediately if the engine is initialized
		if (GEngine->IsInitialized())
		{
			UpdateLoadingScreen();
		}
	}
}

// 所属 GameInstance 的新 World 完成加载后清除地图加载标志。
void ULoadingScreenManager::HandlePostLoadMap(UWorld* World)
{
	if ((World != nullptr) && (World->GetGameInstance() == GetGameInstance()))
	{
		bCurrentlyInLoadMap = false;
	}
}

// 根据当前加载需求显示或隐藏画面，同时维护卡死检测检查点和周期性原因日志。
void ULoadingScreenManager::UpdateLoadingScreen()
{
	bool bLogLoadingScreenStatus = LoadingScreenCVars::LogLoadingScreenReasonEveryFrame;

	if (ShouldShowLoadingScreen())
	{
		const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();
		
		// 若在配置时限内未结束该检查点，则触发卡死检测以定位加载停滞位置。
		// If we don't make it to the specified checkpoint in the given time will trigger the hang detector so we can better determine where progress stalled.
 		FThreadHeartBeat::Get().MonitorCheckpointStart(GetFName(), Settings->LoadingScreenHeartbeatHangDuration);

		ShowLoadingScreen();

 		if ((Settings->LogLoadingScreenHeartbeatInterval > 0.0f) && (TimeUntilNextLogHeartbeatSeconds <= 0.0))
 		{
			bLogLoadingScreenStatus = true;
 			TimeUntilNextLogHeartbeatSeconds = Settings->LogLoadingScreenHeartbeatInterval;
 		}
	}
	else
	{
		HideLoadingScreen();
 
 		FThreadHeartBeat::Get().MonitorCheckpointEnd(GetFName());
	}

	if (bLogLoadingScreenStatus)
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("Loading screen showing: %d. Reason: %s"), bCurrentlyShowingLoadingScreen ? 1 : 0, *DebugReasonForShowingOrHidingLoadingScreen);
	}
}

// 依次检查强制开关、World/旅行状态、游戏状态、外部处理器和本地玩家，返回首个要求显示加载画面的原因。
bool ULoadingScreenManager::CheckForAnyNeedToShowLoadingScreen()
{
	// 先设置未知原因，防止后续新增分支忘记写入明确诊断信息。
	// Start out with 'unknown' reason in case someone forgets to put a reason when changing this in the future.
	DebugReasonForShowingOrHidingLoadingScreen = TEXT("Reason for Showing/Hiding LoadingScreen is unknown!");

	const UGameInstance* LocalGameInstance = GetGameInstance();

	if (LoadingScreenCVars::ForceLoadingScreenVisible)
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("CommonLoadingScreen.AlwaysShow is true"));
		return true;
	}

	const FWorldContext* Context = LocalGameInstance->GetWorldContext();
	if (Context == nullptr)
	{
		// 当前没有 WorldContext，游戏尚不能安全呈现，继续显示加载画面。
		// We don't have a world context right now... better show a loading screen
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("The game instance has a null WorldContext"));
		return true;
	}

	UWorld* World = Context->World();
	if (World == nullptr)
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("We have no world (FWorldContext's World() is null)"));
		return true;
	}

	AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	if (GameState == nullptr)
	{
		// GameState 尚未复制到客户端，继续显示加载画面。
		// The game state has not yet replicated.
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("GameState hasn't yet replicated (it's null)"));
		return true;
	}

	if (bCurrentlyInLoadMap)
	{
		// 处于同步 LoadMap 流程时保持加载画面。
		// Show a loading screen if we are in LoadMap
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("bCurrentlyInLoadMap is true"));
		return true;
	}

	if (!Context->TravelURL.IsEmpty())
	{
		// 存在待处理旅行 URL 时保持加载画面。
		// Show a loading screen when pending travel
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("We have pending travel (the TravelURL is not empty)"));
		return true;
	}

	if (Context->PendingNetGame != nullptr)
	{
		// PendingNetGame 存在表示正在连接另一台服务器。
		// Connecting to another server
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("We are connecting to another server (PendingNetGame != nullptr)"));
		return true;
	}

	if (!World->HasBegunPlay())
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("World hasn't begun play"));
		return true;
	}

	if (World->IsInSeamlessTravel())
	{
		// 无缝旅行期间保持加载画面。
		// Show a loading screen during seamless travel
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("We are in seamless travel"));
		return true;
	}

	// 询问 GameState 自身是否要求显示加载画面。
	// Ask the game state if it needs a loading screen	
	if (ILoadingProcessInterface::ShouldShowLoadingScreen(GameState, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
	{
		return true;
	}

	// 询问 GameState 的各组件是否要求显示加载画面。
	// Ask any game state components if they need a loading screen
	for (UActorComponent* TestComponent : GameState->GetComponents())
	{
		if (ILoadingProcessInterface::ShouldShowLoadingScreen(TestComponent, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
		{
			return true;
		}
	}

	// 询问游戏代码注册的外部加载处理器；这些 Actor 或组件可在资源流送等工作完成前要求保持画面。
	// Ask any of the external loading processors that may have been registered.  These might be actors or components
	// that were registered by game code to tell us to keep the loading screen up while perhaps something finishes
	// streaming in.
	for (const TWeakInterfacePtr<ILoadingProcessInterface>& Processor : ExternalLoadingProcessors)
	{
		if (ILoadingProcessInterface::ShouldShowLoadingScreen(Processor.GetObject(), /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
		{
			return true;
		}
	}

	// 检查每个本地玩家的 PlayerController 及其组件。
	// Check each local player
	bool bFoundAnyLocalPC = false;
	bool bMissingAnyLocalPC = false;

	for (ULocalPlayer* LP : LocalGameInstance->GetLocalPlayers())
	{
		if (LP != nullptr)
		{
			if (APlayerController* PC = LP->PlayerController)
			{
				bFoundAnyLocalPC = true;

				// 询问 PlayerController 自身是否要求显示加载画面。
				// Ask the PC itself if it needs a loading screen
				if (ILoadingProcessInterface::ShouldShowLoadingScreen(PC, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
				{
					return true;
				}

				// 询问 PlayerController 的各组件是否要求显示加载画面。
				// Ask any PC components if they need a loading screen
				for (UActorComponent* TestComponent : PC->GetComponents())
				{
					if (ILoadingProcessInterface::ShouldShowLoadingScreen(TestComponent, /*out*/ DebugReasonForShowingOrHidingLoadingScreen))
					{
						return true;
					}
				}
			}
			else
			{
				bMissingAnyLocalPC = true;
			}
		}
	}

	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();
	const bool bIsInSplitscreen = GameViewportClient->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None;

	// 分屏模式要求所有本地玩家的 PlayerController 均已创建。
	// In splitscreen we need all player controllers to be present
	if (bIsInSplitscreen && bMissingAnyLocalPC)
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("At least one missing local player controller in splitscreen"));
		return true;
	}

	// 非分屏模式至少需要一个本地 PlayerController 已创建。
	// And in non-splitscreen we need at least one player controller to be present
	if (!bIsInSplitscreen && !bFoundAnyLocalPC)
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("Need at least one local player controller"));
		return true;
	}

	// 所有检查均通过，没有对象继续要求显示加载画面。
	// Victory! The loading screen can go away now
	DebugReasonForShowingOrHidingLoadingScreen = TEXT("(nothing wants to show it anymore)");
	return false;
}

// 应用命令行覆盖和视口约束，并在真实加载结束后按配置额外保持画面以等待纹理流送。
bool ULoadingScreenManager::ShouldShowLoadingScreen()
{
	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();

	// 检查可强制关闭加载画面的非 Shipping 调试命令行参数。
	// Check debugging commands that force the state one way or another
#if !UE_BUILD_SHIPPING
	static bool bCmdLineNoLoadingScreen = FParse::Param(FCommandLine::Get(), TEXT("NoLoadingScreen"));
	if (bCmdLineNoLoadingScreen)
	{
		DebugReasonForShowingOrHidingLoadingScreen = FString(TEXT("CommandLine has 'NoLoadingScreen'"));
		return false;
	}
#endif

	// 没有游戏视口时无法显示加载画面。
	// Can't show a loading screen if there's no game viewport
	UGameInstance* LocalGameInstance = GetGameInstance();
	if (LocalGameInstance->GetGameViewportClient() == nullptr)
	{
		return false;
	}

	// 检查当前是否存在真实的加载画面需求。
	// Check for a need to show the loading screen
	const bool bNeedToShowLoadingScreen = CheckForAnyNeedToShowLoadingScreen();

	// 根据配置决定是否在真实需求结束后继续短暂保持加载画面。
	// Keep the loading screen up a bit longer if desired
	bool bWantToForceShowLoadingScreen = false;
	if (bNeedToShowLoadingScreen)
	{
		// 仍有真实需求时重置最近一次请求关闭的时间。
		// Still need to show it
		TimeLoadingScreenLastDismissed = -1.0;
	}
	else
	{
		// 已无强制需求，但可能仍需额外显示一段时间以遮挡流送过程。
		// Don't *need* to show the screen anymore, but might still want to for a bit
		const double CurrentTime = FPlatformTime::Seconds();
		const bool bCanHoldLoadingScreen = (!GIsEditor || Settings->HoldLoadingScreenAdditionalSecsEvenInEditor);
		const double HoldLoadingScreenAdditionalSecs = bCanHoldLoadingScreen ? LoadingScreenCVars::HoldLoadingScreenAdditionalSecs : 0.0;

		if (TimeLoadingScreenLastDismissed < 0.0)
		{
			TimeLoadingScreenLastDismissed = CurrentTime;
		}
		const double TimeSinceScreenDismissed = CurrentTime - TimeLoadingScreenLastDismissed;

		// 额外保持配置的秒数以遮挡资源流送。
		// hold for an extra X seconds, to cover up streaming
		if ((HoldLoadingScreenAdditionalSecs > 0.0) && (TimeSinceScreenDismissed < HoldLoadingScreenAdditionalSecs))
		{
			// 此阶段重新启用世界渲染，确保纹理流送系统能够实际请求所需资源。
			//@TODO: 若 bNeedToShowLoadingScreen 在延时窗口内重新变为 true，当前实现不会再次关闭世界渲染。
			// Make sure we're rendering the world at this point, so that textures will actually stream in
			//@TODO: If bNeedToShowLoadingScreen bounces back true during this window, we won't turn this off again...
			UGameViewportClient* GameViewportClient = GetGameInstance()->GetGameViewportClient();
			GameViewportClient->bDisableWorldRendering = false;

			DebugReasonForShowingOrHidingLoadingScreen = FString::Printf(TEXT("Keeping loading screen up for an additional %.2f seconds to allow texture streaming"), HoldLoadingScreenAdditionalSecs);
			bWantToForceShowLoadingScreen = true;
		}
	}

	return bNeedToShowLoadingScreen || bWantToForceShowLoadingScreen;
}

// 查询预加载屏幕管理器，判断引擎启动阶段的初始加载画面是否仍然有效。
bool ULoadingScreenManager::IsShowingInitialLoadingScreen() const
{
	FPreLoadScreenManager* PreLoadScreenManager = FPreLoadScreenManager::Get();
	return (PreLoadScreenManager != nullptr) && PreLoadScreenManager->HasValidActivePreLoadScreen();
}

// 首次进入显示状态时记录时间；若引擎预加载画面已结束，则创建全局或逐玩家控件、阻断输入并切换性能策略。
void ULoadingScreenManager::ShowLoadingScreen()
{
	if (bCurrentlyShowingLoadingScreen)
	{
		return;
	}

	// 引擎自己的启动加载画面仍处于活动状态时，不叠加游戏加载画面。
	// Unable to show loading screen if the engine is still loading with its loading screen.
	if (FPreLoadScreenManager::Get() && FPreLoadScreenManager::Get()->HasActivePreLoadScreenType(EPreLoadScreenTypes::EngineLoadingScreen))
	{
		return;
	}

	TimeLoadingScreenShown = FPlatformTime::Seconds();

	bCurrentlyShowingLoadingScreen = true;

	CSV_EVENT(LoadingScreen, TEXT("Show"));

	const UCommonLoadingScreenSettings* Settings = GetDefault<UCommonLoadingScreenSettings>();

	if (IsShowingInitialLoadingScreen())
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("Showing loading screen when 'IsShowingInitialLoadingScreen()' is true."));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);
	}
	else
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("Showing loading screen when 'IsShowingInitialLoadingScreen()' is false."));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);

		UGameInstance* LocalGameInstance = GetGameInstance();

		// 加载画面显示期间吞掉游戏输入。
		// Eat input while the loading screen is displayed
		StartBlockingInput();

		LoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ true);

		// 加载配置的控件类，并根据是否启用逐玩家分屏渲染创建对应实例。
		// Create the loading screen widget
		TSubclassOf<UUserWidget> LoadingScreenWidgetClass = Settings->LoadingScreenWidget.TryLoadClass<UUserWidget>();
		UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

		if (GameViewportClient->bEnablePlayersSplitRT)
		{
			for (ULocalPlayer* Player : LocalGameInstance->GetLocalPlayers())
			{
				if (Player)
				{
					TSharedPtr<SWidget> PlayerWidget;

					if (UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, LoadingScreenWidgetClass, NAME_None))
					{
						PlayerWidget = UserWidget->TakeWidget();
					}
					else
					{
						UE_LOG(LogLoadingScreen, Error, TEXT("Failed to load the loading screen widget %s, falling back to placeholder."), *Settings->LoadingScreenWidget.ToString());
						PlayerWidget = SNew(SThrobber);
					}

					PlayersLoadingScreenWidgets.Add(Player, PlayerWidget);
					GameViewportClient->AddViewportWidgetForPlayer(Player, PlayerWidget.ToSharedRef(), Settings->LoadingScreenZOrder);
				}
			}
		}
		else
		{
			if (UUserWidget* UserWidget = UUserWidget::CreateWidgetInstance(*LocalGameInstance, LoadingScreenWidgetClass, NAME_None))
			{
				LoadingScreenWidget = UserWidget->TakeWidget();
			}
			else
			{
				UE_LOG(LogLoadingScreen, Error, TEXT("Failed to load the loading screen widget %s, falling back to placeholder."), *Settings->LoadingScreenWidget.ToString());
				LoadingScreenWidget = SNew(SThrobber);
			}

			// 以较高 ZOrder 加入视口，确保加载画面覆盖大多数其他控件。
			// Add to the viewport at a high ZOrder to make sure it is on top of most things
			GameViewportClient->AddViewportWidgetContent(LoadingScreenWidget.ToSharedRef(), Settings->LoadingScreenZOrder);
		}

		ChangePerformanceSettings(/*bEnableLoadingScreen=*/ true);

		if (!GIsEditor || Settings->ForceTickLoadingScreenEvenInEditor)
		{
			// 主动 Tick 一次 Slate，确保加载画面立即提交显示。
			// Tick Slate to make sure the loading screen is displayed immediately
			FSlateApplication::Get().Tick();
		}
	}
}

// 退出显示状态并恢复输入；由游戏加载画面接管时还会执行 GC、移除控件、恢复性能设置并广播不可见通知。
void ULoadingScreenManager::HideLoadingScreen()
{
	if (!bCurrentlyShowingLoadingScreen)
	{
		return;
	}

	StopBlockingInput();

	if (IsShowingInitialLoadingScreen())
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("Hiding loading screen when 'IsShowingInitialLoadingScreen()' is true."));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);
	}
	else
	{
		UE_LOG(LogLoadingScreen, Log, TEXT("Hiding loading screen when 'IsShowingInitialLoadingScreen()' is false."));
		UE_LOG(LogLoadingScreen, Log, TEXT("%s"), *DebugReasonForShowingOrHidingLoadingScreen);

		UE_LOG(LogLoadingScreen, Log, TEXT("Garbage Collecting before dropping load screen"));
		GEngine->ForceGarbageCollection(true);

		RemoveWidgetFromViewport();
	
		ChangePerformanceSettings(/*bEnableLoadingScreen=*/ false);

		// 通知观察者加载画面已经结束。
		// Let observers know that the loading screen is done
		LoadingScreenVisibilityChanged.Broadcast(/*bIsVisible=*/ false);
	}

	CSV_EVENT(LoadingScreen, TEXT("Hide"));

	const double LoadingScreenDuration = FPlatformTime::Seconds() - TimeLoadingScreenShown;
	UE_LOG(LogLoadingScreen, Log, TEXT("LoadingScreen was visible for %.2fs"), LoadingScreenDuration);

	bCurrentlyShowingLoadingScreen = false;
}

// 根据逐玩家分屏渲染模式移除各玩家控件或全局控件，并清空持有的 Slate 引用。
void ULoadingScreenManager::RemoveWidgetFromViewport()
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	bool bUseSplitRT = false;
	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

	if (GameViewportClient && GameViewportClient->bEnablePlayersSplitRT)
	{
		bUseSplitRT = true;
	}

	if (bUseSplitRT)
	{
		if (GameViewportClient)
		{
			for (auto& Pair : PlayersLoadingScreenWidgets)
			{
				if (Pair.Key.IsValid() && Pair.Value.IsValid())
				{
					GameViewportClient->RemoveViewportWidgetForPlayer(Pair.Key.Get(), Pair.Value.ToSharedRef());
				}
			}
		}
		PlayersLoadingScreenWidgets.Empty();
	}
	else
	{
		if (LoadingScreenWidget.IsValid())
		{
			if (GameViewportClient)
			{
				GameViewportClient->RemoveViewportWidgetContent(LoadingScreenWidget.ToSharedRef());
			}
			LoadingScreenWidget.Reset();
		}
	}
}

// 创建并以最高优先级注册输入预处理器，避免加载期间底层界面和游戏响应操作。
void ULoadingScreenManager::StartBlockingInput()
{
	if (!InputPreProcessor.IsValid())
	{
		InputPreProcessor = MakeShareable<FLoadingScreenInputPreProcessor>(new FLoadingScreenInputPreProcessor());
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreProcessor, 0);
	}
}

// 注销并释放当前输入预处理器，恢复正常输入分发。
void ULoadingScreenManager::StopBlockingInput()
{
	if (InputPreProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreProcessor);
		InputPreProcessor.Reset();
	}
}

// 在加载期切换着色器批处理、世界渲染、关卡流送优先级及卡死/卡顿检测策略，并在结束时恢复。
void ULoadingScreenManager::ChangePerformanceSettings(bool bEnabingLoadingScreen)
{
	UGameInstance* LocalGameInstance = GetGameInstance();
	UGameViewportClient* GameViewportClient = LocalGameInstance->GetGameViewportClient();

	FShaderPipelineCache::SetBatchMode(bEnabingLoadingScreen ? FShaderPipelineCache::BatchMode::Fast : FShaderPipelineCache::BatchMode::Background);

	// 加载期间无需绘制 3D 世界。
	// Don't bother drawing the 3D world while we're loading
	GameViewportClient->bDisableWorldRendering = bEnabingLoadingScreen;

	// 加载画面显示时提高本地关卡流送优先级。
	// Make sure to prioritize streaming in levels if the loading screen is up
	if (UWorld* ViewportWorld = GameViewportClient->GetWorld())
	{
		if (AWorldSettings* WorldSettings = ViewportWorld->GetWorldSettings(false, false))
		{
			WorldSettings->bHighPriorityLoadingLocal = bEnabingLoadingScreen;
		}
	}

	if (bEnabingLoadingScreen)
	{
		// 加载画面可见时应用配置的卡死检测超时倍率。
		// Set a new hang detector timeout multiplier when the loading screen is visible.
		double HangDurationMultiplier;
		if (!GConfig || !GConfig->GetDouble(TEXT("Core.System"), TEXT("LoadingScreenHangDurationMultiplier"), /*out*/ HangDurationMultiplier, GEngineIni))
		{
			HangDurationMultiplier = 1.0;
		}
		FThreadHeartBeat::Get().SetDurationMultiplier(HangDurationMultiplier);

		// 加载画面显示期间暂停报告游戏线程卡顿。
		// Do not report hitches while the loading screen is up
		FGameThreadHitchHeartBeat::Get().SuspendHeartBeat();
	}
	else
	{
		// 隐藏加载画面后恢复默认卡死检测超时倍率。
		// Restore the hang detector timeout when we hide the loading screen
		FThreadHeartBeat::Get().SetDurationMultiplier(1.0);

		// 加载画面结束后恢复游戏线程卡顿报告。
		// Resume reporting hitches now that the loading screen is down
		FGameThreadHitchHeartBeat::Get().ResumeHeartBeat();
	}
}

