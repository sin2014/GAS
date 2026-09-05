// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTabListWidgetBase.h"

#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTabListWidgetBase)

// 仅完成基类初始化；标签内容创建延后到 NativeConstruct 或 Switcher 重新关联时执行。
void ULyraTabListWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

// 运行时构造标签列表，并创建或重新挂接预注册标签内容。
void ULyraTabListWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	SetupTabs();
}

// 销毁前从父级移除已创建的标签内容并清空引用，防止下次构造复用失效层级。
void ULyraTabListWidgetBase::NativeDestruct()
{
	for (FLyraTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.CreatedTabContentWidget)
		{
			TabInfo.CreatedTabContentWidget->RemoveFromParent();
			TabInfo.CreatedTabContentWidget = nullptr;
		}
	}

	Super::NativeDestruct();
}

// 按 TabId 查找预注册描述并复制到输出参数；未找到时返回 false。
bool ULyraTabListWidgetBase::GetPreregisteredTabInfo(const FName TabNameId, FLyraTabDescriptor& OutTabInfo)
{
	const FLyraTabDescriptor* const FoundTabInfo = PreregisteredTabInfoArray.FindByPredicate([&](FLyraTabDescriptor& TabInfo) -> bool
	{
		return TabInfo.TabId == TabNameId;
	});

	if (!FoundTabInfo)
	{
		return false;
	}

	OutTabInfo = *FoundTabInfo;
	return true;
}

// 更新指定预注册标签的隐藏标记；找不到标签时不修改。
void ULyraTabListWidgetBase::SetTabHiddenState(FName TabNameId, bool bHidden)
{
	for (FLyraTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.TabId == TabNameId)
		{
			TabInfo.bHidden = bHidden;
			break;
		}
	}
}

// 缓存动态标签描述并注册按钮与内容；隐藏标签直接视为成功而不创建 UI。
bool ULyraTabListWidgetBase::RegisterDynamicTab(const FLyraTabDescriptor& TabDescriptor)
{
	// 隐藏的动态标签视为注册成功但不创建按钮或内容，便于调用方使用同一配置流程。
	// If it's hidden we just ignore it.
	if (TabDescriptor.bHidden)
	{
		return true;
	}
	
	PendingTabLabelInfoMap.Add(TabDescriptor.TabId, TabDescriptor);

	return RegisterTab(TabDescriptor.TabId, TabDescriptor.TabButtonType, TabDescriptor.CreatedTabContentWidget);
}

// Switcher 解除关联前移除各标签内容的父级关系，但保留控件实例供重新挂接。
void ULyraTabListWidgetBase::HandlePreLinkedSwitcherChanged()
{
	for (const FLyraTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		// Switcher 即将解除关联时，从其面板层级移除现有标签内容，但保留实例供后续重新关联。
		// Remove tab content widget from linked switcher, as it is being disassociated.
		if (TabInfo.CreatedTabContentWidget)
		{
			TabInfo.CreatedTabContentWidget->RemoveFromParent();
		}
	}

	Super::HandlePreLinkedSwitcherChanged();
}

// Switcher 重新关联后，仅在运行时且 Slate 已构建时重建标签关系，再调用基类完成切换。
void ULyraTabListWidgetBase::HandlePostLinkedSwitcherChanged()
{
	if (!IsDesignTime() && GetCachedWidget().IsValid())
	{
		// 仅在运行时且 Slate 控件已构建后创建标签，避免设计器预览或未构造阶段产生实例。
		// Don't bother making tabs if we're in the designer or haven't been constructed yet
		SetupTabs();
	}

	Super::HandlePostLinkedSwitcherChanged();
}

// 从预注册或待处理描述取得标签信息，通过接口配置新按钮，并清除一次性动态标签缓存。
void ULyraTabListWidgetBase::HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton)
{
	FLyraTabDescriptor* TabInfoPtr = nullptr;
	
	FLyraTabDescriptor TabInfo;
	if (GetPreregisteredTabInfo(TabId, TabInfo))
	{
		TabInfoPtr = &TabInfo;
	}
	else
	{
		TabInfoPtr = PendingTabLabelInfoMap.Find(TabId);
	}
	
	if (TabButton->GetClass()->ImplementsInterface(ULyraTabButtonInterface::StaticClass()))
	{
		if (ensureMsgf(TabInfoPtr, TEXT("A tab button was created with id %s but no label info was specified. RegisterDynamicTab should be used over RegisterTab to provide label info."), *TabId.ToString()))
		{
			ILyraTabButtonInterface::Execute_SetTabLabelInfo(TabButton, *TabInfoPtr);
		}
	}

	PendingTabLabelInfoMap.Remove(TabId);
}

// 返回当前活动标签是否为预注册数组首项；数组为空时返回 false。
bool ULyraTabListWidgetBase::IsFirstTabActive() const
{
	if (PreregisteredTabInfoArray.Num() > 0)
	{
		return GetActiveTab() == PreregisteredTabInfoArray[0].TabId;
	}

	return false;
}

// 返回当前活动标签是否为预注册数组末项；数组为空时返回 false。
bool ULyraTabListWidgetBase::IsLastTabActive() const
{
	if (PreregisteredTabInfoArray.Num() > 0)
	{
		return GetActiveTab() == PreregisteredTabInfoArray.Last().TabId;
	}

	return false;
}

// 检查指定标签按钮是否采用任一 Slate 可见状态；按钮不存在时返回 false。
bool ULyraTabListWidgetBase::IsTabVisible(FName TabId)
{
	if (const UCommonButtonBase* Button = GetTabButtonBaseByID(TabId))
	{
		const ESlateVisibility TabVisibility = Button->GetVisibility();
		return (TabVisibility == ESlateVisibility::Visible
			|| TabVisibility == ESlateVisibility::HitTestInvisible
			|| TabVisibility == ESlateVisibility::SelfHitTestInvisible);
	}

	return false;
}

// 遍历已注册标签并统计当前可见按钮数量。
int32 ULyraTabListWidgetBase::GetVisibleTabCount()
{
	int32 Result = 0;
	const int32 TabCount = GetTabCount();
	for ( int32 Index = 0; Index < TabCount; Index++ )
	{
		if (IsTabVisible(GetTabIdAtIndex( Index )))
		{
			Result++;
		}
	}

	return Result;
}

// 为未隐藏的预注册标签创建一次内容控件、广播创建事件、挂入当前 Switcher，并避免重复注册按钮。
void ULyraTabListWidgetBase::SetupTabs()
{
	for (FLyraTabDescriptor& TabInfo : PreregisteredTabInfoArray)
	{
		if (TabInfo.bHidden)
		{
			continue;
		}

		// 每个预注册标签只创建一次内容控件，并在创建后同时广播原生和蓝图通知。
		// If the tab content hasn't been created already, create it.
		if (!TabInfo.CreatedTabContentWidget && TabInfo.TabContentType)
		{
			TabInfo.CreatedTabContentWidget = CreateWidget<UCommonUserWidget>(GetOwningPlayer(), TabInfo.TabContentType);
			OnTabContentCreatedNative.Broadcast(TabInfo.TabId, Cast<UCommonUserWidget>(TabInfo.CreatedTabContentWidget));
			OnTabContentCreated.Broadcast(TabInfo.TabId, Cast<UCommonUserWidget>(TabInfo.CreatedTabContentWidget));
		}

		if (UCommonAnimatedSwitcher* CurrentLinkedSwitcher = GetLinkedSwitcher())
		{
			// 将已创建内容加入当前新关联的 Switcher，避免重复添加已有子控件。
			// Add the tab content to the newly linked switcher.
			if (!CurrentLinkedSwitcher->HasChild(TabInfo.CreatedTabContentWidget))
			{
				CurrentLinkedSwitcher->AddChild(TabInfo.CreatedTabContentWidget);
			}
		}

		// 标签按钮尚不存在时才向 Common Tab List 注册，防止重复注册。
		// If the tab is not already registered, register it.
		if (GetTabButtonBaseByID(TabInfo.TabId) == nullptr)
		{
			RegisterTab(TabInfo.TabId, TabInfo.TabButtonType, TabInfo.CreatedTabContentWidget);
		}
	}
}

