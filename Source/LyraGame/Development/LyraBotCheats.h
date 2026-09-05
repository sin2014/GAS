// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "LyraBotCheats.generated.h"

class ULyraBotCreationComponent;
class UObject;
struct FFrame;

/** 在权威端增减玩家 Bot 的开发调试命令。 */
/** Cheats related to bots */
UCLASS(NotBlueprintable)
class ULyraBotCheats final : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	ULyraBotCheats();

	// 请求 BotCreationComponent 添加一个玩家 Bot。
	// Adds a bot player
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void AddPlayerBot();

	// 随机移除一个现有玩家 Bot。
	// Removes a random bot player
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void RemovePlayerBot();

private:
	ULyraBotCreationComponent* GetBotComponent() const;
};
