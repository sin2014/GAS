// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SCompoundWidget.h"

class FArrangedChildren;
class SWidget;
struct FGeometry;

class SGameResponsivePanel : public SCompoundWidget
{
public:

	typedef SGridPanel::FSlot FSlot;

public:

	SLATE_BEGIN_ARGS(SGameResponsivePanel)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

	SLATE_END_ARGS()

public:

	SGameResponsivePanel();

	// 根据 Slate 声明参数构造该响应式面板。
	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	// 添加一个内容槽位并返回新槽位的作用域参数。
	/**
	 * Adds a content slot.
	 *
	 * @return The added slot.
	 */
	FSlot& AddSlot();

	// 移除包含指定控件的内容槽位。
	/**
	 * Removes a particular content slot.
	 *
	 * @param SlotWidget The widget in the slot to remove.
	 */
	int32 RemoveSlot(const TSharedRef<SWidget>& SlotWidget);

	// 移除面板中的全部槽位。
	/**
	 * Removes all slots from the panel.
	 */
	void ClearChildren();

	void EnableVerticalStacking(const bool bCanVerticallyWrap);

protected:
	// 以下函数覆盖 SWidget 的布局与子控件管理接口。
	// Begin SWidget overrides.
	virtual bool CustomPrepass(float LayoutScaleMultiplier) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual float GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;
	// 以上为 SWidget 的布局与子控件覆盖函数。
	// End SWidget overrides.

	bool ShouldWrap() const;

	void RefreshResponsiveness();
	void RefreshLayout();

protected:

	TSharedRef<SGridPanel> InnerGrid;
	TArray<SGridPanel::FSlot*> InnerSlots;

	FVector2D PhysialScreenSize = FVector2D(0, 0);
	float Scale = 1;

	uint8 bCanWrapVertically : 1;
};
