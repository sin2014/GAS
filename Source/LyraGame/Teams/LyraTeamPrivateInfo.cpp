// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTeamPrivateInfo.h"
#include "Teams/LyraTeamInfoBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTeamPrivateInfo)

// 构造队伍私有信息 Actor；当前仍使用基础复制策略，等待 ReplicationGraph 限制可见范围。
ALyraTeamPrivateInfo::ALyraTeamPrivateInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//@TODO：通过 ReplicationGraph 将该信息真正限制为仅队伍成员可见。
	//@TODO: Actually make private (using replication graph)
}

