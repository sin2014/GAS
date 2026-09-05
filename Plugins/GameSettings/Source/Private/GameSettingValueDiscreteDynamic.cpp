// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingValueDiscreteDynamic.h"
#include "DataSource/GameSettingDataSource.h"
#include "UObject/WeakObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingValueDiscreteDynamic)

#define LOCTEXT_NAMESPACE "GameSettingValueDiscreteDynamic"

// 动态离散值实现负责数据源解析、字符串读写、选项维护和初始值恢复。
//////////////////////////////////////////////////////////////////////////
// UGameSettingValueDiscreteDynamic
//////////////////////////////////////////////////////////////////////////

// 创建动态离散值设置；Getter、Setter 和选项将在注册阶段注入。
UGameSettingValueDiscreteDynamic::UGameSettingValueDiscreteDynamic()
{
}

// 设置动态读取数据源，用于从玩家或配置对象取得当前值。
void UGameSettingValueDiscreteDynamic::SetDynamicGetter(const TSharedRef<FGameSettingDataSource>& InGetter)
{
	Getter = InGetter;
}

// 设置动态写入数据源，用于把用户选择提交到目标属性。
void UGameSettingValueDiscreteDynamic::SetDynamicSetter(const TSharedRef<FGameSettingDataSource>& InSetter)
{
	Setter = InSetter;
}

// 保存字符串形式的默认值，供恢复默认和默认索引查询使用。
void UGameSettingValueDiscreteDynamic::SetDefaultValueFromString(FString InOptionValue)
{
	DefaultValue = InOptionValue;
}

// 添加或更新动态选项的内部值与本地化显示文本。
void UGameSettingValueDiscreteDynamic::AddDynamicOption(FString InOptionValue, FText InOptionText)
{
#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!OptionValues.Contains(InOptionValue), TEXT("You already added this option InOptionValue: %s InOptionText %s."), *InOptionValue, *InOptionText.ToString());
#endif

	OptionValues.Add(InOptionValue);
	OptionDisplayTexts.Add(InOptionText);
}

// 移除指定动态选项，并同步删除其显示文本映射。
void UGameSettingValueDiscreteDynamic::RemoveDynamicOption(FString InOptionValue)
{
	const int32 Index = OptionValues.IndexOfByKey(InOptionValue);
	if (Index != INDEX_NONE)
	{
		OptionValues.RemoveAt(Index);
		OptionDisplayTexts.RemoveAt(Index);
	}
}

// 返回当前动态选项的内部字符串值列表。
const TArray<FString>& UGameSettingValueDiscreteDynamic::GetDynamicOptions()
{
	return OptionValues;
}

// 按设置定义的相等规则检查指定值是否存在于动态选项。
bool UGameSettingValueDiscreteDynamic::HasDynamicOption(const FString& InOptionValue)
{
	return OptionValues.Contains(InOptionValue);
}

// 通过动态 Getter 读取当前值的字符串表示。
FString UGameSettingValueDiscreteDynamic::GetValueAsString() const
{
	return Getter->GetValueAsString(LocalPlayer);
}

// 通过动态 Setter 写入字符串值；值确实变化后按给定原因广播设置更新。
void UGameSettingValueDiscreteDynamic::SetValueFromString(FString InStringValue)
{
	SetValueFromString(InStringValue, EGameSettingChangeReason::Change);
}

// 通过动态 Setter 写入字符串值；值确实变化后按给定原因广播设置更新。
void UGameSettingValueDiscreteDynamic::SetValueFromString(FString InStringValue, EGameSettingChangeReason Reason)
{
	check(Setter);
	Setter->SetValue(LocalPlayer, InStringValue);

	NotifySettingChanged(Reason);
}

// 比较两个选项的内部值；基类采用字符串精确比较。
bool UGameSettingValueDiscreteDynamic::AreOptionsEqual(const FString& InOptionA, const FString& InOptionB) const
{
	return InOptionA == InOptionB;
}

// 校验动态 Getter 与 Setter 均能针对本地玩家解析，然后完成值设置初始化。
void UGameSettingValueDiscreteDynamic::OnInitialized()
{
#if !UE_BUILD_SHIPPING
	ensureAlways(Getter);
	ensureAlwaysMsgf(Getter->Resolve(LocalPlayer), TEXT("%s: %s did not resolve, are all functions and properties valid, and are they UFunctions/UProperties? Does the getter function have no parameters?"), *GetDevName().ToString(), *Getter->ToString());
	ensureAlways(Setter);
	ensureAlwaysMsgf(Setter->Resolve(LocalPlayer), TEXT("%s: %s did not resolve, are all functions and properties valid, and are they UFunctions/UProperties? Does the setting function have exactly one parameter?"), *GetDevName().ToString(), *Setter->ToString());
#endif

	Super::OnInitialized();
}

// 并行解析动态 Getter 与 Setter；两者都就绪后才完成设置启动。
void UGameSettingValueDiscreteDynamic::Startup()
{
	// TODO：需要确认数据源重解析时是否也应处理 Setter。
	// Should I also do something with Setter?
	check(Getter);
	Getter->Startup(LocalPlayer, FSimpleDelegate::CreateUObject(this, &ThisClass::OnDataSourcesReady));
}

// 在数据源解析完成后保存当前值作为初始基线，并标记设置可用。
void UGameSettingValueDiscreteDynamic::OnDataSourcesReady()
{
	StartupComplete();
}

// 读取当前值并保存为进入设置界面时的恢复基线。
void UGameSettingValueDiscreteDynamic::StoreInitial()
{
	InitialValue = GetValueAsString();
}

// 存在默认值时把设置写回默认值，并标记为恢复默认操作。
void UGameSettingValueDiscreteDynamic::ResetToDefault()
{
	if (DefaultValue.IsSet())
	{
		SetValueFromString(DefaultValue.GetValue(), EGameSettingChangeReason::ResetToDefault);
	}
}

// 把设置写回初始化时记录的值，并标记为还原操作。
void UGameSettingValueDiscreteDynamic::RestoreToInitial()
{
	SetValueFromString(InitialValue, EGameSettingChangeReason::RestoreToInitial);
}

// 校验索引后写入对应动态选项，并把变化标记为用户主动修改。
void UGameSettingValueDiscreteDynamic::SetDiscreteOptionByIndex(int32 Index)
{
	if (ensure(OptionValues.IsValidIndex(Index)))
	{
		SetValueFromString(OptionValues[Index]);
	}
}

// 读取当前字符串值并按可覆写的相等规则匹配选项；无法匹配时回退到默认索引。
int32 UGameSettingValueDiscreteDynamic::GetDiscreteOptionIndex() const
{
	const FString CurrentValue = GetValueAsString();
	const int32 Index = OptionValues.IndexOfByPredicate([this, CurrentValue](const FString& InOption) {
		return AreOptionsEqual(CurrentValue, InOption);
	});

	// 如果当前值无法匹配任何选项，则回退到默认选项索引。
	// If we can't find the correct index, send the default index.
	if (Index == INDEX_NONE)
	{
		return GetDiscreteOptionDefaultIndex();
	}

	return Index;
}

// 在动态选项中查找默认字符串值；未配置或未匹配时返回无效索引。
int32 UGameSettingValueDiscreteDynamic::GetDiscreteOptionDefaultIndex() const
{
	if (DefaultValue.IsSet())
	{
		return OptionValues.IndexOfByPredicate([this](const FString& InOption) {
			return AreOptionsEqual(DefaultValue.GetValue(), InOption);
		});
	}

	return INDEX_NONE;
}

// 按动态选项顺序生成界面显示文本数组。
TArray<FText> UGameSettingValueDiscreteDynamic::GetDiscreteOptions() const
{
	const TArray<FString>& DisabledOptions = GetEditState().GetDisabledOptions();

	if (DisabledOptions.Num() > 0)
	{
		TArray<FText> AllowedOptions;

		for (int32 OptionIndex = 0; OptionIndex < OptionValues.Num(); ++OptionIndex)
		{
			if (!DisabledOptions.Contains(OptionValues[OptionIndex]))
			{
				AllowedOptions.Add(OptionDisplayTexts[OptionIndex]);
			}
		}

		return AllowedOptions;
	}

	return OptionDisplayTexts;
}

// 布尔离散值实现维护“关闭/开启”两个可重排选项。
//////////////////////////////////////////////////////////////////////////
// UGameSettingValueDiscreteDynamic_Bool
//////////////////////////////////////////////////////////////////////////

// 创建布尔离散值设置，并按“关闭、开启”顺序注册 false 与 true 两个选项。
UGameSettingValueDiscreteDynamic_Bool::UGameSettingValueDiscreteDynamic_Bool()
{
	AddDynamicOption(TEXT("false"), LOCTEXT("OFF", "OFF"));
	AddDynamicOption(TEXT("true"), LOCTEXT("ON", "ON"));
}

// 更新“真”选项文本，并重新排序布尔选项以保持配置顺序。
void UGameSettingValueDiscreteDynamic_Bool::SetTrueText(const FText& InText)
{
	// 先移除再重新加入选项，使修改真假文本时也能控制二者的显示顺序。
	// We remove and then re-add it, so that by changing the true/false text you can also control the order they appear.
	RemoveDynamicOption(TEXT("true"));
	AddDynamicOption(TEXT("true"), InText);
}

// 更新“假”选项文本，并重新排序布尔选项以保持配置顺序。
void UGameSettingValueDiscreteDynamic_Bool::SetFalseText(const FText& InText)
{
	// 先移除再重新加入选项，使修改真假文本时也能控制二者的显示顺序。
	// We remove and then re-add it, so that by changing the true/false text you can also control the order they appear.
	RemoveDynamicOption(TEXT("false"));
	AddDynamicOption(TEXT("false"), InText);
}

// 保存类型化默认值，并转换为底层数据源使用的表示。
void UGameSettingValueDiscreteDynamic_Bool::SetDefaultValue(bool Value)
{
	DefaultValue = LexToString(Value);
}

// 数值离散值实现校验由注册代码提供的数字候选项。
//////////////////////////////////////////////////////////////////////////
// UGameSettingValueDiscreteDynamic_Number
//////////////////////////////////////////////////////////////////////////

// 创建数值离散值设置；可选数字将在初始化前由配置代码填充。
UGameSettingValueDiscreteDynamic_Number::UGameSettingValueDiscreteDynamic_Number()
{

}

// 完成基类初始化并校验数值选项非空，防止生成无法选择的条目。
void UGameSettingValueDiscreteDynamic_Number::OnInitialized()
{
	Super::OnInitialized();

	ensure(OptionValues.Num() > 0);
}

//////////////////////////////////////////////////////////////////////////
// 枚举离散值设置把枚举值映射为动态选项。
// UGameSettingValueDiscreteDynamic_Enum
//////////////////////////////////////////////////////////////////////////

// 创建枚举离散值设置；枚举值与显示文本将在初始化前由配置代码填充。
UGameSettingValueDiscreteDynamic_Enum::UGameSettingValueDiscreteDynamic_Enum()
{

}

// 完成基类初始化并校验枚举选项非空，确保当前值能够映射到有效索引。
void UGameSettingValueDiscreteDynamic_Enum::OnInitialized()
{
	Super::OnInitialized();

	ensure(OptionValues.Num() > 0);
}

//////////////////////////////////////////////////////////////////////////
// 颜色离散值设置把颜色序列化结果作为动态选项值。
// UGameSettingValueDiscreteDynamic_Color
//////////////////////////////////////////////////////////////////////////

// 创建颜色离散值设置；颜色字符串与本地化名称由使用方注册为动态选项。
UGameSettingValueDiscreteDynamic_Color::UGameSettingValueDiscreteDynamic_Color()
{

}


#undef LOCTEXT_NAMESPACE
