// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "UObject/WeakInterfacePtr.h"

#include "LoadingScreenManager.generated.h"

#define UE_API COMMONLOADINGSCREEN_API

template <typename InterfaceType> class TScriptInterface;

class FSubsystemCollectionBase;
class IInputProcessor;
class ILoadingProcessInterface;
class SWidget;
class ULocalPlayer;
class UObject;
class UWorld;
struct FFrame;
struct FWorldContext;

/** 负责判断加载状态，并管理加载画面的显示、隐藏、输入阻断和相关性能设置。 */
/**
 * Handles showing/hiding the loading screen
 */
UCLASS(MinimalAPI)
class ULoadingScreenManager : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	//~USubsystem 接口
	//~USubsystem interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~USubsystem 接口结束
	//~End of USubsystem interface

	//~FTickableObjectBase 接口
	//~FTickableObjectBase interface
	UE_API virtual void Tick(float DeltaTime) override;
	UE_API virtual ETickableTickType GetTickableTickType() const override;
	UE_API virtual bool IsTickable() const override;
	UE_API virtual TStatId GetStatId() const override;
	UE_API virtual UWorld* GetTickableGameObjectWorld() const override;
	//~FTickableObjectBase 接口结束
	//~End of FTickableObjectBase interface

	UFUNCTION(BlueprintCallable, Category=LoadingScreen)
	FString GetDebugReasonForShowingOrHidingLoadingScreen() const
	{
		return DebugReasonForShowingOrHidingLoadingScreen;
	}

	/** 返回当前是否正在显示加载画面。 */
	/** Returns True when the loading screen is currently being shown */
	bool GetLoadingScreenDisplayStatus() const
	{
		return bCurrentlyShowingLoadingScreen;
	}

	/** 加载画面可见性变化时广播。 */
	/** Called when the loading screen visibility changes  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLoadingScreenVisibilityChangedDelegate, bool);
	FORCEINLINE FOnLoadingScreenVisibilityChangedDelegate& OnLoadingScreenVisibilityChangedDelegate() { return LoadingScreenVisibilityChanged; }

	UE_API void RegisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface);
	UE_API void UnregisterLoadingProcessor(TScriptInterface<ILoadingProcessInterface> Interface);
	
private:
	UE_API void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	UE_API void HandlePostLoadMap(UWorld* World);

	/** 每帧重新判断并显示或隐藏加载画面。 */
	/** Determines if we should show or hide the loading screen. Called every frame. */
	UE_API void UpdateLoadingScreen();

	/** 检查运行状态和所有加载处理器，返回当前是否必须显示加载画面。 */
	/** Returns true if we need to be showing the loading screen. */
	UE_API bool CheckForAnyNeedToShowLoadingScreen();

	/** 返回是否希望继续显示加载画面，包括真实加载需求和调试或延时强制显示。 */
	/** Returns true if we want to be showing the loading screen (if we need to or are artificially forcing it on for other reasons). */
	UE_API bool ShouldShowLoadingScreen();

	/** 返回引擎的初始预加载画面是否仍在接管显示流程。 */
	/** Returns true if we are in the initial loading flow before this screen should be used */
	UE_API bool IsShowingInitialLoadingScreen() const;

	/** 创建加载控件并将其添加到视口，同时应用输入阻断和加载期性能设置。 */
	/** Shows the loading screen. Sets up the loading screen widget on the viewport */
	UE_API void ShowLoadingScreen();

	/** 隐藏加载画面、移除控件并恢复输入与性能设置。 */
	/** Hides the loading screen. The loading screen widget will be destroyed */
	UE_API void HideLoadingScreen();

	/** 从全局视口或各玩家的分屏视口中移除加载控件。 */
	/** Removes the widget from the viewport */
	UE_API void RemoveWidgetFromViewport();

	/** 注册输入预处理器，在加载画面可见时阻止输入传入游戏。 */
	/** Prevents input from being used in-game while the loading screen is visible */
	UE_API void StartBlockingInput();

	/** 如果正在阻断输入，则撤销预处理器并恢复游戏输入。 */
	/** Resumes in-game input, if blocking */
	UE_API void StopBlockingInput();

	UE_API void ChangePerformanceSettings(bool bEnabingLoadingScreen);

private:
	/** 加载画面可见性变化时广播的委托。 */
	/** Delegate broadcast when the loading screen visibility changes */
	FOnLoadingScreenVisibilityChangedDelegate LoadingScreenVisibilityChanged;

	/** 当前显示的全局加载控件，以及分屏渲染时按本地玩家保存的控件。 */
	/** A reference to the loading screen widget we are displaying (if any) */
	TSharedPtr<SWidget> LoadingScreenWidget;
	TMap<TWeakObjectPtr<ULocalPlayer>, TSharedPtr<SWidget>> PlayersLoadingScreenWidgets;

	/** 加载画面显示期间吞掉输入事件的预处理器。 */
	/** Input processor to eat all input while the loading screen is shown */
	TSharedPtr<IInputProcessor> InputPreProcessor;

	/** 由游戏代码注册、可要求延长加载画面的外部对象或组件。 */
	/** External loading processors, components maybe actors that delay the loading. */
	TArray<TWeakInterfacePtr<ILoadingProcessInterface>> ExternalLoadingProcessors;

	/** 当前显示或隐藏加载画面的诊断原因。 */
	/** The reason why the loading screen is up (or not) */
	FString DebugReasonForShowingOrHidingLoadingScreen;

	/** 本次开始显示加载画面的平台时间。 */
	/** The time when we started showing the loading screen */
	double TimeLoadingScreenShown = 0.0;

	/** 最近一次不再需要加载画面的时间；最短显示时长可能仍要求画面保持可见。 */
	/** The time the loading screen most recently wanted to be dismissed (might still be up due to a min display duration requirement) **/
	double TimeLoadingScreenLastDismissed = -1.0;

	/** 距离下次输出“加载画面仍未关闭”心跳日志的剩余时间。 */
	/** The time until the next log for why the loading screen is still up */
	double TimeUntilNextLogHeartbeatSeconds = 0.0;

	/** 当前是否处于 PreLoadMap 与 PostLoadMap 回调之间。 */
	/** True when we are between PreLoadMap and PostLoadMap */
	bool bCurrentlyInLoadMap = false;

	/** 当前是否已经进入加载画面显示状态。 */
	/** True when the loading screen is currently being shown */
	bool bCurrentlyShowingLoadingScreen = false;
};

#undef UE_API
