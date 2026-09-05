// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraCharacter.h"

#include "LyraCharacterWithAbilities.generated.h"

#define UE_API LYRAGAME_API

class UAbilitySystemComponent;
class ULyraAbilitySystemComponent;
class UObject;

// ALyraCharacter 通常从控制它的 PlayerState 获取 ASC；此派生类则由角色自身持有完整 ASC。
// 适用于不依赖 PlayerState 持久化技能状态的角色，例如独立 AI 或非玩家单位。
// ALyraCharacter typically gets the ability system component from the possessing player state
// This represents a character with a self-contained ability system component.
UCLASS(MinimalAPI, Blueprintable)
class ALyraCharacterWithAbilities : public ALyraCharacter
{
	GENERATED_BODY()

public:
	UE_API ALyraCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void PostInitializeComponents() override;

	UE_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:

	// 由该角色自身持有的 ASC 子对象。
	// The ability system component sub-object used by player characters.
	UPROPERTY(VisibleAnywhere, Category = "Lyra|PlayerState")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;
	
	// 该角色使用的生命 AttributeSet。
	// Health attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class ULyraHealthSet> HealthSet;
	// 该角色使用的战斗 AttributeSet。
	// Combat attribute set used by this actor.
	UPROPERTY()
	TObjectPtr<const class ULyraCombatSet> CombatSet;
};

#undef UE_API
