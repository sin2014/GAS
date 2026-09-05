// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraRuntimeOptions.h"

#include "UObject/Class.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraRuntimeOptions)

// 将运行时选项控制台命令前缀设置为 ro。
ULyraRuntimeOptions::ULyraRuntimeOptions()
{
	OptionCommandPrefix = TEXT("ro");
}

// 返回可修改的运行时选项类默认对象。
ULyraRuntimeOptions* ULyraRuntimeOptions::GetRuntimeOptions()
{
	return GetMutableDefault<ULyraRuntimeOptions>();
}

// 返回只读的运行时选项类默认对象引用。
const ULyraRuntimeOptions& ULyraRuntimeOptions::Get()
{
	const ULyraRuntimeOptions& RuntimeOptions = *GetDefault<ULyraRuntimeOptions>();
	return RuntimeOptions;
}
