// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularCharacter.h"
#include "Components/GameFrameworkComponentManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularCharacter)

// 组件预初始化后将 Character 注册为 GameFrameworkComponent 接收者，允许游戏特性动态注入组件。
void AModularCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// BeginPlay 时广播 GameActorReady 扩展事件，再执行父类逻辑，使扩展组件获知角色已可参与游戏。
void AModularCharacter::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

// Character 结束生命周期时撤销组件接收者登记，再执行父类清理。
void AModularCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}

