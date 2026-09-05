// Copyright Epic Games, Inc. All Rights Reserved.

#include "SHitMarkerConfirmationWidget.h"

#include "Weapons/LyraWeaponStateComponent.h"

class FPaintArgs;

// 构造命中确认 Slate 控件的默认状态。
SHitMarkerConfirmationWidget::SHitMarkerConfirmationWidget()
{
}

// 保存单点与汇总命中画刷、命中区域覆盖、颜色属性和本地玩家上下文。
void SHitMarkerConfirmationWidget::Construct(const FArguments& InArgs, const FLocalPlayerContext& InContext, const TMap<FGameplayTag, FSlateBrush>& ZoneOverrideImages)
{
	PerHitMarkerImage = InArgs._PerHitMarkerImage;
	PerHitMarkerZoneOverrideImages = ZoneOverrideImages;
	AnyHitsMarkerImage = InArgs._AnyHitsMarkerImage;
	bColorAndOpacitySet = InArgs._ColorAndOpacity.IsSet();
	ColorAndOpacity = InArgs._ColorAndOpacity;

	MyContext = InContext;
}

// 在淡出期间绘制最近命中的屏幕空间位置和可选中心汇总标记，并修正窗口客户区偏移。
int32 SHitMarkerConfirmationWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FVector2D LocalCenter = AllottedGeometry.GetLocalPositionAtCoordinates(FVector2D(0.5f, 0.5f));

	const bool bDrawMarkers = (HitNotifyOpacity > KINDA_SMALL_NUMBER);

	if (bDrawMarkers)
	{
		// 从所属玩家的武器状态组件取得最近一组已确认伤害的屏幕空间位置。
		// Check if we should use screen-space damage location hit notifies
		TArray<FLyraScreenSpaceHitLocation> LastWeaponDamageScreenLocations;
		if (APlayerController* PC = MyContext.IsInitialized() ? MyContext.GetPlayerController() : nullptr)
		{
			if (ULyraWeaponStateComponent* WeaponStateComponent = PC->FindComponentByClass<ULyraWeaponStateComponent>())
			{
				WeaponStateComponent->GetLastWeaponDamageScreenLocations(/*out*/ LastWeaponDamageScreenLocations);
			}
		}

		if ((LastWeaponDamageScreenLocations.Num() > 0) && (PerHitMarkerImage != nullptr))
		{
			const FVector2D HalfAbsoluteSize = AllottedGeometry.GetAbsoluteSize() * 0.5f;

			for (const FLyraScreenSpaceHitLocation& Hit : LastWeaponDamageScreenLocations)
			{
				const FSlateBrush* LocationMarkerImage = PerHitMarkerZoneOverrideImages.Find(Hit.HitZone);
				if (LocationMarkerImage == nullptr)
				{
					LocationMarkerImage = PerHitMarkerImage;
				}

				FLinearColor MarkerColor = bColorAndOpacitySet ?
					ColorAndOpacity.Get().GetColor(InWidgetStyle) :
					(InWidgetStyle.GetColorAndOpacityTint() * LocationMarkerImage->GetTint(InWidgetStyle));
				MarkerColor.A *= HitNotifyOpacity;

				const FVector2D WindowSSLocation = Hit.Location + MyCullingRect.GetTopLeft(); /* 非全屏模式下加上窗口客户区偏移，使屏幕空间命中位置与实际绘制区域对齐。 */ // Accounting for window trim when not in fullscreen mode
				const FSlateRenderTransform DrawPos(AllottedGeometry.AbsoluteToLocal(WindowSSLocation));

				const FPaintGeometry Geometry(AllottedGeometry.ToPaintGeometry(LocationMarkerImage->ImageSize, FSlateLayoutTransform(-(LocationMarkerImage->ImageSize * 0.5f)), DrawPos));
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry, LocationMarkerImage, DrawEffects, MarkerColor);
			}
		}
		
		if (AnyHitsMarkerImage != nullptr)
		{
			FLinearColor MarkerColor = bColorAndOpacitySet ?
				ColorAndOpacity.Get().GetColor(InWidgetStyle) :
				(InWidgetStyle.GetColorAndOpacityTint() * AnyHitsMarkerImage->GetTint(InWidgetStyle));
			MarkerColor.A *= HitNotifyOpacity;

			// 汇总命中标记始终绘制在准星控件中心，与单点位置标记可同时显示。
			// Otherwise show the hit notify in the center of the reticle
			const FPaintGeometry Geometry(AllottedGeometry.ToPaintGeometry(AnyHitsMarkerImage->ImageSize, FSlateLayoutTransform(LocalCenter - (AnyHitsMarkerImage->ImageSize * 0.5f))));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry, AnyHitsMarkerImage, DrawEffects, MarkerColor);
		}
	}

	return LayerId;
}

// 返回命中确认控件固定的 100×100 期望尺寸。
FVector2D SHitMarkerConfirmationWidget::ComputeDesiredSize(float) const
{
	return FVector2D(100.0f, 100.0f);
}

// 根据武器状态组件记录的最近命中时间计算线性衰减透明度，超出持续时间后归零。
void SHitMarkerConfirmationWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	HitNotifyOpacity = 0.0f;

	if (APlayerController* PC = MyContext.IsInitialized() ? MyContext.GetPlayerController() : nullptr)
	{
		if (ULyraWeaponStateComponent* DamageMarkerComponent = PC->FindComponentByClass<ULyraWeaponStateComponent>())
		{
			const double TimeSinceLastHitNotification = DamageMarkerComponent->GetTimeSinceLastHitNotification();
			if (TimeSinceLastHitNotification < HitNotifyDuration)
			{
				HitNotifyOpacity = FMath::Clamp(1.0f - (float)(TimeSinceLastHitNotification / HitNotifyDuration), 0.0f, 1.0f);
			}
		}
	}
}
