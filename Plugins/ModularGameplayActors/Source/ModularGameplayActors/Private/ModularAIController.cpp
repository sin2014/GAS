// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularAIController.h"
#include "Components/GameFrameworkComponentManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAIController)

// 组件预初始化后将 AIController 注册为 GameFrameworkComponent 接收者，使游戏特性可以动态附加组件。
void AModularAIController::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// BeginPlay 时先广播 GameActorReady 扩展事件，再进入父类流程，通知已注册扩展该 Controller 可用。
void AModularAIController::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

// Actor 结束生命周期时撤销组件接收者登记，再执行父类清理，避免扩展继续引用失效 Controller。
void AModularAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}
