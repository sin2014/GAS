// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_PerfStat.h"

#include "CommonUIVisibilitySubsystem.h"
#include "Performance/LyraPerformanceSettings.h"
#include "Performance/LyraPerformanceStatTypes.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_PerfStat)

class ULocalPlayer;

#define LOCTEXT_NAMESPACE "LyraSettings"

//////////////////////////////////////////////////////////////////////

class FGameSettingEditCondition_PerfStatAllowed : public FGameSettingEditCondition
{
public:
	// 按性能统计类型复用并返回对应的编辑条件实例。
	static TSharedRef<FGameSettingEditCondition_PerfStatAllowed> Get(ELyraDisplayablePerformanceStat Stat)
	{
		return MakeShared<FGameSettingEditCondition_PerfStatAllowed>(Stat);
	}

	// 记录此编辑条件所约束的性能统计类型。
	FGameSettingEditCondition_PerfStatAllowed(ELyraDisplayablePerformanceStat Stat)
		: AssociatedStat(Stat)
	{
	}

	//~FGameSettingEditCondition interface
	// 根据平台允许显示的性能统计集合决定设置项是否可编辑。
	virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override
	{
		const FGameplayTagContainer& VisibilityTags = UCommonUIVisibilitySubsystem::GetChecked(InLocalPlayer)->GetVisibilityTags();

		bool bCanShowStat = false;
		for (const FLyraPerformanceStatGroup& Group : GetDefault<ULyraPerformanceSettings>()->UserFacingPerformanceStats) /* @TODO：考虑将性能统计项的可见性配置改为按平台提供，避免运行时额外执行可见性标签查询。 */ //@TODO: Move this stuff to per-platform instead of doing vis queries too?
		{
			if (Group.AllowedStats.Contains(AssociatedStat))
			{
				const bool bShowGroup = (Group.VisibilityQuery.IsEmpty() || Group.VisibilityQuery.Matches(VisibilityTags));
				if (bShowGroup)
				{
					bCanShowStat = true;
					break;
				}
			}
		}

		if (!bCanShowStat)
		{
			InOutEditState.Hide(TEXT("Stat is not listed in ULyraPerformanceSettings or is suppressed by current platform traits"));
		}
	}
	//~End of FGameSettingEditCondition interface

private:
	ELyraDisplayablePerformanceStat AssociatedStat;
};

//////////////////////////////////////////////////////////////////////

// 构造性能统计显示模式设置项并初始化内部状态。
ULyraSettingValueDiscrete_PerfStat::ULyraSettingValueDiscrete_PerfStat()
{
}

// 指定此设置项控制的性能统计类型，并附加相应平台编辑条件。
void ULyraSettingValueDiscrete_PerfStat::SetStat(ELyraDisplayablePerformanceStat InStat)
{
	StatToDisplay = InStat;
	SetDevName(FName(*FString::Printf(TEXT("PerfStat_%d"), (int32)StatToDisplay)));
	AddEditCondition(FGameSettingEditCondition_PerfStatAllowed::Get(StatToDisplay));
}

// 向性能统计设置项追加一个显示标签与模式的对应关系。
void ULyraSettingValueDiscrete_PerfStat::AddMode(FText&& Label, ELyraStatDisplayMode Mode)
{
	Options.Emplace(MoveTemp(Label));
	DisplayModes.Add(Mode);
}

// 初始化性能统计显示模式选项、数据源及依赖关系。
void ULyraSettingValueDiscrete_PerfStat::OnInitialized()
{
	Super::OnInitialized();

	AddMode(LOCTEXT("PerfStatDisplayMode_None", "None"), ELyraStatDisplayMode::Hidden);
	AddMode(LOCTEXT("PerfStatDisplayMode_TextOnly", "Text Only"), ELyraStatDisplayMode::TextOnly);
	AddMode(LOCTEXT("PerfStatDisplayMode_GraphOnly", "Graph Only"), ELyraStatDisplayMode::GraphOnly);
	AddMode(LOCTEXT("PerfStatDisplayMode_TextAndGraph", "Text and Graph"), ELyraStatDisplayMode::TextAndGraph);
}

// 记录当前性能统计显示模式，供取消修改时恢复。
void ULyraSettingValueDiscrete_PerfStat::StoreInitial()
{
	const ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	InitialMode = Settings->GetPerfStatDisplayState(StatToDisplay);
}

// 将性能统计显示模式重置为默认值并通知设置系统。
void ULyraSettingValueDiscrete_PerfStat::ResetToDefault()
{
	ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	Settings->SetPerfStatDisplayState(StatToDisplay, ELyraStatDisplayMode::Hidden);
	NotifySettingChanged(EGameSettingChangeReason::ResetToDefault);
}

// 恢复初始化时记录的性能统计显示模式并通知设置系统。
void ULyraSettingValueDiscrete_PerfStat::RestoreToInitial()
{
	ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	Settings->SetPerfStatDisplayState(StatToDisplay, InitialMode);
	NotifySettingChanged(EGameSettingChangeReason::RestoreToInitial);
}

// 将有效索引对应的选项写入性能统计显示模式；索引无效时不修改。
void ULyraSettingValueDiscrete_PerfStat::SetDiscreteOptionByIndex(int32 Index)
{
	if (DisplayModes.IsValidIndex(Index))
	{
		const ELyraStatDisplayMode DisplayMode = DisplayModes[Index];

		ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
		Settings->SetPerfStatDisplayState(StatToDisplay, DisplayMode);
	}
	NotifySettingChanged(EGameSettingChangeReason::Change);
}

// 返回当前显示模式在可选模式数组中的索引；未找到时返回 INDEX_NONE。
int32 ULyraSettingValueDiscrete_PerfStat::GetDiscreteOptionIndex() const
{
	const ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	return DisplayModes.Find(Settings->GetPerfStatDisplayState(StatToDisplay));
}

// 返回当前可供界面显示的性能统计显示模式选项文本。
TArray<FText> ULyraSettingValueDiscrete_PerfStat::GetDiscreteOptions() const
{
	return Options;
}

#undef LOCTEXT_NAMESPACE
