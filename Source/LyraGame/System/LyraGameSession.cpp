// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameSession.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameSession)


// 构造 Lyra GameSession，当前不增加基础会话状态。
ALyraGameSession::ALyraGameSession(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 跳过 AGameSession 的自动登录实现并直接报告成功，专用服务器登录由 LyraGameMode 负责。
bool ALyraGameSession::ProcessAutoLogin()
{
	// 自动登录实际由 LyraGameMode::TryDedicatedServerLogin 统一处理。
	// This is actually handled in LyraGameMode::TryDedicatedServerLogin
	return true;
}

// 将比赛开始通知透传给基础 GameSession，保留项目级会话扩展入口。
void ALyraGameSession::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
}

// 将比赛结束通知透传给基础 GameSession，保留项目级会话收尾入口。
void ALyraGameSession::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
}

