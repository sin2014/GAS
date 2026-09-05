// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraExperienceManager.h"
#include "GameModes/LyraExperienceManager.h"
#include "Engine/Engine.h"
#include "Subsystems/SubsystemCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraExperienceManager)

#if WITH_EDITOR

// 新 PIE 运行开始时清空跨 Experience 的插件请求引用计数。
void ULyraExperienceManager::OnPlayInEditorBegun()
{
	ensure(GameFeaturePluginRequestCountMap.IsEmpty());
	GameFeaturePluginRequestCountMap.Empty();
}

// 增加指定 GameFeature 插件的活动请求计数，允许多 PIE Experience 共享激活状态。
void ULyraExperienceManager::NotifyOfPluginActivation(const FString PluginURL)
{
	if (GIsEditor)
	{
		ULyraExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<ULyraExperienceManager>();
		check(ExperienceManagerSubsystem);

		// 记录请求激活该插件的 Experience 数量；GameFeaturesSubsystem 支持并发重复加载/激活请求。
		// Track the number of requesters who activate this plugin. Multiple load/activation requests are always allowed because concurrent requests are handled.
		int32& Count = ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.FindOrAdd(PluginURL);
		++Count;
	}
}

// 减少插件请求计数，仅最后一个请求者释放时返回 true 允许实际停用。
bool ULyraExperienceManager::RequestToDeactivatePlugin(const FString PluginURL)
{
	if (GIsEditor)
	{
		ULyraExperienceManager* ExperienceManagerSubsystem = GEngine->GetEngineSubsystem<ULyraExperienceManager>();
		check(ExperienceManagerSubsystem);

		// 只有最后一个释放该插件的请求者才真正允许执行停用。
		// Only let the last requester to get this far deactivate the plugin
		int32& Count = ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.FindChecked(PluginURL);
		--Count;

		if (Count == 0)
		{
			ExperienceManagerSubsystem->GameFeaturePluginRequestCountMap.Remove(PluginURL);
			return true;
		}

		return false;
	}

	return true;
}

#endif
