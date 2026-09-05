// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameViewportClient.h"

#include "CommonUISettings.h"
#include "ICommonUIModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameViewportClient)

class UGameInstance;

namespace GameViewportTags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_Input_HardwareCursor, "Platform.Trait.Input.HardwareCursor");
}

// 使用默认对象初始化器构造 Lyra 游戏视口客户端。
ULyraGameViewportClient::ULyraGameViewportClient()
	: Super(FObjectInitializer::Get())
{
}

// 视口初始化后按平台硬件光标特性启用或禁用软件光标控件。
void ULyraGameViewportClient::Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	
	// 项目设置中的软件光标用于主机和移动端；桌面平台优先使用系统硬件光标，因此不创建软件光标控件。
	// We have software cursors set up in our project settings for console/mobile use, but on desktop we're fine with
	// the standard hardware cursors
	const bool UseHardwareCursor = ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(GameViewportTags::TAG_Platform_Trait_Input_HardwareCursor);
	SetUseSoftwareCursorWidgets(!UseHardwareCursor);
}
