// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraButtonBase.h"
#include "CommonActionWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraButtonBase)

// 设计时和运行时预构造阶段同步按钮样式及显示文本。
void ULyraButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	UpdateButtonStyle();
	RefreshButtonText();
}

// 输入动作控件刷新后重新计算按钮样式和动作文本。
void ULyraButtonBase::UpdateInputActionWidget()
{
	Super::UpdateInputActionWidget();

	UpdateButtonStyle();
	RefreshButtonText();
}

// 保存调用方文本并刷新显示；空文本允许输入动作的显示名称接管按钮文案。
void ULyraButtonBase::SetButtonText(const FText& InText)
{
	bOverride_ButtonText = InText.IsEmpty();
	ButtonText = InText;
	RefreshButtonText();
}

// 需要动作文本且动作控件提供有效文案时优先显示该文案，否则显示保存的按钮文本。
void ULyraButtonBase::RefreshButtonText()
{
	if (bOverride_ButtonText || ButtonText.IsEmpty())
	{
		if (InputActionWidget)
		{
			const FText ActionDisplayText = InputActionWidget->GetDisplayText();	
			if (!ActionDisplayText.IsEmpty())
			{
				UpdateButtonText(ActionDisplayText);
				return;
			}
		}
	}
	
	UpdateButtonText(ButtonText);	
}

// 输入方式变化后调用基类更新动作图标，并刷新按钮样式。
void ULyraButtonBase::OnInputMethodChanged(ECommonInputType CurrentInputType)
{
	Super::OnInputMethodChanged(CurrentInputType);

	UpdateButtonStyle();
}
