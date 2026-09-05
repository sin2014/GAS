// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Foundation/LyraLoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraLoadingScreenSubsystem)

class UUserWidget;

//////////////////////////////////////////////////////////////////////
// ULyraLoadingScreenSubsystem

// 构造用于保存加载界面内容控件类的引擎子系统。
ULyraLoadingScreenSubsystem::ULyraLoadingScreenSubsystem()
{
}

// 加载界面控件类变化时更新缓存并广播新类。
void ULyraLoadingScreenSubsystem::SetLoadingScreenContentWidget(TSubclassOf<UUserWidget> NewWidgetClass)
{
	if (LoadingScreenWidgetClass != NewWidgetClass)
	{
		LoadingScreenWidgetClass = NewWidgetClass;

		OnLoadingScreenWidgetChanged.Broadcast(LoadingScreenWidgetClass);
	}
}

// 返回当前配置的加载界面内容控件类。
TSubclassOf<UUserWidget> ULyraLoadingScreenSubsystem::GetLoadingScreenContentWidget() const
{
	return LoadingScreenWidgetClass;
}

