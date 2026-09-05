// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularPawn.h"
#include "Components/GameFrameworkComponentManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPawn)

// 组件预初始化后将 Pawn 注册为 GameFrameworkComponent 接收者，允许游戏特性动态注入组件。
void AModularPawn::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// BeginPlay 时先广播 GameActorReady 扩展事件，再调用 APawn::BeginPlay 保留基础 Pawn 启动行为。
void AModularPawn::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

// Pawn 结束生命周期时撤销扩展接收者登记，再执行父类清理。
void AModularPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}
