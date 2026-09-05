// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGameResponsivePanel.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SViewport.h"

#define LOCTEXT_NAMESPACE "GameSetting"

// TODO：该响应式面板仍处于设置界面原型阶段，尚未作为通用控件正式使用。
//TODO Nick Darnell
// Hello.  It appears you've discovered this widget.
// This widget currently isn't being generally used.  I'm prototyping out some
// ideas for settings.  Talk to me.

// 创建内部网格，关闭逐帧 Tick 和焦点支持，并启用自定义预遍历与相对布局缩放。
SGameResponsivePanel::SGameResponsivePanel()
	: InnerGrid(SNew(SGridPanel))
{
	SetCanTick(false);
	bCanSupportFocus = false;
	bHasCustomPrepass = true;
	bHasRelativeLayoutScale = true;
	bCanWrapVertically = true;
}

// 根据 Slate 参数构建响应式面板并挂接声明的子槽位。
void SGameResponsivePanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		InnerGrid
	];
}

// 在内部网格添加内容槽位，并使布局失效以触发重新排列。
SGridPanel::FSlot& SGameResponsivePanel::AddSlot()
{
	SGridPanel::FSlot* Slot;
	InnerGrid->AddSlot(InnerGrid->GetChildren()->Num(), 0)
		.Expose(Slot);
	InnerSlots.Add(Slot);

	RefreshLayout();

	return *Slot;
}

// 移除包含指定控件的槽位；成功后重建剩余布局。
int32 SGameResponsivePanel::RemoveSlot(const TSharedRef<SWidget>& SlotWidget)
{
	for (int32 SlotIdx = 0; SlotIdx < InnerSlots.Num(); ++SlotIdx)
	{
		if (SlotWidget == InnerSlots[SlotIdx]->GetWidget())
		{
			InnerSlots.RemoveAt(SlotIdx);
			break;
		}
	}

	return InnerGrid->RemoveSlot(SlotWidget);
}

// 清空全部子槽位并使布局缓存失效。
void SGameResponsivePanel::ClearChildren()
{
	InnerGrid->ClearChildren();
}

// 设置空间不足时是否允许改为纵向堆叠，并刷新响应状态。
void SGameResponsivePanel::EnableVerticalStacking(const bool bCanVerticallyWrap)
{
	bCanWrapVertically = bCanVerticallyWrap;
}

// 根据可用宽度预先判断是否换行，必要时刷新布局后继续 Slate 预遍历。
bool SGameResponsivePanel::CustomPrepass(float LayoutScaleMultiplier)
{
	RefreshResponsiveness();
	return true;
}

// 采用当前响应模式排列子控件：宽度充足时横向网格布局，否则纵向堆叠。
void SGameResponsivePanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
		ChildSlot.GetWidget(),
		FVector2D(0, 0),
		AllottedGeometry.GetLocalSize() / Scale,
		Scale
	));
}

// 根据当前横排或竖排模式计算面板期望尺寸。
FVector2D SGameResponsivePanel::ComputeDesiredSize(float InLayoutScale) const
{
	return SCompoundWidget::ComputeDesiredSize(InLayoutScale) * Scale;
}

// 返回指定子控件相对布局缩放；当前沿用统一缩放倍率。
float SGameResponsivePanel::GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const
{
	return Scale;
}

// 返回当前是否因宽度不足而切换到纵向布局。
bool SGameResponsivePanel::ShouldWrap() const
{
	if (PhysialScreenSize.IsZero() || !bCanWrapVertically)
	{
		return false;
	}

	return (PhysialScreenSize.X < 7);
}

// 比较子控件总期望宽度与面板宽度，更新换行状态并在变化时使布局失效。
void SGameResponsivePanel::RefreshResponsiveness()
{
	PhysialScreenSize = FVector2D(0, 0);

	TSharedPtr<SViewport> GameViewport = FSlateApplication::Get().GetGameViewport();
	if (GameViewport.IsValid())
	{
		TSharedPtr<ISlateViewport> ViewportInterface = GameViewport->GetViewportInterface().Pin();
		if (ViewportInterface.IsValid())
		{
			const FIntPoint ViewportSize = ViewportInterface->GetSize();

			int32 ScreenDensity = 0;
			FPlatformApplicationMisc::GetPhysicalScreenDensity(ScreenDensity);
			
			if (ScreenDensity != 0)
			{
				PhysialScreenSize = ViewportSize / (float)ScreenDensity;
			}
		}
	}

	const bool bShouldWrap = ShouldWrap();
	const float NewScale = bShouldWrap ? 1.5f : 1.0f;
	if (!FMath::IsNearlyEqual(NewScale, Scale))
	{
		Scale = NewScale;
		RefreshLayout();
		Invalidate(EInvalidateWidgetReason::Prepass);
	}
}

// 重建网格槽位；根据换行状态选择逐列横排或逐行竖排。
void SGameResponsivePanel::RefreshLayout()
{
	const bool bShouldWrap = ShouldWrap();

	InnerGrid->ClearFill();

	for (int32 SlotIdx = 0; SlotIdx < InnerSlots.Num(); ++SlotIdx)
	{
		InnerSlots[SlotIdx]->SetColumn(bShouldWrap ? 0 : SlotIdx);
		InnerSlots[SlotIdx]->SetRow(bShouldWrap ? SlotIdx : 0);

		if (!bShouldWrap)
		{
			InnerGrid->SetColumnFill(SlotIdx, 1.0f);
		}
	}

	if (bShouldWrap)
	{
		InnerGrid->SetColumnFill(0, 1.0f);
	}
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
