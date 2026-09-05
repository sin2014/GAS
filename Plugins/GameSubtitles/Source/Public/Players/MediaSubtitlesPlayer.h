// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"

#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "MediaSubtitlesPlayer.generated.h"

#define UE_API GAMESUBTITLES_API

class UMediaPlayer;
class UOverlays;
struct FFrame;

/**
 * 面向游戏媒体字幕的时间轴播放器。它应与 MediaPlayer 一同存在，并在媒体执行 Play、Pause 或 Stop 时由调用方同步控制，
 * 从而使用媒体当前时间选择正确的 Overlay 字幕。
 */
/**
 * A Game-specific player for media subtitles. This needs to exist next to Media Players
 * and have its Play() / Pause() / Stop() methods called at the same time as the media players'
 * methods.
 */
UCLASS(MinimalAPI, BlueprintType)
class UMediaSubtitlesPlayer
	: public UObject
	, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:

	/** 当前播放器用于按时间查询文本的 Overlay 字幕资源。 */
	/** The subtitles to use for this player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitles Source")
	TObjectPtr<UOverlays> SourceSubtitles;

public:

	UE_API virtual void BeginDestroy() override;

	/** 开始按绑定媒体时间更新当前字幕源。 */
	/** Begins playing the currently set subtitles. */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void Play();

	/** 停止字幕更新并清除当前显示文本。 */
	/** Stops the subtitle player. */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void Stop();

	/** 替换播放器使用的 Overlay 字幕源。 */
	/** Sets the source with the new subtitles set. */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void SetSubtitles(UOverlays* Subtitles);

	/** 绑定提供播放时间的 MediaPlayer，字幕播放器不会控制媒体本身。 */
	/** Binds the subtitle playback to the tick of a media player. */
	UFUNCTION(BlueprintCallable, Category="Game Subtitles|Subtitles Player")
	UE_API void BindToMediaPlayer(UMediaPlayer* InMediaPlayer);

public:

	//~ FTickableGameObject 接口实现。
	//~ FTickableGameObject interface
	UE_API virtual void Tick(float DeltaSeconds) override;
	virtual ETickableTickType GetTickableTickType() const override { return (HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Always); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMediaSubtitlesPlayer, STATGROUP_Tickables); }

private:

	/** 提供当前播放时间的 MediaPlayer 弱引用。 */
	/** A reference to our media player */
	TWeakObjectPtr<class UMediaPlayer> MediaPlayer;

	/** 是否启用 Tick 中的字幕查询和显示更新。 */
	/** Whether the subtitles are currently being displayed */
	bool bEnabled;
};

#undef UE_API
