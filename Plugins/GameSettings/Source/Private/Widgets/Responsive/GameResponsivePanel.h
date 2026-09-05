// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PanelWidget.h"
#include "GameResponsivePanel.generated.h"

class UGameResponsivePanelSlot;

// 允许多个子控件按水平方向流式排列，并在空间不足时响应式换行。
/**
 * Allows widgets to be laid out in a flow horizontally.
 *
 * * Many Children
 * * Flow Horizontal
 */
UCLASS()
class UGameResponsivePanel : public UPanelWidget
{
	GENERATED_UCLASS_BODY()

	/**  */
	UFUNCTION(BlueprintCallable, Category="Widget")
	UGameResponsivePanelSlot* AddChildToGameResponsivePanel(UWidget* Content);

#if WITH_EDITOR
	// 以下函数覆盖 UWidget 的 Slate 资源释放和控件重建接口。
	// UWidget interface
	virtual const FText GetPaletteCategory() override;
	// 以上为 UWidget 接口覆盖。
	// End UWidget interface
#endif

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behavior")
	bool bCanStackVertically = true;

protected:

	// 以下函数覆盖 UPanelWidget 的槽位创建、添加和移除接口。
	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// 以上为 UPanelWidget 槽位管理接口覆盖。
	// End UPanelWidget

protected:

	TSharedPtr<class SGameResponsivePanel> MyGameResponsivePanel;

protected:
	// 以下函数覆盖 UWidget 的设计器调色板分类接口。
	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// 以上为 UWidget 编辑器接口覆盖。
	// End of UWidget interface
};
