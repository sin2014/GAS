// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonLocalPlayer.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameUIManagerSubsystem.h"
#include "GameUIPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonLocalPlayer)

class APawn;
class APlayerState;
class FViewport;
struct FSceneViewProjectionData;

// 创建本地玩家；控制器、玩家状态和 Pawn 的委托在对象分配后按需触发。
UCommonLocalPlayer::UCommonLocalPlayer()
	: Super(FObjectInitializer::Get())
{
}

// 立即用当前控制器调用回调并登记后续控制器变更通知。
FDelegateHandle UCommonLocalPlayer::CallAndRegister_OnPlayerControllerSet(FPlayerControllerSetDelegate::FDelegate Delegate)
{
	APlayerController* PC = GetPlayerController(GetWorld());

	if (PC)
	{
		Delegate.Execute(this, PC);
	}

	return OnPlayerControllerSet.Add(Delegate);
}

// 立即用当前玩家状态调用回调并登记后续状态变更通知。
FDelegateHandle UCommonLocalPlayer::CallAndRegister_OnPlayerStateSet(FPlayerStateSetDelegate::FDelegate Delegate)
{
	APlayerController* PC = GetPlayerController(GetWorld());
	APlayerState* PlayerState = PC ? PC->PlayerState : nullptr;

	if (PlayerState)
	{
		Delegate.Execute(this, PlayerState);
	}
	
	return OnPlayerStateSet.Add(Delegate);
}

// 立即用当前 Pawn 调用回调并登记后续受控 Pawn 变更通知。
FDelegateHandle UCommonLocalPlayer::CallAndRegister_OnPlayerPawnSet(FPlayerPawnSetDelegate::FDelegate Delegate)
{
	APlayerController* PC = GetPlayerController(GetWorld());
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (Pawn)
	{
		Delegate.Execute(this, Pawn);
	}

	return OnPlayerPawnSet.Add(Delegate);
}

// 根据本地玩家视口和投影设置生成场景视图投影数据。
bool UCommonLocalPlayer::GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex) const
{
	if (!bIsPlayerViewEnabled)
	{
		return false;
	}

	return Super::GetProjectionData(Viewport, ProjectionData, StereoViewIndex);
}

// 通过当前 UI 策略查找该本地玩家对应的主根布局。
UPrimaryGameLayout* UCommonLocalPlayer::GetRootUILayout() const
{
	if (UGameUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UGameUIManagerSubsystem>())
	{
		if (UGameUIPolicy* Policy = UIManager->GetCurrentUIPolicy())
		{
			return Policy->GetRootLayout(this);
		}
	}

	return nullptr;
}
