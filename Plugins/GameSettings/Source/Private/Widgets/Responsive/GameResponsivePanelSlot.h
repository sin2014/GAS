// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PanelSlot.h"
#include "SGameResponsivePanel.h"

#include "GameResponsivePanelSlot.generated.h"

class UObject;

UCLASS()
class UGameResponsivePanelSlot : public UPanelSlot
{
	GENERATED_UCLASS_BODY()

public:
	

public:

	void BuildSlot(TSharedRef<SGameResponsivePanel> GameResponsivePanel);

	// 以下函数覆盖 UPanelSlot 的资源释放与属性同步接口。
	// UPanelSlot interface
	virtual void SynchronizeProperties() override;
	// 以上为 UPanelSlot 接口覆盖。
	// End of UPanelSlot interface

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	SGameResponsivePanel::FSlot* Slot;
};
