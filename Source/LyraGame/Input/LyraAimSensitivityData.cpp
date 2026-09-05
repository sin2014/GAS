// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAimSensitivityData.h"

#include "Settings/LyraSettingsShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAimSensitivityData)

// 建立手柄灵敏度档位到实际缩放系数的默认映射，供输入修饰器按用户预设查询。
ULyraAimSensitivityData::ULyraAimSensitivityData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SensitivityMap =
	{
		{ ELyraGamepadSensitivity::Slow,			0.5f },
		{ ELyraGamepadSensitivity::SlowPlus,		0.75f },
		{ ELyraGamepadSensitivity::SlowPlusPlus,	0.9f },
		{ ELyraGamepadSensitivity::Normal,		1.0f },
		{ ELyraGamepadSensitivity::NormalPlus,	1.1f },
		{ ELyraGamepadSensitivity::NormalPlusPlus,1.25f },
		{ ELyraGamepadSensitivity::Fast,			1.5f },
		{ ELyraGamepadSensitivity::FastPlus,		1.75f },
		{ ELyraGamepadSensitivity::FastPlusPlus,	2.0f },
		{ ELyraGamepadSensitivity::Insane,		2.5f },
	};
}

// 将灵敏度枚举转换为配置系数；资产未包含该档位时回退到不缩放的 1.0。
const float ULyraAimSensitivityData::SensitivtyEnumToFloat(const ELyraGamepadSensitivity InSensitivity) const
{
	if (const float* Sens = SensitivityMap.Find(InSensitivity))
	{
		return *Sens;
	}

	return 1.0f;
}

