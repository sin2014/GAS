// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCosmeticAnimationTypes.h"

#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCosmeticAnimationTypes)

// 按顺序返回第一个 RequiredTags 全部匹配的动画层；无规则匹配时返回 DefaultLayer。
TSubclassOf<UAnimInstance> FLyraAnimLayerSelectionSet::SelectBestLayer(const FGameplayTagContainer& CosmeticTags) const
{
	for (const FLyraAnimLayerSelectionEntry& Rule : LayerRules)
	{
		if ((Rule.Layer != nullptr) && CosmeticTags.HasAll(Rule.RequiredTags))
		{
			return Rule.Layer;
		}
	}

	return DefaultLayer;
}

// 按顺序返回第一个 RequiredTags 全部匹配的 SkeletalMesh；无规则匹配时返回 DefaultMesh。
USkeletalMesh* FLyraAnimBodyStyleSelectionSet::SelectBestBodyStyle(const FGameplayTagContainer& CosmeticTags) const
{
	for (const FLyraAnimBodyStyleSelectionEntry& Rule : MeshRules)
	{
		if ((Rule.Mesh != nullptr) && CosmeticTags.HasAll(Rule.RequiredTags))
		{
			return Rule.Mesh;
		}
	}

	return DefaultMesh;
}

