// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularGameState.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Components/GameStateComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularGameState)

// 组件预初始化后将 GameStateBase 注册为扩展接收者，允许游戏特性注入 GameStateComponent。
void AModularGameStateBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// BeginPlay 时广播 GameActorReady，再执行父类逻辑，使模块化 GameState 扩展开始工作。
void AModularGameStateBase::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

// GameStateBase 结束时撤销扩展接收者登记，再交由父类完成清理。
void AModularGameStateBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}


// 组件预初始化后将完整 GameState 注册为扩展接收者，允许动态附加 GameStateComponent。
void AModularGameState::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// BeginPlay 时广播 GameActorReady，再进入 AGameState 的正常启动流程。
void AModularGameState::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

// GameState 结束时撤销扩展接收者登记，再执行父类清理。
void AModularGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}

// 比赛开始后将通知转发给当前挂载的全部 GameStateComponent，使模块化组件同步进入比赛状态。
void AModularGameState::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	TArray<UGameStateComponent*> ModularComponents;
	GetComponents(ModularComponents);
	for (UGameStateComponent* Component : ModularComponents)
	{
		Component->HandleMatchHasStarted();
	}
}

