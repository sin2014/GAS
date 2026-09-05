// Copyright Epic Games, Inc. All Rights Reserved.


#include "LyraContextEffectsSubsystem.h"

#include "Feedback/ContextEffects/LyraContextEffectsLibrary.h"
#include "Feedback/ContextEffects/LyraContextEffectsSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraContextEffectsSubsystem)

class AActor;
class UAudioComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;

// 从 SpawningActor 已登记且 Loaded 的 Library 中匹配 Effect/Contexts，附着生成音效与 Niagara 并写入输出数组。
void ULyraContextEffectsSubsystem::SpawnContextEffects(
	const AActor* SpawningActor
	, USceneComponent* AttachToComponent
	, const FName AttachPoint
	, const FVector LocationOffset
	, const FRotator RotationOffset
	, FGameplayTag Effect
	, FGameplayTagContainer Contexts
	, TArray<UAudioComponent*>& AudioOut
	, TArray<UNiagaraComponent*>& NiagaraOut
	, FVector VFXScale
	, float AudioVolume
	, float AudioPitch)
{
	// 先查找该 Actor 已登记的 Context Effects Library 集合。
	// First determine if this Actor has a matching Set of Libraries
	if (TObjectPtr<ULyraContextEffectsSet>* EffectsLibrariesSetPtr = ActiveActorEffectsMap.Find(SpawningActor))
	{
		// 验证映射中保存的运行时 Library Set 仍然有效。
		// Validate the pointers from the Map Find
		if (ULyraContextEffectsSet* EffectsLibraries = *EffectsLibrariesSetPtr)
		{
			// 汇总全部匹配 Library 返回的 Sound 与 NiagaraSystem。
			// Prepare Arrays for Sounds and Niagara Systems
			TArray<USoundBase*> TotalSounds;
			TArray<UNiagaraSystem*> TotalNiagaraSystems;

			// 逐个查询该 Actor 当前启用的效果库。
			// Cycle through Effect Libraries
			for (ULyraContextEffectsLibrary* EffectLibrary : EffectsLibraries->LyraContextEffectsLibraries)
			{
				// 只有有效且 Loaded 的 Library 才能立即提供匹配结果。
				// Check if the Effect Library is valid and data Loaded
				if (EffectLibrary && EffectLibrary->GetContextEffectsLibraryLoadState() == EContextEffectsLibraryLoadState::Loaded)
				{
					// 使用局部数组接收当前 Library 的匹配结果。
					// Set up local list of Sounds and Niagara Systems
					TArray<USoundBase*> Sounds;
					TArray<UNiagaraSystem*> NiagaraSystems;

					// 按 EffectTag 与 Contexts 从当前 Library 获取效果资源。
					// Get Sounds and Niagara Systems
					EffectLibrary->GetEffects(Effect, Contexts, Sounds, NiagaraSystems);

					// 合并到本次生成请求的总资源列表。
					// Append to accumulating array
					TotalSounds.Append(Sounds);
					TotalNiagaraSystems.Append(NiagaraSystems);
				}
				else if (EffectLibrary && EffectLibrary->GetContextEffectsLibraryLoadState() == EContextEffectsLibraryLoadState::Unloaded)
				{
					// Library 尚未就绪时触发加载；本次调用不等待加载完成，因此不会生成该库的效果。
					// Else load effects
					EffectLibrary->LoadEffects();
				}
			}

			// 将所有匹配音效附着生成到指定组件和骨骼/Socket。
			// Cycle through found Sounds
			for (USoundBase* Sound : TotalSounds)
			{
				// 生成附着音效，并把返回的 AudioComponent 写入输出数组供调用方跟踪。
				// Spawn Sounds Attached, add Audio Component to List of ACs
				UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(Sound, AttachToComponent, AttachPoint, LocationOffset, RotationOffset, EAttachLocation::KeepRelativeOffset,
					false, AudioVolume, AudioPitch, 0.0f, nullptr, nullptr, true);

				AudioOut.Add(AudioComponent);
			}

			// 将所有匹配 NiagaraSystem 附着生成到指定组件和骨骼/Socket。
			// Cycle through found Niagara Systems
			for (UNiagaraSystem* NiagaraSystem : TotalNiagaraSystems)
			{
				// 生成附着特效，并把返回的 NiagaraComponent 写入输出数组供调用方跟踪。
				// Spawn Niagara Systems Attached, add Niagara Component to List of NCs
				UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, AttachToComponent, AttachPoint, LocationOffset,
					RotationOffset, VFXScale, EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true, true);

				NiagaraOut.Add(NiagaraComponent);
			}
		}
	}
}

// 通过项目设置将 PhysicalSurface 转为 Context GameplayTag，并以返回值表示映射是否有效。
bool ULyraContextEffectsSubsystem::GetContextFromSurfaceType(
	TEnumAsByte<EPhysicalSurface> PhysicalSurface, FGameplayTag& Context)
{
	// 从项目设置读取物理表面到 Context 标签的映射。
	// Get Project Settings
	if (const ULyraContextEffectsSettings* ProjectSettings = GetDefault<ULyraContextEffectsSettings>())
	{
		// 查询该 PhysicalSurface 对应的 GameplayTag。
		// Find which Gameplay Tag the Surface Type is mapped to
		if (const FGameplayTag* GameplayTagPtr = ProjectSettings->SurfaceTypeToContextMap.Find(PhysicalSurface))
		{
			Context = *GameplayTagPtr;
		}
	}

	// 只有得到有效 GameplayTag 时才视为转换成功。
	// Return true if Context is Valid
	return Context.IsValid();
}

// 为指定 Actor 同步解析 Library 软引用、触发内部资源加载，并用新集合覆盖活动映射。
void ULyraContextEffectsSubsystem::LoadAndAddContextEffectsLibraries(AActor* OwningActor,
	TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> ContextEffectsLibraries)
{
	// Actor 无效或未配置任何 Library 时无需创建登记项。
	// Early out if Owning Actor is invalid or if the associated Libraries is 0 (or less)
	if (OwningActor == nullptr || ContextEffectsLibraries.Num() <= 0)
	{
		return;
	}

	// 为该 Actor 创建保存已加载 Library 强引用的运行时集合。
	// Create new Context Effect Set
	ULyraContextEffectsSet* EffectsLibrariesSet = NewObject<ULyraContextEffectsSet>(this);

	// 遍历 Actor 配置的 Library 软引用。
	// Cycle through Libraries getting Soft Obj Refs
	for (const TSoftObjectPtr<ULyraContextEffectsLibrary>& ContextEffectSoftObj : ContextEffectsLibraries)
	{
		// 当前同步解析 Library 资产软引用。
		// TODO：支持 Library 资产异步加载，并处理完成前的效果请求。
		// Load Library Assets from Soft Obj refs
		// TODO Support Async Loading of Asset Data
		if (ULyraContextEffectsLibrary* EffectsLibrary = ContextEffectSoftObj.LoadSynchronous())
		{
			// 有效 Library 继续加载其内部 Sound 与 NiagaraSystem 资源。
			// Call load on valid Libraries
			EffectsLibrary->LoadEffects();

			// 将 Library 加入 Actor 的活动集合并保持强引用。
			// Add new library to Set
			EffectsLibrariesSet->LyraContextEffectsLibraries.Add(EffectsLibrary);
		}
	}

	// 用新集合更新 Actor 到活动效果库的映射。
	// Update Active Actor Effects Map
	ActiveActorEffectsMap.Emplace(OwningActor, EffectsLibrariesSet);
}

// 移除 Actor 到 LibrarySet 的强引用映射，使不再被引用的运行时效果资源可回收。
void ULyraContextEffectsSubsystem::UnloadAndRemoveContextEffectsLibraries(AActor* OwningActor)
{
	// Actor 无效时无需执行卸载登记。
	// Early out if Owning Actor is invalid
	if (OwningActor == nullptr)
	{
		return;
	}

	// 移除 Actor 的 Library Set 强引用；资源在无其他引用后可被回收。
	// Remove ref from Active Actor/Effects Set Map
	ActiveActorEffectsMap.Remove(OwningActor);
}

