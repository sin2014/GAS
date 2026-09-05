// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraUICameraManagerComponent.h"

#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "LyraPlayerCameraManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUICameraManagerComponent)

class AActor;
class FDebugDisplayInfo;

// 从 PlayerController 的 PlayerCameraManager 中取得 UI 相机组件；Controller、CameraManager 或组件不存在时返回 nullptr。
ULyraUICameraManagerComponent* ULyraUICameraManagerComponent::GetComponent(APlayerController* PC)
{
	if (PC != nullptr)
	{
		if (ALyraPlayerCameraManager* PCCamera = Cast<ALyraPlayerCameraManager>(PC->PlayerCameraManager))
		{
			return PCCamera->GetUICameraComponent();
		}
	}

	return nullptr;
}

// 构造 UI 相机组件，启用初始化回调并将 ViewTarget 与更新标志置为空闲状态。
ULyraUICameraManagerComponent::ULyraUICameraManagerComponent()
{
	bWantsInitializeComponent = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// 非专用服务器注册 showdebug 回调，以显示 UI 相机调试信息。
		// Register "showdebug" hook.
		if (!IsRunningDedicatedServer())
		{
			AHUD::OnShowDebugInfo.AddUObject(this, &ThisClass::OnShowDebugInfo);
		}
	}
}

// 初始化 ULyraUICameraManagerComponent，注册其运行期回调并准备所需状态。
void ULyraUICameraManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

// 保存 UI 指定的 ViewTarget，并调用 PlayerCameraManager 以给定过渡参数切换观察目标。
void ULyraUICameraManagerComponent::SetViewTarget(AActor* InViewTarget, FViewTargetTransitionParams TransitionParams)
{
	TGuardValue<bool> UpdatingViewTargetGuard(bUpdatingViewTarget, true);

	ViewTarget = InViewTarget;
	CastChecked<ALyraPlayerCameraManager>(GetOwner())->SetViewTarget(ViewTarget, TransitionParams);
}

// 仅当 UI ViewTarget 有效且当前未处于递归更新过程中时返回 true。
bool ULyraUICameraManagerComponent::NeedsToUpdateViewTarget() const
{
	return false;
}

// 设置递归保护标志后让 PlayerCameraManager 更新 UI ViewTarget，完成后恢复标志。
void ULyraUICameraManagerComponent::UpdateViewTarget(struct FTViewTarget& OutVT, float DeltaTime)
{
}

// 在 showdebug 输出中显示当前 UI ViewTarget 及是否正在更新镜头。
void ULyraUICameraManagerComponent::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
}
