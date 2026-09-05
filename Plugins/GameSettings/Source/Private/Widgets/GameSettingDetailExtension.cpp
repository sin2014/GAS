// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GameSettingDetailExtension.h"

#include "GameSetting.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingDetailExtension)

// 将设置分配流程转交 NativeSettingAssigned，以统一处理 C++ 订阅和蓝图通知。
void UGameSettingDetailExtension::SetSetting(UGameSetting* InSetting)
{
	NativeSettingAssigned(InSetting);
}

// 设置分配给详情扩展时调用蓝图事件，并开始监听值变化。
void UGameSettingDetailExtension::NativeSettingAssigned(UGameSetting* InSetting)
{
	if (Setting)
	{
		Setting->OnSettingChangedEvent.RemoveAll(this);
	}

	Setting = InSetting;
	Setting->OnSettingChangedEvent.AddUObject(this, &ThisClass::NativeSettingValueChanged);

	OnSettingAssigned(InSetting);
}

// 绑定设置变化时转发到详情扩展蓝图事件。
void UGameSettingDetailExtension::NativeSettingValueChanged(UGameSetting* InSetting, EGameSettingChangeReason Reason)
{
	OnSettingValueChanged(InSetting);
}
