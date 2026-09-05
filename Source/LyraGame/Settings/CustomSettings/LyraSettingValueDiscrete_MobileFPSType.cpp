// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_MobileFPSType.h"

#include "Performance/LyraPerformanceSettings.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_MobileFPSType)

#define LOCTEXT_NAMESPACE "LyraSettings"

// 构造移动端帧率档位设置项并初始化内部状态。
ULyraSettingValueDiscrete_MobileFPSType::ULyraSettingValueDiscrete_MobileFPSType()
{
}

// 初始化移动端帧率档位选项、数据源及依赖关系。
void ULyraSettingValueDiscrete_MobileFPSType::OnInitialized()
{
	Super::OnInitialized();

	const ULyraPlatformSpecificRenderingSettings* PlatformSettings = ULyraPlatformSpecificRenderingSettings::Get();
	const ULyraSettingsLocal* UserSettings = ULyraSettingsLocal::Get();

	for (int32 TestLimit : PlatformSettings->MobileFrameRateLimits)
	{
		if (ULyraSettingsLocal::IsSupportedMobileFramePace(TestLimit))
		{
			FPSOptions.Add(TestLimit, MakeLimitString(TestLimit));
		}
	}

	const int32 FirstFrameRateWithQualityLimit = UserSettings->GetFirstFrameRateWithQualityLimit();
	if (FirstFrameRateWithQualityLimit > 0)
	{
		SetWarningRichText(FText::Format(LOCTEXT("MobileFPSType_Note", "<strong>Note: Changing the framerate setting to {0} or higher might lower your Quality Presets.</>"), MakeLimitString(FirstFrameRateWithQualityLimit)));
	}
}

// 返回平台配置的移动端默认 FPS。
int32 ULyraSettingValueDiscrete_MobileFPSType::GetDefaultFPS() const
{
	return ULyraSettingsLocal::GetDefaultMobileFrameRate();
}

// 将 FPS 数值格式化为设置界面使用的限制文本。
FText ULyraSettingValueDiscrete_MobileFPSType::MakeLimitString(int32 Number)
{
	return FText::Format(LOCTEXT("MobileFrameRateOption", "{0} FPS"), FText::AsNumber(Number));
}

// 记录当前移动端帧率档位，供取消修改时恢复。
void ULyraSettingValueDiscrete_MobileFPSType::StoreInitial()
{
	InitialValue = GetValue();
}

// 将移动端帧率档位重置为默认值并通知设置系统。
void ULyraSettingValueDiscrete_MobileFPSType::ResetToDefault()
{
	SetValue(GetDefaultFPS(), EGameSettingChangeReason::ResetToDefault);
}

// 恢复初始化时记录的移动端帧率档位并通知设置系统。
void ULyraSettingValueDiscrete_MobileFPSType::RestoreToInitial()
{
	SetValue(InitialValue, EGameSettingChangeReason::RestoreToInitial);
}

// 应用索引对应的移动端 FPS；索引无效时回退到设备默认帧率。
void ULyraSettingValueDiscrete_MobileFPSType::SetDiscreteOptionByIndex(int32 Index)
{
	TArray<int32> FPSOptionsModes;
	FPSOptions.GenerateKeyArray(FPSOptionsModes);

	int32 NewMode = FPSOptionsModes.IsValidIndex(Index) ? FPSOptionsModes[Index] : GetDefaultFPS();

	SetValue(NewMode, EGameSettingChangeReason::Change);
}

// 返回当前期望 FPS 在可用档位中的索引；未找到时返回 INDEX_NONE。
int32 ULyraSettingValueDiscrete_MobileFPSType::GetDiscreteOptionIndex() const
{
	TArray<int32> FPSOptionsModes;
	FPSOptions.GenerateKeyArray(FPSOptionsModes);
	return FPSOptionsModes.IndexOfByKey(GetValue());
}

// 返回当前可供界面显示的移动端帧率档位选项文本。
TArray<FText> ULyraSettingValueDiscrete_MobileFPSType::GetDiscreteOptions() const
{
	TArray<FText> Options;
	FPSOptions.GenerateValueArray(Options);

	return Options;
}

// 返回本地设置中用户期望的移动端帧率上限。
int32 ULyraSettingValueDiscrete_MobileFPSType::GetValue() const
{
	return ULyraSettingsLocal::Get()->GetDesiredMobileFrameRateLimit();
}

// 更新用户期望的移动端帧率上限，并按变更原因通知设置系统。
void ULyraSettingValueDiscrete_MobileFPSType::SetValue(int32 NewLimitFPS, EGameSettingChangeReason InReason)
{
	ULyraSettingsLocal::Get()->SetDesiredMobileFrameRateLimit(NewLimitFPS);

	NotifySettingChanged(InReason);
}

#undef LOCTEXT_NAMESPACE

