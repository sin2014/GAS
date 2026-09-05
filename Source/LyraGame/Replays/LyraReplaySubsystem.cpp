// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraReplaySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DemoNetDriver.h"
#include "Internationalization/Text.h"
#include "Misc/DateTime.h"
#include "CommonUISettings.h"
#include "ICommonUIModule.h"
#include "LyraLogChannels.h"
#include "Player/LyraLocalPlayer.h"
#include "Settings/LyraSettingsLocal.h"
#include "Templates/Greater.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraReplaySubsystem)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Platform_Trait_ReplaySupport, "Platform.Trait.ReplaySupport");

// 构造负责回放播放、录制、清理和时间控制的 GameInstance 子系统。
ULyraReplaySubsystem::ULyraReplaySubsystem()
{
}

// 根据 CommonUI 平台特性标签判断当前平台是否支持回放。
bool ULyraReplaySubsystem::DoesPlatformSupportReplays()
{
	if (ICommonUIModule::GetSettings().GetPlatformTraits().HasTag(GetPlatformSupportTraitTag()))
	{
		return true;
	}
	return false;
}

// 返回声明回放支持能力的平台特性标签。
FGameplayTag ULyraReplaySubsystem::GetPlatformSupportTraitTag()
{
	return TAG_Platform_Trait_ReplaySupport.GetTag();
}

// 使用列表条目的流名称请求 GameInstance 播放回放；条目为空时不处理。
void ULyraReplaySubsystem::PlayReplay(ULyraReplayListEntry* Replay)
{
	if (Replay != nullptr)
	{
		FString DemoName = Replay->StreamInfo.Name;
		GetGameInstance()->PlayReplay(DemoName);
	}
}

// 平台和控制器有效时开始客户端回放录制，并按本地设置启动旧回放清理。
void ULyraReplaySubsystem::RecordClientReplay(APlayerController* PlayerController)
{
	if (ensure(DoesPlatformSupportReplays() && PlayerController))
	{
		FText FriendlyNameText = FText::Format(NSLOCTEXT("Lyra", "LyraReplayName_Format", "Client Replay {0}"), FText::AsDateTime(FDateTime::UtcNow(), EDateTimeStyle::Short, EDateTimeStyle::Short));
		GetGameInstance()->StartRecordingReplay(FString(), FriendlyNameText.ToString());

		if (ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(PlayerController->GetLocalPlayer()))
		{
			// 录制开始后按用户设置启动旧回放清理，同时把当前实时流计入保留数量。
			// Start a cleanup of existing saved streams
			int32 NumToKeep = LyraLocalPlayer->GetLocalSettings()->GetNumberOfReplaysToKeep();
			CleanupLocalReplays(LyraLocalPlayer, NumToKeep);
		}
	}
}

// 在没有其他清理进行且保留数非零时创建 Streamer，枚举该用户全部版本回放并启动逐条删除流程。
void ULyraReplaySubsystem::CleanupLocalReplays(ULocalPlayer* LocalPlayer, int32 NumReplaysToKeep)
{
	// TODO：此清理流程只在通用文件流实现上测试过，使用存档流实现时可能不完全兼容。
	// 每轮只删除一条并重新枚举，因为删除可能涉及服务器或存档查询，使此前取得的回放列表失效；失败或数量达标时停止。
	// TODO this was only tested with the generic file streamer and may not fully work with the save game streamer
	// This only handles one delete at a time, and will loop until it gets an error or goes below NumReplaysToKeep
	// It does it this way because each delete may involve a server or save game query that invalidates the replay list
	if (LocalPlayer != nullptr && LocalPlayerDeletingReplays == nullptr && NumReplaysToKeep != 0)
	{
		LocalPlayerDeletingReplays = LocalPlayer;
		DeletingReplaysNumberToKeep = NumReplaysToKeep;

		CurrentReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();
		if (CurrentReplayStreamer.IsValid())
		{
			// 使用默认空版本条件枚举，确保旧版本录制的回放也参与清理。
			// Use the default version to get old version replays as well
			FNetworkReplayVersion EnumerateStreamsVersion;

			CurrentReplayStreamer->EnumerateStreams(EnumerateStreamsVersion, LocalPlayer->GetPlatformUserIndex(), FString(), TArray<FString>(), FEnumerateStreamsCallback::CreateUObject(this, &ThisClass::OnEnumerateStreamsCompleteForDelete));
		}
	}
}

// 过滤保留流、按时间排序并计入可能缺失的实时录制；超限时删除首条旧流，否则释放清理上下文。
void ULyraReplaySubsystem::OnEnumerateStreamsCompleteForDelete(const FEnumerateStreamsResult& Result)
{
	if (!CurrentReplayStreamer.IsValid() || !IsValid(LocalPlayerDeletingReplays))
	{
		// 回放流或发起清理的本地玩家已失效，终止当前清理回调。
		// Lost context, don't do anything
		return;
	}

	TArray<FNetworkReplayStreamInfo> StreamsToDelete;
	for (const FNetworkReplayStreamInfo& StreamInfo : Result.FoundStreams)
	{
		// 永不删除显式标记为保留的回放流。
		// Never delete keep streams
		if (!StreamInfo.bShouldKeep)
		{
			StreamsToDelete.Add(StreamInfo);
		}
	}

	// 按录制时间从新到旧排序，索引超过保留数量的第一项即为最旧待删除项。
	// Sort by date
	Algo::SortBy(StreamsToDelete, [](const FNetworkReplayStreamInfo& Data) { return Data.Timestamp.GetTicks(); }, TGreater<>());

	if (UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		if (DemoDriver->IsRecording())
		{
			// 正在录制的实时流不一定出现在枚举结果中；缺失时插入占位项，使保留数量仍包含当前录制。
			// If we're recording, the live stream may or may not show up in the query which affects the keep count
			// Add a fake live stream if the active one is missing from the results
			if (StreamsToDelete.Num() > 0 && !StreamsToDelete[0].bIsLive)
			{
				StreamsToDelete.Insert(FNetworkReplayStreamInfo(), 0);
			}
		}
	}

	if (StreamsToDelete.Num() > DeletingReplaysNumberToKeep)
	{
		// 删除保留范围之外的第一条回放；成功后重新枚举继续清理，失败则停止循环。
		// Delete the first replay above the limit, if successful it won't be in the loop during the next loop
		// If unsuccessful, it will stop looping
		FString ReplayName = StreamsToDelete[DeletingReplaysNumberToKeep].Name;
		UE_LOG(LogLyra, Log, TEXT("LyraReplaySubsystem asked to delete replay %s"), *ReplayName);
		CurrentReplayStreamer->DeleteFinishedStream(ReplayName, LocalPlayerDeletingReplays->GetPlatformUserIndex(), FDeleteFinishedStreamCallback::CreateUObject(this, &ThisClass::OnDeleteReplay));
	}
	else
	{
		// 数量已不超过限制，释放清理上下文并结束迭代。
		// We're below the limit so stop iterating
		CurrentReplayStreamer = nullptr;
		LocalPlayerDeletingReplays = nullptr;
		DeletingReplaysNumberToKeep = 0;
	}
}

// 删除成功后重新枚举继续清理；失败时记录警告并释放清理上下文。
void ULyraReplaySubsystem::OnDeleteReplay(const FDeleteFinishedStreamResult& DeleteResult)
{
	if (!CurrentReplayStreamer.IsValid() || !IsValid(LocalPlayerDeletingReplays))
	{
		// 回放流或发起清理的本地玩家已失效，忽略删除完成回调。
		// Lost context, don't do anything
		return;
	}

	if (DeleteResult.WasSuccessful())
	{
		// 删除成功后重新枚举，确认是否还需继续删除。
		// Enumerate list again to see if we're under the limit yet
		FNetworkReplayVersion EnumerateStreamsVersion;

		CurrentReplayStreamer->EnumerateStreams(EnumerateStreamsVersion, LocalPlayerDeletingReplays->GetPlatformUserIndex(), FString(), TArray<FString>(), FEnumerateStreamsCallback::CreateUObject(this, &ThisClass::OnEnumerateStreamsCompleteForDelete));
	}
	else
	{
		// 删除失败后停止后续清理。
		// TODO：应接入平台专用错误报告流程，而不只是写日志。
		// Failed, stop trying to delete anything else
		// TODO properly integrate with platform-specific error reporting
		UE_LOG(LogLyra, Warning, TEXT("Failed to delete replay with error %d!"), (int32)DeleteResult.Result);

		CurrentReplayStreamer = nullptr;
		LocalPlayerDeletingReplays = nullptr;
		DeletingReplaysNumberToKeep = 0;
	}
}

// 活动 DemoNetDriver 存在时跳转到指定回放时间。
void ULyraReplaySubsystem::SeekInActiveReplay(float TimeInSeconds)
{
	if (UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		DemoDriver->GotoTimeInSeconds(TimeInSeconds);
	}
}

// 返回活动回放总时长；没有 DemoNetDriver 时返回 0。
float ULyraReplaySubsystem::GetReplayLengthInSeconds() const
{
	if (UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		return DemoDriver->GetDemoTotalTime();
	}
	return 0.0f;
}

// 返回活动回放当前时间；没有 DemoNetDriver 时返回 0。
float ULyraReplaySubsystem::GetReplayCurrentTime() const
{
	if (UDemoNetDriver* DemoDriver = GetDemoDriver())
	{
		return DemoDriver->GetDemoCurrentTime();
	}
	return 0.0f;
}

// 从 GameInstance 当前世界取得 DemoNetDriver；世界不存在时返回空指针。
UDemoNetDriver* ULyraReplaySubsystem::GetDemoDriver() const
{
	if (UWorld* World = GetGameInstance()->GetWorld())
	{
		return World->GetDemoNetDriver();
	}
	return nullptr;
}



