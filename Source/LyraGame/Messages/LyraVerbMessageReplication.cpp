// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraVerbMessageReplication.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messages/LyraVerbMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraVerbMessageReplication)

//////////////////////////////////////////////////////////////////////
// FLyraVerbMessageReplicationEntry

// 返回复制条目中 Verb 消息的可读调试文本。
FString FLyraVerbMessageReplicationEntry::GetDebugString() const
{
	return Message.ToString();
}

//////////////////////////////////////////////////////////////////////
// FLyraVerbMessageReplication

// 向 FastArray 追加 Verb 消息条目并标记为脏，使其进入复制。
void FLyraVerbMessageReplication::AddMessage(const FLyraVerbMessage& Message)
{
	FLyraVerbMessageReplicationEntry& NewStack = CurrentMessages.Emplace_GetRef(Message);
	MarkItemDirty(NewStack);
}

// FastArray 条目移除前回调；当前没有需要维护的客户端派生状态。
void FLyraVerbMessageReplication::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
// 	for (int32 Index : RemovedIndices)
// 	{
// 		const FGameplayTag Tag = CurrentMessages[Index].Tag;
// 		TagToCountMap.Remove(Tag);
// 	}
}

// 客户端收到新增 FastArray 条目后，逐条重新广播到本地 Gameplay Message 子系统。
void FLyraVerbMessageReplication::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		const FLyraVerbMessageReplicationEntry& Entry = CurrentMessages[Index];
		RebroadcastMessage(Entry.Message);
	}
}

// 客户端收到已存在条目变化后，逐条重新广播更新后的 Verb 消息。
void FLyraVerbMessageReplication::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		const FLyraVerbMessageReplicationEntry& Entry = CurrentMessages[Index];
		RebroadcastMessage(Entry.Message);
	}
}

// 通过 Owner 所在世界的 Gameplay Message 子系统，以消息 Verb 为频道广播复制消息。
void FLyraVerbMessageReplication::RebroadcastMessage(const FLyraVerbMessage& Message)
{
	check(Owner);
	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(Owner);
	MessageSystem.BroadcastMessage(Message.Verb, Message);
}

