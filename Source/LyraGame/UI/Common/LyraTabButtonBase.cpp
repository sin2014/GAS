// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTabButtonBase.h"

#include "CommonLazyImage.h"
#include "UI/Common/LyraTabListWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTabButtonBase)

class UObject;
struct FSlateBrush;

// 将软对象交给懒加载图像控件异步解析并显示；图像控件不存在时不处理。
void ULyraTabButtonBase::SetIconFromLazyObject(TSoftObjectPtr<UObject> LazyObject)
{
	if (LazyImage_Icon)
	{
		LazyImage_Icon->SetBrushFromLazyDisplayAsset(LazyObject);
	}
}

// 直接把 Slate 画刷应用到标签图标控件。
void ULyraTabButtonBase::SetIconBrush(const FSlateBrush& Brush)
{
	if (LazyImage_Icon)
	{
		LazyImage_Icon->SetBrush(Brush);
	}
}

// 将标签描述中的文本和图标同步到按钮。
void ULyraTabButtonBase::SetTabLabelInfo_Implementation(const FLyraTabDescriptor& TabLabelInfo)
{
	SetButtonText(TabLabelInfo.TabText);
	SetIconBrush(TabLabelInfo.IconBrush);
}

