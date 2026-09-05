// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestsActorTestHelper.h"

#include "Character/LyraCharacter.h"
#include "Components/SkeletalMeshComponent.h"

// 验证 Pawn 为 LyraCharacter，并缓存其 Mesh、ASC 以及表示生成过程的 GameplayCue.Character.Spawn 标签。
FShooterTestsActorTestHelper::FShooterTestsActorTestHelper(APawn* Pawn)
{
	checkf(Pawn, TEXT("Pawn is invalid."));

	LyraCharacter = Cast<ALyraCharacter>(Pawn);
	checkf(LyraCharacter, TEXT("Cannot cast Pawn to a Lyra Character."));

	UActorComponent* ActorComponent = LyraCharacter->GetComponentByClass(USkeletalMeshComponent::StaticClass());
	checkf(ActorComponent, TEXT("Cannot find SkeletalMeshComponent from the LyraCharacter."));

	SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ActorComponent);
	checkf(SkeletalMeshComponent, TEXT("Cannot cast component to SkeletalMeshComponent."));
	
	AbilitySystemComponent = LyraCharacter->GetLyraAbilitySystemComponent();
	checkf(AbilitySystemComponent, TEXT("Lyra Character does not have a valid AbilitySystemComponent."));

	const FName CharacterSpawn = TEXT("GameplayCue.Character.Spawn");
	GameplayCueCharacterSpawnTag = FGameplayTag::RequestGameplayTag(CharacterSpawn, true);
}

// 当 ASC 不再报告 Spawn GameplayCue 激活时返回 true，表示生成阶段结束且输入不再被阻塞。
bool FShooterTestsActorTestHelper::IsPawnFullySpawned()
{
	bool bIsCurrentlySpawning = AbilitySystemComponent->IsGameplayCueActive(GameplayCueCharacterSpawnTag);
	return !bIsCurrentlySpawning;
}

// 在基础 Actor 状态缓存之上，为同一 Pawn 创建 Enhanced Input 测试动作代理。
FShooterTestsActorInputTestHelper::FShooterTestsActorInputTestHelper(APawn* Pawn) : FShooterTestsActorTestHelper(Pawn)
{
	PawnActions = MakeUnique<FShooterTestsPawnTestActions>(Pawn);
}

// 将测试枚举分派到具体 Lyra 输入动作；未知值通过 checkNoEntry 暴露测试配置错误。
void FShooterTestsActorInputTestHelper::PerformInput(InputActionType Type)
{
	switch (Type)
	{
		case InputActionType::Crouch:
			PawnActions->ToggleCrouch();
			break;
		case InputActionType::Melee:
			PawnActions->PerformMelee();
			break;
		case InputActionType::Jump:
			PawnActions->PerformJump();
			break;
		case InputActionType::MoveForward:
			PawnActions->MoveForward();
			break;
		case InputActionType::MoveBackward:
			PawnActions->MoveBackward();
			break;
		case InputActionType::StrafeLeft:
			PawnActions->StrafeLeft();
			break;
		case InputActionType::StrafeRight:
			PawnActions->StrafeRight();
			break;
		default:
			checkNoEntry();
	}
}

// 停止测试代理当前持续注入的全部输入动作。
void FShooterTestsActorInputTestHelper::StopAllInput()
{
	PawnActions->StopAllActions();
}
