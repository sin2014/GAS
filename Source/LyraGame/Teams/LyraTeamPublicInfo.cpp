// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTeamPublicInfo.h"

#include "Net/UnrealNetwork.h"
#include "Teams/LyraTeamInfoBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTeamPublicInfo)

class FLifetimeProperty;

// 构造队伍公开信息 Actor，并将 TeamDisplayAsset 初始化为空。
ALyraTeamPublicInfo::ALyraTeamPublicInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 在基础 TeamInfo 属性之外登记 TeamDisplayAsset 复制。
void ALyraTeamPublicInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, TeamDisplayAsset, COND_InitialOnly);
}

// 仅允许权威端设置一次 DisplayAsset，并立即通知本地 TeamSubsystem 更新显示信息。
void ALyraTeamPublicInfo::SetTeamDisplayAsset(TObjectPtr<ULyraTeamDisplayAsset> NewDisplayAsset)
{
	check(HasAuthority());
	check(TeamDisplayAsset == nullptr);

	TeamDisplayAsset = NewDisplayAsset;

	TryRegisterWithTeamSubsystem();
}

// 客户端收到新的 DisplayAsset 后重新向 TeamSubsystem 注册公开信息并触发观察者更新。
void ALyraTeamPublicInfo::OnRep_TeamDisplayAsset()
{
	TryRegisterWithTeamSubsystem();
}

