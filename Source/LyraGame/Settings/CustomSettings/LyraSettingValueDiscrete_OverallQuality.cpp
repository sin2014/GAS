// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_OverallQuality.h"

#include "Engine/Engine.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_OverallQuality)

#define LOCTEXT_NAMESPACE "LyraSettings"

// 构造整体画质档位设置项并初始化内部状态。
ULyraSettingValueDiscrete_OverallQuality::ULyraSettingValueDiscrete_OverallQuality()
{
}

// 初始化整体画质档位选项、数据源及依赖关系。
void ULyraSettingValueDiscrete_OverallQuality::OnInitialized()
{
	Super::OnInitialized();

	ULyraSettingsLocal* UserSettings = ULyraSettingsLocal::Get();
	const int32 MaxQualityLevel = UserSettings->GetMaxSupportedOverallQualityLevel();

	auto AddOptionIfPossible = [&](int Index, FText&& Value) { if ((MaxQualityLevel < 0) || (Index <= MaxQualityLevel)) { Options.Add(Value); }};

	AddOptionIfPossible(0, LOCTEXT("VideoQualityOverall_Low", "Low"));
	AddOptionIfPossible(1, LOCTEXT("VideoQualityOverall_Medium", "Medium"));
	AddOptionIfPossible(2, LOCTEXT("VideoQualityOverall_High", "High"));
	AddOptionIfPossible(3, LOCTEXT("VideoQualityOverall_Epic", "Epic"));

	OptionsWithCustom = Options;
	OptionsWithCustom.Add(LOCTEXT("VideoQualityOverall_Custom", "Custom"));

	const int32 LowestQualityWithFrameRateLimit = UserSettings->GetLowestQualityWithFrameRateLimit();
	if (Options.IsValidIndex(LowestQualityWithFrameRateLimit))
	{
		SetWarningRichText(FText::Format(LOCTEXT("OverallQuality_Mobile_ImpactsFramerate", "<strong>Note: Changing the Quality setting to {0} or higher might limit your framerate.</>"), Options[LowestQualityWithFrameRateLimit]));
	}
}

// 此设置项不单独保存整体画质初始值，状态由 UGameUserSettings 管理。
void ULyraSettingValueDiscrete_OverallQuality::StoreInitial()
{
}

// 当前未在此离散设置项中实现独立的整体画质默认重置。
void ULyraSettingValueDiscrete_OverallQuality::ResetToDefault()
{
}

// 当前未在此离散设置项中实现独立的整体画质初始值恢复。
void ULyraSettingValueDiscrete_OverallQuality::RestoreToInitial()
{
}

// 选择“自定义”时保留各通道现值，否则将索引作为统一可伸缩性等级应用。
void ULyraSettingValueDiscrete_OverallQuality::SetDiscreteOptionByIndex(int32 Index)
{
	UGameUserSettings* UserSettings = CastChecked<UGameUserSettings>(GEngine->GetGameUserSettings());

	if (Index == GetCustomOptionIndex())
	{
		// 选择“自定义”时保留各画质通道现有值，不套用统一预设。
		// Leave everything as is we're in a custom setup.
	}
	else
	{
		// 其余索引依次对应低、中、高和史诗整体画质预设。
		// Low / Medium / High / Epic
		UserSettings->SetOverallScalabilityLevel(Index);
	}

	NotifySettingChanged(EGameSettingChangeReason::Change);
}

// 返回当前统一画质等级；各通道不一致时返回“自定义”选项索引。
int32 ULyraSettingValueDiscrete_OverallQuality::GetDiscreteOptionIndex() const
{
	const int32 OverallQualityLevel = GetOverallQualityLevel();
	if (OverallQualityLevel == INDEX_NONE)
	{
		return GetCustomOptionIndex();
	}

	return OverallQualityLevel;
}

// 返回当前可供界面显示的整体画质档位选项文本。
TArray<FText> ULyraSettingValueDiscrete_OverallQuality::GetDiscreteOptions() const
{
	const int32 OverallQualityLevel = GetOverallQualityLevel();
	if (OverallQualityLevel == INDEX_NONE)
	{
		return OptionsWithCustom;
	}
	else
	{
		return Options;
	}
}

// 返回“自定义”画质选项在离散列表中的索引。
int32 ULyraSettingValueDiscrete_OverallQuality::GetCustomOptionIndex() const
{
	return OptionsWithCustom.Num() - 1;
}

// 返回本地设置当前统一画质等级；各通道不一致时返回自定义状态。
int32 ULyraSettingValueDiscrete_OverallQuality::GetOverallQualityLevel() const
{
	const UGameUserSettings* UserSettings = CastChecked<const UGameUserSettings>(GEngine->GetGameUserSettings());
	return UserSettings->GetOverallScalabilityLevel();
}

#undef LOCTEXT_NAMESPACE
