// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/LocalPlayer.h"
#include "GameplayTagContainer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Accessibility/SlateWidgetAccessibleTypes.h"
#include "Widgets/SLeafWidget.h"

class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
struct FGameplayTag;
struct FGeometry;
struct FSlateBrush;

class SHitMarkerConfirmationWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SHitMarkerConfirmationWidget)
		: _PerHitMarkerImage(FCoreStyle::Get().GetBrush("Throbber.CircleChunk"))
		, _AnyHitsMarkerImage(nullptr)
		, _HitNotifyDuration(0.4f)
	{
	}
		/** 单个屏幕空间命中点的默认画刷。 */
		/** The marker image to draw for individual hit markers. */
		SLATE_ARGUMENT(const FSlateBrush*, PerHitMarkerImage)
		/** 存在任一命中时绘制在准星中心的汇总画刷。 */
		/** The marker image to draw if there are any hits at all. */
		SLATE_ARGUMENT(const FSlateBrush*, AnyHitsMarkerImage)
		/** 命中反馈淡出持续时间，单位秒。 */
		/** The duration (in seconds) to display hit notifies (they fade to transparent over this time)  */
		SLATE_ATTRIBUTE(float, HitNotifyDuration)
		/** 标记颜色与基础透明度。 */
		/** The color and opacity of the marker */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InContext, const TMap<FGameplayTag, FSlateBrush>& ZoneOverrideImages);

	SHitMarkerConfirmationWidget();

	//~SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool ComputeVolatility() const override { return true; }
	//~End of SWidget interface

private:
	/** 单个屏幕空间命中点的默认画刷。 */
	/** The marker image to draw for individual hit markers. */
	const FSlateBrush* PerHitMarkerImage = nullptr;

	/** 按命中区域标签选择的单点画刷覆盖表。 */
	/** Map from zone tag (e.g., weak spot) to override marker images. */
	TMap<FGameplayTag, FSlateBrush> PerHitMarkerZoneOverrideImages;

	/** 存在任一命中时绘制在准星中心的汇总画刷。 */
	/** The marker image to draw if there are any hits at all. */
	const FSlateBrush* AnyHitsMarkerImage = nullptr;

	/** 根据最近命中时间每 Tick 计算的当前淡出透明度。 */
	/** The opacity for the hit markers */
	float HitNotifyOpacity = 0.0f;

	/** 命中反馈从不透明衰减到透明的持续时间，单位秒。 */
	/** The duration (in seconds) to display hit notifies (they fade to transparent over this time)  */
	float HitNotifyDuration = 0.4f;

	/** 可绑定的标记颜色与基础透明度。 */
	/** Color and opacity of the markers */
	TAttribute<FSlateColor> ColorAndOpacity;
	bool bColorAndOpacitySet;

	/** 用于定位所属 PlayerController 和武器状态组件的本地玩家上下文。 */
	/** Player context for the owning HUD */
	FLocalPlayerContext MyContext;
};
