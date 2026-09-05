// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_Display.h"

#include "GameFramework/GameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_Display)

#define LOCTEXT_NAMESPACE "LyraSettings"

// 构造目标显示器设置项并初始化内部状态。
ULyraSettingValueDiscrete_Display::ULyraSettingValueDiscrete_Display()
{
}

// 销毁设置项前解除与目标显示器相关的外部事件监听。
void ULyraSettingValueDiscrete_Display::BeginDestroy()
{
	Super::BeginDestroy();

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<class GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
		if (PlatformApplication.IsValid())
		{
			GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
			DisplayMetricsChangedEvent.Remove(DisplayMetricsChangedHandle);
		}
	}
}

// 初始化目标显示器选项、数据源及依赖关系。
void ULyraSettingValueDiscrete_Display::OnInitialized()
{
	Super::OnInitialized();

	TSharedPtr<class GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (ensure(PlatformApplication.IsValid()))
	{
		FDisplayMetrics::RebuildDisplayMetrics(CurrentDisplayMetrics);

		GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
		if (!DisplayMetricsChangedEvent.IsBoundToObject(this))
		{
			DisplayMetricsChangedHandle = DisplayMetricsChangedEvent.AddUObject(this, &ULyraSettingValueDiscrete_Display::OnDisplayMetricsChanged);
		}
	}
}

// 记录当前目标显示器，供取消修改时恢复。
void ULyraSettingValueDiscrete_Display::StoreInitial()
{
	const UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();
	InitialMonitorID = UserSettings->GetDisplayID();
	InitialMonitorIndex = UserSettings->GetDisplayIndex();
}

// 当前未实现显示器选项的独立默认重置，保留现有选择。
void ULyraSettingValueDiscrete_Display::ResetToDefault()
{
	// 当前没有定义显示器选项的默认重置行为。
	// Initially not implemented.
}

// 当前未实现显示器选项的独立初始值恢复，交由外部视频设置流程处理。
void ULyraSettingValueDiscrete_Display::RestoreToInitial()
{
	// 当前没有定义将显示器选项恢复到进入界面时状态的行为。
	// Initially not implemented.
}

// 将有效索引对应的选项写入目标显示器；索引无效时不修改。
void ULyraSettingValueDiscrete_Display::SetDiscreteOptionByIndex(int32 Index)
{
	if (CurrentDisplayMetrics.MonitorInfo.IsValidIndex(Index))
	{
		GEngine->GetGameUserSettings()->SetDisplayProperties(CurrentDisplayMetrics.MonitorInfo[Index].ID, Index);
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

// 按保存的显示器 ID 和索引返回最接近的当前显示器选项。
int32 ULyraSettingValueDiscrete_Display::GetDiscreteOptionIndex() const
{
	const UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();

	return CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(UserSettings->GetDisplayID(), UserSettings->GetDisplayIndex());
}

// 按进入设置界面时记录的显示器 ID 和索引返回最接近的默认选项。
int32 ULyraSettingValueDiscrete_Display::GetDiscreteOptionDefaultIndex() const
{
	return CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(InitialMonitorID, InitialMonitorIndex);
}

// 返回当前可供界面显示的目标显示器选项文本。
TArray<FText> ULyraSettingValueDiscrete_Display::GetDiscreteOptions() const
{
	TArray<FText> Options;

	static FText UnknownDisplayText = LOCTEXT("UnknownDisplay", "[Unknown]");
	if (CurrentDisplayMetrics.MonitorInfo.IsEmpty())
	{
		Options.Emplace(UnknownDisplayText);
	}
	else
	{
		for (const FMonitorInfo& Monitor : CurrentDisplayMetrics.MonitorInfo)
		{
			Options.Emplace(Monitor.FriendlyName.IsEmpty()
				? (Monitor.Name.IsEmpty()
					? UnknownDisplayText
					: FText::FromString(Monitor.Name))
				: FText::FromString(Monitor.FriendlyName));
		}
	}

	return Options;
}

// 依赖设置变化后将已保存的显示器身份重新映射到当前显示器列表。
void ULyraSettingValueDiscrete_Display::OnDependencyChanged()
{
	UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();
	const FString DisplayID = UserSettings->GetDisplayID();
	const int32 DisplayIndex = UserSettings->GetDisplayIndex();
	SetDiscreteOptionByIndex(CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(DisplayID, DisplayIndex));
}

// 显示器拓扑或工作区变化后重新生成目标显示器选项。
void ULyraSettingValueDiscrete_Display::OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics)
{
	CurrentDisplayMetrics = NewDisplayMetrics;
}

#undef LOCTEXT_NAMESPACE
