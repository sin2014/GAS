// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/WaitingWidget.h"
#include "Components/TextBlock.h"

void UWaitingWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

FOnButtonClickedEvent& UWaitingWidget::ClearAndGetButtonClickedEvent()
{
	CancelBtn->OnClicked.Clear();
	return CancelBtn->OnClicked;
}

void UWaitingWidget::SetWaitInfo(const FText& WaitInfo, bool bAllowCancel)
{
	if (CancelBtn)
	{
		CancelBtn->SetVisibility(bAllowCancel ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (WaitInfoText)
	{
		WaitInfoText->SetText(WaitInfo);
	}
}
