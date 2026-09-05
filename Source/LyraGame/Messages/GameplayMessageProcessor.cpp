// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayMessageProcessor.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayMessageProcessor)

// Actor 开始游戏后调用可覆写的消息监听启动入口。
void UGameplayMessageProcessor::BeginPlay()
{
	Super::BeginPlay();

	StartListening();
}

// 结束游戏时停止自定义监听，注销所有已记录消息句柄并清空列表。
void UGameplayMessageProcessor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	StopListening();

	// 组件结束时注销所有已记录的监听句柄，防止消息子系统继续回调已停止的处理器。
	// Remove any listener handles
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	for (FGameplayMessageListenerHandle& Handle : ListenerHandles)
	{
		MessageSubsystem.UnregisterListener(Handle);
	}
	ListenerHandles.Empty();
}

// 供派生类注册 Gameplay Message 监听的空扩展点。
void UGameplayMessageProcessor::StartListening()
{

}

// 供派生类停止自定义消息处理的空扩展点；通用句柄注销由 EndPlay 统一执行。
void UGameplayMessageProcessor::StopListening()
{
}

// 取得监听句柄所有权并保存，供 EndPlay 批量注销。
void UGameplayMessageProcessor::AddListenerHandle(FGameplayMessageListenerHandle&& Handle)
{
	ListenerHandles.Add(MoveTemp(Handle));
}

// 返回 GameState 同步的服务器世界时间；GameState 不可用时返回 0。
double UGameplayMessageProcessor::GetServerTime() const
{
	if (AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	else
	{
		return 0.0;
	}
}

