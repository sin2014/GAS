// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerCameraManager.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LyraCameraComponent.h"
#include "LyraUICameraManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerCameraManager)

class FDebugDisplayInfo;

// PlayerCameraManager 创建并查找 UI 相机管理组件时使用的稳定子对象名称。
static FName UICameraComponentName(TEXT("UICamera"));

// 配置项目默认 FOV 与俯仰限制，并创建优先处理 UI 镜头的 UICameraManagerComponent。
ALyraPlayerCameraManager::ALyraPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultFOV = LYRA_CAMERA_DEFAULT_FOV;
	ViewPitchMin = LYRA_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = LYRA_CAMERA_DEFAULT_PITCH_MAX;

	UICamera = CreateDefaultSubobject<ULyraUICameraManagerComponent>(UICameraComponentName);
}

// 返回构造时创建并缓存的 UICameraManagerComponent。
ULyraUICameraManagerComponent* ALyraPlayerCameraManager::GetUICameraComponent() const
{
	return UICamera;
}

// UI 相机有有效目标时让父类更新该目标；否则继续 Lyra 常规 ViewTarget 流程。
void ALyraPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	// UI 相机设置了 ViewTarget 时优先使用它，否则继续普通玩法相机更新。
	// If the UI Camera is looking at something, let it have priority.
	if (UICamera->NeedsToUpdateViewTarget())
	{
		Super::UpdateViewTarget(OutVT, DeltaTime);
		UICamera->UpdateViewTarget(OutVT, DeltaTime);
		return;
	}

	Super::UpdateViewTarget(OutVT, DeltaTime);
}

// 先绘制父类相机调试信息，再追加 UI 相机组件状态。
void ALyraPlayerCameraManager::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("LyraPlayerCameraManager: %s"), *GetNameSafe(this)));

	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	const APawn* Pawn = (PCOwner ? PCOwner->GetPawn() : nullptr);

	if (const ULyraCameraComponent* CameraComponent = ULyraCameraComponent::FindCameraComponent(Pawn))
	{
		CameraComponent->DrawDebug(Canvas);
	}
}

