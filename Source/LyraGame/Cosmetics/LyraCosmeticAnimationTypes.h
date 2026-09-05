// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "LyraCosmeticAnimationTypes.generated.h"

class UAnimInstance;
class UPhysicsAsset;
class USkeletalMesh;

//////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FLyraAnimLayerSelectionEntry
{
	GENERATED_BODY()

	// 规则匹配时链接到主 AnimInstance 的动画层类。
	// Layer to apply if the tag matches
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> Layer;

	// 规则要求的外观标签；全部存在时才算匹配。
	// Cosmetic tags required (all of these must be present to be considered a match)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Cosmetic"))
	FGameplayTagContainer RequiredTags;
};

USTRUCT(BlueprintType)
struct FLyraAnimLayerSelectionSet
{
	GENERATED_BODY()
		
	// 按数组顺序匹配动画层规则，使用第一个满足条件的条目。
	// List of layer rules to apply, first one that matches will be used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty=Layer))
	TArray<FLyraAnimLayerSelectionEntry> LayerRules;

	// 没有任何 LayerRules 匹配时使用的默认动画层。
	// The layer to use if none of the LayerRules matches
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> DefaultLayer;

	// 根据合并后的外观标签选择首个匹配动画层或默认层。
	// Choose the best layer given the rules
	TSubclassOf<UAnimInstance> SelectBestLayer(const FGameplayTagContainer& CosmeticTags) const;
};

//////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct FLyraAnimBodyStyleSelectionEntry
{
	GENERATED_BODY()

	// 规则匹配时作为角色主体使用的 SkeletalMesh。
	// Layer to apply if the tag matches
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

	// 规则要求的外观标签；全部存在时才算匹配。
	// Cosmetic tags required (all of these must be present to be considered a match)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Cosmetic"))
	FGameplayTagContainer RequiredTags;
};

USTRUCT(BlueprintType)
struct FLyraAnimBodyStyleSelectionSet
{
	GENERATED_BODY()
		
	// 按数组顺序匹配身体网格规则，使用第一个满足条件的条目。
	// List of layer rules to apply, first one that matches will be used
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(TitleProperty=Mesh))
	TArray<FLyraAnimBodyStyleSelectionEntry> MeshRules;

	// 没有任何 MeshRules 匹配时使用的默认 SkeletalMesh。
	// The layer to use if none of the LayerRules matches
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> DefaultMesh = nullptr;

	// 设置后强制使用该 PhysicsAsset，覆盖 SkeletalMesh 自带物理资产。
	// If set, ensures this physics asset is always used
	UPROPERTY(EditAnywhere)
	TObjectPtr<UPhysicsAsset> ForcedPhysicsAsset = nullptr;

	// 根据合并后的外观标签选择首个匹配身体网格或默认网格。
	// Choose the best body style skeletal mesh given the rules
	USkeletalMesh* SelectBestBodyStyle(const FGameplayTagContainer& CosmeticTags) const;
};
