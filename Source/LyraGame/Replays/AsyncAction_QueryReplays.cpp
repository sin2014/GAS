// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replays/AsyncAction_QueryReplays.h"

#include "GameFramework/PlayerController.h"
#include "LyraReplaySubsystem.h"
#include "Templates/Greater.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_QueryReplays)

// 构造用于枚举本地回放流的蓝图异步动作。
UAsyncAction_QueryReplays::UAsyncAction_QueryReplays(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 玩家控制器有效时创建异步动作并保存弱引用；输入为空时返回空指针。
UAsyncAction_QueryReplays* UAsyncAction_QueryReplays::QueryReplays(APlayerController* InPlayerController)
{
	UAsyncAction_QueryReplays* Action = nullptr;

	if (InPlayerController != nullptr)
	{
		Action = NewObject<UAsyncAction_QueryReplays>();
		Action->PlayerController = InPlayerController;
	}

	return Action;
}

// 创建 Replay Streamer 和结果列表，异步枚举当前版本回放；Streamer 创建失败时立即广播空列表。
void UAsyncAction_QueryReplays::Activate()
{
	ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();

	ResultList = NewObject<ULyraReplayList>();
	if (ReplayStreamer.IsValid())
	{
		FNetworkReplayVersion EnumerateStreamsVersion = FNetworkVersion::GetReplayVersion();

		ReplayStreamer->EnumerateStreams(EnumerateStreamsVersion, INDEX_NONE, FString(), TArray<FString>(), FEnumerateStreamsCallback::CreateUObject(this, &ThisClass::OnEnumerateStreamsComplete));
	}
	else
	{
		QueryComplete.Broadcast(ResultList);
	}
}

// 把枚举结果包装为列表条目，按时间从新到旧排序后广播查询完成。
void UAsyncAction_QueryReplays::OnEnumerateStreamsComplete(const FEnumerateStreamsResult& Result)
{
	for (const FNetworkReplayStreamInfo& StreamInfo : Result.FoundStreams)
	{
		ULyraReplayListEntry* NewReplayEntry = NewObject<ULyraReplayListEntry>(ResultList);
		NewReplayEntry->StreamInfo = StreamInfo;
		ResultList->Results.Add(NewReplayEntry);
	}

	// 按录制时间从新到旧排列回放，而不是按名称排序。
	// Sort demo names by date
	Algo::SortBy(ResultList->Results, [](const TObjectPtr<ULyraReplayListEntry>& Data) { return Data->StreamInfo.Timestamp.GetTicks(); }, TGreater<>());

	QueryComplete.Broadcast(ResultList);
}

