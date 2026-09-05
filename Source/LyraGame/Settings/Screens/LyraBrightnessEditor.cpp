// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraBrightnessEditor.h"

#include "CommonButtonBase.h"
#include "CommonRichTextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameSettingValueScalar.h"
#include "Settings/LyraSettingsLocal.h"
#include "Widgets/Layout/SSafeZone.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraBrightnessEditor)

struct FGeometry;

#define LOCTEXT_NAMESPACE "Lyra"

namespace BrightnessEditor
{
	// 忽略幅度低于该值的手柄摇杆输入。
	const float JoystickDeadZone = 0.2f;
	// 每单位模拟输入或滚轮增量对应的全局安全区比例变化量。
	const float SafeZoneChangeSpeed = 0.1f;
}

// 构造亮度校准界面并设置其输入与焦点行为。
ULyraBrightnessEditor::ULyraBrightnessEditor(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

// 初始化界面并显示默认操作说明。
void ULyraBrightnessEditor::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Switcher_SafeZoneMessage->SetActiveWidget(RichText_Default);
}

// 激活界面时从本地设置恢复全局安全区比例，并绑定确认与取消按钮。
void ULyraBrightnessEditor::NativeOnActivated()
{
	Super::NativeOnActivated();

	SSafeZone::SetGlobalSafeZoneScale(ULyraSettingsLocal::Get()->GetSafeZone());
	
	Button_Done->OnClicked().AddUObject(this, &ThisClass::HandleDoneClicked);

	Button_Back->SetVisibility((bCanCancel)? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bCanCancel)
	{
		Button_Back->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);
	}
}

// 从动作设置的首个子项取得标量数据源；没有可用子项时仍允许界面使用本地设置兜底。
bool ULyraBrightnessEditor::ExecuteActionForSetting_Implementation(FGameplayTag ActionTag, UGameSetting* InSetting)
{
	TArray<UGameSetting*> ChildSettings = InSetting ? InSetting->GetChildSettings() : TArray<UGameSetting*>();
	if (ChildSettings.Num() > 0 && ChildSettings[0])
	{
		ValueSetting = Cast<UGameSettingValueScalar>(ChildSettings[0]);
	}

	return true;
}

// 使用左摇杆纵轴调整全局安全区比例；输入未越过死区时交由基类处理。
FReply ULyraBrightnessEditor::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY && FMath::Abs(InAnalogEvent.GetAnalogValue()) >= BrightnessEditor::JoystickDeadZone)
	{
		const float SafeZoneMultiplier = FMath::Clamp(SSafeZone::GetGlobalSafeZoneScale().Get(1.0f) + InAnalogEvent.GetAnalogValue() * BrightnessEditor::SafeZoneChangeSpeed, 0.0f, 1.0f);
		SSafeZone::SetGlobalSafeZoneScale(SafeZoneMultiplier >= 0 ? SafeZoneMultiplier : 0);
		
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

// 使用鼠标滚轮调整全局安全区比例，并消费该输入。
FReply ULyraBrightnessEditor::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const float SafeZoneMultiplier = FMath::Clamp(SSafeZone::GetGlobalSafeZoneScale().Get(1.0f) + InMouseEvent.GetWheelDelta() * BrightnessEditor::SafeZoneChangeSpeed, 0.0f, 1.0f);
	SSafeZone::SetGlobalSafeZoneScale(SafeZoneMultiplier >= 0 ? SafeZoneMultiplier : 0);

	return FReply::Handled();
}

// 根据当前输入方式切换摇杆或滚轮操作提示。
void ULyraBrightnessEditor::HandleInputModeChanged(ECommonInputType InInputType)
{
	const FText KeyName = InInputType == ECommonInputType::Gamepad ? LOCTEXT("SafeZone_KeyToPress_Gamepad", "Left Stick") : LOCTEXT("SafeZone_KeyToPress_Mouse", "Mouse Wheel");
	RichText_Default->SetText(FText::Format(LOCTEXT("BrightnessAdjustInstructions", "Use <text color=\"FFFFFFFF\" fontface=\"black\">{0}</> to adjust the brightness"), KeyName));
}

// 取消本次编辑，恢复本地设置中已保存的安全区比例后关闭界面。
void ULyraBrightnessEditor::HandleBackClicked()
{
	DeactivateWidget();
	SSafeZone::SetGlobalSafeZoneScale(ULyraSettingsLocal::Get()->GetSafeZone());
}

// 将当前全局安全区比例写入绑定设置或本地设置，广播完成事件后关闭界面。
void ULyraBrightnessEditor::HandleDoneClicked()
{
	if (ValueSetting.IsValid())
	{
		ValueSetting.Get()->SetValue(SSafeZone::GetGlobalSafeZoneScale().Get(1.0f));
	}
	else
	{
		ULyraSettingsLocal::Get()->SetSafeZone(SSafeZone::GetGlobalSafeZoneScale().Get(1.0f));
	}
	OnSafeZoneSet.Broadcast();
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
