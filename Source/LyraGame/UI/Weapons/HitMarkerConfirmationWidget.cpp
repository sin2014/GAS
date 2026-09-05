// Copyright Epic Games, Inc. All Rights Reserved.

#include "HitMarkerConfirmationWidget.h"

#include "Blueprint/UserWidget.h"
#include "SHitMarkerConfirmationWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HitMarkerConfirmationWidget)

class SWidget;

// 构造不可命中测试且标记为易变的命中确认 UMG 包装控件，并默认关闭汇总画刷绘制。
UHitMarkerConfirmationWidget::UHitMarkerConfirmationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bIsVolatile = true;
	AnyHitsMarkerImage.DrawAs = ESlateBrushDrawType::NoDrawType;
}

// 释放 UMG Slate 资源时清空底层命中标记控件引用。
void UHitMarkerConfirmationWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMarkerWidget.Reset();
}

// 取得外层 UserWidget 的玩家上下文，并用命中画刷、区域覆盖和持续时间创建底层 Slate 控件。
TSharedRef<SWidget> UHitMarkerConfirmationWidget::RebuildWidget()
{
	UUserWidget* OuterUserWidget = GetTypedOuter<UUserWidget>();
	FLocalPlayerContext DummyContext;
	const FLocalPlayerContext& PlayerContextRef = (OuterUserWidget != nullptr) ? OuterUserWidget->GetPlayerContext() : DummyContext;

	MyMarkerWidget = SNew(SHitMarkerConfirmationWidget, PlayerContextRef, PerHitMarkerZoneOverrideImages)
		.PerHitMarkerImage(&(this->PerHitMarkerImage))
		.AnyHitsMarkerImage(&(this->AnyHitsMarkerImage))
		.HitNotifyDuration(this->HitNotifyDuration);

	return MyMarkerWidget.ToSharedRef();
}

