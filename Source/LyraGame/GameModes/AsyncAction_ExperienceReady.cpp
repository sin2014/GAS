// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/AsyncAction_ExperienceReady.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AsyncAction_ExperienceReady)

// 构造 ExperienceReady 异步节点，WorldContext 在工厂函数中设置。
UAsyncAction_ExperienceReady::UAsyncAction_ExperienceReady(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 创建等待节点、保存 WorldContext，并注册到 GameInstance 以维持异步生命周期。
UAsyncAction_ExperienceReady* UAsyncAction_ExperienceReady::WaitForExperienceReady(UObject* InWorldContextObject)
{
	UAsyncAction_ExperienceReady* Action = nullptr;

	if (UWorld* World = GEngine->GetWorldFromContextObject(InWorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		Action = NewObject<UAsyncAction_ExperienceReady>();
		Action->WorldPtr = World;
		Action->RegisterWithGameInstance(World);
	}

	return Action;
}

// 从 World 获取 GameState；尚未设置时监听其创建事件，无 World 则立即结束节点。
void UAsyncAction_ExperienceReady::Activate()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			Step2_ListenToExperienceLoading(GameState);
		}
		else
		{
			World->GameStateSetEvent.AddUObject(this, &ThisClass::Step1_HandleGameStateSet);
		}
	}
	else
	{
		// 无有效 World 时不可能自然完成等待，直接结束异步对象生命周期。
		// No world so we'll never finish naturally
		SetReadyToDestroy();
	}
}

// GameState 可用后解除临时监听，并进入 ExperienceManager 监听阶段。
void UAsyncAction_ExperienceReady::Step1_HandleGameStateSet(AGameStateBase* GameState)
{
	if (UWorld* World = WorldPtr.Get())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}

	Step2_ListenToExperienceLoading(GameState);
}

// 查找 ExperienceManagerComponent；未加载时注册回调，已加载时仍延后一帧保持一致时序。
void UAsyncAction_ExperienceReady::Step2_ListenToExperienceLoading(AGameStateBase* GameState)
{
	check(GameState);
	ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
	check(ExperienceComponent);

	if (ExperienceComponent->IsExperienceLoaded())
	{
		UWorld* World = GameState->GetWorld();
		check(World);

		// 即使 Experience 已加载，也延后一帧广播，避免调用方意外依赖“已就绪时同步回调”的时序差异。
		//@TODO：对加载界面结束后动态创建的节点，可考虑取消这帧延迟。
		//@TODO：也可在 Experience 加载流程中注入随机 0 到 1 秒延迟来测试时序依赖。
		// The experience happened to be already loaded, but still delay a frame to
		// make sure people don't write stuff that relies on this always being true
		//@TODO: Consider not delaying for dynamically spawned stuff / any time after the loading screen has dropped?
		//@TODO: Maybe just inject a random 0-1s delay in the experience load itself?
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::Step4_BroadcastReady));
	}
	else
	{
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::Step3_HandleExperienceLoaded));
	}
}

// Experience 加载完成回调转入最终广播阶段，不直接暴露内部资产参数。
void UAsyncAction_ExperienceReady::Step3_HandleExperienceLoaded(const ULyraExperienceDefinition* CurrentExperience)
{
	Step4_BroadcastReady();
}

// 广播 OnReady，并将异步节点标记为可销毁，确保只完成一次。
void UAsyncAction_ExperienceReady::Step4_BroadcastReady()
{
	OnReady.Broadcast();

	SetReadyToDestroy();
}

