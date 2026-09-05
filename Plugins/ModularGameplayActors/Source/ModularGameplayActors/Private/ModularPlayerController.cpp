// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularPlayerController.h"

#include "Components/ControllerComponent.h"
#include "Components/GameFrameworkComponentManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPlayerController)

// 组件预初始化后将 PlayerController 注册为扩展接收者，允许游戏特性附加 ControllerComponent。
void AModularPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// PlayerController 结束时撤销扩展接收者登记，再执行父类清理。
void AModularPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}

// Player 对象分配后广播 GameActorReady，并把 ReceivedPlayer 通知转发给全部模块化 ControllerComponent。
void AModularPlayerController::ReceivedPlayer()
{
	// PlayerController 必须先取得 Player 才能完成大部分初始化，因此此时才宣告 Actor 已就绪。
	// Player controllers always get assigned a player and can't do much until then
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::ReceivedPlayer();

	TArray<UControllerComponent*> ModularComponents;
	GetComponents(ModularComponents);
	for (UControllerComponent* Component : ModularComponents)
	{
		Component->ReceivedPlayer();
	}
}

// 执行父类玩家 Tick 后，将同一帧间隔转发给全部模块化 ControllerComponent。
void AModularPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	TArray<UControllerComponent*> ModularComponents;
	GetComponents(ModularComponents);
	for (UControllerComponent* Component : ModularComponents)
	{
		Component->PlayerTick(DeltaTime);
	}
}
