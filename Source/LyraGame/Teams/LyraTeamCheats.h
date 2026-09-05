// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "LyraTeamCheats.generated.h"

class UObject;
struct FFrame;

/** 用于查看和修改玩家队伍归属的作弊命令扩展。 */
/** Cheats related to teams */
UCLASS()
class ULyraTeamCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	// 将当前玩家切换到下一个已注册队伍；到达末尾后循环到第一个队伍。
	// Moves this player to the next available team, wrapping around to the
	// first team if at the end of the list of teams
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void CycleTeam();

	// 在权威端将当前玩家切换到指定 TeamId。
	// Moves this player to the specified team
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void SetTeam(int32 TeamID);

	// 输出当前 TeamSubsystem 中所有已注册队伍。
	// Prints a list of all of the teams
	UFUNCTION(Exec)
	virtual void ListTeams();
};
