// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNotify_LyraContextEffects.h"
#include "Feedback/ContextEffects/LyraContextEffectsLibrary.h"
#include "LyraContextEffectsInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "LyraContextEffectsSubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_LyraContextEffects)



// 构造 Context Effects 动画通知；当前不在构造函数中覆盖资产属性的默认值。
UAnimNotify_LyraContextEffects::UAnimNotify_LyraContextEffects()
{
}

// 资产加载后仅执行父类的 PostLoad 处理，当前没有额外的 Lyra 迁移逻辑。
void UAnimNotify_LyraContextEffects::PostLoad()
{
	Super::PostLoad();
}

#if WITH_EDITOR
// 编辑器属性变化时仅将事件交给父类处理，当前没有额外的 Lyra 校验或刷新逻辑。
void UAnimNotify_LyraContextEffects::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

// EffectTag 有效时将其作为时间轴显示名，否则回退到父类通知名称。
FString UAnimNotify_LyraContextEffects::GetNotifyName_Implementation() const
{
	// EffectTag 有效时用其字符串作为动画时间轴中的 Notify 显示名称。
	// If the Effect Tag is valid, pass the string name to the notify name
	if (Effect.IsValid())
	{
		return Effect.ToString();
	}

	return Super::GetNotifyName_Implementation();
}

// 通知触发时执行可选 Trace，向 Actor/组件接口接收者分发动作事件，并在编辑器预览 World 直接生成效果。
void UAnimNotify_LyraContextEffects::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp)
	{
		// 仅在 SkeletalMeshComponent 与其所属 Actor 均有效时处理通知。
		// Make sure both MeshComp and Owning Actor is valid
		if (AActor* OwningActor = MeshComp->GetOwner())
		{
			// 准备可选的表面射线检测结果。
			// Prepare Trace Data
			bool bHitSuccess = false;
			FHitResult HitResult;
			FCollisionQueryParams QueryParams;

			if (TraceProperties.bIgnoreActor)
			{
				QueryParams.AddIgnoredActor(OwningActor);
			}

			QueryParams.bReturnPhysicalMaterial = true;

			if (bPerformTrace)
			{
				// 射线起点取附着 Socket 位置；未附着时取 MeshComponent 位置。
				// If trace is needed, set up Start Location to Attached
				FVector TraceStart = bAttached ? MeshComp->GetSocketLocation(SocketName) : MeshComp->GetComponentLocation();

				// World 有效时执行射线检测。
				// Make sure World is valid
				if (UWorld* World = OwningActor->GetWorld())
				{
					// 按配置通道和终点偏移执行单次射线，并按需忽略所属 Actor。
					// Call Line Trace, Pass in relevant properties
					bHitSuccess = World->LineTraceSingleByChannel(HitResult, TraceStart, (TraceStart + TraceProperties.EndTraceLocationOffset),
						TraceProperties.TraceChannel, QueryParams, FCollisionResponseParams::DefaultResponseParam);
				}
			}

			// 准备传给接口实现的附加 Context 容器。
			// Prepare Contexts in advance
			FGameplayTagContainer Contexts;

			// 收集所属 Actor 及其组件中实现 Context Effects 接口的对象。
			// Set up Array of Objects that implement the Context Effects Interface
			TArray<UObject*> LyraContextEffectImplementingObjects;

			// 所属 Actor 自身实现接口时也加入接收者列表。
			// Determine if the Owning Actor is one of the Objects that implements the Context Effects Interface
			if (OwningActor->Implements<ULyraContextEffectsInterface>())
			{
				// 将 Actor 加入动画动作事件接收者。
				// If so, add it to the Array
				LyraContextEffectImplementingObjects.Add(OwningActor);
			}

			// 遍历所属 Actor 的组件，查找 Context Effects 接口实现。
			// Cycle through Owning Actor's Components and determine if any of them is a Component implementing the Context Effect Interface
			for (const auto Component : OwningActor->GetComponents())
			{
				if (Component)
				{
					// 将实现接口的组件加入接收者列表。
					// If the Component implements the Context Effects Interface, add it to the list
					if (Component->Implements<ULyraContextEffectsInterface>())
					{
						LyraContextEffectImplementingObjects.Add(Component);
					}
				}
			}

			// 向所有有效接口接收者分发本次动画动作事件。
			// Cycle through all objects implementing the Context Effect Interface
			for (UObject* LyraContextEffectImplementingObject : LyraContextEffectImplementingObjects)
			{
				if (LyraContextEffectImplementingObject)
				{
					// 传入动作标签、附着信息、命中结果和表现参数，执行 AnimMotionEffect 接口事件。
					// If the object is still valid, Execute the AnimMotionEffect Event on it, passing in relevant data
					ILyraContextEffectsInterface::Execute_AnimMotionEffect(LyraContextEffectImplementingObject,
						(bAttached ? SocketName : FName("None")),
						Effect, MeshComp, LocationOffset, RotationOffset,
						Animation, bHitSuccess, HitResult, Contexts, VFXProperties.Scale,
						AudioProperties.VolumeMultiplier, AudioProperties.PitchMultiplier);
				}
			}

#if WITH_EDITORONLY_DATA
			// 编辑器预览路径直接复现接口与子系统的关键调用，不依赖运行时 Actor 组件登记。
			// This is for Anim Editor previewing, it is a deconstruction of the calls made by the Interface and the Subsystem
			if (bPreviewInEditor)
			{
				UWorld* World = OwningActor->GetWorld();

				// 仅在动画编辑器的 EditorPreview World 中执行预览生成。
				// Get the world, make sure it's an Editor Preview World
				if (World && World->WorldType == EWorldType::EditorPreview)
				{
					// 加入通知上配置的预览 Context 标签。
					// Add Preview contexts if necessary
					Contexts.AppendTags(PreviewProperties.PreviewContexts);

					// 按项目设置把选定预览表面转换为 Context，并加入匹配条件。
					// Convert given Surface Type to Context and Add it to the Contexts for this Preview
					if (PreviewProperties.bPreviewPhysicalSurfaceAsContext)
					{
						TEnumAsByte<EPhysicalSurface> PhysicalSurfaceType = PreviewProperties.PreviewPhysicalSurface;

						if (const ULyraContextEffectsSettings* LyraContextEffectsSettings = GetDefault<ULyraContextEffectsSettings>())
						{
							if (const FGameplayTag* SurfaceContextPtr = LyraContextEffectsSettings->SurfaceTypeToContextMap.Find(PhysicalSurfaceType))
							{
								FGameplayTag SurfaceContext = *SurfaceContextPtr;

								Contexts.AddTag(SurfaceContext);
							}
						}
					}

					// 预览 Library 是软引用，使用前需要加载资产。
					// TODO：支持编辑器预览资源异步加载。
					// Libraries are soft referenced, so you will want to try to load them now
					// TODO Async Asset Loading
					if (UObject* EffectsLibrariesObj = PreviewProperties.PreviewContextEffectsLibrary.TryLoad())
					{
						// 确认软引用实际指向 ULyraContextEffectsLibrary。
						// Check if it is in fact a ULyraContextEffectLibrary type
						if (ULyraContextEffectsLibrary* EffectLibrary = Cast<ULyraContextEffectsLibrary>(EffectsLibrariesObj))
						{
							// 准备汇总预览匹配结果的资源数组。
							// Prepare Sounds and Niagara System Arrays
							TArray<USoundBase*> TotalSounds;
							TArray<UNiagaraSystem*> TotalNiagaraSystems;

							// 加载 Library 内容，并把解析后的强引用缓存到资产的 Transient 运行时数据中。
							// Attempt to load the Effect Library content (will cache in Transient data on the Effect Library Asset)
							EffectLibrary->LoadEffects();

							// Library 加载完成后，按 EffectTag 和预览 Context 查询资源。
							// If the Effect Library is valid and marked as Loaded, Get Effects from it
							if (EffectLibrary && EffectLibrary->GetContextEffectsLibraryLoadState() == EContextEffectsLibraryLoadState::Loaded)
							{
								// 使用局部数组接收当前 Library 的匹配结果。
								// Prepare local arrays
								TArray<USoundBase*> Sounds;
								TArray<UNiagaraSystem*> NiagaraSystems;

								// 获取所有匹配的 Sound 与 NiagaraSystem。
								// Get the Effects
								EffectLibrary->GetEffects(Effect, Contexts, Sounds, NiagaraSystems);

								// 合并到本次预览的总资源列表。
								// Append to the accumulating arrays
								TotalSounds.Append(Sounds);
								TotalNiagaraSystems.Append(NiagaraSystems);
							}

							// 将匹配音效附着生成到预览 MeshComponent。
							// Cycle through Sounds and call Spawn Sound Attached, passing in relevant data
							for (USoundBase* Sound : TotalSounds)
							{
								UGameplayStatics::SpawnSoundAttached(Sound, MeshComp, (bAttached ? SocketName : FName("None")), LocationOffset, RotationOffset, EAttachLocation::KeepRelativeOffset,
									false, AudioProperties.VolumeMultiplier, AudioProperties.PitchMultiplier, 0.0f, nullptr, nullptr, true);
							}

							// 将匹配 NiagaraSystem 附着生成到预览 MeshComponent。
							// Cycle through Niagara Systems and call Spawn System Attached, passing in relevant data
							for (UNiagaraSystem* NiagaraSystem : TotalNiagaraSystems)
							{
								UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, MeshComp, (bAttached ? SocketName : FName("None")), LocationOffset,
									RotationOffset, VFXProperties.Scale, EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true, true);
							}
						}
					}
						
				}
			}
#endif

		}
	}
}

#if WITH_EDITOR
// 编辑器验证通知关联的 EffectTag 与预览 Library 配置，并汇总资产问题。
void UAnimNotify_LyraContextEffects::ValidateAssociatedAssets()
{
	Super::ValidateAssociatedAssets();
}

// 编辑器批量写入通知的动作标签、变换、表现、附着和 Trace 参数。
void UAnimNotify_LyraContextEffects::SetParameters(FGameplayTag EffectIn, FVector LocationOffsetIn, FRotator RotationOffsetIn,
	FLyraContextEffectAnimNotifyVFXSettings VFXPropertiesIn, FLyraContextEffectAnimNotifyAudioSettings AudioPropertiesIn,
	bool bAttachedIn, FName SocketNameIn, bool bPerformTraceIn, FLyraContextEffectAnimNotifyTraceSettings TracePropertiesIn)
{
	Effect = EffectIn;
	LocationOffset = LocationOffsetIn;
	RotationOffset = RotationOffsetIn;
	VFXProperties.Scale = VFXPropertiesIn.Scale;
	AudioProperties.PitchMultiplier = AudioPropertiesIn.PitchMultiplier;
	AudioProperties.VolumeMultiplier = AudioPropertiesIn.VolumeMultiplier;
	bAttached = bAttachedIn;
	SocketName = SocketNameIn;
	bPerformTrace = bPerformTraceIn;
	TraceProperties.EndTraceLocationOffset = TracePropertiesIn.EndTraceLocationOffset;
	TraceProperties.TraceChannel = TracePropertiesIn.TraceChannel;
	TraceProperties.bIgnoreActor = TracePropertiesIn.bIgnoreActor;

}
#endif

