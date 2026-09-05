// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/LyraInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraInputUserSettings)

// 先应用 Enhanced Input 基础用户设置，再保留 Lyra 项目在设置落地后执行附加同步的入口。
void ULyraInputUserSettings::ApplySettings()
{
	Super::ApplySettings();

	// 可在此添加输入设置应用到用户时需要执行的项目逻辑；也是调试设置应用流程的入口。
	// Add any functionality you want to happen when the input settings are applied to the user
	// This is a good place to put a breakpoint in your debugger to see the flow of
	// how input settings are used :)
}
