// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Misc/KeyAlreadyBoundWarning.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KeyAlreadyBoundWarning)

// 更新按键冲突警告正文。
void UKeyAlreadyBoundWarning::SetWarningText(const FText& InText)
{
	WarningText->SetText(InText);
}

// 更新按键捕获界面的取消提示文本。
void UKeyAlreadyBoundWarning::SetCancelText(const FText& InText)
{
	CancelText->SetText(InText);
}
