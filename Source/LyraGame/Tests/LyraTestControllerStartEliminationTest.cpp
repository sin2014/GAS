// Copyright Epic Games, Inc.All Rights Reserved.

#include "Tests/LyraTestControllerStartEliminationTest.h"

#include "GameModes/LyraExperienceDefinition.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "System/LyraGameInstance.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTestControllerStartEliminationTest)

// 当 GameState 的 Experience 已加载且资产名称包含 Elimination 时判定启动测试完成，否则继续等待。
bool ULyraTestControllerStartEliminationTest::IsBootProcessComplete() const
{

	if (const UWorld* World = GetWorld(); World != nullptr && World->IsGameWorld())
	{
		AGameStateBase* GameState = World->GetGameState();
		if (GameState != nullptr)
		{
			ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
			if (ExperienceComponent != nullptr && ExperienceComponent->IsExperienceLoaded())
			{
				return ExperienceComponent->GetCurrentExperienceChecked()->GetName().Contains("Elimination");
			}
		}
	}

	return false;
}
