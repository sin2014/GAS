// Copyright Epic Games, Inc. All Rights Reserved.

#include "IndicatorLayer.h"

#include "SActorCanvas.h"
#include "Widgets/Layout/SBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IndicatorLayer)

class SWidget;

/////////////////////////////////////////////////////
// UIndicatorLayer

// 构造不可命中测试、可在蓝图中引用的指示器 UMG 容器。
UIndicatorLayer::UIndicatorLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 释放 UMG Slate 资源时清空底层 Actor Canvas 共享引用。
void UIndicatorLayer::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyActorCanvas.Reset();
}

// 运行时为所属本地玩家创建 SActorCanvas；设计器或玩家无效时返回安全的空 SBox。
TSharedRef<SWidget> UIndicatorLayer::RebuildWidget()
{
	if (!IsDesignTime())
	{
		ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
		if (ensureMsgf(LocalPlayer, TEXT("Attempting to rebuild a UActorCanvas without a valid LocalPlayer!")))
		{
			MyActorCanvas = SNew(SActorCanvas, FLocalPlayerContext(LocalPlayer), &ArrowBrush);
			return MyActorCanvas.ToSharedRef();
		}
	}

	// 设计器或缺少本地玩家时返回空 SBox；UWidget 不能安全地直接返回 SNullWidget。
	// Give it a trivial box, NullWidget isn't safe to use from a UWidget
	return SNew(SBox);
}

