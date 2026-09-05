// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "LyraAbilityTagRelationshipMapping.generated.h"

class UObject;

/** 定义某个技能标签与其他技能标签之间的阻塞、取消及激活条件关系。 */
/** Struct that defines the relationship between different ability tags */
USTRUCT()
struct FLyraAbilityTagRelationship
{
	GENERATED_BODY()

	/** 本条关系的主技能标签；一条关系只配置一个标签，但一个技能可同时命中多条关系。 */
	/** The tag that this container relationship is about. Single tag, but abilities can have multiple of these */
	UPROPERTY(EditAnywhere, Category = Ability, meta = (Categories = "Gameplay.Action"))
	FGameplayTag AbilityTag;

	/** 带有 AbilityTag 的技能运行时，将阻止带有这些标签的其他技能激活。 */
	/** The other ability tags that will be blocked by any ability using this tag */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer AbilityTagsToBlock;

	/** 带有 AbilityTag 的技能开始运行时，将取消带有这些标签的其他技能。 */
	/** The other ability tags that will be canceled by any ability using this tag */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer AbilityTagsToCancel;

	/** 技能带有 AbilityTag 时，隐式追加到该技能 ActivationRequiredTags 的标签。 */
	/** If an ability has the tag, this is implicitly added to the activation required tags of the ability */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer ActivationRequiredTags;

	/** 技能带有 AbilityTag 时，隐式追加到该技能 ActivationBlockedTags 的标签。 */
	/** If an ability has the tag, this is implicitly added to the activation blocked tags of the ability */
	UPROPERTY(EditAnywhere, Category = Ability)
	FGameplayTagContainer ActivationBlockedTags;
};


/** 以数据资产形式集中配置技能标签之间的阻塞、取消和激活条件关系。 */
/** Mapping of how ability tags block or cancel other abilities */
UCLASS()
class ULyraAbilityTagRelationshipMapping : public UDataAsset
{
	GENERATED_BODY()

private:
	/** 技能 GameplayTag 之间的全部关系配置，包括阻塞、取消及附加激活条件。 */
	/** The list of relationships between different gameplay tags (which ones block or cancel others) */
	UPROPERTY(EditAnywhere, Category = Ability, meta=(TitleProperty="AbilityTag"))
	TArray<FLyraAbilityTagRelationship> AbilityTagRelationships;

public:
	/** 根据一组技能标签合并所有匹配关系，输出应阻塞和应取消的技能标签。 */
	/** Given a set of ability tags, parse the tag relationship and fill out tags to block and cancel */
	void GetAbilityTagsToBlockAndCancel(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer* OutTagsToBlock, FGameplayTagContainer* OutTagsToCancel) const;

	/** 根据一组技能标签，向输出容器追加隐含的激活所需标签与激活阻塞标签。 */
	/** Given a set of ability tags, add additional required and blocking tags */
	void GetRequiredAndBlockedActivationTags(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer* OutActivationRequired, FGameplayTagContainer* OutActivationBlocked) const;

	/** 若 ActionTag 对应关系会取消带有 AbilityTags 的技能，则返回 true。 */
	/** Returns true if the specified ability tags are canceled by the passed in action tag */
	bool IsAbilityCancelledByTag(const FGameplayTagContainer& AbilityTags, const FGameplayTag& ActionTag) const;
};
