// Copyright Epic Games, Inc. All Rights Reserved.

#include "Teams/AsyncAction_ObserveTeamColors.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Teams/LyraTeamAgentInterface.h"
#include "Teams/LyraTeamStatics.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ObserveTeamColors)

// 构造队伍颜色观察动作，并把最近广播 TeamId 初始化为未分配。
UAsyncAction_ObserveTeamColors::UAsyncAction_ObserveTeamColors(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 为实现队伍接口的对象创建颜色观察任务，并保留目标 UObject 供 World 与子系统查询。
UAsyncAction_ObserveTeamColors* UAsyncAction_ObserveTeamColors::ObserveTeamColors(UObject* TeamAgent)
{
	UAsyncAction_ObserveTeamColors* Action = nullptr;

	if (TeamAgent != nullptr)
	{
		Action = NewObject<UAsyncAction_ObserveTeamColors>();
		Action->TeamInterfacePtr = TWeakInterfacePtr<ILyraTeamAgentInterface>(TeamAgent);
		Action->TeamInterfaceObj = TeamAgent;
		Action->RegisterWithGameInstance(TeamAgent);
	}

	return Action;
}

// 解除目标队伍委托和当前 TeamId 的显示资产委托，并清空所有弱引用后释放任务。
void UAsyncAction_ObserveTeamColors::SetReadyToDestroy()
{
	Super::SetReadyToDestroy();

	// 取消或销毁时解除 TeamAgent 与 TeamSubsystem 上的全部观察委托。
	// If we're being canceled we need to unhook everything we might have tried listening to.
	if (ILyraTeamAgentInterface* TeamInterface = TeamInterfacePtr.Get())
	{
		TeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
	}
}

// 读取目标当前 TeamId 与 DisplayAsset，绑定队伍变化，并通过 BroadcastChange 建立显示资产监听；无法持续观察时结束任务。
void UAsyncAction_ObserveTeamColors::Activate()
{
	bool bCouldSucceed = false;
	int32 CurrentTeamIndex = INDEX_NONE;
	ULyraTeamDisplayAsset* CurrentDisplayAsset = nullptr;

	if (ILyraTeamAgentInterface* TeamInterface = TeamInterfacePtr.Get())
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(TeamInterfaceObj.Get(), EGetWorldErrorMode::LogAndReturnNull))
		{
			// 读取目标当前 TeamId 及对应显示资产。
			// Get current team info
			CurrentTeamIndex = GenericTeamIdToInteger(TeamInterface->GetGenericTeamId());
			CurrentDisplayAsset = ULyraTeamStatics::GetTeamDisplayAsset(World, CurrentTeamIndex);

			// 监听目标后续队伍归属变化。
			// Listen for team changes in the future
			TeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnWatchedAgentChangedTeam);

			bCouldSucceed = true;
		}
	}

	// 激活后立即广播当前队伍和显示资产。
	// Broadcast once so users get the current state
	BroadcastChange(CurrentTeamIndex, CurrentDisplayAsset);

	// 无法绑定目标队伍委托时不会再有后续更新，首次广播后即可销毁。
	// We weren't able to bind to a delegate so we'll never get any additional updates
	if (!bCouldSucceed)
	{
		SetReadyToDestroy();
	}
}

// 队伍切换时解绑旧队伍显示资产、广播新 TeamId 与资产，再绑定新队伍的资产变化委托。
void UAsyncAction_ObserveTeamColors::BroadcastChange(int32 NewTeam, const ULyraTeamDisplayAsset* DisplayAsset)
{
	UWorld* World = GEngine->GetWorldFromContextObject(TeamInterfaceObj.Get(), EGetWorldErrorMode::LogAndReturnNull);
	ULyraTeamSubsystem* TeamSubsystem = UWorld::GetSubsystem<ULyraTeamSubsystem>(World);

	const bool bTeamChanged = (LastBroadcastTeamId != NewTeam);

	// 队伍发生变化时，先停止监听旧队伍的显示资产更新。
	// Stop listening on the old team
	if ((TeamSubsystem != nullptr) && bTeamChanged && (LastBroadcastTeamId != INDEX_NONE))
	{
		TeamSubsystem->GetTeamDisplayAssetChangedDelegate(LastBroadcastTeamId).RemoveAll(this);
	}

	// 广播新的 TeamId 与显示资产，并记录本次已广播队伍。
	// Broadcast
	LastBroadcastTeamId = NewTeam;
	OnTeamChanged.Broadcast(NewTeam != INDEX_NONE, NewTeam, DisplayAsset);

	// 开始监听新队伍的显示资产更新。
	// Start listening on the new team
	if ((TeamSubsystem != nullptr) && bTeamChanged && (NewTeam != INDEX_NONE))
	{
		TeamSubsystem->GetTeamDisplayAssetChangedDelegate(NewTeam).AddDynamic(this, &ThisClass::OnDisplayAssetChanged);
	}
}

// 确认通知来自被观察对象后，查询新队伍 DisplayAsset 并切换广播和监听。
void UAsyncAction_ObserveTeamColors::OnWatchedAgentChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ULyraTeamDisplayAsset* DisplayAsset = ULyraTeamStatics::GetTeamDisplayAsset(TeamAgent, NewTeam);
	BroadcastChange(NewTeam, DisplayAsset);
}

// 当前队伍显示资产变化时，使用最近广播的 TeamId 再次通知蓝图监听者。
void UAsyncAction_ObserveTeamColors::OnDisplayAssetChanged(const ULyraTeamDisplayAsset* DisplayAsset)
{
	BroadcastChange(LastBroadcastTeamId, DisplayAsset);
}

