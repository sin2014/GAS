// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureAction_SplitscreenConfig.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFeatures/GameFeatureAction_WorldActionBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_SplitscreenConfig)

#define LOCTEXT_NAMESPACE "LyraGameFeatures"

//////////////////////////////////////////////////////////////////////
// UGameFeatureAction_SplitscreenConfig

TMap<FObjectKey, int32> UGameFeatureAction_SplitscreenConfig::GlobalDisableVotes;

// 仅移除当前 ChangeContext 创建的分屏覆盖请求，其他并行上下文保持不变。
void UGameFeatureAction_SplitscreenConfig::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	for (int32 i = LocalDisableVotes.Num() - 1; i >= 0; i-- )
	{
		FObjectKey ViewportKey = LocalDisableVotes[i];
		UGameViewportClient* GVP = Cast<UGameViewportClient>(ViewportKey.ResolveObjectPtr());
		const FWorldContext* WorldContext = GEngine->GetWorldContextFromGameViewport(GVP);
		if (GVP && WorldContext)
		{
			if (!Context.ShouldApplyToWorldContext(*WorldContext))
			{
				// 忽略其他状态变更上下文的数据；已失效对象仍视为属于当前上下文，以便完成清理。
				// Wrong context so ignore it, dead objects count as part of this context
				continue;
			}
		}

		int32& VoteCount = GlobalDisableVotes[ViewportKey];
		if (VoteCount <= 1)
		{
			GlobalDisableVotes.Remove(ViewportKey);

			if (GVP && WorldContext)
			{
				GVP->SetForceDisableSplitscreen(false);
			}
		}
		else
		{
			--VoteCount;
		}
		LocalDisableVotes.RemoveAt(i);
	}
}

// 对目标 GameInstance 创建分屏覆盖请求，并按 ChangeContext 保存句柄供停用撤销。
void UGameFeatureAction_SplitscreenConfig::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	if (bDisableSplitscreen)
	{
		if (UGameInstance* GameInstance = WorldContext.OwningGameInstance)
		{
			if (UGameViewportClient* VC = GameInstance->GetGameViewportClient())
			{
				FObjectKey ViewportKey(VC);

				LocalDisableVotes.Add(ViewportKey);

				int32& VoteCount = GlobalDisableVotes.FindOrAdd(ViewportKey);
				VoteCount++;
				if (VoteCount == 1)
				{
					VC->SetForceDisableSplitscreen(true);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

