// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SSubtitleDisplay.h"

#include "Kismet/GameplayStatics.h"
#include "SubtitleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SRichTextBlock.h"

struct FSlateBrush;

// 构建背景与富文本控件；非手动模式监听全局 SubtitleManager 的文本变化。
void SSubtitleDisplay::Construct(const FArguments& InArgs)
{
	if (!InArgs._ManualSubtitles.Get())
	{
		FSubtitleManagerSetSubtitleText& OnSetSubtitleText = FSubtitleManager::GetSubtitleManager()->OnSetSubtitleText();
		OnSetSubtitleText.AddSP(this, &SSubtitleDisplay::HandleSubtitleChanged);
	}

	ChildSlot
	[
		SAssignNew(Background, SBorder)
		.Visibility(EVisibility::Collapsed)
		.Padding(FMargin(7.0, 5.0))
		[
			SAssignNew(TextDisplay, SRichTextBlock)
			.TextStyle(InArgs._TextStyle)
			.Justification(ETextJustify::Center)
			.WrapTextAt(InArgs._WrapTextAt)
		]
	];
}

// Slate 控件销毁时解除 SubtitleManager 委托，避免共享管理器回调失效 Widget。
SSubtitleDisplay::~SSubtitleDisplay()
{
	FSubtitleManager::GetSubtitleManager()->OnSetSubtitleText().RemoveAll(this);
}

// 将新的文本样式应用到内部 RichTextBlock。
void SSubtitleDisplay::SetTextStyle(const FTextBlockStyle& InTextStyle)
{
	TextDisplay->SetTextStyle(InTextStyle);
}

// 替换字幕背景 Border 使用的 SlateBrush。
void SSubtitleDisplay::SetBackgroundBrush(const FSlateBrush* InSlateBrush)
{
	Background->SetBorderImage(InSlateBrush);
}

// 直接设置字幕文本，并根据是否为空切换背景可见性，供预览或手动字幕模式使用。
void SSubtitleDisplay::SetCurrentSubtitleText(const FText& InSubtitleText)
{
	Background->SetVisibility(InSubtitleText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	TextDisplay->SetText(InSubtitleText);
}

// 根据内部文本控件是否包含非空文本判断当前是否有可显示字幕。
bool SSubtitleDisplay::HasSubtitles() const
{
	return !TextDisplay->GetText().IsEmpty();
}

// 更新内部文本控件的自动换行宽度属性。
void SSubtitleDisplay::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	TextDisplay->SetWrapTextAt(InWrapTextAt);
}

// 响应全局字幕文本变化；仅在游戏字幕开关启用时显示文本，否则强制隐藏背景。
void SSubtitleDisplay::HandleSubtitleChanged(const FText& InSubtitleText)
{
	if (UGameplayStatics::AreSubtitlesEnabled())
	{
		Background->SetVisibility(InSubtitleText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
		TextDisplay->SetText(InSubtitleText);
	}
	else
	{
		Background->SetVisibility(EVisibility::Collapsed);
	}
}
