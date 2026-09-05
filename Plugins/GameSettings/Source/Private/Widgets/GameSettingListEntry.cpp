// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GameSettingListEntry.h"

#include "AnalogSlider.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "CommonTextBlock.h"
#include "GameSettingAction.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscrete.h"
#include "GameSettingValueScalar.h"
#include "Widgets/Misc/GameSettingRotator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingListEntry)

class SWidget;
struct FGeometry;

#define LOCTEXT_NAMESPACE "GameSetting"

// 设置列表条目基类负责绑定设置事件、应用可编辑状态并管理对象池释放时的清理。
//////////////////////////////////////////////////////////////////////////
// UGameSettingListEntryBase
//////////////////////////////////////////////////////////////////////////

// 保存设置，订阅值变化与编辑条件变化，并立即应用当前可编辑状态。
void UGameSettingListEntryBase::SetSetting(UGameSetting* InSetting)
{
	Setting = InSetting;
	Setting->OnSettingEditConditionChangedEvent.AddUObject(this, &ThisClass::HandleEditConditionChanged);
	Setting->OnSettingChangedEvent.AddUObject(this, &ThisClass::HandleSettingChanged);

	HandleEditConditionChanged(Setting);
}

// 设置条目名称覆盖文本，并立即刷新显示名称。
void UGameSettingListEntryBase::SetDisplayNameOverride(const FText& OverrideName)
{
	DisplayNameOverride = OverrideName;
}

// 条目回收到对象池前解除设置事件和控件回调，清空持有状态。
void UGameSettingListEntryBase::NativeOnEntryReleased()
{
	StopAllAnimations();

	if (Background)
	{
		Background->StopAllAnimations();
	}

	if (ensure(Setting))
	{
		Setting->OnSettingEditConditionChangedEvent.RemoveAll(this);
		Setting->OnSettingChangedEvent.RemoveAll(this);
	}

	Setting = nullptr;
}

// 接收设置值变化并刷新设置列表条目基类，同时保留变化原因供扩展使用。
void UGameSettingListEntryBase::HandleSettingChanged(UGameSetting* InSetting, EGameSettingChangeReason Reason)
{
	if (!bSuspendChangeUpdates)
	{
		OnSettingChanged();
	}
}

// 提供设置值变化后的派生类扩展点；基类不附加处理。
void UGameSettingListEntryBase::OnSettingChanged()
{
	// 该基类默认不执行额外操作，交由派生类按需覆盖。
	// No-Op
}

// 重新读取设置可编辑状态并同步设置列表条目基类的可见性、启用状态和选项。
void UGameSettingListEntryBase::HandleEditConditionChanged(UGameSetting* InSetting)
{
	const FGameSettingEditableState EditableState = Setting->GetEditState();
	RefreshEditableState(EditableState);
}

// 把可编辑状态应用到设置列表条目基类及其交互控件。
void UGameSettingListEntryBase::RefreshEditableState(const FGameSettingEditableState& InEditableState)
{
	// 该基类默认不执行额外操作，交由派生类按需覆盖。
	// No-Op
}

// 接收焦点后把焦点转交给适合手柄操作的内部控件。
FReply UGameSettingListEntryBase::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	const UCommonInputSubsystem* InputSubsystem = GetInputSubsystem();
	if (InputSubsystem && InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		if (UWidget* PrimaryFocus = GetPrimaryGamepadFocusWidget())
		{
			TSharedPtr<SWidget> WidgetToFocus = PrimaryFocus->GetCachedWidget();
			if (WidgetToFocus.IsValid())
			{
				return FReply::Handled().SetUserFocus(WidgetToFocus.ToSharedRef(), InFocusEvent.GetCause());
			}
		}
	}

	return FReply::Unhandled();
}

// 通用设置条目把设置显示名称同步到绑定文本控件。
//////////////////////////////////////////////////////////////////////////
// UGameSettingListEntry_Setting
//////////////////////////////////////////////////////////////////////////

// 完成基础绑定后，把设置名称或名称覆盖写入文本控件并同步名称可见性。
void UGameSettingListEntry_Setting::SetSetting(UGameSetting* InSetting)
{
	Super::SetSetting(InSetting);

	Text_SettingName->SetText(DisplayNameOverride.IsEmpty() ? Setting->GetDisplayName() : DisplayNameOverride);
	Text_SettingName->SetVisibility(InSetting->GetDisplayNameVisibility());
}

//////////////////////////////////////////////////////////////////////////
// 离散值条目使用轮转控件显示和修改有限选项。
// UGameSettingListEntrySetting_Discrete
//////////////////////////////////////////////////////////////////////////

// 校验并保存离散值设置，完成基础事件绑定后同步选项和当前索引。
void UGameSettingListEntrySetting_Discrete::SetSetting(UGameSetting* InSetting)
{
	DiscreteSetting = Cast<UGameSettingValueDiscrete>(InSetting);

	Super::SetSetting(InSetting);
	
	Refresh();
}

// 从绑定设置读取最新值和显示文本，刷新离散值设置条目。
void UGameSettingListEntrySetting_Discrete::Refresh()
{
	if (ensure(DiscreteSetting))
	{
		const TArray<FText> Options = DiscreteSetting->GetDiscreteOptions();
		ensure(Options.Num() > 0);

		Rotator_SettingValue->PopulateTextLabels(Options);
		Rotator_SettingValue->SetSelectedItem(DiscreteSetting->GetDiscreteOptionIndex());
		Rotator_SettingValue->SetDefaultOption(DiscreteSetting->GetDiscreteOptionDefaultIndex());
	}
}

// 把可编辑状态应用到离散值设置条目及其交互控件。
void UGameSettingListEntrySetting_Discrete::RefreshEditableState(const FGameSettingEditableState& InEditableState)
{
	Super::RefreshEditableState(InEditableState);

	const bool bLocalIsEnabled = InEditableState.IsEnabled();
	Button_Decrease->SetIsEnabled(bLocalIsEnabled);
	Rotator_SettingValue->SetIsEnabled(bLocalIsEnabled);
	Button_Increase->SetIsEnabled(bLocalIsEnabled);
	Panel_Value->SetIsEnabled(bLocalIsEnabled);
}

// 绑定轮转器值变化以及前后选项按钮的点击事件。
void UGameSettingListEntrySetting_Discrete::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Rotator_SettingValue->OnRotatedEvent.AddUObject(this, &ThisClass::HandleRotatorChangedValue);
	Button_Decrease->OnClicked().AddUObject(this, &ThisClass::HandleOptionDecrease);
	Button_Increase->OnClicked().AddUObject(this, &ThisClass::HandleOptionIncrease);
}

// 基类解除事件后清空离散值设置引用，避免对象池复用时读取旧设置。
void UGameSettingListEntrySetting_Discrete::NativeOnEntryReleased()
{
	Super::NativeOnEntryReleased();

	DiscreteSetting = nullptr;
}

// 将离散选项移动到前一项，并把变化提交给设置。
void UGameSettingListEntrySetting_Discrete::HandleOptionDecrease()
{
	// TODO：由 UI 直接修改选项并不理想，应该通过 Setting 对象完成。
	//TODO NDarnell Doing this through the UI feels wrong, should use Setting directly.
	Rotator_SettingValue->ShiftTextLeft();
	DiscreteSetting->SetDiscreteOptionByIndex(Rotator_SettingValue->GetSelectedIndex());
}

// 将离散选项移动到后一项，并把变化提交给设置。
void UGameSettingListEntrySetting_Discrete::HandleOptionIncrease()
{
	// TODO：由 UI 直接修改选项并不理想，应该通过 Setting 对象完成。
	//TODO NDarnell Doing this through the UI feels wrong, should use Setting directly.
	Rotator_SettingValue->ShiftTextRight();
	DiscreteSetting->SetDiscreteOptionByIndex(Rotator_SettingValue->GetSelectedIndex());
}

// 把轮转控件的新索引写入离散设置；仅在用户操作时提交。
void UGameSettingListEntrySetting_Discrete::HandleRotatorChangedValue(int32 Value, bool bUserInitiated)
{
	if (bUserInitiated)
	{
		DiscreteSetting->SetDiscreteOptionByIndex(Value);
	}
}

// 设置值变化后重新读取离散选项和当前索引。
void UGameSettingListEntrySetting_Discrete::OnSettingChanged()
{
	Refresh();
}

// 重新读取设置可编辑状态并同步离散值设置条目的可见性、启用状态和选项。
void UGameSettingListEntrySetting_Discrete::HandleEditConditionChanged(UGameSetting* InSetting)
{
	Super::HandleEditConditionChanged(InSetting);

	Refresh();
}


// 标量条目通过滑块编辑归一化数值，并显示格式化后的源值。
//////////////////////////////////////////////////////////////////////////
// UGameSettingListEntrySetting_Scalar
//////////////////////////////////////////////////////////////////////////

// 校验并保存标量设置，完成基础事件绑定后同步滑块、文本和默认值。
void UGameSettingListEntrySetting_Scalar::SetSetting(UGameSetting* InSetting)
{
	ScalarSetting = Cast<UGameSettingValueScalar>(InSetting);

	Super::SetSetting(InSetting);

	Refresh();
}

// 从绑定设置读取最新值和显示文本，刷新标量设置条目。
void UGameSettingListEntrySetting_Scalar::Refresh()
{
	if (ensure(ScalarSetting))
	{
		const float Value = ScalarSetting->GetValueNormalized();

		Slider_SettingValue->SetValue(Value);
		Slider_SettingValue->SetStepSize(ScalarSetting->GetNormalizedStepSize());
		Text_SettingValue->SetText(ScalarSetting->GetFormattedText());

		TOptional<double> DefaultValue = ScalarSetting->GetDefaultValueNormalized();
		OnDefaultValueChanged(DefaultValue.IsSet() ? DefaultValue.GetValue() : -1.0);

		OnValueChanged(Value);
	}
}

// 设置值变化后重新同步滑块、格式化文本和默认值标记。
void UGameSettingListEntrySetting_Scalar::OnSettingChanged()
{
	Refresh();
}

// 绑定滑块数值变化及鼠标、手柄捕获结束事件。
void UGameSettingListEntrySetting_Scalar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Slider_SettingValue->OnValueChanged.AddDynamic(this, &ThisClass::HandleSliderValueChanged);

	Slider_SettingValue->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleSliderCaptureEnded);
	Slider_SettingValue->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleSliderCaptureEnded);
}

// 基类解除事件后清空标量设置引用，避免对象池复用时读取旧设置。
void UGameSettingListEntrySetting_Scalar::NativeOnEntryReleased()
{
	Super::NativeOnEntryReleased();

	ScalarSetting = nullptr;
}

// 将滑块值写入标量设置，并在拖动期间连续更新显示。
void UGameSettingListEntrySetting_Scalar::HandleSliderValueChanged(float Value)
{
	TGuardValue<bool> Suspend(bSuspendChangeUpdates, true);

	if (ensure(ScalarSetting))
	{
		ScalarSetting->SetValueNormalized(Value);
		Value = ScalarSetting->GetValueNormalized();

		Slider_SettingValue->SetValue(Value);
		Text_SettingValue->SetText(ScalarSetting->GetFormattedText());

		OnValueChanged(Value);
	}
}

// 滑块拖动结束时提交最终值，并为需要延迟应用的设置保留提交点。
void UGameSettingListEntrySetting_Scalar::HandleSliderCaptureEnded()
{
	TGuardValue<bool> Suspend(bSuspendChangeUpdates, true);

	// TODO：需要确认此处是否应显式提交滑块值。
	//commit?
}

// 把可编辑状态应用到标量设置条目及其交互控件。
void UGameSettingListEntrySetting_Scalar::RefreshEditableState(const FGameSettingEditableState& InEditableState)
{
	Super::RefreshEditableState(InEditableState);

	const bool bLocalIsEnabled = InEditableState.IsEnabled();
	Slider_SettingValue->SetIsEnabled(bLocalIsEnabled);
	Panel_Value->SetIsEnabled(bLocalIsEnabled);
}

// 动作条目通过按钮触发设置动作，并把配置文本交给蓝图呈现。
//////////////////////////////////////////////////////////////////////////
// UGameSettingListEntrySetting_Action
//////////////////////////////////////////////////////////////////////////

// 保存动作设置并把动作按钮文本通知给蓝图条目样式。
void UGameSettingListEntrySetting_Action::SetSetting(UGameSetting* InSetting)
{
	Super::SetSetting(InSetting);

	ActionSetting = Cast<UGameSettingAction>(InSetting);
	if (ensure(ActionSetting))
	{
		OnSettingAssigned(ActionSetting->GetActionText());
	}
}

// 绑定动作按钮点击事件。
void UGameSettingListEntrySetting_Action::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button_Action->OnClicked().AddUObject(this, &ThisClass::HandleActionButtonClicked);
}

// 基类解除事件后清空动作设置引用，避免对象池复用时触发旧动作。
void UGameSettingListEntrySetting_Action::NativeOnEntryReleased()
{
	Super::NativeOnEntryReleased();

	ActionSetting = nullptr;
}

// 把可编辑状态应用到动作设置条目及其交互控件。
void UGameSettingListEntrySetting_Action::RefreshEditableState(const FGameSettingEditableState& InEditableState)
{
	Super::RefreshEditableState(InEditableState);

	const bool bLocalIsEnabled = InEditableState.IsEnabled();
	Button_Action->SetIsEnabled(bLocalIsEnabled);
}

// 触发绑定动作设置的自定义回调与命名动作事件。
void UGameSettingListEntrySetting_Action::HandleActionButtonClicked()
{
	ActionSetting->ExecuteAction();
}

//////////////////////////////////////////////////////////////////////////
// 导航条目把用户操作转换为进入子设置页面的请求。
// UGameSettingListEntrySetting_Navigation
//////////////////////////////////////////////////////////////////////////

// 保存分页集合并把导航文本通知给蓝图条目样式。
void UGameSettingListEntrySetting_Navigation::SetSetting(UGameSetting* InSetting)
{
	CollectionSetting = Cast<UGameSettingCollectionPage>(InSetting);

	Super::SetSetting(InSetting);

	if (ensure(CollectionSetting))
	{
		OnSettingAssigned(CollectionSetting->GetNavigationText());
	}
}

// 绑定导航按钮点击事件。
void UGameSettingListEntrySetting_Navigation::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button_Navigate->OnClicked().AddUObject(this, &ThisClass::HandleNavigationButtonClicked);
}

// 基类解除事件后清空分页集合引用，避免对象池复用时导航到旧页面。
void UGameSettingListEntrySetting_Navigation::NativeOnEntryReleased()
{
	Super::NativeOnEntryReleased();

	CollectionSetting = nullptr;
}

// 把可编辑状态应用到导航设置条目及其交互控件。
void UGameSettingListEntrySetting_Navigation::RefreshEditableState(const FGameSettingEditableState& InEditableState)
{
	Super::RefreshEditableState(InEditableState);

	const bool bLocalIsEnabled = InEditableState.IsEnabled();
	Button_Navigate->SetIsEnabled(bLocalIsEnabled);
}

// 请求设置面板进入绑定导航设置的子页面。
void UGameSettingListEntrySetting_Navigation::HandleNavigationButtonClicked()
{
	CollectionSetting->ExecuteNavigation();
}

#undef LOCTEXT_NAMESPACE
