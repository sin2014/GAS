// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/WeakObjectPtr.h"

#include "LyraContextEffectsLibrary.generated.h"

#define UE_API LYRAGAME_API

class UNiagaraSystem;
class USoundBase;
struct FFrame;

/**
 * Context Effects Library 的资源加载状态。
 *
 */
UENUM()
enum class EContextEffectsLibraryLoadState : uint8 {
	Unloaded = 0,
	Loading = 1,
	Loaded = 2
};

/**
 * 一条上下文效果配置：由动作 EffectTag、所需 Context 标签集合和待加载的音效/VFX 软引用组成。
 *
 */
USTRUCT(BlueprintType)
struct FLyraContextEffects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer Context;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowedClasses = "/Script/Engine.SoundBase, /Script/Niagara.NiagaraSystem"))
	TArray<FSoftObjectPath> Effects;

};

/**
 * 配置加载后的运行时条目，按资源类型保存可直接生成的 Sound 与 NiagaraSystem。
 *
 */
UCLASS(MinimalAPI)
class ULyraActiveContextEffects : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FGameplayTag EffectTag;

	UPROPERTY(VisibleAnywhere)
	FGameplayTagContainer Context;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UNiagaraSystem>> NiagaraSystems;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FLyraContextEffectLibraryLoadingComplete, TArray<ULyraActiveContextEffects*>, LyraActiveContextEffects);

/**
 * 上下文效果库；负责加载软引用资源，并按 EffectTag 精确匹配且要求 Context 满足配置标签后返回效果。
 * 
 */
UCLASS(MinimalAPI, BlueprintType)
class ULyraContextEffectsLibrary : public UObject
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FLyraContextEffects> ContextEffects;

	UFUNCTION(BlueprintCallable)
	UE_API void GetEffects(const FGameplayTag Effect, const FGameplayTagContainer Context, TArray<USoundBase*>& Sounds, TArray<UNiagaraSystem*>& NiagaraSystems);

	UFUNCTION(BlueprintCallable)
	UE_API void LoadEffects();

	UE_API EContextEffectsLibraryLoadState GetContextEffectsLibraryLoadState();

private:
	void LoadEffectsInternal();

	void LyraContextEffectLibraryLoadingComplete(TArray<ULyraActiveContextEffects*> LyraActiveContextEffects);

	UPROPERTY(Transient)
	TArray< TObjectPtr<ULyraActiveContextEffects>> ActiveContextEffects;

	UPROPERTY(Transient)
	EContextEffectsLibraryLoadState EffectsLoadState = EContextEffectsLibraryLoadState::Unloaded;
};

#undef UE_API
