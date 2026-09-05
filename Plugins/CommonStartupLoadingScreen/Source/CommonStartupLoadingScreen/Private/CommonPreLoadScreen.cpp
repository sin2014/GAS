// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonPreLoadScreen.h"

#include "Misc/App.h"
#include "SCommonPreLoadingScreenWidget.h"

#define LOCTEXT_NAMESPACE "CommonPreLoadingScreen"

// 仅在非编辑器且进程具备渲染能力时创建引擎启动阶段使用的 Slate 加载控件。
void FCommonPreLoadScreen::Init()
{
	if (!GIsEditor && FApp::CanEverRender())
	{
		EngineLoadingWidget = SNew(SCommonPreLoadingScreenWidget);
	}
}

#undef LOCTEXT_NAMESPACE
