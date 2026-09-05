// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraBotCheats.h"
#include "Engine/World.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameModes/LyraBotCreationComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraBotCheats)

//////////////////////////////////////////////////////////////////////
// ULyraBotCheats

// 构造 Bot 作弊扩展，不持有独立 Bot 状态。
ULyraBotCheats::ULyraBotCheats()
{
#if WITH_SERVER_CODE && UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}

// 从当前 GameState 查找 BotCreationComponent，并在权威端请求创建一个玩家 Bot。
void ULyraBotCheats::AddPlayerBot()
{
#if WITH_SERVER_CODE && UE_WITH_CHEAT_MANAGER
	if (ULyraBotCreationComponent* BotComponent = GetBotComponent())
	{
		BotComponent->Cheat_AddBot();
	}
#endif	
}

// 从 BotCreationComponent 随机选择并移除一个现有玩家 Bot。
void ULyraBotCheats::RemovePlayerBot()
{
#if WITH_SERVER_CODE && UE_WITH_CHEAT_MANAGER
	if (ULyraBotCreationComponent* BotComponent = GetBotComponent())
	{
		BotComponent->Cheat_RemoveBot();
	}
#endif	
}

// 通过所属 PlayerController 的 World 和 GameState 查找 BotCreationComponent；任一对象无效时返回 nullptr。
ULyraBotCreationComponent* ULyraBotCheats::GetBotComponent() const
{
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->FindComponentByClass<ULyraBotCreationComponent>();
		}
	}

	return nullptr;
}

