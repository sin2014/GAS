// Copyright Epic Games, Inc. All Rights Reserved.

#include "Accolades/LyraAccoladeHostWidget.h"

#include "DataRegistrySubsystem.h"
#include "LyraLogChannels.h"
#include "Messages/LyraNotificationMessage.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAccoladeHostWidget)

class UUserWidget;

// 定义本 Host 接受的 ShooterGame 嘉奖通知目标通道。
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_ShooterGame_Accolade, "Lyra.ShooterGame.Accolade");

// 指定从 Data Registry 获取嘉奖定义行时使用的注册表类型名称。
static FName NAME_AccoladeRegistryID("Accolades");

// Widget 构建后注册通知消息监听器，开始接收本地玩家的嘉奖请求。
void ULyraAccoladeHostWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	ListenerHandle = MessageSubsystem.RegisterListener(TAG_Lyra_AddNotification_Message, this, &ThisClass::OnNotificationMessage);
}

// Widget 销毁前注销消息监听并取消全部异步资源加载，避免回调访问失效 UI。
void ULyraAccoladeHostWidget::NativeDestruct()
{
	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	CancelAsyncLoading();

	Super::NativeDestruct();
}

// 接受 ShooterGame.Accolade 通知，过滤非本地玩家目标后按递增序号异步获取 PayloadTag 对应的 Registry 行。
void ULyraAccoladeHostWidget::OnNotificationMessage(FGameplayTag Channel, const FLyraNotificationMessage& Notification)
{
	if (Notification.TargetChannel == TAG_Lyra_ShooterGame_Accolade)
	{
		// 带 TargetPlayer 的通知只允许由对应本地 PlayerController 的 Host 展示。
		// Ignore notifications for other players
		if (Notification.TargetPlayer != nullptr)
		{
			APlayerController* PC = GetOwningPlayer();
			if ((PC == nullptr) || (PC->PlayerState != Notification.TargetPlayer))
			{
				return;
			}
		}

		// 按通知顺序分配序号并异步读取该嘉奖的 Data Registry 行。
		// Load the data registry row for this accolade
		const int32 NextID = AllocatedSequenceID;
		++AllocatedSequenceID;

		FDataRegistryId ItemID(NAME_AccoladeRegistryID, Notification.PayloadTag.GetTagName());
		if (!UDataRegistrySubsystem::Get()->AcquireItem(ItemID, FDataRegistryItemAcquiredCallback::CreateUObject(this, &ThisClass::OnRegistryLoadCompleted, NextID)))
		{
			UE_LOG(LogLyra, Error, TEXT("Failed to find accolade registry for tag %s, accolades will not appear"), *Notification.PayloadTag.GetTagName().ToString());
			--AllocatedSequenceID;
		}
	}
}

// Registry 行到达后异步加载软引用的声音和图标，完成时标记对应序号并尝试按原通知顺序入展示队列。
void ULyraAccoladeHostWidget::OnRegistryLoadCompleted(const FDataRegistryAcquireResult& AccoladeHandle, int32 SequenceID)
{
	if (const FLyraAccoladeDefinitionRow* AccoladeRow = AccoladeHandle.GetItem<FLyraAccoladeDefinitionRow>())
	{
		FPendingAccoladeEntry& PendingEntry = PendingAccoladeLoads.AddDefaulted_GetRef();
		PendingEntry.Row = *AccoladeRow;
		PendingEntry.SequenceID = SequenceID;

		TArray<FSoftObjectPath> AssetsToLoad;
		AssetsToLoad.Add(AccoladeRow->Sound.ToSoftObjectPath());
		AssetsToLoad.Add(AccoladeRow->Icon.ToSoftObjectPath());
		AsyncLoad(AssetsToLoad, [this, SequenceID]
		{
			FPendingAccoladeEntry* EntryThatFinishedLoading = PendingAccoladeLoads.FindByPredicate([SequenceID](const FPendingAccoladeEntry& Entry) { return Entry.SequenceID == SequenceID; });
			if (ensure(EntryThatFinishedLoading))
			{
				EntryThatFinishedLoading->Sound = EntryThatFinishedLoading->Row.Sound.Get();
				EntryThatFinishedLoading->Icon = EntryThatFinishedLoading->Row.Icon.Get();
				EntryThatFinishedLoading->bFinishedLoading = true;
				ConsiderLoadedAccolades();
			}
		});
		StartAsyncLoading();
	}
	else
	{
		ensure(false);
	}
}

// 仅从 NextDisplaySequenceID 开始连续消费已经完成资源加载的条目，消除异步完成乱序对展示顺序的影响。
void ULyraAccoladeHostWidget::ConsiderLoadedAccolades()
{
	int32 PendingIndexToDisplay;
	do
	{
		PendingIndexToDisplay = PendingAccoladeLoads.IndexOfByPredicate([DesiredID=NextDisplaySequenceID](const FPendingAccoladeEntry& Entry) { return Entry.bFinishedLoading && Entry.SequenceID == DesiredID; });
		if (PendingIndexToDisplay != INDEX_NONE)
		{
			FPendingAccoladeEntry Entry = MoveTemp(PendingAccoladeLoads[PendingIndexToDisplay]);
			PendingAccoladeLoads.RemoveAtSwap(PendingIndexToDisplay);

			ProcessLoadedAccolade(Entry);
			++NextDisplaySequenceID;
		}
	} while (PendingIndexToDisplay != INDEX_NONE);
}

// 过滤不属于本 Host 位置的条目，按 CancelAccoladesWithTag 移除互斥嘉奖，再把新条目加入串行展示队列。
void ULyraAccoladeHostWidget::ProcessLoadedAccolade(const FPendingAccoladeEntry& Entry)
{
	if (Entry.Row.LocationTag == LocationName)
	{
		bool bRecreateWidget = PendingAccoladeDisplays.Num() == 0;
		for (int32 Index = 0; Index < PendingAccoladeDisplays.Num(); )
		{
			if (PendingAccoladeDisplays[Index].Row.AccoladeTags.HasAny(Entry.Row.CancelAccoladesWithTag))
			{
				if (UUserWidget* OldWidget = PendingAccoladeDisplays[Index].AllocatedWidget)
				{
					DestroyAccoladeWidget(OldWidget);
					bRecreateWidget = true;
				}
				PendingAccoladeDisplays.RemoveAt(Index);
			}
			else
			{
				++Index;
			}
		}

		PendingAccoladeDisplays.Add(Entry);
		
		if (bRecreateWidget)
		{
			DisplayNextAccolade();
		}
	}
}

// 为展示队列首项创建蓝图 Widget，并按该行的 DisplayDuration 安排自动弹出。
void ULyraAccoladeHostWidget::DisplayNextAccolade()
{
	if (PendingAccoladeDisplays.Num() > 0)
	{
		FPendingAccoladeEntry& Entry = PendingAccoladeDisplays[0];

		GetWorld()->GetTimerManager().SetTimer(NextTimeToReconsiderHandle, this, &ThisClass::PopDisplayedAccolade, Entry.Row.DisplayDuration);
		Entry.AllocatedWidget = CreateAccoladeWidget(Entry);
	}
}

// 销毁当前首项的展示 Widget、移出队列并立即开始下一项。
void ULyraAccoladeHostWidget::PopDisplayedAccolade()
{
	if (PendingAccoladeDisplays.Num() > 0)
	{
		if (UUserWidget* OldWidget = PendingAccoladeDisplays[0].AllocatedWidget)
		{
			DestroyAccoladeWidget(OldWidget);
		}
		PendingAccoladeDisplays.RemoveAt(0);
	}

	DisplayNextAccolade();
}

