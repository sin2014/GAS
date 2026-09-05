// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestsAnimationTestHelper.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UObjectIterator.h"

namespace
{

// 在一组活动动画 Tick Record 中检查预期资产是否正作为 SourceAsset 播放。
bool IsExpectedAnimationPlaying(const UAnimationAsset* ExpectedAnimation, const TArray<FAnimTickRecord>& Records)
{
	for (int32 PlayerIndex = 0; PlayerIndex < Records.Num(); ++PlayerIndex)
	{
		const FAnimTickRecord& TickRecord = Records[PlayerIndex];
		if (TickRecord.SourceAsset == ExpectedAnimation)
		{
			return true;
		}
	}

	return false;
}

} //anonymous

// 遍历已加载动画资产，返回名称匹配且使用被测 Mesh 同一 Skeleton 的资产；找不到时返回空。
UAnimationAsset* FShooterTestsAnimationTestHelper::FindAnimationAsset(USkeletalMeshComponent* SkeletalMeshComponent, const FString& AnimationName)
{
	check(SkeletalMeshComponent);

	if (const USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset()) 
	{
		if (const USkeleton* Skeleton = SkeletalMesh->GetSkeleton()) 
		{
			for (TObjectIterator<UAnimationAsset> Itr; Itr; ++Itr) 
			{
				UAnimationAsset* AnimationAsset = (*Itr);
				if (!IsValid(AnimationAsset)) 
				{
					continue;
				}

				if (Skeleton == AnimationAsset->GetSkeleton() && AnimationAsset->GetName().Equals(AnimationName)) 
				{
					return AnimationAsset;
				}
			}
		}
	}

	return nullptr;
}

// 检查组件全部 AnimInstance：先查活动 Montage，再查同步组和非分组播放器；任一实例命中即返回 true。
bool FShooterTestsAnimationTestHelper::IsAnimationPlaying(USkeletalMeshComponent* SkeletalMeshComponent, const UAnimationAsset* ExpectedAnimation)
{
	check(SkeletalMeshComponent);
	
	bool bIsAnimationPlaying = false;
	SkeletalMeshComponent->ForEachAnimInstance([&ExpectedAnimation, &bIsAnimationPlaying](UAnimInstance* AnimInstance)
		{
			// 先前 AnimInstance 已找到预期动画时，不再继续扫描后续实例。
			// Early out if we found our animation from a prior AnimInstance
			if (bIsAnimationPlaying)
			{
				return;
			}

			FAnimMontageInstance* AnimMontageInstance = AnimInstance->GetActiveMontageInstance();
			if (AnimMontageInstance && AnimMontageInstance->IsPlaying() && IsValid(AnimMontageInstance->Montage))
			{
				bIsAnimationPlaying = AnimMontageInstance->Montage == ExpectedAnimation;
			}
			else
			{
				const FAnimInstanceProxy::FSyncGroupMap& SyncGroupMap = AnimInstance->GetSyncGroupMapRead();
				const TArray<FAnimTickRecord>& UngroupedActivePlayers = AnimInstance->GetUngroupedActivePlayersRead();
				for (const auto& SyncGroupPair : SyncGroupMap)
				{
					const FAnimGroupInstance& SyncGroup = SyncGroupPair.Value;
					if (SyncGroup.ActivePlayers.Num() > 0)
					{
						if (SyncGroup.GroupLeaderIndex != -1)
						{
							bIsAnimationPlaying = IsExpectedAnimationPlaying(ExpectedAnimation, SyncGroup.ActivePlayers);
							return;
						}
					}
				}

				bIsAnimationPlaying = IsExpectedAnimationPlaying(ExpectedAnimation, UngroupedActivePlayers);
			}
		});

	return bIsAnimationPlaying;
}
