// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingValueScalar.h"

#include "GameSettingValueScalarDynamic.generated.h"

#define UE_API GAMESETTINGS_API

struct FNumberFormattingOptions;

class FGameSettingDataSource;
class UObject;

// 动态标量设置通过可解析数据源读写连续数值，并配置范围、步长、限制和显示格式。
//////////////////////////////////////////////////////////////////////////
// UGameSettingValueScalarDynamic
//////////////////////////////////////////////////////////////////////////

typedef TFunction<FText(double SourceValue, double NormalizedValue)> FSettingScalarFormatFunction;

UCLASS(MinimalAPI)
class UGameSettingValueScalarDynamic : public UGameSettingValueScalar
{
	GENERATED_BODY()

public:
	static UE_API FSettingScalarFormatFunction Raw;
	static UE_API FSettingScalarFormatFunction RawOneDecimal;
	static UE_API FSettingScalarFormatFunction RawTwoDecimals;
	static UE_API FSettingScalarFormatFunction ZeroToOnePercent;
	static UE_API FSettingScalarFormatFunction ZeroToOnePercent_OneDecimal;
	static UE_API FSettingScalarFormatFunction SourceAsPercent1;
	static UE_API FSettingScalarFormatFunction SourceAsPercent100;
	static UE_API FSettingScalarFormatFunction SourceAsInteger;
private:
	static const FNumberFormattingOptions& GetOneDecimalFormattingOptions();
	
public:
	UE_API UGameSettingValueScalarDynamic();

	// 以下函数覆盖值设置的启动、初始值保存、恢复默认和还原接口。
	/** UGameSettingValue */
	UE_API virtual void Startup() override;
	UE_API virtual void StoreInitial() override;
	UE_API virtual void ResetToDefault() override;
	UE_API virtual void RestoreToInitial() override;

	// 以下函数实现标量默认值、实际读写、源范围、步长和格式化接口。
	/** UGameSettingValueScalar */
	UE_API virtual TOptional<double> GetDefaultValue() const override;
	UE_API virtual void SetValue(double Value, EGameSettingChangeReason Reason = EGameSettingChangeReason::Change) override;
	UE_API virtual double GetValue() const override;
	UE_API virtual TRange<double> GetSourceRange() const override;
	UE_API virtual double GetSourceStep() const override;
	UE_API virtual FText GetFormattedText() const override;

	// 以下接口配置动态 Getter/Setter、默认值及界面显示格式。
	/** UGameSettingValueDiscreteDynamic */
	UE_API void SetDynamicGetter(const TSharedRef<FGameSettingDataSource>& InGetter);
	UE_API void SetDynamicSetter(const TSharedRef<FGameSettingDataSource>& InSetter);
	UE_API void SetDefaultValue(double InValue);

	/**  */
	UE_API void SetDisplayFormat(FSettingScalarFormatFunction InDisplayFormat);
	
	/**  */
	UE_API void SetSourceRangeAndStep(const TRange<double>& InRange, double InSourceStep);
	
	// SetSourceRangeAndStep 定义底层数值范围；最小限制可在不改变滑条完整显示范围的前提下，阻止用户设置低于指定值的数值，例如将 0..100 的滑条最低可选值限制为 1。
	/**
	 * The SetSourceRangeAndStep defines the actual range the numbers could move in, but often
	 * the true minimum for the user is greater than the minimum source range, so for example, the range
	 * of some slider might be 0..100, but you want to restrict the slider so that while it shows 
	 * a bar that travels from 0 to 100, the user can't set anything lower than some minimum, e.g. 1.
	 * That is the Minimum Limit.
	 */
	UE_API void SetMinimumLimit(const TOptional<double>& InMinimum);

	// SetSourceRangeAndStep 定义底层数值范围；最大限制可在不改变滑条完整显示范围的前提下，阻止用户设置高于指定值的数值，例如将 0..100 的滑条最高可选值限制为 95。
	/**
	 * The SetSourceRangeAndStep defines the actual range the numbers could move in, but rarely
	 * the true maximum for the user is less than the maximum source range, so for example, the range
	 * of some slider might be 0..100, but you want to restrict the slider so that while it shows
	 * a bar that travels from 0 to 100, the user can't set anything lower than some maximum, e.g. 95.
	 * That is the Maximum Limit.
	 */
	UE_API void SetMaximumLimit(const TOptional<double>& InMaximum);
	
protected:
	// 初始化阶段校验显示格式和动态数据源，再保存本次会话的初始值。
	/** UGameSettingValue */
	UE_API virtual void OnInitialized() override;

	UE_API void OnDataSourcesReady();

protected:

	TSharedPtr<FGameSettingDataSource> Getter;
	TSharedPtr<FGameSettingDataSource> Setter;

	TOptional<double> DefaultValue;
	double InitialValue = 0;

	TRange<double> SourceRange = TRange<double>(0, 1);
	double SourceStep = 0.01;
	TOptional<double> Minimum;
	TOptional<double> Maximum;

	FSettingScalarFormatFunction DisplayFormat;
};

#undef UE_API
