// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingFilterState.h"
#include "GameplayTagContainer.h"

#define UE_API GAMESETTINGS_API

class ULocalPlayer;

//////////////////////////////////////////////////////////////////////
// 平台特性编辑条件依据 CommonUI 特性标签隐藏或禁用设置。
// FWhenPlatformHasTrait

// 根据 CommonUI 平台特性决定设置是否显示或可编辑的编辑条件。
// Edit condition for game settings that checks CommonUI's platform traits
// to determine whether or not to show a setting
class FWhenPlatformHasTrait : public FGameSettingEditCondition
{
public:
	static UE_API TSharedRef<FWhenPlatformHasTrait> KillIfMissing(FGameplayTag InVisibilityTag, const FString& InKillReason);
	static UE_API TSharedRef<FWhenPlatformHasTrait> DisableIfMissing(FGameplayTag InVisibilityTag, const FText& InDisableReason);

	static UE_API TSharedRef<FWhenPlatformHasTrait> KillIfPresent(FGameplayTag InVisibilityTag, const FString& InKillReason);
	static UE_API TSharedRef<FWhenPlatformHasTrait> DisableIfPresent(FGameplayTag InVisibilityTag, const FText& InDisableReason);

	// 以下函数根据平台特性向设置可编辑状态写入隐藏或禁用结果。
	//~FGameSettingEditCondition interface
	UE_API virtual void GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const override;
	// 以上为 FGameSettingEditCondition 接口覆盖。
	//~End of FGameSettingEditCondition interface

private:
	FGameplayTag VisibilityTag;
	bool bTagDesired;
	FString KillReason;
	FText DisableReason;
};

#undef UE_API
