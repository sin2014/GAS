// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubtitleDisplaySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SubtitleDisplaySubsystem)

class FSubsystemCollectionBase;

// 通过 LocalPlayer 所属 GameInstance 获取共享字幕显示子系统，输入为空时返回空。
USubtitleDisplaySubsystem* USubtitleDisplaySubsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetGameInstance()->GetSubsystem<USubtitleDisplaySubsystem>() : nullptr;
}

// 构造字幕显示子系统，FSubtitleFormat 使用结构体默认显示配置。
USubtitleDisplaySubsystem::USubtitleDisplaySubsystem()
{
}

// GameInstanceSubsystem 初始化入口当前不注册额外依赖或外部回调。
void USubtitleDisplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{

}

// GameInstanceSubsystem 反初始化入口当前没有额外资源需要释放。
void USubtitleDisplaySubsystem::Deinitialize()
{

}

// 返回当前字幕字号、颜色、边框和背景透明度格式的只读引用。
const FSubtitleFormat& USubtitleDisplaySubsystem::GetSubtitleDisplayOptions() const
{
	return SubtitleFormat;
}

// 替换当前字幕格式并广播变更事件，使已构建的字幕控件立即重建样式。
void USubtitleDisplaySubsystem::SetSubtitleDisplayOptions(const FSubtitleFormat& InOptions)
{
	SubtitleFormat = InOptions;
	DisplayFormatChangedEvent.Broadcast(SubtitleFormat);
}

