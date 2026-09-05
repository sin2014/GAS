// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Misc/GameSettingRotator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingRotator)

// 创建设置选项轮转控件；默认选项索引由绑定设置随后提供。
UGameSettingRotator::UGameSettingRotator(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

// 设置轮转控件的默认选项索引，供恢复默认状态显示。
void UGameSettingRotator::SetDefaultOption(int32 DefaultOptionIndex)
{
	BP_OnDefaultOptionSpecified(DefaultOptionIndex);
}

