// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"
#include "LyraCheatManager.generated.h"

class ULyraAbilitySystemComponent;


#ifndef USING_CHEAT_MANAGER
#define USING_CHEAT_MANAGER (1 && !UE_BUILD_SHIPPING)
#endif // #ifndef USING_CHEAT_MANAGER

DECLARE_LOG_CATEGORY_EXTERN(LogLyraCheat, Log, All);


/**
 * 项目的基础作弊管理器，提供技能、生命、相机和比赛流程调试命令。
 */
/**
 * ULyraCheatManager
 *
 *	Base cheat manager class used by this project.
 */
UCLASS(config = Game, Within = PlayerController, MinimalAPI)
class ULyraCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	ULyraCheatManager();

	virtual void InitCheatManager() override;

	// 同时向游戏控制台和日志输出作弊命令反馈文本。
	// Helper function to write text to the console and to the log.
	static void CheatOutputText(const FString& TextToOutput);

	// 将作弊命令发送到服务器，并只对所属玩家执行。
	// Runs a cheat on the server for the owning player.
	UFUNCTION(exec)
	void Cheat(const FString& Msg);

	// 将作弊命令发送到服务器，并对所有玩家执行。
	// Runs a cheat on the server for the all players.
	UFUNCTION(exec)
	void CheatAll(const FString& Msg);

	// 结束当前流程并启动下一场比赛。
	// Starts the next match
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void PlayNextGame();

	UFUNCTION(Exec)
	virtual void ToggleFixedCamera();

	UFUNCTION(Exec)
	virtual void CycleDebugCameras();

	UFUNCTION(Exec)
	virtual void CycleAbilitySystemDebug();

	// 强制取消由输入激活的技能，用于排查技能中断与取消问题。
	// Forces input activated abilities to be canceled.  Useful for tracking down ability interruption bugs. 
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void CancelActivatedAbilities();

	// 向所属玩家的 ASC 添加指定动态 GameplayTag。
	// Adds the dynamic tag to the owning player's ability system component.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void AddTagToSelf(FString TagName);

	// 从所属玩家的 ASC 移除指定动态 GameplayTag。
	// Removes the dynamic tag from the owning player's ability system component.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void RemoveTagFromSelf(FString TagName);

	// 通过 SetByCaller GameplayEffect 对所属玩家施加指定伤害。
	// Applies the specified damage amount to the owning player.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void DamageSelf(float DamageAmount);

	// 对玩家准星指向的 Actor 施加指定伤害。
	// Applies the specified damage amount to the actor that the player is looking at.
	virtual void DamageTarget(float DamageAmount) override;

	// 通过 SetByCaller GameplayEffect 为所属玩家恢复指定生命值。
	// Applies the specified amount of healing to the owning player.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void HealSelf(float HealAmount);

	// 为玩家准星指向的 Actor 恢复指定生命值。
	// Applies the specified amount of healing to the actor that the player is looking at.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void HealTarget(float HealAmount);

	// 对所属玩家施加足以致死的自毁伤害。
	// Applies enough damage to kill the owning player.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void DamageSelfDestruct();

	// 切换所属玩家的无敌状态，使其不受任何伤害。
	// Prevents the owning player from taking any damage.
	virtual void God() override;

	// 切换无限生命状态，使所属玩家生命值不会降到 1 以下。
	// Prevents the owning player from dropping below 1 health.
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	virtual void UnlimitedHealth(int32 Enabled = -1);

protected:

	virtual void EnableDebugCamera() override;
	virtual void DisableDebugCamera() override;
	bool InDebugCamera() const;

	virtual void EnableFixedCamera();
	virtual void DisableFixedCamera();
	bool InFixedCamera() const;

	void ApplySetByCallerDamage(ULyraAbilitySystemComponent* LyraASC, float DamageAmount);
	void ApplySetByCallerHeal(ULyraAbilitySystemComponent* LyraASC, float HealAmount);

	ULyraAbilitySystemComponent* GetPlayerAbilitySystemComponent() const;
};
