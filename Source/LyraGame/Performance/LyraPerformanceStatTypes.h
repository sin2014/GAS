// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumRange.h"

#include "LyraPerformanceStatTypes.generated.h"

//////////////////////////////////////////////////////////////////////

// 性能统计项在 HUD 中的显示方式。
// Way to display the stat
UENUM(BlueprintType)
enum class ELyraStatDisplayMode : uint8
{
	// 不显示此统计项。
	// Don't show this stat
	Hidden,

	// 仅以文本显示。
	// Show this stat in text form
	TextOnly,

	// 仅以曲线图显示。
	// Show this stat in graph form
	GraphOnly,

	// 同时显示文本和曲线图。
	// Show this stat as both text and graph
	TextAndGraph
};

//////////////////////////////////////////////////////////////////////

// 可显示在屏幕上的性能统计类型。
// Different kinds of stats that can be displayed on-screen
UENUM(BlueprintType)
enum class ELyraDisplayablePerformanceStat : uint8
{
	// 客户端实际帧率，单位 Hz。
	// stat fps (in Hz)
	ClientFPS,

	// 服务器 Tick 频率，单位 Hz。
	// server tick rate (in Hz)
	ServerFPS,
	
	// 等待垂直同步或帧率上限所消耗的空闲时间，单位秒。
	// idle time spent waiting for vsync or frame rate limit (in seconds)
	IdleTime,

	// 整帧总耗时，单位秒。
	// Stat unit overall (in seconds)
	FrameTime,

	// 游戏线程帧耗时，单位秒。
	// Stat unit (game thread, in seconds)
	FrameTime_GameThread,

	// 渲染线程帧耗时，单位秒。
	// Stat unit (render thread, in seconds)
	FrameTime_RenderThread,

	// RHI 线程帧耗时，单位秒。
	// Stat unit (RHI thread, in seconds)
	FrameTime_RHIThread,

	// 推算的 GPU 帧耗时，单位秒。
	// Stat unit (inferred GPU time, in seconds)
	FrameTime_GPU,

	// 网络往返延迟，单位毫秒。
	// Network ping (in ms)
	Ping,

	// 入站数据包平均丢包百分比。
	// The incoming packet loss percentage (%)
	PacketLoss_Incoming,

	// 出站数据包平均丢包百分比。
	// The outgoing packet loss percentage (%)
	PacketLoss_Outgoing,

	// 最近一秒接收的数据包数量。
	// The number of packets received in the last second
	PacketRate_Incoming,

	// 最近一秒发送的数据包数量。
	// The number of packets sent in the past second
	PacketRate_Outgoing,

	// 接收数据包的平均大小，单位字节。
	// The avg. size (in bytes) of packets received
	PacketSize_Incoming,

	// 发送数据包的平均大小，单位字节。
	// The avg. size (in bytes) of packets sent
	PacketSize_Outgoing,

	// 从输入到最终呈现的总延迟，单位毫秒。
	// The total latency in MS of the game
	Latency_Total,

	// 从游戏模拟开始到图形驱动提交结束的延迟。
	// Game simulation start to driver submission end
	Latency_Game,

	// 从操作系统渲染队列开始到 GPU 渲染结束的延迟。
	// OS render queue start to GPU render end
	Latency_Render,

	// 新增可显示统计类型必须放在 Count 之前。
	// New stats should go above here
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(ELyraDisplayablePerformanceStat, ELyraDisplayablePerformanceStat::Count);

//////////////////////////////////////////////////////////////////////
