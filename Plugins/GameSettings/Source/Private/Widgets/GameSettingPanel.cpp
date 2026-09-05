// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GameSettingPanel.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "GameSettingRegistry.h"
#include "Widgets/GameSettingDetailView.h"
#include "Widgets/GameSettingListView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingPanel)

class SWidget;
struct FFocusEvent;
struct FGeometry;

// 创建可接收焦点的设置面板，使键盘和手柄能够直接进入列表导航。
UGameSettingPanel::UGameSettingPanel()
{
	SetIsFocusable(true);
}

// 订阅设置列表的悬停与选择事件，用于驱动右侧详情内容。
void UGameSettingPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ListView_Settings->OnItemIsHoveredChanged().AddUObject(this, &ThisClass::HandleSettingItemHoveredChanged);
	ListView_Settings->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleSettingItemSelectionChanged);
}

// 面板进入运行时后重新绑定当前注册表事件，避免重复订阅。
void UGameSettingPanel::NativeConstruct()
{
	Super::NativeConstruct();

	UnregisterRegistryEvents();
	RegisterRegistryEvents();
}

// 销毁设置面板前解除运行时事件，避免对象池或页面关闭后继续回调。
void UGameSettingPanel::NativeDestruct()
{
	Super::NativeDestruct();

	UnregisterRegistryEvents();
}

// 接收焦点后把焦点转交给适合手柄操作的内部控件。
FReply UGameSettingPanel::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	const UCommonInputSubsystem* InputSubsystem = GetInputSubsystem();
	if (InputSubsystem && InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		if (TSharedPtr<SWidget> PrimarySlateWidget = ListView_Settings->GetCachedWidget())
		{
			ListView_Settings->NavigateToIndex(0);
			ListView_Settings->SetSelectedIndex(0);

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

// 切换设置注册表，解除旧事件、绑定新事件并刷新当前列表。
void UGameSettingPanel::SetRegistry(UGameSettingRegistry* InRegistry)
{
	if (Registry != InRegistry)
	{
		UnregisterRegistryEvents();

		if (RefreshHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(RefreshHandle);
		}

		Registry = InRegistry;

		RegisterRegistryEvents();

		RefreshSettingsList();
	}
}

// 订阅注册表的动作、导航和编辑状态事件。
void UGameSettingPanel::RegisterRegistryEvents()
{
	if (Registry)
	{
		Registry->OnSettingEditConditionChangedEvent.AddUObject(this, &ThisClass::HandleSettingEditConditionsChanged);
		Registry->OnSettingNamedActionEvent.AddUObject(this, &ThisClass::HandleSettingNamedAction);
		Registry->OnExecuteNavigationEvent.AddUObject(this, &ThisClass::HandleSettingNavigation);
	}
}

// 解除注册表事件订阅，防止切换注册表或销毁面板后收到回调。
void UGameSettingPanel::UnregisterRegistryEvents()
{
	if (Registry)
	{
		Registry->OnSettingEditConditionChangedEvent.RemoveAll(this);
		Registry->OnSettingNamedActionEvent.RemoveAll(this);
		Registry->OnExecuteNavigationEvent.RemoveAll(this);
	}
}

// 更新列表过滤条件；可选清空页面导航栈，然后重新生成可见设置。
void UGameSettingPanel::SetFilterState(const FGameSettingFilterState& InFilterState, bool bClearNavigationStack)
{
	FilterState = InFilterState;

	if (bClearNavigationStack)
	{
		FilterNavigationStack.Reset();
	}

	RefreshSettingsList();
}

// 判断设置面板是否保存了可返回的上层过滤状态。
bool UGameSettingPanel::CanPopNavigationStack() const
{
	return FilterNavigationStack.Num() > 0;
}

// 返回上一层过滤状态并刷新列表；导航栈为空时保持当前页面。
void UGameSettingPanel::PopNavigationStack()
{
	if (FilterNavigationStack.Num() > 0)
	{
		FilterState = FilterNavigationStack.Pop();
		RefreshSettingsList();
	}
}

// 保存当前过滤状态，改为仅展示目标设置的子项，并刷新列表。
void UGameSettingPanel::HandleSettingNavigation(UGameSetting* Setting)
{
	if (VisibleSettings.Contains(Setting))
	{
		FilterNavigationStack.Push(FilterState);

		FGameSettingFilterState NewPageFilterState;
		NewPageFilterState.AddSettingToRootList(Setting);
		SetFilterState(NewPageFilterState, false);
	}
}

// 从当前页面候选设置中排除不可变项，返回允许恢复默认值的设置。
TArray<UGameSetting*> UGameSettingPanel::GetSettingsWeCanResetToDefault() const
{
	TArray<UGameSetting*> AvailableSettings;

	if (ensure(Registry->IsFinishedInitializing()))
	{
		// 为了取得本页面所有潜在设置，沿用相同允许列表，但忽略其他限制条件。
		// We want to get all available settings on this "screen" so we include the same allowlist, but ignore 
		FGameSettingFilterState AllAvailableFilter = FilterState;
		AllAvailableFilter.bIncludeDisabled = true;
		AllAvailableFilter.bIncludeHidden = true;
		AllAvailableFilter.bIncludeResetable = false;
		AllAvailableFilter.bIncludeNestedPages = false;

		Registry->GetSettingsForFilter(AllAvailableFilter, AvailableSettings);
	}

	return AvailableSettings;
}

// 按注册表与过滤状态重建列表、修正选择和焦点，并只刷新一次可编辑状态。
void UGameSettingPanel::RefreshSettingsList()
{
	if (RefreshHandle.IsValid())
	{
		return;
	}

	RefreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameSettingPanel_RefreshSettingsList);

		if (Registry->IsFinishedInitializing())
		{
			VisibleSettings.Reset();
			Registry->GetSettingsForFilter(FilterState, MutableView(VisibleSettings));

			ListView_Settings->SetListItems(VisibleSettings);

			RefreshHandle.Reset();

			int32 IndexToSelect = 0;
			if (DesiredSelectionPostRefresh != NAME_None)
			{
				for (int32 SettingIdx = 0; SettingIdx < VisibleSettings.Num(); ++SettingIdx)
				{
					UGameSetting* Setting = VisibleSettings[SettingIdx];
					if (Setting->GetDevName() == DesiredSelectionPostRefresh)
					{
						IndexToSelect = SettingIdx;
						break;
					}
				}
				DesiredSelectionPostRefresh = NAME_None;
			}

			// 如果焦点仍停留在列表本身而非条目上，通常说明面板收到焦点时条目尚未创建；此时把焦点转到第一项。
			// If the list directly has the focus, instead of a child widget, then it's likely the panel and items
			// were not yet available when we received focus, so lets go ahead and focus the first item now.
			//if (HasUserFocus(GetOwningPlayer()))
			if (bAdjustListViewPostRefresh)
			{
				ListView_Settings->NavigateToIndex(IndexToSelect);
				ListView_Settings->SetSelectedIndex(IndexToSelect);
			}

			bAdjustListViewPostRefresh = true;

			// 最后统一刷新一次可编辑状态，避免每个注册步骤重复计算。
			// finally, refresh the editable state, but only once.
			for (int32 SettingIdx = 0; SettingIdx < VisibleSettings.Num(); ++SettingIdx)
			{
				if (UGameSetting* Setting = VisibleSettings[SettingIdx])
				{
					Setting->RefreshEditableState(false);
				}
			}			

			return false;
		}

		return true;
	}));
}

// 悬停条目变化时更新详情面板，但保留键盘或手柄的当前选择。
void UGameSettingPanel::HandleSettingItemHoveredChanged(UObject* Item, bool bHovered)
{
	UGameSetting* Setting = bHovered ? Cast<UGameSetting>(Item) : ToRawPtr(LastHoveredOrSelectedSetting);
	if (bHovered && Setting)
	{
		LastHoveredOrSelectedSetting = Setting;
	}

	FillSettingDetails(Setting);
}

// 列表选择变化时更新详情面板中的当前设置。
void UGameSettingPanel::HandleSettingItemSelectionChanged(UObject* Item)
{
	UGameSetting* Setting = Cast<UGameSetting>(Item);
	if (Setting)
	{
		LastHoveredOrSelectedSetting = Setting;
	}

	FillSettingDetails(Cast<UGameSetting>(Item));
}

// 将指定设置交给详情视图展示；空设置会清空详情。
void UGameSettingPanel::FillSettingDetails(UGameSetting* InSetting)
{
	if (Details_Settings)
	{
		Details_Settings->FillSettingDetails(InSetting);
	}

	OnFocusedSettingChanged.Broadcast(InSetting);
}

// 将设置发出的命名动作交给面板蓝图扩展点处理。
void UGameSettingPanel::HandleSettingNamedAction(UGameSetting* Setting, FGameplayTag GameSettings_Action_Tag)
{
	BP_OnExecuteNamedAction.Broadcast(Setting, GameSettings_Action_Tag);
}

// 设置编辑条件变化后刷新列表和详情，反映新的可见性与选项。
void UGameSettingPanel::HandleSettingEditConditionsChanged(UGameSetting* Setting)
{
	const bool bWasSettingVisible = VisibleSettings.Contains(Setting);
	const bool bIsSettingVisible = Setting->GetEditState().IsVisible();

	if (bIsSettingVisible != bWasSettingVisible)
	{
		bAdjustListViewPostRefresh = Setting->GetAdjustListViewPostRefresh();
		RefreshSettingsList();
	}
}

// 按开发者名称在当前列表中查找并选中设置。
void UGameSettingPanel::SelectSetting(const FName& SettingDevName)
{
	DesiredSelectionPostRefresh = SettingDevName;
	RefreshSettingsList();
}

// 返回列表当前选中的设置对象；没有选择时返回空指针。
UGameSetting* UGameSettingPanel::GetSelectedSetting() const
{
	return Cast<UGameSetting>(ListView_Settings->GetSelectedItem());
}

