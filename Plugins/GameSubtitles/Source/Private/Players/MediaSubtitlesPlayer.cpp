// Copyright Epic Games, Inc. All Rights Reserved.

#include "Players/MediaSubtitlesPlayer.h"

#include "MediaPlayer.h"
#include "Overlays.h"
#include "Stats/Stats.h"
#include "SubtitleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaSubtitlesPlayer)

// 初始化媒体字幕播放器为未绑定 MediaPlayer 且停止显示的状态。
UMediaSubtitlesPlayer::UMediaSubtitlesPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MediaPlayer(nullptr)
	, bEnabled(false)
{
}

// UObject 销毁前停止字幕并清空 SubtitleManager 中以当前对象为键的电影字幕。
void UMediaSubtitlesPlayer::BeginDestroy()
{
	Stop();

	Super::BeginDestroy();
}

// 启用后续 Tick 的字幕时间轴采样，不主动改变绑定媒体的播放状态。
void UMediaSubtitlesPlayer::Play()
{
	bEnabled = true;
}

// 停止字幕时间轴采样，并立即清除当前对象提交给 SubtitleManager 的文本。
void UMediaSubtitlesPlayer::Stop()
{
	bEnabled = false;

	// 清除当前播放器对象对应的电影字幕，避免停止后残留最后一帧文本。
	// Clear the movie subtitle for this object
	FSubtitleManager::GetSubtitleManager()->SetMovieSubtitle(this, TArray<FString>());
}

// 替换用于按媒体时间查询的 Overlay 字幕源。
void UMediaSubtitlesPlayer::SetSubtitles(UOverlays* Subtitles)
{
	SourceSubtitles = Subtitles;
}

// 以弱引用绑定提供当前播放时间的 MediaPlayer，不取得其生命周期所有权。
void UMediaSubtitlesPlayer::BindToMediaPlayer(UMediaPlayer* InMediaPlayer)
{
	MediaPlayer = InMediaPlayer;
}

// 启用时按绑定媒体的当前时间查询 Overlay 条目并提交文本；媒体失效则自动停止并清空字幕。
void UMediaSubtitlesPlayer::Tick(float DeltaSeconds)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_UMediaSubtitlesPlayer_Tick);

	if (bEnabled && SourceSubtitles)
	{
		UMediaPlayer* MediaPlayerPtr = MediaPlayer.Get();
		if (MediaPlayerPtr)
		{
			FTimespan CurrentTime = MediaPlayerPtr->GetTime();
			TArray<FOverlayItem> CurrentSubtitles;
			SourceSubtitles->GetOverlaysForTime(CurrentTime, CurrentSubtitles);

			TArray<FString> SubtitlesText;
			for (const FOverlayItem& Subtitle : CurrentSubtitles)
			{
				SubtitlesText.Add(Subtitle.Text);
			}

			FSubtitleManager::GetSubtitleManager()->SetMovieSubtitle(this, SubtitlesText);
		}
		else
		{
			Stop();
		}
	}
}

