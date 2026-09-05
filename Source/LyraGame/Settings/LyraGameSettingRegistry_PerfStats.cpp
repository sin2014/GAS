// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomSettings/LyraSettingValueDiscrete_PerfStat.h"
#include "EditCondition/WhenPlayingAsPrimaryPlayer.h"
#include "GameSettingCollection.h"
#include "GameSettingValueDiscreteDynamic.h"
#include "HAL/IConsoleManager.h"
#include "LyraGameSettingRegistry.h"
#include "LyraSettingsLocal.h"
#include "Performance/LyraPerformanceStatTypes.h"
#include "Player/LyraLocalPlayer.h"
#include "RHI.h"

class ULyraLocalPlayer;

#define LOCTEXT_NAMESPACE "Lyra"

// 控制延迟标记是否仅允许在 NVIDIA 图形设备上启用。
static TAutoConsoleVariable<bool> CVarLatencyMarkersRequireNVIDIA(TEXT("Lyra.Settings.LatencyMarkersRequireNVIDIA"),
	true,
	TEXT("If true, then only allow latency markers to be enabled on NVIDIA hardware"),
	ECVF_Default);

// 检查当前平台是否具备采集游戏延迟、渲染延迟和总延迟等统计数据的基础能力。
// Checks if the current platform can even support latency stats (game, render, total, etc latency stats)
class FGameSettingEditCondition_LatencyStatsSupported final : public FGameSettingEditCondition
{
public:
	// 构造平台延迟统计能力编辑条件；无需额外状态。
	FGameSettingEditCondition_LatencyStatsSupported() = default;

	// 当前平台不支持延迟统计时禁用相关设置项。
	virtual void GatherEditState(const ULocalPlayer * InLocalPlayer, FGameSettingEditableState & InOutEditState) const override
	{
		if (!ULyraSettingsLocal::DoesPlatformSupportLatencyTrackingStats())
		{
			InOutEditState.Disable(LOCTEXT("PlatformDoesNotSupportLatencyStates", "Latency performance stats are not supported on this device"));
		}
	}
};

// 检查延迟统计当前是否已启用，并监听其变化以重新计算依赖设置项的可编辑状态。
// Checks if latency stats are currently enabled and listens for when that changes to correclt update the edit condition state
class FGameSettingEditCondition_LatencyStatsCurrentlyEnabled final : public FGameSettingEditCondition
{
public:
	// 构造“延迟统计已启用”编辑条件，事件监听在 Initialize 中建立。
	FGameSettingEditCondition_LatencyStatsCurrentlyEnabled() = default;

	// 销毁编辑条件时解除延迟统计设置变更监听，避免遗留委托。
	virtual ~FGameSettingEditCondition_LatencyStatsCurrentlyEnabled() override
	{
		if (!SettingChangedDelegate.IsValid())
		{
			return;
		}
		
		ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
		if (!Settings)
		{
			return;
		}
		
		Settings->OnLatencyStatIndicatorSettingsChangedEvent().Remove(SettingChangedDelegate);
	}

private:
	
	// 监听延迟统计启用状态变化，以便重新评估依赖设置项。
	virtual void Initialize(const ULocalPlayer* InLocalPlayer) override
	{
		// 监听延迟统计启用状态的变化，并在变化时通知设置系统重新计算此编辑条件。
		// Bind to an event for when the settings are updated so that we can broadcast that we need
		// to be re-evaluated
		ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
		if (!Settings)
		{
			return;
		}

		SettingChangedDelegate = Settings->OnLatencyStatIndicatorSettingsChangedEvent().AddSP(this->AsShared(), &FGameSettingEditCondition_LatencyStatsCurrentlyEnabled::BroadcastEditConditionChanged);
	}
	
	// 延迟统计未启用时禁用依赖它的设置项。
	virtual void GatherEditState(const ULocalPlayer * InLocalPlayer, FGameSettingEditableState & InOutEditState) const override
	{
		const ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
		if (!Settings)
		{
			return;
		}

		if (!Settings->GetEnableLatencyTrackingStats())
		{
			InOutEditState.Disable(LOCTEXT("LatencyMarkerRequireStatsEnabled", "Latency Tracking Stats must be enabled to use this."));
		}	
	}
	
	FDelegateHandle SettingChangedDelegate;
};

// 检查当前平台是否支持用于测量输入延迟的闪烁标记。
// Checks if latency markers are supported on the current platform
class FGameSettingEditCondition_LatencyMarkersSupported final : public FGameSettingEditCondition
{
public:
	// 构造平台延迟标记能力编辑条件；无需额外状态。
	FGameSettingEditCondition_LatencyMarkersSupported() = default;

	// 仅在平台支持延迟标记且满足可选 NVIDIA 限制时允许编辑该设置。
	virtual void GatherEditState(const ULocalPlayer * InLocalPlayer, FGameSettingEditableState & InOutEditState) const override
	{
		if (!ULyraSettingsLocal::DoesPlatformSupportLatencyMarkers())
		{
			InOutEditState.Disable(LOCTEXT("PlatformDoesNotSupportLatencyMarkers", "Latency markers are not supported on this device"));
		}

		// Lyra 仅通过 Reflex 插件采集此类延迟统计，因此将相关设置限制为 NVIDIA 设备可见。
		// Lyra is only going to use the "Reflex" plugin to track these latency stats, so restrict these settings to NVIDIA devices. 
		if (CVarLatencyMarkersRequireNVIDIA.GetValueOnAnyThread() && !IsRHIDeviceNVIDIA())
		{
			InOutEditState.Disable(LOCTEXT("InputLatencyMarkersRequiresNVIDIA", "Latency markers only work on NVIDIA devices."));
		}
	}
};

//////////////////////////////////////////////////////////////////////

// 按平台允许的统计类型构建性能统计页面，并为延迟项附加能力与依赖条件。
void ULyraGameSettingRegistry::AddPerformanceStatPage(UGameSettingCollection* PerfStatsOuterCategory, ULyraLocalPlayer* InLocalPlayer)
{
	//----------------------------------------------------------------------------------
	{
		static_assert((int32)ELyraDisplayablePerformanceStat::Count == 18, "Consider updating this function to deal with new performance stats");

		UGameSettingCollectionPage* StatsPage = NewObject<UGameSettingCollectionPage>();
		StatsPage->SetDevName(TEXT("PerfStatsPage"));
		StatsPage->SetDisplayName(LOCTEXT("PerfStatsPage_Name", "Performance Stats"));
		StatsPage->SetDescriptionRichText(LOCTEXT("PerfStatsPage_Description", "Configure the display of performance statistics."));
		StatsPage->SetNavigationText(LOCTEXT("PerfStatsPage_Navigation", "Edit"));

		StatsPage->AddEditCondition(FWhenPlayingAsPrimaryPlayer::Get());

		PerfStatsOuterCategory->AddSetting(StatsPage);

		// 本机渲染与运行性能统计项。
		// Performance stats
		////////////////////////////////////////////////////////////////////////////////////
		{
			UGameSettingCollection* StatCategory_Performance = NewObject<UGameSettingCollection>();
			StatCategory_Performance->SetDevName(TEXT("StatCategory_Performance"));
			StatCategory_Performance->SetDisplayName(LOCTEXT("StatCategory_Performance_Name", "Performance"));
			StatsPage->AddSetting(StatCategory_Performance);

			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::ClientFPS);
				Setting->SetDisplayName(LOCTEXT("PerfStat_ClientFPS", "Client FPS"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_ClientFPS", "Client frame rate (higher is better)"));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::ServerFPS);
				Setting->SetDisplayName(LOCTEXT("PerfStat_ServerFPS", "Server FPS"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_ServerFPS", "Server frame rate"));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::FrameTime);
				Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime", "Frame Time"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime", "The total frame time."));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::IdleTime);
				Setting->SetDisplayName(LOCTEXT("PerfStat_IdleTime", "Idle Time"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_IdleTime", "The amount of time spent waiting idle for frame pacing."));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::FrameTime_GameThread);
				Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_GameThread", "CPU Game Time"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_GameThread", "The amount of time spent on the main game thread."));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::FrameTime_RenderThread);
				Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_RenderThread", "CPU Render Time"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_RenderThread", "The amount of time spent on the rendering thread."));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::FrameTime_RHIThread);
				Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_RHIThread", "CPU RHI Time"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_RHIThread", "The amount of time spent on the Render Hardware Interface thread."));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::FrameTime_GPU);
				Setting->SetDisplayName(LOCTEXT("PerfStat_FrameTime_GPU", "GPU Render Time"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_FrameTime_GPU", "The amount of time spent on the GPU."));
				StatCategory_Performance->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
		}

		// 网络连接与网络模拟统计项。
		// Network stats
		////////////////////////////////////////////////////////////////////////////////////
		{
			UGameSettingCollection* StatCategory_Network = NewObject<UGameSettingCollection>();
			StatCategory_Network->SetDevName(TEXT("StatCategory_Network"));
			StatCategory_Network->SetDisplayName(LOCTEXT("StatCategory_Network_Name", "Network"));
			StatsPage->AddSetting(StatCategory_Network);

			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::Ping);
				Setting->SetDisplayName(LOCTEXT("PerfStat_Ping", "Ping"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Ping", "The roundtrip latency of your connection to the server."));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::PacketLoss_Incoming);
				Setting->SetDisplayName(LOCTEXT("PerfStat_PacketLoss_Incoming", "Incoming Packet Loss"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketLoss_Incoming", "The percentage of incoming packets lost."));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::PacketLoss_Outgoing);
				Setting->SetDisplayName(LOCTEXT("PerfStat_PacketLoss_Outgoing", "Outgoing Packet Loss"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketLoss_Outgoing", "The percentage of outgoing packets lost."));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::PacketRate_Incoming);
				Setting->SetDisplayName(LOCTEXT("PerfStat_PacketRate_Incoming", "Incoming Packet Rate"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketRate_Incoming", "Rate of incoming packets (per second)"));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::PacketRate_Outgoing);
				Setting->SetDisplayName(LOCTEXT("PerfStat_PacketRate_Outgoing", "Outgoing Packet Rate"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketRate_Outgoing", "Rate of outgoing packets (per second)"));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::PacketSize_Incoming);
				Setting->SetDisplayName(LOCTEXT("PerfStat_PacketSize_Incoming", "Incoming Packet Size"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketSize_Incoming", "The average size (in bytes) of packets recieved in the last second."));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::PacketSize_Outgoing);
				Setting->SetDisplayName(LOCTEXT("PerfStat_PacketSize_Outgoing", "Outgoing Packet Size"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_PacketSize_Outgoing", "The average size (in bytes) of packets sent in the last second."));
				StatCategory_Network->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
		}

		// Reflex 延迟跟踪相关统计项。
		// Latency stats
		////////////////////////////////////////////////////////////////////////////////////
		{
			UGameSettingCollection* StatCategory_Latency = NewObject<UGameSettingCollection>();
			StatCategory_Latency->SetDevName(TEXT("StatCategory_Latency"));
			StatCategory_Latency->SetDisplayName(LOCTEXT("StatCategory_Latency_Name", "Latency"));
			StatsPage->AddSetting(StatCategory_Latency);

			//----------------------------------------------------------------------------------
			{
				UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
				Setting->SetDevName(TEXT("InputLatencyTrackingStats"));
				Setting->SetDisplayName(LOCTEXT("InputLatencyTrackingStats_Name", "Enable Latency Tracking Stats"));
				Setting->SetDescriptionRichText(LOCTEXT("InputLatencyTrackingStats_Description", "Enabling Input Latency stat tracking"));

				Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetEnableLatencyTrackingStats));
				Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetEnableLatencyTrackingStats));

				// 平台支持延迟统计时默认启用，否则默认关闭。
				// Set the default value to true if the platform supports latency tracking stats
				Setting->SetDefaultValue(ULyraSettingsLocal::DoesPlatformSupportLatencyTrackingStats());
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
				
				StatCategory_Latency->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				UGameSettingValueDiscreteDynamic_Bool* Setting = NewObject<UGameSettingValueDiscreteDynamic_Bool>();
				Setting->SetDevName(TEXT("InputLatencyMarkers"));
				Setting->SetDisplayName(LOCTEXT("InputLatencyMarkers_Name", "Enable Latency Markers"));
				Setting->SetDescriptionRichText(LOCTEXT("InputLatencyMarkers_Description", "Enabling Input Latency Markers to flash the screen"));

				Setting->SetDynamicGetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(GetEnableLatencyFlashIndicators));
				Setting->SetDynamicSetter(GET_LOCAL_SETTINGS_FUNCTION_PATH(SetEnableLatencyFlashIndicators));
				Setting->SetDefaultValue(false);

				// 延迟闪烁标记只有在平台支持且已启用延迟统计时才允许开启。
				// Latency markers require the stats to be supported and enabled
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
				
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyMarkersSupported>());

				StatCategory_Latency->AddSetting(Setting);
			}
			
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::Latency_Total);
				Setting->SetDisplayName(LOCTEXT("PerfStat_Latency_Total", "Total Game Latency"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Latency_Total", "The total amount of latency"));
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
				StatCategory_Latency->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::Latency_Game);
				Setting->SetDisplayName(LOCTEXT("PerfStat_Latency_Game", "Game Latency"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Latency_Game", "Game simulation start to driver submission end"));
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
				StatCategory_Latency->AddSetting(Setting);
			}
			//----------------------------------------------------------------------------------
			{
				ULyraSettingValueDiscrete_PerfStat* Setting = NewObject<ULyraSettingValueDiscrete_PerfStat>();
				Setting->SetStat(ELyraDisplayablePerformanceStat::Latency_Render);
				Setting->SetDisplayName(LOCTEXT("PerfStat_Latency_Render", "Render Latency"));
				Setting->SetDescriptionRichText(LOCTEXT("PerfStatDescription_Latency_Render", "OS render queue start to GPU render end"));
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsSupported>());
				Setting->AddEditCondition(MakeShared<FGameSettingEditCondition_LatencyStatsCurrentlyEnabled>());
				StatCategory_Latency->AddSetting(Setting);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
