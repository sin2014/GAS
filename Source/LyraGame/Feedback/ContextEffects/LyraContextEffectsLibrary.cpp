// Copyright Epic Games, Inc. All Rights Reserved.

#include "Feedback/ContextEffects/LyraContextEffectsLibrary.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraContextEffectsLibrary)


// 仅在 Loaded 状态下精确匹配 EffectTag，并汇总 Context 满足全部要求的 Sound 与 NiagaraSystem。
void ULyraContextEffectsLibrary::GetEffects(const FGameplayTag Effect, const FGameplayTagContainer Context, 
	TArray<USoundBase*>& Sounds, TArray<UNiagaraSystem*>& NiagaraSystems)
{
	// 只有动作标签、Context 容器有效且资源库已加载完成时才执行匹配。
	// Make sure Effect is valid and Library is loaded
	if (Effect.IsValid() && Context.IsValid() && EffectsLoadState == EContextEffectsLibraryLoadState::Loaded)
	{
		// 遍历加载后的运行时上下文效果条目。
		// Loop through Context Effects
		for (const auto& ActiveContextEffect : ActiveContextEffects)
		{
			// EffectTag 必须精确相等；传入 Context 必须包含配置要求的全部标签，并正确处理双方空容器情况。
			// Make sure the Effect is an exact Tag Match and ensure the Context has all tags in the Effect (and neither or both are empty)
			if (Effect.MatchesTagExact(ActiveContextEffect->EffectTag)
				&& Context.HasAllExact(ActiveContextEffect->Context)
				&& (ActiveContextEffect->Context.IsEmpty() == Context.IsEmpty()))
			{
				// 合并所有匹配条目的 Sound 与 NiagaraSystem，允许同一动作同时产生多组效果。
				// Get all Matching Sounds and Niagara Systems
				Sounds.Append(ActiveContextEffect->Sounds);
				NiagaraSystems.Append(ActiveContextEffect->NiagaraSystems);
			}
		}
	}
}

// 非 Loading 状态下清空旧运行时条目、切换为 Loading，并启动内部资源解析。
void ULyraContextEffectsLibrary::LoadEffects()
{
	// 非 Loading 状态下重新构建运行时资源；已 Loaded 时调用也会清空并重载。
	// Load Effects into Library if not currently loading
	if (EffectsLoadState != EContextEffectsLibraryLoadState::Loading)
	{
		// 先标记为 Loading，防止加载过程中重复进入。
		// Set load state to loading
		EffectsLoadState = EContextEffectsLibraryLoadState::Loading;

		// 清除旧的运行时条目，避免重载后保留失效资源。
		// Clear out any old Active Effects
		ActiveContextEffects.Empty();

		// 进入内部资源解析与加载流程。
		// Call internal loading function
		LoadEffectsInternal();
	}
}

// 返回 Library 当前 Unloaded、Loading 或 Loaded 状态，供调用方决定查询还是触发加载。
EContextEffectsLibraryLoadState ULyraContextEffectsLibrary::GetContextEffectsLibraryLoadState()
{
	// 返回当前资源库加载状态。
	// Return current Load State
	return EffectsLoadState;
}

// 当前同步加载配置中的软资源，按类型构建 ULyraActiveContextEffects，并在全部完成后提交结果。
void ULyraContextEffectsLibrary::LoadEffectsInternal()
{
	// TODO：将当前同步加载流程改为真正的异步资源加载。
	// TODO Add Async Loading for Libraries

	// 复制配置数据，为后续异步化时避免直接依赖可变资产数组。
	// Copy data for async load
	TArray<FLyraContextEffects> LocalContextEffects = ContextEffects;

	// 构建本次加载完成后要一次性提交的运行时条目数组。
	// Prepare Active Context Effects Array
	TArray<ULyraActiveContextEffects*> ActiveContextEffectsArray;

	// 遍历每条 Context Effects 配置并解析其软资源引用。
	// Loop through Context Effects
	for (const FLyraContextEffects& ContextEffect : LocalContextEffects)
	{
		// 仅加载动作标签与 Context 均有效的配置。
		// Make sure Tags are Valid
		if (ContextEffect.EffectTag.IsValid() && ContextEffect.Context.IsValid())
		{
			// 为该配置创建保存强引用的运行时条目。
			// Create new Active Context Effect
			ULyraActiveContextEffects* NewActiveContextEffects = NewObject<ULyraActiveContextEffects>(this);

			// 复制后续查询所需的动作标签和 Context 条件。
			// Pass relevant tag data
			NewActiveContextEffects->EffectTag = ContextEffect.EffectTag;
			NewActiveContextEffects->Context = ContextEffect.Context;

			// 同步加载每个软引用，并按 SoundBase 或 NiagaraSystem 分类保存。
			// Try to load and add Effects to New Active Context Effects
			for (const FSoftObjectPath& Effect : ContextEffect.Effects)
			{
				if (UObject* Object = Effect.TryLoad())
				{
					if (Object->IsA(USoundBase::StaticClass()))
					{
						if (USoundBase* SoundBase = Cast<USoundBase>(Object))
						{
							NewActiveContextEffects->Sounds.Add(SoundBase);
						}
					}
					else if (Object->IsA(UNiagaraSystem::StaticClass()))
					{
						if (UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(Object))
						{
							NewActiveContextEffects->NiagaraSystems.Add(NiagaraSystem);
						}
					}
				}
			}

			// 将完成解析的条目加入本次加载结果。
			// Add New Active Context to the Active Context Effects Array
			ActiveContextEffectsArray.Add(NewActiveContextEffects);
		}
	}

	// TODO：异步化后应在全部资源请求完成的回调中调用加载完成函数。
	// 当前同步流程在循环结束后立即标记加载完成。
	// TODO Call Load Complete after Async Load
	// Mark loading complete
	this->LyraContextEffectLibraryLoadingComplete(ActiveContextEffectsArray);
}

// 接管加载结果强引用并将状态切换为 Loaded，使后续 GetEffects 可以查询。
void ULyraContextEffectsLibrary::LyraContextEffectLibraryLoadingComplete(
	TArray<ULyraActiveContextEffects*> LyraActiveContextEffects)
{
	// 将资源库状态切换为 Loaded，允许 GetEffects 开始查询。
	// Flag data as loaded
	EffectsLoadState = EContextEffectsLibraryLoadState::Loaded;

	// 保存加载完成的运行时条目强引用，防止其中资源在查询期间被回收。
	// Append incoming Context Effects Array to current list of Active Context Effects
	ActiveContextEffects.Append(LyraActiveContextEffects);
}

