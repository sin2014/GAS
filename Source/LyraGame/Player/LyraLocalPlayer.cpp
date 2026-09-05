// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/LyraLocalPlayer.h"

#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Settings/LyraSettingsLocal.h"
#include "Settings/LyraSettingsShared.h"
#include "CommonUserSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraLocalPlayer)

class UObject;

// 构造 LocalPlayer，并将共享设置 NetId 和 Controller 团队绑定初始化为空。
ULyraLocalPlayer::ULyraLocalPlayer()
{
}

// 对象初始化后绑定本地设置的音频输出设备变化委托。
void ULyraLocalPlayer::PostInitProperties()
{
	Super::PostInitProperties();

	if (ULyraSettingsLocal* LocalSettings = GetLocalSettings())
	{
		LocalSettings->OnAudioOutputDeviceChanged.AddUObject(this, &ULyraLocalPlayer::OnAudioOutputDeviceChanged);
	}
}

// 切换 PlayerController 后重新绑定团队来源，并保留 UPlayer 的标准切换流程。
void ULyraLocalPlayer::SwitchController(class APlayerController* PC)
{
	Super::SwitchController(PC);

	OnPlayerControllerChanged(PlayerController);
}

// 生成本地玩家 Actor 后绑定新的 PlayerController；生成失败时保留 OutError 并返回 false。
bool ULyraLocalPlayer::SpawnPlayActor(const FString& URL, FString& OutError, UWorld* InWorld)
{
	const bool bResult = Super::SpawnPlayActor(URL, OutError, InWorld);

	OnPlayerControllerChanged(PlayerController);

	return bResult;
}

// 初始化在线会话后启动共享用户设置加载。
void ULyraLocalPlayer::InitOnlineSession()
{
	OnPlayerControllerChanged(PlayerController);

	Super::InitOnlineSession();
}

// 解除旧 Controller 队伍监听，绑定新 Controller，并把 TeamId 变化转发为 LocalPlayer 事件。
void ULyraLocalPlayer::OnPlayerControllerChanged(APlayerController* NewController)
{
	// 解除旧 PlayerController 的团队变化监听，并保存旧 TeamId。
	// Stop listening for changes from the old controller
	FGenericTeamId OldTeamID = FGenericTeamId::NoTeam;
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(LastBoundPC.Get()))
	{
		OldTeamID = ControllerAsTeamProvider->GetGenericTeamId();
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().RemoveAll(this);
	}

	// 从新 PlayerController 镜像当前 TeamId，并监听后续变化。
	// Grab the current team ID and listen for future changes
	FGenericTeamId NewTeamID = FGenericTeamId::NoTeam;
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(NewController))
	{
		NewTeamID = ControllerAsTeamProvider->GetGenericTeamId();
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnControllerChangedTeam);
		LastBoundPC = NewController;
	}

	ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);
}

// LocalPlayer 不拥有 TeamId，忽略直接设置请求；其队伍始终镜像关联 Controller。
void ULyraLocalPlayer::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	// LocalPlayer 不拥有团队状态，只镜像关联 PlayerController，因此拒绝直接设置。
	// Do nothing, we merely observe the team of our associated player controller
}

// 从当前绑定的 PlayerController 读取 TeamId；没有有效队伍接口时返回 NoTeam。
FGenericTeamId ULyraLocalPlayer::GetGenericTeamId() const
{
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(PlayerController))
	{
		return ControllerAsTeamProvider->GetGenericTeamId();
	}
	else
	{
		return FGenericTeamId::NoTeam;
	}
}

// 返回 LocalPlayer 用于转发 Controller 队伍变化的多播委托。
FOnLyraTeamIndexChangedDelegate* ULyraLocalPlayer::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

// 返回进程级默认 LyraSettingsLocal 对象，该对象在启动后始终有效。
ULyraSettingsLocal* ULyraLocalPlayer::GetLocalSettings() const
{
	return ULyraSettingsLocal::Get();
}

// 返回当前缓存的用户共享设置；尚未加载时按平台条件读取磁盘或创建临时设置。
ULyraSettingsShared* ULyraLocalPlayer::GetSharedSettings() const
{
	if (!SharedSettings)
	{
		// 桌面平台此处只访问本地磁盘，可在登录前同步加载设置。
		// 后续可改用平台标签更准确地判断 SaveGame 支持能力。
		// On PC it's okay to use the sync load because it only checks the disk
		// This could use a platform tag to check for proper save support instead
		bool bCanLoadBeforeLogin = PLATFORM_DESKTOP;
		
		if (bCanLoadBeforeLogin)
		{
			SharedSettings = ULyraSettingsShared::LoadOrCreateSettings(this);
		}
		else
		{
			// 真实共享设置依赖用户登录，登录前先返回临时设置对象。
			// We need to wait for user login to get the real settings so return temp ones
			SharedSettings = ULyraSettingsShared::CreateTemporarySettings(this);
		}
	}

	return SharedSettings;
}

// 按 NetId 异步加载或创建共享设置，除非强制刷新否则避免重复加载。
void ULyraLocalPlayer::LoadSharedSettingsFromDisk(bool bForceLoad)
{
	FUniqueNetIdRepl CurrentNetId = GetCachedUniqueNetId();
	if (!bForceLoad && SharedSettings && CurrentNetId == NetIdForSharedSettings)
	{
		// 已成功加载过共享设置且未强制刷新时，不重复读取磁盘。
		// Already loaded once, don't reload
		return;
	}

	ensure(ULyraSettingsShared::AsyncLoadOrCreateSettings(this, ULyraSettingsShared::FOnSettingsLoadedEvent::CreateUObject(this, &ULyraLocalPlayer::OnSharedSettingsLoaded)));
}

// 用正式加载或新建的设置替换临时缓存，并记录对应 NetId。
void ULyraLocalPlayer::OnSharedSettingsLoaded(ULyraSettingsShared* LoadedOrCreatedSettings)
{
	// LoadedOrCreatedSettings 在进入该回调前已经完成应用。
	// The settings are applied before it gets here
	if (ensure(LoadedOrCreatedSettings))
	{
		// 用正式设置替换临时或旧对象；旧对象失去引用后由 GC 正常回收。
		// This will replace the temporary or previously loaded object which will GC out normally
		SharedSettings = LoadedOrCreatedSettings;

		NetIdForSharedSettings = GetCachedUniqueNetId();
	}
}

// 请求 AudioMixer 异步切换到新的输出设备 ID。
void ULyraLocalPlayer::OnAudioOutputDeviceChanged(const FString& InAudioOutputDeviceId)
{
	FOnCompletedDeviceSwap DevicesSwappedCallback;
	DevicesSwappedCallback.BindUFunction(this, FName("OnCompletedAudioDeviceSwap"));
	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(GetWorld(), InAudioOutputDeviceId, DevicesSwappedCallback);
}

// 音频设备切换完成后记录成功或失败结果。
void ULyraLocalPlayer::OnCompletedAudioDeviceSwap(const FSwapAudioOutputResult& SwapResult)
{
	if (SwapResult.Result == ESwapAudioOutputDeviceResultState::Failure)
	{
	}
}

// 确认通知来自当前 Controller 后，把 TeamId 变化转发给 LocalPlayer 监听者。
void ULyraLocalPlayer::OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
}

