// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingValueScalarDynamic.h"

#include "DataSource/GameSettingDataSource.h"
#include "UObject/WeakObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingValueScalarDynamic)

#define LOCTEXT_NAMESPACE "GameSetting"

// 以下格式化器把源值或归一化值转换为整数、小数和百分比显示文本。
//////////////////////////////////////////////////////////////////////////
// SettingScalarFormats
//////////////////////////////////////////////////////////////////////////

// 所有百分比格式化器共享的本地化文本模板。
static FText PercentFormat = LOCTEXT("PercentFormat", "{0}%");

// 将源数值直接格式化为不带小数的文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::Raw([](double SourceValue, double NormalizedValue) {
	return FText::AsNumber(SourceValue);
});

// 将源数值格式化为一位小数文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::RawOneDecimal([](double SourceValue, double NormalizedValue) {
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumIntegralDigits = 1;
	FormattingOptions.MinimumFractionalDigits = 1;
	FormattingOptions.MaximumFractionalDigits = 1;
	return FText::AsNumber(SourceValue, &FormattingOptions);
});

// 将源数值格式化为两位小数文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::RawTwoDecimals([](double SourceValue, double NormalizedValue) {
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumIntegralDigits = 1;
	FormattingOptions.MinimumFractionalDigits = 2;
	FormattingOptions.MaximumFractionalDigits = 2;
	return FText::AsNumber(SourceValue, &FormattingOptions);
});

// 将源数值四舍五入为整数文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::SourceAsInteger([](double SourceValue, double NormalizedValue) {
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumIntegralDigits = 1;
	FormattingOptions.MinimumFractionalDigits = 0;
	FormattingOptions.MaximumFractionalDigits = 0;
	return FText::AsNumber(SourceValue, &FormattingOptions);
});

// 把归一化值格式化为整数百分比文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::ZeroToOnePercent([](double SourceValue, double NormalizedValue) {
	return FText::Format(PercentFormat, (int32)FMath::RoundHalfFromZero(100.0 * NormalizedValue));
});

// 把归一化值格式化为一位小数百分比文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::ZeroToOnePercent_OneDecimal([](double SourceValue, double NormalizedValue) {
	const FNumberFormattingOptions& FormattingOptions = GetOneDecimalFormattingOptions();
	const double NormalizedValueTo100_0 = FMath::RoundHalfFromZero(1000.0 * NormalizedValue);
	return FText::Format(PercentFormat, FText::AsNumber(NormalizedValueTo100_0 / 10.0, &FormattingOptions));
});

// 返回复用的一位小数格式配置，避免重复构造格式选项。
const FNumberFormattingOptions& UGameSettingValueScalarDynamic::GetOneDecimalFormattingOptions()
{
	static FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumFractionalDigits = 1;
	FormattingOptions.MaximumFractionalDigits = 1;

	return FormattingOptions;
}

// 把 0 到 1 的源值转换为整数百分比文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::SourceAsPercent1([](double SourceValue, double NormalizedValue) {
	return FText::Format(PercentFormat, (int32)FMath::RoundHalfFromZero(100.0 * SourceValue));
});

// 把 0 到 100 的源值格式化为整数百分比文本。
FSettingScalarFormatFunction UGameSettingValueScalarDynamic::SourceAsPercent100([](double SourceValue, double NormalizedValue) {
	return FText::Format(PercentFormat, (int32)FMath::RoundHalfFromZero(SourceValue));
});

//////////////////////////////////////////////////////////////////////////
// 动态标量设置通过 Getter/Setter 读写数值，并管理范围、步长和显示格式。
// UGameSettingValueScalarDynamic
//////////////////////////////////////////////////////////////////////////

// 创建动态标量设置；数据源、范围、步长和显示格式将在注册阶段配置。
UGameSettingValueScalarDynamic::UGameSettingValueScalarDynamic()
{
}

// 解析标量 Getter 与 Setter；全部数据源就绪后再开放读写。
void UGameSettingValueScalarDynamic::Startup()
{
	// TODO：需要确认数据源重解析时是否也应处理 Setter。
	// Should I also do something with Setter?
	Getter->Startup(LocalPlayer, FSimpleDelegate::CreateUObject(this, &ThisClass::OnDataSourcesReady));
}

// 数据源可用后记录初始值并完成启动，保证恢复操作有可靠基线。
void UGameSettingValueScalarDynamic::OnDataSourcesReady()
{
	StartupComplete();
}

// 校验显示格式和动态读写数据源均有效，再交由值设置基类记录初始值。
void UGameSettingValueScalarDynamic::OnInitialized()
{
#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(DisplayFormat, TEXT("%s: Has no DisplayFormat set.  Please call SetDisplayFormat."), *GetDevName().ToString());
#endif

#if !UE_BUILD_SHIPPING
	ensureAlways(Getter);
	ensureAlwaysMsgf(Getter->Resolve(LocalPlayer), TEXT("%s: %s did not resolve, are all functions and properties valid, and are they UFunctions/UProperties?"), *GetDevName().ToString(), *Getter->ToString());
	ensureAlways(Setter);
	ensureAlwaysMsgf(Setter->Resolve(LocalPlayer), TEXT("%s: %s did not resolve, are all functions and properties valid, and are they UFunctions/UProperties?"), *GetDevName().ToString(), *Setter->ToString());
#endif

	Super::OnInitialized();
}

// 读取当前值并保存为进入设置界面时的恢复基线。
void UGameSettingValueScalarDynamic::StoreInitial()
{
	InitialValue = GetValue();
}

// 存在默认值时把设置写回默认值，并标记为恢复默认操作。
void UGameSettingValueScalarDynamic::ResetToDefault()
{
	if (DefaultValue.IsSet())
	{
		SetValue(DefaultValue.GetValue(), EGameSettingChangeReason::ResetToDefault);
	}
}

// 把设置写回初始化时记录的值，并标记为还原操作。
void UGameSettingValueScalarDynamic::RestoreToInitial()
{
	SetValue(InitialValue, EGameSettingChangeReason::RestoreToInitial);
}

// 设置动态读取数据源，用于从玩家或配置对象取得当前值。
void UGameSettingValueScalarDynamic::SetDynamicGetter(const TSharedRef<FGameSettingDataSource>& InGetter)
{
	Getter = InGetter;
}

// 设置动态写入数据源，用于把用户选择提交到目标属性。
void UGameSettingValueScalarDynamic::SetDynamicSetter(const TSharedRef<FGameSettingDataSource>& InSetter)
{
	Setter = InSetter;
}

// 保存类型化默认值，并转换为底层数据源使用的表示。
void UGameSettingValueScalarDynamic::SetDefaultValue(double InValue)
{
	DefaultValue = InValue;
}

// 设置标量到显示文本的格式化回调。
void UGameSettingValueScalarDynamic::SetDisplayFormat(FSettingScalarFormatFunction InDisplayFormat)
{
	DisplayFormat = InDisplayFormat;
}

// 配置底层数值范围与步长，并清除不再落在新范围内的用户限制。
void UGameSettingValueScalarDynamic::SetSourceRangeAndStep(const TRange<double>& InRange, double InStep)
{
	SourceRange = InRange;
	SourceStep = InStep;
}

// 配置用户可选下限，并确保它落在底层源范围内。
void UGameSettingValueScalarDynamic::SetMinimumLimit(const TOptional<double>& InMinimum)
{
	Minimum = InMinimum;
}

// 配置用户可选上限，并确保它落在底层源范围内。
void UGameSettingValueScalarDynamic::SetMaximumLimit(const TOptional<double>& InMaximum)
{
	Maximum = InMaximum;
}

// 从动态 Getter 读取并解析标量值，再限制到允许的源范围。
double UGameSettingValueScalarDynamic::GetValue() const
{
	const FString OutValue = Getter->GetValueAsString(LocalPlayer);

	double Value;
	LexFromString(Value, *OutValue);

	return Value;
}

// 返回标量底层可表示的完整数值范围。
TRange<double> UGameSettingValueScalarDynamic::GetSourceRange() const
{
	return SourceRange;
}

// 返回用户调整标量时采用的步长。
double UGameSettingValueScalarDynamic::GetSourceStep() const
{
	return SourceStep;
}

// 返回可选的标量默认值。
TOptional<double> UGameSettingValueScalarDynamic::GetDefaultValue() const
{
	return DefaultValue;
}

// 按源范围和用户上下限夹紧数值，经动态 Setter 写入；变化后广播指定原因。
void UGameSettingValueScalarDynamic::SetValue(double InValue, EGameSettingChangeReason Reason)
{
	InValue = FMath::RoundHalfFromZero(InValue / SourceStep);
	InValue = InValue * SourceStep;

	if (Minimum.IsSet())
	{
		InValue = FMath::Max(Minimum.GetValue(), InValue);
	}

	if (Maximum.IsSet())
	{
		InValue = FMath::Min(Maximum.GetValue(), InValue);
	}

	const FString StringValue = LexToString(InValue);
	Setter->SetValue(LocalPlayer, StringValue);

	NotifySettingChanged(Reason);
}

// 读取当前标量并通过配置的格式化函数生成界面显示文本。
FText UGameSettingValueScalarDynamic::GetFormattedText() const
{
	const double SourceValue = GetValue();
	const double NormalizedValue = GetValueNormalized();

	return DisplayFormat(SourceValue, NormalizedValue);
}

#undef LOCTEXT_NAMESPACE
