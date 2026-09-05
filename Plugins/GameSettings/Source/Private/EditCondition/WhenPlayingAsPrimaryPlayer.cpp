// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditCondition/WhenPlayingAsPrimaryPlayer.h"

#include "Engine/LocalPlayer.h"

#define LOCTEXT_NAMESPACE "GameSetting"

// 返回共享的主玩家编辑条件实例，避免重复分配无状态条件。
TSharedRef<FWhenPlayingAsPrimaryPlayer> FWhenPlayingAsPrimaryPlayer::Get()
{
	// 条件本身无可变状态，因此所有设置共享同一实例。
	static TSharedRef<FWhenPlayingAsPrimaryPlayer> Instance = MakeShared<FWhenPlayingAsPrimaryPlayer>();
	return Instance;
}

// 仅允许主本地玩家编辑该设置；其他本地玩家会收到禁用状态。
void FWhenPlayingAsPrimaryPlayer::GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const
{
	if (!InLocalPlayer->IsPrimaryPlayer())
	{
		InOutEditState.Disable(LOCTEXT("OnlyPrimaryPlayerEditable", "Can only be changed by the primary player."));
	}
}

#undef LOCTEXT_NAMESPACE
