// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingRegistryChangeTracker.h"

#include "GameSettingRegistry.h"
#include "GameSettingValue.h"

#define LOCTEXT_NAMESPACE "GameSetting"

// 创建尚未观察注册表的变更跟踪器，脏设置集合初始为空。
FGameSettingRegistryChangeTracker::FGameSettingRegistryChangeTracker()
{
}

// 销毁注册表变更跟踪器前解除仍在生效的事件订阅和外部引用。
FGameSettingRegistryChangeTracker::~FGameSettingRegistryChangeTracker()
{
	if (UGameSettingRegistry* StrongRegistry = Registry.Get())
	{
		StrongRegistry->OnSettingChangedEvent.RemoveAll(this);
	}
}

// 切换到新的设置注册表，订阅值变化事件并重新建立当前脏状态基线。
void FGameSettingRegistryChangeTracker::WatchRegistry(UGameSettingRegistry* InRegistry)
{
	ClearDirtyState();
	StopWatchingRegistry();

	if (Registry.Get() != InRegistry)
	{
		Registry = InRegistry;
		InRegistry->OnSettingChangedEvent.AddRaw(this, &FGameSettingRegistryChangeTracker::HandleSettingChanged);
	}
}

// 解除注册表事件订阅并清空弱引用，避免继续接收已离开界面的设置变化。
void FGameSettingRegistryChangeTracker::StopWatchingRegistry()
{
	if (UGameSettingRegistry* StrongRegistry = Registry.Get())
	{
		StrongRegistry->OnSettingChangedEvent.RemoveAll(this);
		Registry.Reset();
	}
}

// 清空已修改设置集合，并广播脏状态已更新。
void FGameSettingRegistryChangeTracker::ClearDirtyState()
{
	ensure(!bRestoringSettings);
	if (bRestoringSettings)
	{
		return;
	}

	bSettingsChanged = false;
	DirtySettings.Reset();
}

// 逐项应用已修改设置、刷新其初始值基线，然后清除脏状态。
void FGameSettingRegistryChangeTracker::ApplyChanges()
{
	for (auto Entry : DirtySettings)
	{
		if (UGameSettingValue* SettingValue = Cast<UGameSettingValue>(Entry.Value))
		{
			SettingValue->Apply();
			SettingValue->StoreInitial();
		}
	}

	ClearDirtyState();
}

// 将所有已修改设置还原到进入界面时的初始值，然后清除脏状态。
void FGameSettingRegistryChangeTracker::RestoreToInitial()
{
	ensure(!bRestoringSettings);
	if (bRestoringSettings)
	{
		return;
	}

	{
		TGuardValue<bool> LocalGuard(bRestoringSettings, true);
		for (auto Entry : DirtySettings)
		{
			if (UGameSettingValue* SettingValue = Cast<UGameSettingValue>(Entry.Value))
			{
				SettingValue->RestoreToInitial();
			}
		}
	}

	ClearDirtyState();
}

// 根据变化原因维护脏设置集合；恢复到初始值时移除，否则加入，并广播状态变化。
void FGameSettingRegistryChangeTracker::HandleSettingChanged(UGameSetting* Setting, EGameSettingChangeReason Reason)
{
	if (bRestoringSettings)
	{
		return;
	}

	bSettingsChanged = true;
	DirtySettings.Add(FObjectKey(Setting), Setting);
}

#undef LOCTEXT_NAMESPACE
