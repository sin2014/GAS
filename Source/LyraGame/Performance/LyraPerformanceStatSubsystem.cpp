// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerformanceStatSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameState.h"
#include "Performance/LyraPerformanceStatTypes.h"
#include "Performance/LatencyMarkerModule.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerformanceStatSubsystem)

CSV_DEFINE_CATEGORY(LyraPerformance, /*bIsEnabledByDefault=*/false);

class FSubsystemCollectionBase;

//////////////////////////////////////////////////////////////////////
// FLyraPerformanceStatCache

// 性能数据消费者开始采样的扩展点；当前不需要额外初始化。
void FLyraPerformanceStatCache::StartCharting()
{
}

// 每帧缓存客户端与服务器帧率、线程耗时、网络统计和首个有效延迟标记模块的数据。
void FLyraPerformanceStatCache::ProcessFrame(const FFrameData& FrameData)
{
	// 从引擎帧数据记录客户端 FPS、等待时间以及各处理线程和 GPU 的帧耗时。
	// Record stats about the frame data
	{
		RecordStat(
			ELyraDisplayablePerformanceStat::ClientFPS,
			(FrameData.TrueDeltaSeconds != 0.0) ?
			1.0 / FrameData.TrueDeltaSeconds :
			0.0);

		RecordStat(ELyraDisplayablePerformanceStat::IdleTime, FrameData.IdleSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime, FrameData.TrueDeltaSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_GameThread, FrameData.GameThreadTimeSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_RenderThread, FrameData.RenderThreadTimeSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_RHIThread, FrameData.RHIThreadTimeSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_GPU, FrameData.GPUTimeSeconds);	
	}

	if (UWorld* World = MySubsystem->GetGameInstance()->GetWorld())
	{
		// 从 GameState、本地 PlayerState 和 NetConnection 记录服务器 FPS、Ping、丢包率、包速率及平均包大小。
		// Record some networking related stats
		if (const ALyraGameState* GameState = World->GetGameState<ALyraGameState>())
		{
			RecordStat(ELyraDisplayablePerformanceStat::ServerFPS, GameState->GetServerFPS());
		}

		if (APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(World))
		{
			if (APlayerState* PS = LocalPC->GetPlayerState<APlayerState>())
			{
				RecordStat(ELyraDisplayablePerformanceStat::Ping, PS->GetPingInMilliseconds());
			}

			if (UNetConnection* NetConnection = LocalPC->GetNetConnection())
			{
				const UNetConnection::FNetConnectionPacketLoss& InLoss = NetConnection->GetInLossPercentage();
				RecordStat(ELyraDisplayablePerformanceStat::PacketLoss_Incoming, InLoss.GetAvgLossPercentage());
				
				const UNetConnection::FNetConnectionPacketLoss& OutLoss = NetConnection->GetOutLossPercentage();
				RecordStat(ELyraDisplayablePerformanceStat::PacketLoss_Outgoing, OutLoss.GetAvgLossPercentage());
				
				RecordStat(ELyraDisplayablePerformanceStat::PacketRate_Incoming, NetConnection->InPacketsPerSecond);
				RecordStat(ELyraDisplayablePerformanceStat::PacketRate_Outgoing, NetConnection->OutPacketsPerSecond);

				RecordStat(ELyraDisplayablePerformanceStat::PacketSize_Incoming, (NetConnection->InPacketsPerSecond != 0) ? (NetConnection->InBytesPerSecond / (float)NetConnection->InPacketsPerSecond) : 0.0f);
				RecordStat(ELyraDisplayablePerformanceStat::PacketSize_Outgoing, (NetConnection->OutPacketsPerSecond != 0) ? (NetConnection->OutBytesPerSecond / (float)NetConnection->OutPacketsPerSecond) : 0.0f);
			}
			
			// 最后从首个已启用且返回有效总延迟的 ILatencyMarkerModule 记录输入到呈现的延迟指标。
			// Finally, record some input latency related stats if they are enabled
			TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
			for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
			{
				if (LatencyMarkerModule->GetEnabled())
				{
					const float TotalLatencyMs = LatencyMarkerModule->GetTotalLatencyInMs();
					if (TotalLatencyMs > 0.0f)
					{
						// 缓存总延迟、游戏阶段延迟和渲染阶段延迟，供 HUD 查询。
						// Record some stats about the latency of the game
						RecordStat(ELyraDisplayablePerformanceStat::Latency_Total, TotalLatencyMs);
						RecordStat(ELyraDisplayablePerformanceStat::Latency_Game, LatencyMarkerModule->GetGameLatencyInMs());
						RecordStat(ELyraDisplayablePerformanceStat::Latency_Render, LatencyMarkerModule->GetRenderLatencyInMs());

						// 同时将延迟值写入 LyraPerformance CSV 分类；可通过下列 CsvProfile 命令开始、停止或限定帧数采样。
						// Record some CSV profile stats.
						// You can see these by using the following commands
						// Start and stop the profile:
						//	CsvProfile Start
						//	CsvProfile Stop
						//
						// 也可以只采集指定帧数。
						// Or, you can profile for a certain number of frames:
						// CsvProfile Frames=10
						//
						// 分析结果会输出为 Saved\Profiling\CSV 目录下的 .csv 文件。
						// And this will output a .csv file to the Saved\Profiling\CSV folder
#if CSV_PROFILER
						if (FCsvProfiler* Profiler = FCsvProfiler::Get())
						{
							static const FName TotalLatencyStatName = TEXT("Lyra_Latency_Total");
							Profiler->RecordCustomStat(TotalLatencyStatName, CSV_CATEGORY_INDEX(LyraPerformance), TotalLatencyMs, ECsvCustomStatOp::Set);

							static const FName GameLatencyStatName = TEXT("Lyra_Latency_Game");
							Profiler->RecordCustomStat(GameLatencyStatName, CSV_CATEGORY_INDEX(LyraPerformance), LatencyMarkerModule->GetGameLatencyInMs(), ECsvCustomStatOp::Set);

							static const FName RenderLatencyStatName = TEXT("Lyra_Latency_Render");
							Profiler->RecordCustomStat(RenderLatencyStatName, CSV_CATEGORY_INDEX(LyraPerformance), LatencyMarkerModule->GetRenderLatencyInMs(), ECsvCustomStatOp::Set);
						}
#endif

						// 如需更细粒度数据，还可从延迟标记模块查询驱动、操作系统渲染队列和 GPU 渲染延迟。
						// Some more fine grain latency numbers can be found on the marker module if desired
						//LatencyMarkerModule->GetRenderLatencyInMs()));
						//LatencyMarkerModule->GetDriverLatencyInMs()));
						//LatencyMarkerModule->GetOSRenderQueueLatencyInMs()));
						//LatencyMarkerModule->GetGPURenderLatencyInMs()));
						break;	
					}
				}
			}
		}
	}
}

// 性能数据消费者停止采样的扩展点；当前不需要额外清理。
void FLyraPerformanceStatCache::StopCharting()
{
}

// 取得指定统计项的采样缓存并追加本次值。
void FLyraPerformanceStatCache::RecordStat(const ELyraDisplayablePerformanceStat Stat, const double Value)
{
	PerfStateCache.FindOrAdd(Stat).RecordSample(Value);
}

// 返回指定统计项最近一次缓存值；尚无缓存时返回 0。
double FLyraPerformanceStatCache::GetCachedStat(ELyraDisplayablePerformanceStat Stat) const
{
	static_assert((int32)ELyraDisplayablePerformanceStat::Count == 18, "Need to update this function to deal with new performance stats");

	if (const FSampledStatCache* Cache = GetCachedStatData(Stat))
	{
		return Cache->GetLastCachedStat();
	}

	return 0.0;
}

// 返回指定统计项的完整采样缓存指针；尚未记录时返回空指针。
const FSampledStatCache* FLyraPerformanceStatCache::GetCachedStatData(const ELyraDisplayablePerformanceStat Stat) const
{
	static_assert((int32)ELyraDisplayablePerformanceStat::Count == 18, "Need to update this function to deal with new performance stats");
	
	return PerfStateCache.Find(Stat);
}

//////////////////////////////////////////////////////////////////////
// ULyraPerformanceStatSubsystem

// 创建性能统计缓存并注册为 GEngine 的性能数据消费者。
void ULyraPerformanceStatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Tracker = MakeShared<FLyraPerformanceStatCache>(this);
	GEngine->AddPerformanceDataConsumer(Tracker);
}

// 从 GEngine 移除性能数据消费者并释放缓存共享指针。
void ULyraPerformanceStatSubsystem::Deinitialize()
{
	GEngine->RemovePerformanceDataConsumer(Tracker);
	Tracker.Reset();
}

// 转发到性能缓存并返回指定统计项的最近值。
double ULyraPerformanceStatSubsystem::GetCachedStat(ELyraDisplayablePerformanceStat Stat) const
{
	return Tracker->GetCachedStat(Stat);
}

// 转发到性能缓存并返回指定统计项的采样历史。
const FSampledStatCache* ULyraPerformanceStatSubsystem::GetCachedStatData(const ELyraDisplayablePerformanceStat Stat) const
{
	return Tracker->GetCachedStatData(Stat);
}

