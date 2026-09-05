// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"

#include "IndicatorLayer.generated.h"

class SActorCanvas;
class SWidget;
class UObject;

UCLASS()
class UIndicatorLayer : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** 指示器被钳制到屏幕边缘且要求方向提示时使用的默认箭头画刷。 */
	/** Default arrow brush to use if UI is clamped to the screen and needs to show an arrow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush ArrowBrush;

protected:
	// UWidget interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End UWidget

protected:
	TSharedPtr<SActorCanvas> MyActorCanvas;
};
