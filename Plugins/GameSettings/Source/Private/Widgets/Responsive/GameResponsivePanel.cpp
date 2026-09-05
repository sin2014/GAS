// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameResponsivePanel.h"
#include "GameResponsivePanelSlot.h"
#include "Widgets/Responsive/SGameResponsivePanel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameResponsivePanel)

#define LOCTEXT_NAMESPACE "GameSetting"

/////////////////////////////////////////////////////
// UMG 响应式面板包装 Slate 流式布局，并管理专用槽位。
// UGameResponsivePanel

// 创建非变量 UMG 面板并禁用自身命中测试，使输入直接到达其中的设置控件。
UGameResponsivePanel::UGameResponsivePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// 释放UMG 响应式面板持有的 Slate 控件引用，并按需释放子控件资源。
void UGameResponsivePanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyGameResponsivePanel.Reset();
}

// 指定 UMG 子控件应使用响应式面板专用槽位类。
UClass* UGameResponsivePanel::GetSlotClass() const
{
	return UGameResponsivePanelSlot::StaticClass();
}

// UMG 槽位新增时将其同步构建到已存在的 Slate 面板。
void UGameResponsivePanel::OnSlotAdded(UPanelSlot* InSlot)
{
	// 底层 Slate 面板已经存在时，立即把新增子控件加入实时布局。
	// Add the child to the live canvas if it already exists
	if ( MyGameResponsivePanel.IsValid() )
	{
		CastChecked<UGameResponsivePanelSlot>(InSlot)->BuildSlot(MyGameResponsivePanel.ToSharedRef());
	}
}

// UMG 槽位移除时同步从 Slate 面板删除对应控件。
void UGameResponsivePanel::OnSlotRemoved(UPanelSlot* InSlot)
{
	// 底层 Slate 槽位存在时，立即从实时布局移除该控件。
	// Remove the widget from the live slot if it exists.
	if ( MyGameResponsivePanel.IsValid() && InSlot->Content)
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyGameResponsivePanel->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

// 添加子控件并返回类型安全的响应式面板槽位。
UGameResponsivePanelSlot* UGameResponsivePanel::AddChildToGameResponsivePanel(UWidget* Content)
{
	return Cast<UGameResponsivePanelSlot>( Super::AddChild(Content) );
}

// 创建底层响应式 Slate 面板，并把现有 UMG 槽位逐一挂接到新面板。
TSharedRef<SWidget> UGameResponsivePanel::RebuildWidget()
{
	MyGameResponsivePanel = SNew(SGameResponsivePanel);

	MyGameResponsivePanel->EnableVerticalStacking(bCanStackVertically);

	for ( UPanelSlot* PanelSlot : Slots )
	{
		if ( UGameResponsivePanelSlot* TypedSlot = Cast<UGameResponsivePanelSlot>(PanelSlot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyGameResponsivePanel.ToSharedRef());
		}
	}

	return MyGameResponsivePanel.ToSharedRef();
}

#if WITH_EDITOR

// 返回该控件在 UMG 设计器调色板中的分类文本。
const FText UGameResponsivePanel::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

