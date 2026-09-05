// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonPlayerController.h"

#include "CommonLocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonPlayerController)

class APawn;

// 创建通用玩家控制器；本地玩家关系通知由生命周期回调维护。
ACommonPlayerController::ACommonPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 控制器收到玩家连接后通知本地玩家控制器已经可用。
void ACommonPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	
	if (UCommonLocalPlayer* LocalPlayer = Cast<UCommonLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerControllerSet.Broadcast(LocalPlayer, this);

		if (PlayerState)
		{
			LocalPlayer->OnPlayerStateSet.Broadcast(LocalPlayer, PlayerState);
		}
	}
}

// Pawn 发生变化后通知本地玩家，并保留父类的占有状态更新。
void ACommonPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	if (UCommonLocalPlayer* LocalPlayer = Cast<UCommonLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerPawnSet.Broadcast(LocalPlayer, InPawn);
	}
}

// 完成 Pawn 占有后通知本地玩家新的受控 Pawn。
void ACommonPlayerController::OnPossess(APawn* APawn)
{
	Super::OnPossess(APawn);
	
	if (UCommonLocalPlayer* LocalPlayer = Cast<UCommonLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerPawnSet.Broadcast(LocalPlayer, APawn);
	}
}

// 解除占有后通知本地玩家当前 Pawn 已清空。
void ACommonPlayerController::OnUnPossess()
{
	Super::OnUnPossess();

	if (UCommonLocalPlayer* LocalPlayer = Cast<UCommonLocalPlayer>(Player))
	{
		LocalPlayer->OnPlayerPawnSet.Broadcast(LocalPlayer, nullptr);
	}
}

// 客户端复制到玩家状态后通知本地玩家更新相关绑定。
void ACommonPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (PlayerState)
	{
		if (UCommonLocalPlayer* LocalPlayer = Cast<UCommonLocalPlayer>(Player))
		{
			LocalPlayer->OnPlayerStateSet.Broadcast(LocalPlayer, PlayerState);
		}
	}
}
