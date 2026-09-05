// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "GameplayTagContainer.h"

#include "HitMarkerConfirmationWidget.generated.h"

class SHitMarkerConfirmationWidget;
class SWidget;
class UObject;
struct FGameplayTag;

UCLASS()
class UHitMarkerConfirmationWidget : public UWidget
{
	GENERATED_BODY()

public:
	UHitMarkerConfirmationWidget(const FObjectInitializer& ObjectInitializer);

	//~UWidget interface
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~End of UWidget interface

	//~UVisual interface
public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~End of UVisual interface
	
public:
	/** 命中反馈从完全不透明淡出到透明的持续时间，单位秒。 */
	/** The duration (in seconds) to display hit notifies (they fade to transparent over this time)  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(ClampMin=0.0, ForceUnits=s))
	float HitNotifyDuration = 0.4f;

	/** 在实际屏幕空间命中位置绘制的默认单点标记图像。 */
	/** The marker image to draw for individual hit markers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush PerHitMarkerImage;

	/** 按命中区域标签覆盖单点标记图像，例如弱点或头部命中。 */
	/** Map from zone tag (e.g., weak spot) to override marker images for individual location hits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TMap<FGameplayTag, FSlateBrush> PerHitMarkerZoneOverrideImages;

	/** 只要存在已确认命中就在准星中心绘制的汇总标记图像。 */
	/** The marker image to draw if there are any hits at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FSlateBrush AnyHitsMarkerImage;

private:
	/** 实际读取武器状态并绘制命中反馈的底层 Slate 控件。 */
	/** Internal slate widget representing the actual marker visuals */
	TSharedPtr<SHitMarkerConfirmationWidget> MyMarkerWidget;
};
