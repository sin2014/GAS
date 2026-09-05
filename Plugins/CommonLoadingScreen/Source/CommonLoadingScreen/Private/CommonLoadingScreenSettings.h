// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "UObject/SoftObjectPath.h"

#include "CommonLoadingScreenSettings.generated.h"

class UObject;

/** 加载画面系统的项目配置与调试选项。 */
/**
 * Settings for a loading screen system.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Common Loading Screen"))
class UCommonLoadingScreenSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UCommonLoadingScreenSettings();

public:
	
	// 用作加载画面的 UUserWidget 类软引用。
	// The widget to load for the loading screen.
	UPROPERTY(config, EditAnywhere, Category=Display, meta=(MetaClass="/Script/UMG.UserWidget"))
	FSoftClassPath LoadingScreenWidget;

	// 加载控件在视口控件栈中的 Z 顺序。
	// The z-order of the loading screen widget in the viewport stack
	UPROPERTY(config, EditAnywhere, Category=Display)
	int32 LoadingScreenZOrder = 10000;

	// 其他加载工作结束后额外保持加载画面的秒数，为纹理流送留出时间以减少模糊。
	// 为提高迭代速度，编辑器中默认不应用该延时，可通过 HoldLoadingScreenAdditionalSecsEvenInEditor 启用。
	// How long to hold the loading screen up after other loading finishes (in seconds) to
	// try to give texture streaming a chance to avoid blurriness
	//
	// Note: This is not normally applied in the editor for iteration time, but can be 
	// enabled via HoldLoadingScreenAdditionalSecsEvenInEditor
 	UPROPERTY(config, EditAnywhere, Category=Configuration, meta=(ForceUnits=s, ConsoleVariable="CommonLoadingScreen.HoldLoadingScreenAdditionalSecs"))
	float HoldLoadingScreenAdditionalSecs = 2.0f;

	// 加载画面持续超过该秒数时视为永久卡死；设为零则禁用此检测。
	// The interval in seconds beyond which the loading screen is considered permanently hung (if non-zero).
 	UPROPERTY(config, EditAnywhere, Category=Configuration, meta=(ForceUnits=s))
	float LoadingScreenHeartbeatHangDuration = 0.0f;

	// 输出“何种原因仍在保持加载画面”心跳日志的时间间隔；设为零则禁用。
	// The interval in seconds between each log of what is keeping a loading screen up (if non-zero).
 	UPROPERTY(config, EditAnywhere, Category=Configuration, meta=(ForceUnits=s))
	float LogLoadingScreenHeartbeatInterval = 5.0f;

	// 启用后，每帧将加载画面显示或隐藏的原因写入日志。
	// When true, the reason the loading screen is shown or hidden will be printed to the log every frame.
	UPROPERTY(Transient, EditAnywhere, Category=Debugging, meta=(ConsoleVariable="CommonLoadingScreen.LogLoadingScreenReasonEveryFrame"))
	bool LogLoadingScreenReasonEveryFrame = 0;

	// 强制显示加载画面，用于调试。
	// Force the loading screen to be displayed (useful for debugging)
	UPROPERTY(Transient, EditAnywhere, Category=Debugging, meta=(ConsoleVariable="CommonLoadingScreen.AlwaysShow"))
	bool ForceLoadingScreenVisible = false;

	// 是否在编辑器中也应用 HoldLoadingScreenAdditionalSecs 的额外延时，便于迭代加载画面。
	// Should we apply the additional HoldLoadingScreenAdditionalSecs delay even in the editor
	// (useful when iterating on loading screens)
	UPROPERTY(Transient, EditAnywhere, Category=Debugging)
	bool HoldLoadingScreenAdditionalSecsEvenInEditor = false;

	// 是否在编辑器中也强制 Tick Slate，以确保加载画面立即刷新；原英文注释与字段实际用途不一致。
	// Should we apply the additional HoldLoadingScreenAdditionalSecs delay even in the editor
	// (useful when iterating on loading screens)
	UPROPERTY(config, EditAnywhere, Category=Configuration)
	bool ForceTickLoadingScreenEvenInEditor = true;
};

