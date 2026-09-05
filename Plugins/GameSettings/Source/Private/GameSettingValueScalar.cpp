// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingValueScalar.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingValueScalar)

#define LOCTEXT_NAMESPACE "GameSetting"

//--------------------------------------
// 标量设置提供源数值与归一化滑块值之间的映射接口。
// UGameSettingValueScalar
//--------------------------------------

// 创建标量设置；归一化读写由派生类提供的源范围与实际数值接口完成。
UGameSettingValueScalar::UGameSettingValueScalar()
{

}

// 把 0 到 1 的归一化输入映射到源范围后写入设置。
void UGameSettingValueScalar::SetValueNormalized(double NormalizedValue)
{
	SetValue(FMath::GetMappedRangeValueClamped(TRange<double>(0, 1), GetSourceRange(), NormalizedValue));
}

// 把当前源数值映射为 0 到 1 的归一化值。
double UGameSettingValueScalar::GetValueNormalized() const
{
	return FMath::GetMappedRangeValueClamped(GetSourceRange(), TRange<double>(0, 1), GetValue());
}

#undef LOCTEXT_NAMESPACE

