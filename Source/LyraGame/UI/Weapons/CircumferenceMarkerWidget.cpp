// Copyright Epic Games, Inc. All Rights Reserved.

#include "CircumferenceMarkerWidget.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(CircumferenceMarkerWidget)

class SWidget;

// 构造不可命中测试且标记为易变的圆周标记 UMG 包装控件。
UCircumferenceMarkerWidget::UCircumferenceMarkerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bIsVolatile = true;
}

// 释放 UMG Slate 资源时清空底层圆周标记控件引用。
void UCircumferenceMarkerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMarkerWidget.Reset();
}

// 用当前画刷、半径和标记列表创建底层 SCircumferenceMarkerWidget。
TSharedRef<SWidget> UCircumferenceMarkerWidget::RebuildWidget()
{
	MyMarkerWidget = SNew(SCircumferenceMarkerWidget)
		.MarkerBrush(&MarkerImage)
		.Radius(this->Radius)
		.MarkerList(this->MarkerList);

	return MyMarkerWidget.ToSharedRef();
}

// UMG 属性同步时把半径和标记列表更新到底层 Slate 控件。
void UCircumferenceMarkerWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyMarkerWidget->SetRadius(Radius);
	MyMarkerWidget->SetMarkerList(MarkerList);
}

// 保存新半径，并在底层 Slate 控件已构建时立即同步。
void UCircumferenceMarkerWidget::SetRadius(float InRadius)
{
	Radius = InRadius;
	if (MyMarkerWidget.IsValid())
	{
		MyMarkerWidget->SetRadius(InRadius);
	}
}
