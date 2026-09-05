// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Chaos/ChaosEngineInterface.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "AnimNotify_LyraContextEffects.generated.h"

#define UE_API LYRAGAME_API

/**
 * 动画通知生成 Context VFX 时使用的缩放参数。
 *
 */
USTRUCT(BlueprintType)
struct FLyraContextEffectAnimNotifyVFXSettings
{
	GENERATED_BODY()

	// 生成 Niagara 特效时使用的缩放。
	// Scale to spawn the particle system at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FX)
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

};

/**
 * 动画通知生成 Context 音效时使用的音量与音高参数。
 *
 */
USTRUCT(BlueprintType)
struct FLyraContextEffectAnimNotifyAudioSettings
{
	GENERATED_BODY()

	// 生成音效时使用的音量倍率。
	// Volume Multiplier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	float VolumeMultiplier = 1.0f;

	// 生成音效时使用的音高倍率。
	// Pitch Multiplier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	float PitchMultiplier = 1.0f;
};


/**
 * 为取得 PhysicalSurface 而执行射线检测的参数。
 *
 */
USTRUCT(BlueprintType)
struct FLyraContextEffectAnimNotifyTraceSettings
{
	GENERATED_BODY()

	// 射线检测使用的碰撞通道。
	// Trace Channel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Trace)
	TEnumAsByte<ECollisionChannel> TraceChannel = ECollisionChannel::ECC_Visibility;

	// 相对效果位置的射线终点偏移。
	// Vector offset from Effect Location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Trace)
	FVector EndTraceLocationOffset = FVector::ZeroVector;

	// 射线检测时是否忽略动画所属 Actor。
	// Ignore this Actor when getting trace result
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Trace)
	bool bIgnoreActor = true;
};

/**
 * 动画编辑器预览 Context Effects 时使用的表面、Library 和附加 Context。
 *
 */
USTRUCT(BlueprintType)
struct FLyraContextEffectAnimNotifyPreviewSettings
{
	GENERATED_BODY()

	// 是否通过项目设置把预览 PhysicalSurface 转换为 Context 标签。
	// If true, will attempt to match selected Surface Type to Context Tag via Project Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Preview)
	bool bPreviewPhysicalSurfaceAsContext = true;

	// 编辑器预览使用的物理表面类型。
	// Surface Type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Preview, meta=(EditCondition="bPreviewPhysicalSurfaceAsContext"))
	TEnumAsByte<EPhysicalSurface> PreviewPhysicalSurface = EPhysicalSurface::SurfaceType_Default;

	// 编辑器预览时同步加载并查询的 Context Effects Library。
	// Preview Library
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Preview, meta = (AllowedClasses = "/Script/LyraGame.LyraContextEffectsLibrary"))
	FSoftObjectPath PreviewContextEffectsLibrary;

	// 编辑器预览时额外参与效果匹配的 Context 标签。
	// Preview Context
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Preview)
	FGameplayTagContainer PreviewContexts;
};


/**
 * 动画时间轴通知：在指定骨骼/Socket 位置发出动作标签，按命中表面与 Context 匹配并播放效果。
 * 
 */
UCLASS(MinimalAPI, const, hidecategories=Object, CollapseCategories, Config = Game, meta=(DisplayName="Play Context Effects"))
class UAnimNotify_LyraContextEffects : public UAnimNotify
{
	GENERATED_BODY()

public:
	UE_API UAnimNotify_LyraContextEffects();

	// Begin UObject interface
	UE_API virtual void PostLoad() override;
#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UObject interface

	// Begin UAnimNotify interface
	UE_API virtual FString GetNotifyName_Implementation() const override;
	UE_API virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
#if WITH_EDITOR
	UE_API virtual void ValidateAssociatedAssets() override;
#endif
	// End UAnimNotify interface

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable)
	UE_API void SetParameters(FGameplayTag EffectIn, FVector LocationOffsetIn, FRotator RotationOffsetIn, 
		FLyraContextEffectAnimNotifyVFXSettings VFXPropertiesIn, FLyraContextEffectAnimNotifyAudioSettings AudioPropertiesIn,
		bool bAttachedIn, FName SocketNameIn, bool bPerformTraceIn, FLyraContextEffectAnimNotifyTraceSettings TracePropertiesIn);
#endif


	// 要触发的动作 EffectTag，用于在 Library 中精确匹配配置。
	// Effect to Play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (DisplayName = "Effect", ExposeOnSpawn = true))
	FGameplayTag Effect;

	// 相对目标骨骼或 Socket 的位置偏移。
	// Location offset from the socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FVector LocationOffset = FVector::ZeroVector;

	// 相对目标骨骼或 Socket 的旋转偏移。
	// Rotation offset from socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FRotator RotationOffset = FRotator::ZeroRotator;

	// Niagara 特效的生成参数。
	// Scale to spawn the particle system at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FLyraContextEffectAnimNotifyVFXSettings VFXProperties;

	// 音效的生成参数。
	// Scale to spawn the particle system at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	FLyraContextEffectAnimNotifyAudioSettings AudioProperties;

	// 生成的效果是否附着到指定骨骼或 Socket。
	// Should attach to the bone/socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttachmentProperties", meta = (ExposeOnSpawn = true))
	uint32 bAttached : 1; 	/* 因蓝图属性重定向兼容性而保留此命名，未遵循常规代码规范。 */ //~ Does not follow coding standard due to redirection from BP

	// 效果要附着的骨骼或 Socket 名称。
	// SocketName to attach to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AttachmentProperties", meta = (ExposeOnSpawn = true, EditCondition = "bAttached"))
	FName SocketName;

	// 是否执行射线检测；将 SurfaceType 转换为 Context 时必须启用。
	// Will perform a trace, required for SurfaceType to Context Conversion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true))
	uint32 bPerformTrace : 1; 	

	// 射线检测参数。
	// Scale to spawn the particle system at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ExposeOnSpawn = true, EditCondition = "bPerformTrace"))
	FLyraContextEffectAnimNotifyTraceSettings TraceProperties;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Config, EditAnywhere, Category = "PreviewProperties")
	uint32 bPreviewInEditor : 1;

	UPROPERTY(EditAnywhere, Category = "PreviewProperties", meta = (EditCondition = "bPreviewInEditor"))
	FLyraContextEffectAnimNotifyPreviewSettings PreviewProperties;
#endif


};

#undef UE_API
