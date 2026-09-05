// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTaggedWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTaggedWidget)

// TODO：本文件其余 TODO 都与基于标签控制控件显隐有关，参见 UE-142237。
//@TODO: The other TODOs in this file are all related to tag-based showing/hiding of widgets, see UE-142237

// 构造由调用方可见性和标签抑制状态共同控制的 UMG 控件。
ULyraTaggedWidget::ULyraTaggedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 运行时构造后按当前请求状态计算初始可见性；隐藏标签监听仍为待实现功能。
void ULyraTaggedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsDesignTime())
	{
		// 监听会抑制此控件显示的标签变化。
		// Listen for tag changes on our hidden tags
		//@TODO: That thing I said

		// 构造完成后按调用方期望状态和当前标签计算初始可见性。
		// Set our initial visibility value (checking the tags, etc...)
		SetVisibility(GetVisibility());
	}
}

// 运行时销毁阶段预留标签监听解绑入口，再调用基类清理。
void ULyraTaggedWidget::NativeDestruct()
{
	if (!IsDesignTime())
	{
		// TODO：控件析构时解除 NativeConstruct 中注册的标签变化监听。
		//@TODO: Stop listening for tag changes
	}

	Super::NativeDestruct();
}

// 记录调用方期望的显示或隐藏状态并写入实际 Slate 可见性；当前标签抑制判断尚未实现，固定视为无抑制标签。
void ULyraTaggedWidget::SetVisibility(ESlateVisibility InVisibility)
{
#if WITH_EDITORONLY_DATA
	if (IsDesignTime())
	{
		Super::SetVisibility(InVisibility);
		return;
	}
#endif

	// 记录调用方请求的状态；即使当前被标签抑制，抑制解除后也应恢复到该请求状态。
	// Remember what the caller requested; even if we're currently being
	// suppressed by a tag we should respect this call when we're done
	bWantsToBeVisible = ConvertSerializedVisibilityToRuntime(InVisibility).IsVisible();
	if (bWantsToBeVisible)
	{
		ShownVisibility = InVisibility;
	}
	else
	{
		HiddenVisibility = InVisibility;
	}

	const bool bHasHiddenTags = false;//@TODO: Foo->HasAnyTags(HiddenByTags);

	// 将调用方期望状态与标签抑制结果合并后真正应用可见性。
	// Actually apply the visibility
	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}

// 重新应用调用方期望可见性；当前受监视标签查询尚未实现，固定视为无抑制标签。
void ULyraTaggedWidget::OnWatchedTagsChanged()
{
	const bool bHasHiddenTags = false;//@TODO: Foo->HasAnyTags(HiddenByTags);

	// 标签变化后重新计算并应用最终可见性。
	// Actually apply the visibility
	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}

