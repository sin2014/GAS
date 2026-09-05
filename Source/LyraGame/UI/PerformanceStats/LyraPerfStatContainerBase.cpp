// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerfStatContainerBase.h"

#include "Blueprint/WidgetTree.h"
#include "LyraPerfStatWidgetBase.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerfStatContainerBase)

//////////////////////////////////////////////////////////////////////
// ULyraPerfStatsContainerBase

// 构造按显示模式筛选性能统计子控件的容器。
ULyraPerfStatContainerBase::ULyraPerfStatContainerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 构造时刷新子控件可见性，并监听本地性能统计显示设置变化。
void ULyraPerfStatContainerBase::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateVisibilityOfChildren();

	ULyraSettingsLocal::Get()->OnPerfStatDisplayStateChanged().AddUObject(this, &ThisClass::UpdateVisibilityOfChildren);
}

// 销毁时解除性能统计设置委托，再调用基类清理。
void ULyraPerfStatContainerBase::NativeDestruct()
{
	ULyraSettingsLocal::Get()->OnPerfStatDisplayStateChanged().RemoveAll(this);

	Super::NativeDestruct();
}

// 遍历 WidgetTree 中的性能统计控件，按各统计项设置模式和当前容器过滤类型决定显隐。
void ULyraPerfStatContainerBase::UpdateVisibilityOfChildren()
{
	ULyraSettingsLocal* UserSettings = ULyraSettingsLocal::Get();

	const bool bShowTextWidgets = (StatDisplayModeFilter == ELyraStatDisplayMode::TextOnly) || (StatDisplayModeFilter == ELyraStatDisplayMode::TextAndGraph);
	const bool bShowGraphWidgets = (StatDisplayModeFilter == ELyraStatDisplayMode::GraphOnly) || (StatDisplayModeFilter == ELyraStatDisplayMode::TextAndGraph);
	
	check(WidgetTree);
	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (ULyraPerfStatWidgetBase* TypedWidget = Cast<ULyraPerfStatWidgetBase>(Widget))
		{
			const ELyraStatDisplayMode SettingMode = UserSettings->GetPerfStatDisplayState(TypedWidget->GetStatToDisplay());

			bool bShowWidget = false;
			switch (SettingMode)
			{
			case ELyraStatDisplayMode::Hidden:
				bShowWidget = false;
				break;
			case ELyraStatDisplayMode::TextOnly:
				bShowWidget = bShowTextWidgets;
				break;
			case ELyraStatDisplayMode::GraphOnly:
				bShowWidget = bShowGraphWidgets;
				break;
			case ELyraStatDisplayMode::TextAndGraph:
				bShowWidget = bShowTextWidgets || bShowGraphWidgets;
				break;
			}

			TypedWidget->SetVisibility(bShowWidget ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	});
}

