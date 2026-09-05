// Copyright Epic Games, Inc. All Rights Reserved.


#include "LyraContextEffectComponent.h"

#include "Engine/World.h"
#include "LyraContextEffectsSubsystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraContextEffectComponent)

class UAnimSequenceBase;
class USceneComponent;



// 配置组件默认值。
// Sets default values for this component's properties
// 禁用 Tick、启用自动激活，并初始化 Actor 级 Context Effects 组件。
ULyraContextEffectComponent::ULyraContextEffectComponent()
{
	// 该组件无需 Tick，并在注册后自动激活。
	// Disable component tick, enable Auto Activate
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
	// ...
}


// 游戏开始时初始化 Context 与 Library 登记。
// Called when the game starts
// 合并默认 Context，并向 WorldSubsystem 加载、登记该 Actor 的默认 Library 集合。
void ULyraContextEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	CurrentContexts.AppendTags(DefaultEffectContexts);
	CurrentContextEffectsLibraries = DefaultContextEffectsLibraries;

	// BeginPlay 时让 WorldSubsystem 加载并登记当前 Actor 的 Library 集合。
	// On Begin Play, Load and Add Context Effects pairings
	if (const UWorld* World = GetWorld())
	{
		if (ULyraContextEffectsSubsystem* LyraContextEffectsSubsystem = World->GetSubsystem<ULyraContextEffectsSubsystem>())
		{
			LyraContextEffectsSubsystem->LoadAndAddContextEffectsLibraries(GetOwner(), CurrentContextEffectsLibraries);
		}
	}
}

// 组件结束时从 WorldSubsystem 移除 Actor 的 Library 登记，再执行父类清理。
void ULyraContextEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay 时移除当前 Actor 的 Library 登记，释放不再需要的强引用。
	// On End PLay, remove unnecessary context effects pairings
	if (const UWorld* World = GetWorld())
	{
		if (ULyraContextEffectsSubsystem* LyraContextEffectsSubsystem = World->GetSubsystem<ULyraContextEffectsSubsystem>())
		{
			LyraContextEffectsSubsystem->UnloadAndRemoveContextEffectsLibraries(GetOwner());
		}
	}

	Super::EndPlay(EndPlayReason);
}

// 实现 ILyraContextEffectsInterface 的 AnimMotionEffect 事件。
// Implementation of Interface's AnimMotionEffect function
// 聚合组件与事件 Context、可选转换命中表面，清理旧组件引用，并请求子系统生成本次动作反馈。
void ULyraContextEffectComponent::AnimMotionEffect_Implementation(const FName Bone, const FGameplayTag MotionEffect, USceneComponent* StaticMeshComponent,
	const FVector LocationOffset, const FRotator RotationOffset, const UAnimSequenceBase* AnimationSequence,
	const bool bHitSuccess, const FHitResult HitResult, FGameplayTagContainer Contexts,
	FVector VFXScale, float AudioVolume, float AudioPitch)
{
	// 准备本次生成结果，并清理缓存中的失效组件引用。
	// Prep Components
	TArray<UAudioComponent*> AudioComponentsToAdd;
	TArray<UNiagaraComponent*> NiagaraComponentsToAdd;

	FGameplayTagContainer TotalContexts;

	// 合并组件当前 Context 与本次动画通知额外传入的 Context。
	// Aggregate contexts
	TotalContexts.AppendTags(Contexts);
	TotalContexts.AppendTags(CurrentContexts);

	// 按配置决定是否把命中物理表面加入 Context 匹配条件。
	// Check if converting Physical Surface Type to Context
	if (bConvertPhysicalSurfaceToContext)
	{
		// 从 HitResult 取得命中的 PhysicalMaterial 弱引用。
		// Get Phys Mat Type Pointer
		TWeakObjectPtr<UPhysicalMaterial> PhysicalSurfaceTypePtr = HitResult.PhysMaterial;

		// 仅在物理材质仍有效时读取 SurfaceType。
		// Check if pointer is okay
		if (PhysicalSurfaceTypePtr.IsValid())
		{
			// 获取物理材质配置的 EPhysicalSurface。
			// Get the Surface Type Pointer
			TEnumAsByte<EPhysicalSurface> PhysicalSurfaceType = PhysicalSurfaceTypePtr->SurfaceType;

			// 从 Context Effects 项目设置查询表面映射。
			// If Settings are valid
			if (const ULyraContextEffectsSettings* LyraContextEffectsSettings = GetDefault<ULyraContextEffectsSettings>())
			{
				// 找到映射时把对应 GameplayTag 加入本次总 Context。
				// Convert Surface Type to known
				if (const FGameplayTag* SurfaceContextPtr = LyraContextEffectsSettings->SurfaceTypeToContextMap.Find(PhysicalSurfaceType))
				{
					FGameplayTag SurfaceContext = *SurfaceContextPtr;

					TotalContexts.AddTag(SurfaceContext);
				}
			}
		}
	}

	// 保留仍有效的活动 AudioComponent，剔除已销毁引用。
	// Cycle through Active Audio Components and cache
	for (UAudioComponent* ActiveAudioComponent : ActiveAudioComponents)
	{
		if (ActiveAudioComponent)
		{
			AudioComponentsToAdd.Add(ActiveAudioComponent);
		}
	}

	// 保留仍有效的活动 NiagaraComponent，剔除已销毁引用。
	// Cycle through Active Niagara Components and cache
	for (UNiagaraComponent* ActiveNiagaraComponent : ActiveNiagaraComponents)
	{
		if (ActiveNiagaraComponent)
		{
			NiagaraComponentsToAdd.Add(ActiveNiagaraComponent);
		}
	}

	// 从当前 World 获取 Context Effects 子系统。
	// Get World
	if (const UWorld* World = GetWorld())
	{
		// 获取负责匹配与生成效果的 WorldSubsystem。
		// Get Subsystem
		if (ULyraContextEffectsSubsystem* LyraContextEffectsSubsystem = World->GetSubsystem<ULyraContextEffectsSubsystem>())
		{
			// 使用局部数组接收本次新生成的音频与 Niagara 组件。
			// Set up Audio Components and Niagara
			TArray<UAudioComponent*> AudioComponents;
			TArray<UNiagaraComponent*> NiagaraComponents;

			// 根据动作标签与聚合 Context 请求子系统生成匹配效果。
			// Spawn effects
			LyraContextEffectsSubsystem->SpawnContextEffects(GetOwner(), StaticMeshComponent, Bone, 
				LocationOffset, RotationOffset, MotionEffect, TotalContexts,
				AudioComponents, NiagaraComponents, VFXScale, AudioVolume, AudioPitch);

			// 将本次生成结果加入活动组件缓存。
			// Append resultant effects
			AudioComponentsToAdd.Append(AudioComponents);
			NiagaraComponentsToAdd.Append(NiagaraComponents);
		}
	}

	// 用清理后的旧组件加本次新组件重建活动音频列表。
	// Append Active Audio Components
	ActiveAudioComponents.Empty();
	ActiveAudioComponents.Append(AudioComponentsToAdd);

	// 用清理后的旧组件加本次新组件重建活动 Niagara 列表。
	// Append Active
	ActiveNiagaraComponents.Empty();
	ActiveNiagaraComponents.Append(NiagaraComponentsToAdd);

}

// 用传入标签集合完整替换该组件当前参与效果匹配的 Context。
void ULyraContextEffectComponent::UpdateEffectContexts(FGameplayTagContainer NewEffectContexts)
{
	// 用新标签集合完整替换组件当前 Context。
	// Reset and update
	CurrentContexts.Reset(NewEffectContexts.Num());
	CurrentContexts.AppendTags(NewEffectContexts);
}

// 替换 Actor 使用的 Library 软引用集合，并让子系统重新加载、覆盖活动登记。
void ULyraContextEffectComponent::UpdateLibraries(
	TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> NewContextEffectsLibraries)
{
	// 保存新的 Library 配置，随后覆盖子系统中的 Actor 登记。
	// Clear out existing Effects
	CurrentContextEffectsLibraries = NewContextEffectsLibraries;

	// 从当前 World 获取 Context Effects 子系统。
	// Get World
	if (const UWorld* World = GetWorld())
	{
		// 获取负责 Library 生命周期的 WorldSubsystem。
		// Get Subsystem
		if (ULyraContextEffectsSubsystem* LyraContextEffectsSubsystem = World->GetSubsystem<ULyraContextEffectsSubsystem>())
		{
			// 加载新 Library 集合并替换当前 Actor 的活动登记。
			// Load and Add Libraries to Subsystem                  
			LyraContextEffectsSubsystem->LoadAndAddContextEffectsLibraries(GetOwner(), CurrentContextEffectsLibraries);
		}
	}
}

