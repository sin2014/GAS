// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerStart.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerStart)

// 构造 LyraPlayerStart，使用默认认领状态和定时检查间隔。
ALyraPlayerStart::ALyraPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 用默认 Pawn 碰撞检测出生点；直接可放置为 Empty，可找到传送修正点为 Partial，否则为 Full。
ELyraPlayerStartLocationOccupancy ALyraPlayerStart::GetLocationOccupancy(AController* const ControllerPawnToFit) const
{
	UWorld* const World = GetWorld();
	if (HasAuthority() && World)
	{
		if (AGameModeBase* AuthGameMode = World->GetAuthGameMode())
		{
			TSubclassOf<APawn> PawnClass = AuthGameMode->GetDefaultPawnClassForController(ControllerPawnToFit);
			const APawn* const PawnToFit = PawnClass ? GetDefault<APawn>(PawnClass) : nullptr;

			FVector ActorLocation = GetActorLocation();
			const FRotator ActorRotation = GetActorRotation();

			if (!World->EncroachingBlockingGeometry(PawnToFit, ActorLocation, ActorRotation, nullptr))
			{
				return ELyraPlayerStartLocationOccupancy::Empty;
			}
			else if (World->FindTeleportSpot(PawnToFit, ActorLocation, ActorRotation))
			{
				return ELyraPlayerStartLocationOccupancy::Partial;
			}
		}
	}

	return ELyraPlayerStartLocationOccupancy::Full;
}

// ClaimingController 非空时返回 true。
bool ALyraPlayerStart::IsClaimed() const
{
	return ClaimingController != nullptr;
}

// 仅在有 Controller 且尚未认领时占用出生点，并启动定时检查以在 Pawn 离开后释放。
bool ALyraPlayerStart::TryClaim(AController* OccupyingController)
{
	if (OccupyingController != nullptr && !IsClaimed())
	{
		ClaimingController = OccupyingController;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ExpirationTimerHandle, FTimerDelegate::CreateUObject(this, &ALyraPlayerStart::CheckUnclaimed), ExpirationCheckInterval, true);
		}
		return true;
	}
	return false;
}

// 认领者 Pawn 已离开且出生点恢复 Empty 时释放认领并停止轮询定时器。
void ALyraPlayerStart::CheckUnclaimed()
{
	if (ClaimingController != nullptr && ClaimingController->GetPawn() != nullptr && GetLocationOccupancy(ClaimingController) == ELyraPlayerStartLocationOccupancy::Empty)
	{
		ClaimingController = nullptr;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ExpirationTimerHandle);
		}
	}
}

