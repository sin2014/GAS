// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "ShooterTestsInputTestHelper.h"

class ALyraCharacter;
class USkeletalMeshComponent;

/// 汇总多个测试共用的 Lyra Character、Mesh、ASC 和 Spawn GameplayCue 状态。
/// Class which consolidates the Lyra Actor information that is shared amongst tests.
class FShooterTestsActorTestHelper
{
public:
	/** 从 Pawn 验证并缓存 LyraCharacter、SkeletalMeshComponent、AbilitySystemComponent 和 Spawn GameplayCueTag。 */
	/**
	* Construct the Actor Test Helper object
	* 
	* @param Pawn - Pointer to a Pawn
	*/
	explicit FShooterTestsActorTestHelper(APawn* Pawn);

	/** 当 GameplayCue.Character.Spawn 已不再激活时，认为 Pawn 完成生成并可接受测试操作。 */
	/**
	* Checks to see if the current actor is fully spawned in the level and ready to be used.
	* 
	* @return true is the player is spawned and usable, otherwise false
	*/
	bool IsPawnFullySpawned();

	/** 返回构造时由 Pawn 转换得到的 Lyra Character。 */
	/**
	* Gets the Lyra character which was associated with the Pawn used during the construction of the object.
	*
	* @return constant pointer to the LyraCharacter
	*/
	const ALyraCharacter* GetLyraCharacter() const { return LyraCharacter; }
	
	/** 返回关联 Lyra Character 的 SkeletalMeshComponent。 */
	/**
	* Gets the skeletal mesh component of the associated Lyra Character.
	*
	* @return pointer to the SkeletalMeshComponent
	*/
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }

private:
	/** 关卡中的被测 Lyra Character。 */
	/** Reference to our player in the level. */
	ALyraCharacter* LyraCharacter{ nullptr };

	/** 被测玩家的 SkeletalMeshComponent。 */
	/** Reference to the player's skeletal mesh component. */
	USkeletalMeshComponent* SkeletalMeshComponent;

	/** 被测玩家的 LyraAbilitySystemComponent。 */
	/** Reference to the player's ability system component. */
	ULyraAbilitySystemComponent* AbilitySystemComponent{ nullptr };

	/** 用于判断玩家是否仍处于生成阶段的 GameplayCueTag。 */
	/** Reference to the player's spawning gameplay effect. */
	FGameplayTag GameplayCueCharacterSpawnTag;
};

/** 在基础 Actor 状态辅助器上增加 Lyra 输入动作模拟，用于触发并验证动画。 */
/**
 * Inherited from FShooterTestsActorTestHelper, adds FShooterTestsPawnTestActions to be used for interacting with the Lyra player and triggering animations.
 *
 * @see FShooterTestsActorTestHelper
 */
class FShooterTestsActorInputTestHelper : public FShooterTestsActorTestHelper
{
public:
	/** 列出测试可模拟的玩家输入动作。 */
	/** Defines the available input actions that can be performed. */
	enum class InputActionType : uint8_t
	{
		Crouch,
		Melee,
		Jump,
		MoveForward,
		MoveBackward,
		StrafeLeft,
		StrafeRight,
	};

	explicit FShooterTestsActorInputTestHelper(APawn* Pawn);

	/** 根据枚举向 Lyra Character 注入对应按钮或移动输入；未支持的枚举会触发检查失败。 */
	/** 
	* Simulates input triggers on the Lyra character.
	* 
	* @param Type - InputActionType used to specify which input to perform.
	*/
	void PerformInput(InputActionType Type);

	/** 停止当前由测试输入代理持续执行的全部动作。 */
	/**
	* Stops all actively running inputs.
	*/
	void StopAllInput();

private:
	/** 与 Enhanced Input 系统交互并向 Pawn 注入动作的代理对象。 */
	/** Object which handles interfacing with the Enhanced Input System to perform input actions. */
	TUniquePtr<FShooterTestsPawnTestActions> PawnActions;
};
