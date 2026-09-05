// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularPlayerState.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Components/PlayerStateComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularPlayerState)

// 组件预初始化后将 PlayerState 注册为扩展接收者，允许动态附加 PlayerStateComponent。
void AModularPlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// BeginPlay 时广播 GameActorReady，再执行父类流程，使 PlayerState 扩展获知数据对象已就绪。
void AModularPlayerState::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();
}

// PlayerState 结束时撤销扩展接收者登记，再交由父类清理。
void AModularPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
}

// 重置基础 PlayerState 后，把 Reset 通知转发给所有模块化 PlayerStateComponent。
void AModularPlayerState::Reset()
{
	Super::Reset();

	TArray<UPlayerStateComponent*> ModularComponents;
	GetComponents(ModularComponents);
	for (UPlayerStateComponent* Component : ModularComponents)
	{
		Component->Reset();
	}
}

// PlayerState 迁移时按组件类和名称寻找目标组件，并让每个源 PlayerStateComponent 复制自身状态。
void AModularPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	TInlineComponentArray<UPlayerStateComponent*> PlayerStateComponents;
	GetComponents(PlayerStateComponents);
	for (UPlayerStateComponent* SourcePSComp : PlayerStateComponents)
	{
		if (UPlayerStateComponent* TargetComp = Cast<UPlayerStateComponent>(static_cast<UObject*>(FindObjectWithOuter(PlayerState, SourcePSComp->GetClass(), SourcePSComp->GetFName()))))
		{
			SourcePSComp->CopyProperties(TargetComp);
		}
	}
}
