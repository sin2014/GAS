// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Weapons/SCircumferenceMarkerWidget.h"

#include "Engine/UserInterfaceSettings.h"
#include "Styling/SlateBrush.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SCircumferenceMarkerWidget)

class FPaintArgs;
class FSlateRect;

// 构造圆周标记 Slate 控件的默认状态。
SCircumferenceMarkerWidget::SCircumferenceMarkerWidget()
{
}

// 从 Slate 参数保存标记画刷、标记列表、半径及可选颜色属性。
void SCircumferenceMarkerWidget::Construct(const FArguments& InArgs)
{
	MarkerBrush = InArgs._MarkerBrush;
	MarkerList = InArgs._MarkerList;
	Radius = InArgs._Radius;
	bColorAndOpacitySet = InArgs._ColorAndOpacity.IsSet();
	ColorAndOpacity = InArgs._ColorAndOpacity;
}

// 围绕画刷中心旋转标记，并按方位角、半径、外侧补偿和 UI 缩放计算最终渲染变换。
FSlateRenderTransform SCircumferenceMarkerWidget::GetMarkerRenderTransform(const FCircumferenceMarkerEntry& Marker, const float BaseRadius, const float HUDScale) const
{
	// 默认让图像中心位于半径线上；外侧模式再增加半个画刷宽度。
	// Determine the radius to use for the corners
	float XRadius = BaseRadius;
	float YRadius = BaseRadius;
	if (bReticleCornerOutsideSpreadRadius)
	{
		XRadius += MarkerBrush->ImageSize.X * 0.5f;
		YRadius += MarkerBrush->ImageSize.X * 0.5f;
	}

	// 将标记方位角和图像旋转角从度转换为弧度。
	// Get the angle and orientation for this reticle corner
	const float LocalRotationRadians = FMath::DegreesToRadians(Marker.ImageRotationAngle);
	const float PositionAngleRadians = FMath::DegreesToRadians(Marker.PositionAngle);

	// 先把图像中心移到原点完成自身旋转，再移回图像中心。
	// First rotate the corner image about the origin
	FSlateRenderTransform RotateAboutOrigin(Concatenate(FVector2D(-MarkerBrush->ImageSize.X * 0.5f, -MarkerBrush->ImageSize.Y * 0.5f), FQuat2D(LocalRotationRadians), FVector2D(MarkerBrush->ImageSize.X * 0.5f, MarkerBrush->ImageSize.Y * 0.5f)));

	// 再按圆周角和 UI ApplicationScale 把旋转后的图像移动到目标半径位置。
	// Move the rotated image to the right place on the spread radius
	return TransformCast<FSlateRenderTransform>(Concatenate(RotateAboutOrigin, FVector2D(XRadius * FMath::Sin(PositionAngleRadians) * HUDScale, -YRadius * FMath::Cos(PositionAngleRadians) * HUDScale)));
}

// 在控件中心周围按标记列表逐项计算变换并绘制画刷，颜色来自显式属性或样式与画刷色调。
int32 SCircumferenceMarkerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FVector2D LocalCenter = AllottedGeometry.GetLocalPositionAtCoordinates(FVector2D(0.5f, 0.5f));

	const bool bDrawMarkers = (MarkerList.Num() > 0) && (MarkerBrush != nullptr);

	if (bDrawMarkers == true)
	{
		const FLinearColor MarkerColor = bColorAndOpacitySet ?
			ColorAndOpacity.Get().GetColor(InWidgetStyle) :
			(InWidgetStyle.GetColorAndOpacityTint() * MarkerBrush->GetTint(InWidgetStyle));

		if (MarkerColor.A > KINDA_SMALL_NUMBER)
		{
			const float BaseRadius = Radius.Get();
			const float ApplicationScale = GetDefault<UUserInterfaceSettings>()->ApplicationScale;
			for (const FCircumferenceMarkerEntry& Marker : MarkerList)
			{
				const FSlateRenderTransform MarkerTransform = GetMarkerRenderTransform(Marker, BaseRadius, ApplicationScale);

				const FPaintGeometry Geometry(AllottedGeometry.ToPaintGeometry(MarkerBrush->ImageSize, FSlateLayoutTransform(LocalCenter - (MarkerBrush->ImageSize * 0.5f)), MarkerTransform, FVector2D(0.0f, 0.0f)));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry, MarkerBrush, DrawEffects, MarkerColor);
			}
		}
	}

	return LayerId;
}

// 按画刷尺寸和当前半径计算可容纳完整圆周标记的控件尺寸。
FVector2D SCircumferenceMarkerWidget::ComputeDesiredSize(float) const
{
	check(MarkerBrush);
	const float SampledRadius = Radius.Get();
	return FVector2D((MarkerBrush->ImageSize.X + SampledRadius) * 2.0f, (MarkerBrush->ImageSize.Y + SampledRadius) * 2.0f);
}

// 半径绑定或数值变化时更新属性并使布局失效。
void SCircumferenceMarkerWidget::SetRadius(float NewRadius)
{
	if (Radius.IsBound() || (Radius.Get() != NewRadius))
	{
		Radius = NewRadius;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

// 替换当前圆周标记条目列表。
void SCircumferenceMarkerWidget::SetMarkerList(TArray<FCircumferenceMarkerEntry>& NewMarkerList)
{
	MarkerList = NewMarkerList;
}

