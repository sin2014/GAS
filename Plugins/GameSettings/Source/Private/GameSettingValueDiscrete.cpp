// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingValueDiscrete.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingValueDiscrete)

#define LOCTEXT_NAMESPACE "GameSetting"

//--------------------------------------
// 离散值设置以选项索引和显示文本表示有限候选值。
// UGameSettingValueDiscrete
//--------------------------------------

// 创建离散值设置；选项列表、当前索引和默认索引由具体派生类型提供。
UGameSettingValueDiscrete::UGameSettingValueDiscrete()
{

}

// 返回当前离散选项索引的字符串，作为分析上报值。
FString UGameSettingValueDiscrete::GetAnalyticsValue() const
{
	const TArray<FText> Options = GetDiscreteOptions();
	const int32 CurrentOptionIndex = GetDiscreteOptionIndex();
	if (Options.IsValidIndex(CurrentOptionIndex))
	{
		const FString* SourceString = FTextInspector::GetSourceString(Options[CurrentOptionIndex]);
		if (SourceString)
		{
			return *SourceString;
		}
	}

	return TEXT("<Unknown Index>");
}

#undef LOCTEXT_NAMESPACE

