// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/AsyncAction_PushContentToLayerForPlayer.h"

#include "Engine/Engine.h"
#include "PrimaryGameLayout.h"
#include "UObject/Stack.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_PushContentToLayerForPlayer)

// 创建异步压栈动作，默认在目标控件加载并入栈前暂停玩家输入。
UAsyncAction_PushContentToLayerForPlayer::UAsyncAction_PushContentToLayerForPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 校验软控件类和目标玩家，创建将内容异步压入指定 UI 层的蓝图动作。
UAsyncAction_PushContentToLayerForPlayer* UAsyncAction_PushContentToLayerForPlayer::PushContentToLayerForPlayer(APlayerController* InOwningPlayer, TSoftClassPtr<UCommonActivatableWidget> InWidgetClass, FGameplayTag InLayerName, bool bSuspendInputUntilComplete)
{
	if (InWidgetClass.IsNull())
	{
		FFrame::KismetExecutionMessage(TEXT("PushContentToLayerForPlayer was passed a null WidgetClass"), ELogVerbosity::Error);
		return nullptr;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(InOwningPlayer, EGetWorldErrorMode::LogAndReturnNull))
	{
		UAsyncAction_PushContentToLayerForPlayer* Action = NewObject<UAsyncAction_PushContentToLayerForPlayer>();
		Action->WidgetClass = InWidgetClass;
		Action->OwningPlayerPtr = InOwningPlayer;
		Action->LayerName = InLayerName;
		Action->bSuspendInputUntilComplete = bSuspendInputUntilComplete;
		Action->RegisterWithGameInstance(World);

		return Action;
	}

	return nullptr;
}

// 取消压栈请求、恢复玩家输入并结束异步动作。
void UAsyncAction_PushContentToLayerForPlayer::Cancel()
{
	Super::Cancel();

	if (StreamingHandle.IsValid())
	{
		StreamingHandle->CancelHandle();
		StreamingHandle.Reset();
	}
}

// 暂停输入后调用主布局的异步压栈接口，并绑定初始化和收尾回调。
void UAsyncAction_PushContentToLayerForPlayer::Activate()
{
	if (UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayout(OwningPlayerPtr.Get()))
	{
		TWeakObjectPtr<UAsyncAction_PushContentToLayerForPlayer> WeakThis = this;
		StreamingHandle = RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(LayerName, bSuspendInputUntilComplete, WidgetClass, [this, WeakThis](EAsyncWidgetLayerState State, UCommonActivatableWidget* Widget) {
			if (WeakThis.IsValid())
			{
				switch (State)
				{
					case EAsyncWidgetLayerState::Initialize:
						BeforePush.Broadcast(Widget);
						break;
					case EAsyncWidgetLayerState::AfterPush:
						AfterPush.Broadcast(Widget);
						SetReadyToDestroy();
						break;
					case EAsyncWidgetLayerState::Canceled:
						SetReadyToDestroy();
						break;
				}
			}
			SetReadyToDestroy();
		});
	}
	else
	{
		SetReadyToDestroy();
	}
}

