// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkReplayStreaming.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"

#include "LyraReplaySubsystem.generated.h"

#define UE_API LYRAGAME_API

class UDemoNetDriver;
class APlayerController;
class ULocalPlayer;
struct FFrame;

/** 封装一条可在 UI 中展示和播放的回放流信息。 */
/** An available replay for display in the UI */
UCLASS(MinimalAPI, BlueprintType)
class ULyraReplayListEntry : public UObject
{
	GENERATED_BODY()

public:
	FNetworkReplayStreamInfo StreamInfo;

	/** 返回回放流面向用户的名称。 */
	/** The UI friendly name of the stream */
	UFUNCTION(BlueprintPure, Category=Replays)
	FString GetFriendlyName() const { return StreamInfo.FriendlyName; }

	/** 返回回放录制的日期和时间。 */
	/** The date and time the stream was recorded */
	UFUNCTION(BlueprintPure, Category=Replays)
	FDateTime GetTimestamp() const { return StreamInfo.Timestamp; }

	/** 将回放流的毫秒时长转换为 FTimespan。 */
	/** The duration of the stream in MS */
	UFUNCTION(BlueprintPure, Category=Replays)
	FTimespan GetDuration() const { return FTimespan::FromMilliseconds(StreamInfo.LengthInMS); }

	/** 返回当前观看该流的人数。 */
	/** Number of viewers viewing this stream */
	UFUNCTION(BlueprintPure, Category=Replays)
	int32 GetNumViewers() const { return StreamInfo.NumViewers; }

	/** 流仍在实时录制且对应游戏尚未结束时返回 true。 */
	/** True if the stream is live and the game hasn't completed yet */
	UFUNCTION(BlueprintPure, Category=Replays)
	bool GetIsLive() const { return StreamInfo.bIsLive; }
};

/** 异步回放查询返回给 UI 的结果列表。 */
/** Results of querying for replays list of results for the UI */
UCLASS(MinimalAPI, BlueprintType)
class ULyraReplayList : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category=Replays)
	TArray<TObjectPtr<ULyraReplayListEntry>> Results;
};

/** 管理回放录制、播放、跳转及本地旧回放清理的游戏实例子系统。 */
/** Subsystem to handle recording/loading replays */
UCLASS(MinimalAPI)
class ULyraReplaySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UE_API ULyraReplaySubsystem();

	/** 平台特征包含回放支持标签时返回 true。 */
	/** Returns true if this platform supports replays at all */
	UFUNCTION(BlueprintCallable, Category = Replays, BlueprintPure = false)
	static UE_API bool DoesPlatformSupportReplays();

	/** 返回设置界面判断回放支持能力所使用的平台特征标签。 */
	/** Returns the trait tag for platform support, used in options */
	static UE_API FGameplayTag GetPlatformSupportTraitTag();

	/** 让 GameInstance 按回放流名称加载所需地图并播放回放。 */
	/** Loads the appropriate map and plays a replay */
	UFUNCTION(BlueprintCallable, Category=Replays)
	UE_API void PlayReplay(ULyraReplayListEntry* Replay);

	/** 开始录制客户端回放，并按本机设置异步清理超出保留数量的旧回放。 */
	/** Starts recording a client replay, and handles any file cleanup needed */
	UFUNCTION(BlueprintCallable, Category = Replays)
	UE_API void RecordClientReplay(APlayerController* PlayerController);

	/** 从最旧且未标记保留的回放开始逐个删除，直到数量不超过 NumReplaysToKeep。 */
	/** Starts deleting local replays starting with the oldest until there are NumReplaysToKeep or fewer */
	UFUNCTION(BlueprintCallable, Category = Replays)
	UE_API void CleanupLocalReplays(ULocalPlayer* LocalPlayer, int32 NumReplaysToKeep);

	/** 将当前回放跳转到指定的绝对播放时间。 */
	/** Move forward or back in currently playing replay */
	UFUNCTION(BlueprintCallable, Category=Replays)
	UE_API void SeekInActiveReplay(float TimeInSeconds);

	/** 返回当前回放总时长；没有 DemoNetDriver 时返回 0。 */
	/** Gets length of current replay */
	UFUNCTION(BlueprintCallable, Category = Replays, BlueprintPure = false)
	UE_API float GetReplayLengthInSeconds() const;

	/** 返回当前回放播放位置；没有 DemoNetDriver 时返回 0。 */
	/** Gets current playback time */
	UFUNCTION(BlueprintCallable, Category=Replays, BlueprintPure=false)
	UE_API float GetReplayCurrentTime() const;

private:
	TSharedPtr<INetworkReplayStreamer> CurrentReplayStreamer;

	UPROPERTY()
	TObjectPtr<ULocalPlayer> LocalPlayerDeletingReplays;

	int32 DeletingReplaysNumberToKeep;

	UDemoNetDriver* GetDemoDriver() const;

	void OnEnumerateStreamsCompleteForDelete(const FEnumerateStreamsResult& Result);
	void OnDeleteReplay(const FDeleteFinishedStreamResult& DeleteResult);
};

#undef UE_API
