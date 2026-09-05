// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "LyraContextEffectsInterface.generated.h"

#define UE_API LYRAGAME_API

class UAnimSequenceBase;
class UObject;
class USceneComponent;
struct FFrame;

/**
 * Context 标签的匹配策略；当前接口保留精确匹配与最佳匹配两种语义。
 *
 */
UENUM()
enum EEffectsContextMatchType: int
{
	ExactMatch,
	BestMatch
};

/**
 * 动画动作事件接入 Context Effects 系统的蓝图接口。
 *
 */
 UINTERFACE(MinimalAPI, Blueprintable)
 class ULyraContextEffectsInterface : public UInterface
 {
	 GENERATED_BODY()

 };
 
 class ILyraContextEffectsInterface : public IInterface
 {
	 GENERATED_BODY()

 public:

	/** 根据动作标签、命中结果和上下文标签请求生成对应的音效与 Niagara 特效。 */
	/** */
 	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	UE_API void AnimMotionEffect(const FName Bone
		, const FGameplayTag MotionEffect
		, USceneComponent* StaticMeshComponent
		, const FVector LocationOffset
		, const FRotator RotationOffset
		, const UAnimSequenceBase* AnimationSequence
		, const bool bHitSuccess
		, const FHitResult HitResult
		, FGameplayTagContainer Contexts
		, FVector VFXScale = FVector(1)
		, float AudioVolume = 1
		, float AudioPitch = 1);
 };

#undef UE_API
