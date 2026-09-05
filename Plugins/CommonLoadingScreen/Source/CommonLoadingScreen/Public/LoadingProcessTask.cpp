// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingProcessTask.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LoadingScreenManager.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LoadingProcessTask)

// 在上下文对象对应的加载管理器下创建任务、设置阻塞原因并注册为外部加载处理器。
/*static*/ ULoadingProcessTask* ULoadingProcessTask::CreateLoadingScreenProcessTask(UObject* WorldContextObject, const FString& ShowLoadingScreenReason)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	ULoadingScreenManager* LoadingScreenManager = GameInstance ? GameInstance->GetSubsystem<ULoadingScreenManager>() : nullptr;

	if (LoadingScreenManager)
	{
		ULoadingProcessTask* NewLoadingTask = NewObject<ULoadingProcessTask>(LoadingScreenManager);
		NewLoadingTask->SetShowLoadingScreenReason(ShowLoadingScreenReason);

		LoadingScreenManager->RegisterLoadingProcessor(NewLoadingTask);
		
		return NewLoadingTask;
	}

	return nullptr;
}

// 按工厂创建时确立的 Outer 所有权约定取得加载管理器，并撤销外部处理器注册。
void ULoadingProcessTask::Unregister()
{
	ULoadingScreenManager* LoadingScreenManager = Cast<ULoadingScreenManager>(GetOuter());
	LoadingScreenManager->UnregisterLoadingProcessor(this);
}

// 更新此任务要求加载画面保持显示时提供的诊断原因。
void ULoadingProcessTask::SetShowLoadingScreenReason(const FString& InReason)
{
	Reason = InReason;
}

// 只要任务仍被注册便始终请求显示加载画面，并向管理器返回当前原因。
bool ULoadingProcessTask::ShouldShowLoadingScreen(FString& OutReason) const
{
	OutReason = Reason;
	return true;
}
