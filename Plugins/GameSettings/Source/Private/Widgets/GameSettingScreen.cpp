// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GameSettingScreen.h"

#include "GameSettingCollection.h"
#include "Widgets/GameSettingPanel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingScreen)

class UWidget;

#define LOCTEXT_NAMESPACE "GameSetting"

// 完成设置页面的 CommonActivatableWidget 基础初始化；本类没有额外一次性绑定。
void UGameSettingScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

// 页面激活时让变更跟踪器观察注册表，并同步“设置已修改”状态到界面。
void UGameSettingScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	
	ChangeTracker.WatchRegistry(Registry);

	OnSettingsDirtyStateChanged(HaveSettingsBeenChanged());
}

// 将页面停用生命周期交给父类；设置值是否应用或还原由显式操作决定。
void UGameSettingScreen::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

// 延迟创建并初始化此界面使用的设置注册表，同时让面板和变更跟踪器开始观察。
UGameSettingRegistry* UGameSettingScreen::GetOrCreateRegistry()
{
	if (Registry == nullptr)
	{
		UGameSettingRegistry* NewRegistry = this->CreateRegistry();
		NewRegistry->OnSettingChangedEvent.AddUObject(this, &ThisClass::HandleSettingChanged);

		Settings_Panel->SetRegistry(NewRegistry);

		Registry = NewRegistry;
	}

	return Registry;
}

// 优先把焦点交给设置面板，使键盘和手柄可以直接导航。
UWidget* UGameSettingScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = BP_GetDesiredFocusTarget())
	{
		return Target;
	}

	return Settings_Panel;
}

// 应用变更跟踪器中的全部脏设置，保存注册表持久化数据并更新初始基线。
void UGameSettingScreen::ApplyChanges()
{
	if (ChangeTracker.HaveSettingsBeenChanged())
	{
		ChangeTracker.ApplyChanges();
		ClearDirtyState();
		Registry->SaveChanges();
	}
}

// 把脏设置恢复到进入界面时的初始值，并清除变更状态。
void UGameSettingScreen::CancelChanges()
{
	ChangeTracker.RestoreToInitial();
	ClearDirtyState();
}

// 清除页面变更跟踪器的脏状态。
void UGameSettingScreen::ClearDirtyState()
{
	ChangeTracker.ClearDirtyState();

	OnSettingsDirtyStateChanged(false);
}

// 优先让设置面板返回上一层子页面；无可返回页面时交由外层界面关闭。
bool UGameSettingScreen::AttemptToPopNavigation()
{
	if (Settings_Panel->CanPopNavigationStack())
	{
		Settings_Panel->PopNavigationStack();
		return true;
	}

	return false;
}

// 按开发者名称查找集合，并通过输出参数报告集合是否包含可用设置。
UGameSettingCollection* UGameSettingScreen::GetSettingCollection(FName SettingDevName, bool& HasAnySettings)
{
	HasAnySettings = false;
	
	if (UGameSettingCollection* Collection = GetRegistry()->FindSettingByDevNameChecked<UGameSettingCollection>(SettingDevName))
	{
		TArray<UGameSetting*> InOutSettings;
		
		FGameSettingFilterState FilterState;
		Collection->GetSettingsForFilter(FilterState, InOutSettings);

		HasAnySettings = InOutSettings.Num() > 0;
		
		return Collection;
	}

	return nullptr;
}

// 构造单项名称列表并复用批量导航流程定位设置。
void UGameSettingScreen::NavigateToSetting(FName SettingDevName)
{
	NavigateToSettings({SettingDevName});
}

// 构造仅包含指定开发者名称的过滤条件，并让面板定位到对应设置集合。
void UGameSettingScreen::NavigateToSettings(const TArray<FName>& SettingDevNames)
{
	FGameSettingFilterState FilterState;

	for (const FName SettingDevName : SettingDevNames)
	{
		if (UGameSetting* Setting = GetRegistry()->FindSettingByDevNameChecked<UGameSetting>(SettingDevName))
		{
			FilterState.AddSettingToRootList(Setting);
		}
	}
	
	Settings_Panel->SetFilterState(FilterState);
}

// 接收设置值变化并刷新设置页面，同时保留变化原因供扩展使用。
void UGameSettingScreen::HandleSettingChanged(UGameSetting* Setting, EGameSettingChangeReason Reason)
{
	OnSettingsDirtyStateChanged(true);
}

#undef LOCTEXT_NAMESPACE
