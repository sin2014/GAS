// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "NativeGameplayTags.h"

#include "LyraCharacterMovementComponent.generated.h"

#define UE_API LYRAGAME_API

class UObject;
struct FFrame;

LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_MovementStopped);

/**
 * 缓存角色脚下命中结果、离地距离和当前移动模式，供动画等系统查询。
 */
/**
 * FLyraCharacterGroundInfo
 *
 *	Information about the ground under the character.  It only gets updated as needed.
 */
USTRUCT(BlueprintType)
struct FLyraCharacterGroundInfo
{
	GENERATED_BODY()

	FLyraCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};


/**
 * Lyra 角色移动组件，扩展速度、旋转、跳跃和地面信息查询行为。
 */
/**
 * ULyraCharacterMovementComponent
 *
 *	The base character movement component class used by this project.
 */
UCLASS(MinimalAPI, Config = Game)
class ULyraCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UE_API ULyraCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void SimulateMovement(float DeltaTime) override;

	UE_API virtual bool CanAttemptJump() const override;

	// 返回当前地面信息；若本帧缓存尚未更新，则在调用时执行地面检测。
	// Returns the current ground info.  Calling this will update the ground info if it's out of date.
	UFUNCTION(BlueprintCallable, Category = "Lyra|CharacterMovement")
	UE_API const FLyraCharacterGroundInfo& GetGroundInfo();

	UE_API void SetReplicatedAcceleration(const FVector& InAcceleration);

	//~UMovementComponent interface
	UE_API virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	UE_API virtual float GetMaxSpeed() const override;
	//~End of UMovementComponent interface

protected:

	UE_API virtual void InitializeComponent() override;

protected:

	// 角色地面信息缓存；只能通过 GetGroundInfo() 访问，因为该函数负责按帧刷新缓存。
	// Cached ground info for the character.  Do not access this directly!  It's only updated when accessed via GetGroundInfo().
	FLyraCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;
};

#undef UE_API
