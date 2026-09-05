// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraHDRCalibrationEditor.h"

#include "ColorManagement/TransferFunctions.h"
#include "CommonButtonBase.h"
#include "CommonRichTextBlock.h"
#include "Components/Image.h"
#include "Components/WidgetSwitcher.h"
#include "GameSettingValueScalar.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraHDRCalibrationEditor)

struct FGeometry;

#define LOCTEXT_NAMESPACE "Lyra"

namespace HDRCalibrationEditor
{
	// 忽略幅度低于该值的手柄摇杆输入。
	const float JoystickDeadZone = .2f;
	// 每单位模拟输入或滚轮增量对应的 PQ 编码变化量。
	const float HDRCalibrationChangeSpeed = .01f;
	// 将可校准的最低峰值亮度限制在约 102 尼特对应的 PQ 值。
	const float HDRCalibrationMinimumPQ = .51f; /* PQ 值约 0.51 对应约 102 尼特，接近 SDR 的 100 尼特参考亮度。 */ // about 102 nits, first hundredths value about SDR
}

// 构造 HDR 校准界面并设置其输入与焦点行为。
ULyraHDRCalibrationEditor::ULyraHDRCalibrationEditor(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

// 激活 HDR 校准界面，暂存 UI 亮度状态并将校准图样亮度范围扩展到 10000 尼特。
void ULyraHDRCalibrationEditor::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 校准期间将 UI 渲染亮度设为 10000 尼特，使校准图样覆盖 HDR 峰值亮度范围。
	// Render UI to 10000 nits for calibration.
	IConsoleVariable* const CVarUILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level"));
	bStartingUILevel = CVarUILevel->GetFloat();
	CVarUILevel->SetWithCurrentPriority(1.f);
	ULyraSettingsLocal* const Settings = ULyraSettingsLocal::Get();
	bStartingUILuminance = Settings->GetHDRUILuminanceNits();
	Settings->SetHDRUILuminanceNits(10000.f);
	bStartingUILuminanceSeparate = Settings->IsHDRUILuminanceSeparate();
	Settings->SetHDRUILuminanceSeparate(true);

	const float MaxLuminance = Settings->GetMaximumHDRDisplayNits() / 10000.f;
	MaxLuminancePQ = UE::Color::EncodeNormalizedToST2084(MaxLuminance);
	OnMaxLuminanceChange(MaxLuminance);

	Button_Done->OnClicked().AddUObject(this, &ULyraHDRCalibrationEditor::HandleDoneClicked);

	Button_Back->SetVisibility((bCanCancel)? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCanCancel)
	{
		Button_Back->OnClicked().AddUObject(this, &ULyraHDRCalibrationEditor::HandleBackClicked);
	}
}

// 停用 HDR 校准界面时恢复进入前保存的 UI HDR 亮度状态。
void ULyraHDRCalibrationEditor::NativeOnDeactivated()
{
	// 退出校准界面时恢复进入前保存的 UI HDR 控制台变量。
	// Restore UI CVars.
	IConsoleVariable* const CVarUILevel = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.UI.Level"));
	CVarUILevel->SetWithCurrentPriority(bStartingUILevel);
	ULyraSettingsLocal* const Settings = ULyraSettingsLocal::Get();
	Settings->SetHDRUILuminanceNits(bStartingUILuminance);
	Settings->SetHDRUILuminanceSeparate(bStartingUILuminanceSeparate);

	Super::NativeOnDeactivated();
}

// 从动作设置的首个子项取得最大亮度数据源；没有可用子项时使用本地设置兜底。
bool ULyraHDRCalibrationEditor::ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting)
{
	if (InSetting)
	{
		TArray<UGameSetting*> ChildSettings = InSetting->GetChildSettings();
		if (!ChildSettings.IsEmpty())
		{
			ValueSetting = Cast<UGameSettingValueScalar>(ChildSettings[0]);
		}
	}

	return true;
}

// 使用左摇杆纵轴在 PQ 范围内调整 HDR 峰值亮度；输入未越过死区时交由基类处理。
FReply ULyraHDRCalibrationEditor::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY && FMath::Abs(InAnalogEvent.GetAnalogValue()) >= HDRCalibrationEditor::JoystickDeadZone)
	{
		const float UnclampedPQ = MaxLuminancePQ + InAnalogEvent.GetAnalogValue() * HDRCalibrationEditor::HDRCalibrationChangeSpeed;
		MaxLuminancePQ = FMath::Clamp(UnclampedPQ, HDRCalibrationEditor::HDRCalibrationMinimumPQ, 1.f);
		OnMaxLuminanceChange(UE::Color::DecodeNormalizedFromST2084(MaxLuminancePQ));
		
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

// 使用鼠标滚轮在受限 PQ 范围内调整 HDR 峰值亮度，并消费该输入。
FReply ULyraHDRCalibrationEditor::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const float UnclampedPQ = MaxLuminancePQ + InMouseEvent.GetWheelDelta() * HDRCalibrationEditor::HDRCalibrationChangeSpeed;
	MaxLuminancePQ = FMath::Clamp(UnclampedPQ, HDRCalibrationEditor::HDRCalibrationMinimumPQ, 1.f);
	OnMaxLuminanceChange(UE::Color::DecodeNormalizedFromST2084(MaxLuminancePQ));

	return FReply::Handled();
}

// 不提交当前峰值亮度，仅关闭 HDR 校准界面。
void ULyraHDRCalibrationEditor::HandleBackClicked()
{
	DeactivateWidget();
}

// 将 PQ 值解码为尼特并写入绑定设置或本地设置，然后关闭界面。
void ULyraHDRCalibrationEditor::HandleDoneClicked()
{
	const float MaxLuminance = UE::Color::DecodeST2084(MaxLuminancePQ);
	if (ValueSetting.IsValid())
	{
		ValueSetting.Get()->SetValue(MaxLuminance);
	}
	else
	{
		ULyraSettingsLocal::Get()->SetMaximumHDRDisplayNits(MaxLuminance);
	}
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
