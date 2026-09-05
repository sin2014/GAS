// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditCondition/WhenPlatformHasTrait.h"

#include "CommonUIVisibilitySubsystem.h"

#define LOCTEXT_NAMESPACE "GameSetting"

// 创建“缺少指定平台特性时隐藏设置”的编辑条件。
TSharedRef<FWhenPlatformHasTrait> FWhenPlatformHasTrait::KillIfMissing(FGameplayTag InVisibilityTag, const FString& InKillReason)
{
	check(InVisibilityTag.IsValid());
	check(!InKillReason.IsEmpty());

	TSharedRef<FWhenPlatformHasTrait> Result = MakeShared<FWhenPlatformHasTrait>();
	Result->VisibilityTag = InVisibilityTag;
	Result->KillReason = InKillReason;
	Result->bTagDesired = true;

	return Result;
}

// 创建“缺少指定平台特性时禁用设置”的编辑条件。
TSharedRef<FWhenPlatformHasTrait> FWhenPlatformHasTrait::DisableIfMissing(FGameplayTag InVisibilityTag, const FText& InDisableReason)
{
	check(InVisibilityTag.IsValid());
	check(!InDisableReason.IsEmpty());

	TSharedRef<FWhenPlatformHasTrait> Result = MakeShared<FWhenPlatformHasTrait>();
	Result->VisibilityTag = InVisibilityTag;
	Result->DisableReason = InDisableReason;
	Result->bTagDesired = true;

	return Result;
}

// 创建“存在指定平台特性时隐藏设置”的编辑条件。
TSharedRef<FWhenPlatformHasTrait> FWhenPlatformHasTrait::KillIfPresent(FGameplayTag InVisibilityTag, const FString& InKillReason)
{
	check(InVisibilityTag.IsValid());
	check(!InKillReason.IsEmpty());

	TSharedRef<FWhenPlatformHasTrait> Result = MakeShared<FWhenPlatformHasTrait>();
	Result->VisibilityTag = InVisibilityTag;
	Result->KillReason = InKillReason;
	Result->bTagDesired = false;

	return Result;
}

// 创建“存在指定平台特性时禁用设置”的编辑条件。
TSharedRef<FWhenPlatformHasTrait> FWhenPlatformHasTrait::DisableIfPresent(FGameplayTag InVisibilityTag, const FText& InDisableReason)
{
	check(InVisibilityTag.IsValid());
	check(!InDisableReason.IsEmpty());

	TSharedRef<FWhenPlatformHasTrait> Result = MakeShared<FWhenPlatformHasTrait>();
	Result->VisibilityTag = InVisibilityTag;
	Result->DisableReason = InDisableReason;
	Result->bTagDesired = false;

	return Result;
}

// 查询 CommonUI 平台特性；根据“存在/缺失”条件隐藏或禁用设置，并写入对应原因。
void FWhenPlatformHasTrait::GatherEditState(const ULocalPlayer* InLocalPlayer, FGameSettingEditableState& InOutEditState) const
{
	if (UCommonUIVisibilitySubsystem::GetChecked(InLocalPlayer)->HasVisibilityTag(VisibilityTag) != bTagDesired)
	{
		if (KillReason.IsEmpty())
		{
			InOutEditState.Disable(DisableReason);
		}
		else
		{
			InOutEditState.Kill(KillReason);
		}
	}
}

#undef LOCTEXT_NAMESPACE
