// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeaponDebugSettings.h"
#include "Misc/App.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponDebugSettings)

// 构造武器调试开发者设置对象。
ULyraWeaponDebugSettings::ULyraWeaponDebugSettings()
{
}

// 把武器调试设置归入当前项目名称对应的开发者设置分类。
FName ULyraWeaponDebugSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

