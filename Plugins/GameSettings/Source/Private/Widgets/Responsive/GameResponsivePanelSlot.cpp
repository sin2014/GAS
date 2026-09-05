// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameResponsivePanelSlot.h"

#include "Components/Widget.h"
#include "Widgets/Responsive/SGameResponsivePanel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameResponsivePanelSlot)

/////////////////////////////////////////////////////
// 响应式面板槽位负责连接 UMG 内容与底层 Slate 网格槽位。
// UGameResponsivePanelSlot

// 创建尚未绑定 Slate 槽位的 UMG 包装槽，BuildSlot 时再建立底层连接。
UGameResponsivePanelSlot::UGameResponsivePanelSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Slot = nullptr;
}

// 释放响应式面板槽位持有的 Slate 控件引用，并按需释放子控件资源。
void UGameResponsivePanelSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = nullptr;
}

// 把 UMG 槽位内容添加到底层 Slate 响应式面板并保存槽位引用。
void UGameResponsivePanelSlot::BuildSlot(TSharedRef<SGameResponsivePanel> GameResponsivePanel)
{
	Slot = &GameResponsivePanel->AddSlot()
	[
		Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
	];
}

// 将 UMG 槽位属性同步到底层 Slate 槽位。
void UGameResponsivePanelSlot::SynchronizeProperties()
{
}

