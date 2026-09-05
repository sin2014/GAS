// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "LyraContextEffectsInterface.h"

#include "LyraContextEffectComponent.generated.h"

#define UE_API LYRAGAME_API

namespace EEndPlayReason { enum Type : int; }

class UAnimSequenceBase;
class UAudioComponent;
class ULyraContextEffectsLibrary;
class UNiagaraComponent;
class UObject;
class USceneComponent;
struct FFrame;
struct FHitResult;

UCLASS(MinimalAPI,  ClassGroup=(Custom), hidecategories = (Variable, Tags, ComponentTick, ComponentReplication, Activation, Cooking, AssetUserData, Collision), CollapseCategories, meta=(BlueprintSpawnableComponent) )
class ULyraContextEffectComponent : public UActorComponent, public ILyraContextEffectsInterface
{
	GENERATED_BODY()

public:	
	// 配置组件默认值。
	// Sets default values for this component's properties
	UE_API ULyraContextEffectComponent();

protected:
	// 游戏开始时登记并加载默认 Context Effects Library。
	// Called when the game starts
	UE_API virtual void BeginPlay() override;

	// 游戏结束时从子系统移除本 Actor 的 Library 登记。
	// Called when the game ends
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 接收动画动作事件，聚合 Context 并请求子系统生成效果。
	// AnimMotionEffect Implementation
	UFUNCTION(BlueprintCallable)
	UE_API virtual void AnimMotionEffect_Implementation(const FName Bone, const FGameplayTag MotionEffect, USceneComponent* StaticMeshComponent,
		const FVector LocationOffset, const FRotator RotationOffset, const UAnimSequenceBase* AnimationSequence,
		const bool bHitSuccess, const FHitResult HitResult, FGameplayTagContainer Contexts,
		FVector VFXScale = FVector(1), float AudioVolume = 1, float AudioPitch = 1) override;

	// 是否把 HitResult 的 PhysicalSurface 自动转换为 Context 标签。
	// Auto-Convert Physical Surface from Trace Result to Context
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bConvertPhysicalSurfaceToContext = true;

	// 该 Actor 默认携带并参与效果匹配的 Context 标签。
	// Default Contexts
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer DefaultEffectContexts;

	// 该 Actor 默认使用的 Context Effects Library 软引用集合。
	// Default Libraries for this Actor
	UPROPERTY(EditAnywhere)
	TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> DefaultContextEffectsLibraries;

	UFUNCTION(BlueprintCallable)
	UE_API void UpdateEffectContexts(FGameplayTagContainer NewEffectContexts);

	UFUNCTION(BlueprintCallable)
	UE_API void UpdateLibraries(TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> NewContextEffectsLibraries);

private:
	UPROPERTY(Transient)
	FGameplayTagContainer CurrentContexts;

	UPROPERTY(Transient)
	TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> CurrentContextEffectsLibraries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ActiveAudioComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ActiveNiagaraComponents;
};

#undef UE_API
