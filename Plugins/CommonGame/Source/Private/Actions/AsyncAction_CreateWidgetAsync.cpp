// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actions/AsyncAction_CreateWidgetAsync.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CommonUIExtensions.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "UObject/Stack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_CreateWidgetAsync)

class UUserWidget;

// 异步创建控件时生成输入暂停令牌所使用的原因前缀，便于诊断未恢复的输入过滤器。
static const FName InputFilterReason_Template = FName(TEXT("CreatingWidgetAsync"));

// 创建异步控件动作，默认在加载完成前暂停所属玩家输入。
UAsyncAction_CreateWidgetAsync::UAsyncAction_CreateWidgetAsync(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bSuspendInputUntilComplete(true)
{
}

// 校验软控件类和世界上下文，创建可供蓝图激活的异步加载动作。
UAsyncAction_CreateWidgetAsync* UAsyncAction_CreateWidgetAsync::CreateWidgetAsync(UObject* InWorldContextObject, TSoftClassPtr<UUserWidget> InUserWidgetSoftClass, APlayerController* InOwningPlayer, bool bSuspendInputUntilComplete)
{
	if (InUserWidgetSoftClass.IsNull())
	{
		FFrame::KismetExecutionMessage(TEXT("CreateWidgetAsync was passed a null UserWidgetSoftClass"), ELogVerbosity::Error);
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(InWorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	UAsyncAction_CreateWidgetAsync* Action = NewObject<UAsyncAction_CreateWidgetAsync>();
	Action->UserWidgetSoftClass = InUserWidgetSoftClass;
	Action->OwningPlayer = InOwningPlayer;
	Action->World = World;
	Action->GameInstance = World->GetGameInstance();
	Action->bSuspendInputUntilComplete = bSuspendInputUntilComplete;
	Action->RegisterWithGameInstance(World);

	return Action;
}

// 可选地暂停玩家输入、注册取消恢复回调，并请求异步加载控件类。
void UAsyncAction_CreateWidgetAsync::Activate()
{
	SuspendInputToken = bSuspendInputUntilComplete ? UCommonUIExtensions::SuspendInputForPlayer(OwningPlayer.Get(), InputFilterReason_Template) : NAME_None;

	TWeakObjectPtr<UAsyncAction_CreateWidgetAsync> LocalWeakThis(this);
	StreamingHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		UserWidgetSoftClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnWidgetLoaded),
		FStreamableManager::AsyncLoadHighPriority
	);

	// 注册取消回调；异步处理被取消时也要恢复先前暂停的玩家输入。
	// Setup a cancel delegate so that we can resume input if this handler is canceled.
	StreamingHandle->BindCancelDelegate(FStreamableDelegate::CreateWeakLambda(this,
		[this]()
		{
			UCommonUIExtensions::ResumeInputForPlayer(OwningPlayer.Get(), SuspendInputToken);
		})
	);
}

// 取消尚未完成的加载句柄，恢复输入并将异步动作标记为结束。
void UAsyncAction_CreateWidgetAsync::Cancel()
{
	Super::Cancel();

	if (StreamingHandle.IsValid())
	{
		StreamingHandle->CancelHandle();
		StreamingHandle.Reset();
	}
}

// 加载成功后创建控件并广播完成；随后恢复输入、释放句柄并结束动作。
void UAsyncAction_CreateWidgetAsync::OnWidgetLoaded()
{
	if (bSuspendInputUntilComplete)
	{
		UCommonUIExtensions::ResumeInputForPlayer(OwningPlayer.Get(), SuspendInputToken);
	}

	// 异步加载成功后才创建控件并完成动作；加载失败时不产生完成结果。
	// If the load as successful, create it, otherwise don't complete this.
	TSubclassOf<UUserWidget> UserWidgetClass = UserWidgetSoftClass.Get();
	if (UserWidgetClass)
	{
		UUserWidget* UserWidget = UWidgetBlueprintLibrary::Create(World.Get(), UserWidgetClass, OwningPlayer.Get());
		OnComplete.Broadcast(UserWidget);
	}

	StreamingHandle.Reset();

	SetReadyToDestroy();
}

