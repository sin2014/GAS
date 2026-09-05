// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingAction_HDRCalibrationEditor.h"

#include "DataSource/GameSettingDataSource.h"
#include "Player/LyraLocalPlayer.h"
#include "Settings/LyraGameSettingRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingAction_HDRCalibrationEditor)

#define LOCTEXT_NAMESPACE "LyraSettings"

// 创建 HDR 最大亮度子设置，并绑定本地设置的读写函数。
ULyraSettingAction_HDRCalibrationEditor::ULyraSettingAction_HDRCalibrationEditor()
{
	HDRCalibrationValueSetting = NewObject<UGameSettingValueScalarDynamic>();
	HDRCalibrationValueSetting->SetDevName(TEXT("HDRCalibrationValue"));
	HDRCalibrationValueSetting->SetDisplayName(LOCTEXT("HDRCalibrationValue_Name", "HDR Max Luminance"));
	HDRCalibrationValueSetting->SetDescriptionRichText(LOCTEXT("HDRCalibrationValue_Description", "The maximum luminance for the HDR display."));
	HDRCalibrationValueSetting->SetDefaultValue(0.0f);
	HDRCalibrationValueSetting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetMaximumHDRDisplayNits));
	HDRCalibrationValueSetting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetMaximumHDRDisplayNits));
	HDRCalibrationValueSetting->SetDisplayFormat([](double SourceValue, double NormalizedValue){ return FText::AsNumber(SourceValue); });
	HDRCalibrationValueSetting->SetSettingParent(this);
}

// 返回 HDR 校准动作包含的最大亮度子设置。
TArray<UGameSetting*> ULyraSettingAction_HDRCalibrationEditor::GetChildSettings()
{
	return { HDRCalibrationValueSetting };
}

#undef LOCTEXT_NAMESPACE
