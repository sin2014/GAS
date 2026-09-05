// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "LyraContextEffectsSubsystem.generated.h"

#define UE_API LYRAGAME_API

enum EPhysicalSurface : int;

class AActor;
class UAudioComponent;
class ULyraContextEffectsLibrary;
class UNiagaraComponent;
class USceneComponent;
struct FFrame;
struct FGameplayTag;
struct FGameplayTagContainer;

/**
 * Context Effects 项目设置，将 PhysicalSurface 映射为用于效果筛选的 GameplayTag。
 *
 */
UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "LyraContextEffects"))
class ULyraContextEffectsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 物理表面类型到 Context GameplayTag 的项目级映射。
	//
	UPROPERTY(config, EditAnywhere)
	TMap<TEnumAsByte<EPhysicalSurface>, FGameplayTag> SurfaceTypeToContextMap;
};

/**
 * 某个 Actor 当前启用的 Context Effects Library 集合。
 *
 */
UCLASS(MinimalAPI)
class ULyraContextEffectsSet : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TSet<TObjectPtr<ULyraContextEffectsLibrary>> LyraContextEffectsLibraries;
};


/**
 * 按 Actor 管理上下文效果库，并负责匹配、生成附着音效与 Niagara 特效的 WorldSubsystem。
 * 
 */
UCLASS(MinimalAPI)
class ULyraContextEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	/** 从 SpawningActor 已注册且加载完成的库中匹配 Effect 与 Contexts，并将生成的组件写入输出数组。 */
	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void SpawnContextEffects(
		const AActor* SpawningActor
		, USceneComponent* AttachToComponent
		, const FName AttachPoint
		, const FVector LocationOffset
		, const FRotator RotationOffset
		, FGameplayTag Effect
		, FGameplayTagContainer Contexts
		, TArray<UAudioComponent*>& AudioOut
		, TArray<UNiagaraComponent*>& NiagaraOut
		, FVector VFXScale = FVector(1)
		, float AudioVolume = 1
		, float AudioPitch = 1);

	/** 根据项目设置把 PhysicalSurface 转换为 Context GameplayTag；映射有效时返回 true。 */
	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API bool GetContextFromSurfaceType(TEnumAsByte<EPhysicalSurface> PhysicalSurface, FGameplayTag& Context);

	/** 同步加载并登记某个 Actor 使用的 Context Effects Library 集合。 */
	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void LoadAndAddContextEffectsLibraries(AActor* OwningActor, TSet<TSoftObjectPtr<ULyraContextEffectsLibrary>> ContextEffectsLibraries);

	/** 移除某个 Actor 与其 Context Effects Library 集合的登记关系。 */
	/** */
	UFUNCTION(BlueprintCallable, Category = "ContextEffects")
	UE_API void UnloadAndRemoveContextEffectsLibraries(AActor* OwningActor);

private:

	UPROPERTY(Transient)
	TMap<TObjectPtr<AActor>, TObjectPtr<ULyraContextEffectsSet>> ActiveActorEffectsMap;

};

#undef UE_API
