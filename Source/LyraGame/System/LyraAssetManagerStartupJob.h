// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/StreamableManager.h"

DECLARE_DELEGATE_OneParam(FLyraAssetManagerStartupJobSubstepProgress, float /*NewProgress*/);

/** 封装一个 AssetManager 启动任务，并从 StreamableHandle 汇报带权重的加载进度。 */
/** Handles reporting progress from streamable handles */
struct FLyraAssetManagerStartupJob
{
	FLyraAssetManagerStartupJobSubstepProgress SubstepProgressDelegate;
	TFunction<void(const FLyraAssetManagerStartupJob&, TSharedPtr<FStreamableHandle>&)> JobFunc;
	FString JobName;
	float JobWeight;
	mutable double LastUpdate = 0;

	/** 创建一个同步或可返回异步 Handle 的启动任务。 */
	/** Simple job that is all synchronous */
	FLyraAssetManagerStartupJob(const FString& InJobName, const TFunction<void(const FLyraAssetManagerStartupJob&, TSharedPtr<FStreamableHandle>&)>& InJobFunc, float InJobWeight)
		: JobFunc(InJobFunc)
		, JobName(InJobName)
		, JobWeight(InJobWeight)
	{}

	/** 执行任务；任务发起异步加载时返回对应 StreamableHandle。 */
	/** Perform actual loading, will return a handle if it created one */
	TSharedPtr<FStreamableHandle> DoJob() const;

	void UpdateSubstepProgress(float NewProgress) const
	{
		SubstepProgressDelegate.ExecuteIfBound(NewProgress);
	}

	void UpdateSubstepProgressFromStreamable(TSharedRef<FStreamableHandle> StreamableHandle) const
	{
		if (SubstepProgressDelegate.IsBound())
		{
			// StreamableHandle::GetProgress 会遍历较大的依赖图，开销较高，因此限制查询频率。
			// StreamableHandle::GetProgress traverses() a large graph and is quite expensive
			double Now = FPlatformTime::Seconds();
			if (LastUpdate - Now > 1.0 / 60)
			{
				SubstepProgressDelegate.Execute(StreamableHandle->GetProgress());
				LastUpdate = Now;
			}
		}
	}
};
