// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerStart.h"
#include "GameplayTagContainer.h"

#include "LyraPlayerStart.generated.h"

#define UE_API LYRAGAME_API

class AController;
class UObject;

enum class ELyraPlayerStartLocationOccupancy
{
	Empty,
	Partial,
	Full
};

/**
 * 可供多种玩法模式复用的出生点，支持标签筛选、空间占用检测和 Controller 认领。
 */
/**
 * ALyraPlayerStart
 * 
 * Base player starts that can be used by a lot of modes.
 */
UCLASS(MinimalAPI, Config = Game)
class ALyraPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UE_API ALyraPlayerStart(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const FGameplayTagContainer& GetGameplayTags() { return StartPointTags; }

	UE_API ELyraPlayerStartLocationOccupancy GetLocationOccupancy(AController* const ControllerPawnToFit) const;

	/** 该出生点是否已被某个 Controller 认领。 */
	/** Did this player start get claimed by a controller already? */
	UE_API bool IsClaimed() const;

	/** 若出生点尚未被认领，则将其分配给 OccupyingController 并开始定时检查释放条件。 */
	/** If this PlayerStart was not claimed, claim it for ClaimingController */
	UE_API bool TryClaim(AController* OccupyingController);

protected:
	/** 当认领者已生成 Pawn 且出生点重新变为空闲时，释放该认领。 */
	/** Check if this PlayerStart is still claimed */
	UE_API void CheckUnclaimed();

	/** 当前认领该出生点的 Controller。 */
	/** The controller that claimed this PlayerStart */
	UPROPERTY(Transient)
	TObjectPtr<AController> ClaimingController = nullptr;

	/** 轮询出生点是否已不再与任何待生成 Pawn 冲突的时间间隔。 */
	/** Interval in which we'll check if this player start is not colliding with anyone anymore */
	UPROPERTY(EditDefaultsOnly, Category = "Player Start Claiming")
	float ExpirationCheckInterval = 1.f;

	/** 用于筛选和识别该出生点的 GameplayTag。 */
	/** Tags to identify this player start */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer StartPointTags;

	/** 跟踪认领释放轮询定时器的句柄。 */
	/** Handle to track expiration recurring timer */
	FTimerHandle ExpirationTimerHandle;
};

#undef UE_API
