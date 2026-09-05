// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingValue.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingValue)

#define LOCTEXT_NAMESPACE "GameSetting"

// 值设置基类负责保存初始值，并定义恢复默认、还原和分析上报约定。
//--------------------------------------
// UGameSettingValue
//--------------------------------------

// 创建值设置并默认启用分析上报；具体上报内容由派生类型提供。
UGameSettingValue::UGameSettingValue()
{
	// 值类型设置默认参与分析数据上报。
	// Values will report to analytics.
	bReportAnalytics = true;
}

// 校验值设置具有静态或动态说明，并保存当前值作为本次设置会话的初始基线。
void UGameSettingValue::OnInitialized()
{
	Super::OnInitialized();

#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!DescriptionRichText.IsEmpty() || DynamicDetails.IsBound(), TEXT("You must provide a description or it must specify a dynamic details function for settings with values."));
#endif

	StoreInitial();
}

#undef LOCTEXT_NAMESPACE

