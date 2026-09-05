// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "LyraGameData.generated.h"

#define UE_API LYRAGAME_API

class UGameplayEffect;
class UObject;

/**
 * 保存全局游戏数据的只读资产，包括伤害、治疗和动态标签所使用的基础 GameplayEffect。
 */
/**
 * ULyraGameData
 *
 *	Non-mutable data asset that contains global game data.
 */
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Lyra Game Data", ShortTooltip = "Data asset containing global game data."))
class ULyraGameData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UE_API ULyraGameData();

	// 返回 AssetManager 已加载的全局 GameData。
	// Returns the loaded game data.
	static UE_API const ULyraGameData& Get();

public:

	// 应用伤害的 GameplayEffect，通过 SetByCaller 传入本次伤害幅值。
	// Gameplay effect used to apply damage.  Uses SetByCaller for the damage magnitude.
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", meta = (DisplayName = "Damage Gameplay Effect (SetByCaller)"))
	TSoftClassPtr<UGameplayEffect> DamageGameplayEffect_SetByCaller;

	// 应用治疗的 GameplayEffect，通过 SetByCaller 传入本次治疗幅值。
	// Gameplay effect used to apply healing.  Uses SetByCaller for the healing magnitude.
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", meta = (DisplayName = "Heal Gameplay Effect (SetByCaller)"))
	TSoftClassPtr<UGameplayEffect> HealGameplayEffect_SetByCaller;

	// 通过动态 GrantedTags 向 ASC 添加或移除标签的 GameplayEffect。
	// Gameplay effect used to add and remove dynamic tags.
	UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects")
	TSoftClassPtr<UGameplayEffect> DynamicTagGameplayEffect;
};

#undef UE_API
