// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/CoreStyle.h"
#include "Widgets/Accessibility/SlateWidgetAccessibleTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "SCircumferenceMarkerWidget.generated.h"

class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
struct FGeometry;
struct FSlateBrush;

USTRUCT(BlueprintType)
struct FCircumferenceMarkerEntry
{
	GENERATED_BODY()

	// 标记中心在圆周上的方位角，单位为度。
	// The angle to place this marker around the circle (in degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ForceUnits=deg))
	float PositionAngle = 0.0f;

	// 标记图像自身绕中心旋转的角度，单位为度。
	// The angle to rotate the marker image (in degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ForceUnits=deg))
	float ImageRotationAngle = 0.0f;
};

class SCircumferenceMarkerWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SCircumferenceMarkerWidget)
		: _MarkerBrush(FCoreStyle::Get().GetBrush("Throbber.CircleChunk"))
		, _Radius(48.0f)
	{
	}
		/** 圆周上每个标记使用的画刷。 */
		/** What each marker on the circumference looks like */
		SLATE_ARGUMENT(const FSlateBrush*, MarkerBrush)
		/** 需要绘制标记的位置角和旋转角列表。 */
		/** At which angles should a marker be drawn */
		SLATE_ARGUMENT(TArray<FCircumferenceMarkerEntry>, MarkerList)
		/** 标记圆周半径。 */
		/** The radius of the circle */
		SLATE_ATTRIBUTE(float, Radius)
		/** 标记的颜色和透明度属性。 */
		/** The color and opacity of the marker */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	SCircumferenceMarkerWidget();

	//~SWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool ComputeVolatility() const override { return true; }
	//~End of SWidget interface

	void SetRadius(float NewRadius);
	void SetMarkerList(TArray<FCircumferenceMarkerEntry>& NewMarkerList);

private:
	FSlateRenderTransform GetMarkerRenderTransform(const FCircumferenceMarkerEntry& Marker, const float BaseRadius, const float HUDScale) const;

private:
	/** 圆周标记画刷，不由此控件拥有。 */
	/** What each marker on the circumference looks like */
	const FSlateBrush* MarkerBrush;

	/** 各标记相对准星中心的位置角和图像旋转角。 */
	/** Angles around the reticle center to place ReticleCornerImage icons */
	TArray<FCircumferenceMarkerEntry> MarkerList;

	/** 可绑定的圆周半径属性。 */
	/** The radius of the circle */
	TAttribute<float> Radius;

	/** 可绑定的标记颜色与透明度。 */
	/** Color and opacity of the markers */
	TAttribute<FSlateColor> ColorAndOpacity;
	bool bColorAndOpacitySet;

	/** 是否额外偏移半个图像尺寸，使标记位于散布半径外侧。 */
	/** Whether reticle corner images are placed outside the spread radius */
	// TODO：可改为 0 到 1 的对齐参数，以连续表达内侧、线上和外侧位置。
	//@TODO: Make this a 0-1 float alignment instead (e.g., inside/on/outside the radius)?
	uint8 bReticleCornerOutsideSpreadRadius : 1;
};
