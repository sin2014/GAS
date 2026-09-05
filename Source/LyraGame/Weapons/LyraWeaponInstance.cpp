// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeaponInstance.h"

#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputDeviceProperties.h"
#include "Character/LyraHealthComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponInstance)

class UAnimInstance;
struct FGameplayTagContainer;

// 为玩家控制 Pawn 监听死亡开始事件，确保异常死亡流程也能移除循环输入设备属性。
ULyraWeaponInstance::ULyraWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 监听所属 Pawn 的死亡开始事件；死亡流程可能不会正常卸下武器，因此必须主动清理循环输入设备属性。
	// Listen for death of the owning pawn so that any device properties can be removed if we
	// die and can't unequip
	if (APawn* Pawn = GetPawn())
	{
		// 仅玩家控制的 Pawn 在客户端拥有输入设备，AI 等实体无需注册此回调。
		// We only need to do this for player controlled pawns, since AI and others won't have input devices on the client
		if (Pawn->IsPlayerControlled())
		{
			if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(GetPawn()))
			{
				HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
			}
		}
	}
}

// 记录装备时间并为所属平台用户启用配置的循环输入设备属性。
void ULyraWeaponInstance::OnEquipped()
{
	Super::OnEquipped();

	UWorld* World = GetWorld();
	check(World);
	TimeLastEquipped = World->GetTimeSeconds();

	ApplyDeviceProperties();
}

// 卸下武器时移除此前激活的输入设备属性。
void ULyraWeaponInstance::OnUnequipped()
{
	Super::OnUnequipped();

	RemoveDeviceProperties();
}

// 记录当前世界时间为最近开火时间。
void ULyraWeaponInstance::UpdateFiringTime()
{
	UWorld* World = GetWorld();
	check(World);
	TimeLastFired = World->GetTimeSeconds();
}

// 返回距最近装备或开火中较近一次交互的时间。
float ULyraWeaponInstance::GetTimeSinceLastInteractedWith() const
{
	UWorld* World = GetWorld();
	check(World);
	const double WorldTime = World->GetTimeSeconds();

	double Result = WorldTime - TimeLastEquipped;

	if (TimeLastFired > 0.0)
	{
		const double TimeSinceFired = WorldTime - TimeLastFired;
		Result = FMath::Min(Result, TimeSinceFired);
	}

	return Result;
}

// 按装备状态选择动画层集合，并依据外观标签返回最佳动画层。
TSubclassOf<UAnimInstance> ULyraWeaponInstance::PickBestAnimLayer(bool bEquipped, const FGameplayTagContainer& CosmeticTags) const
{
	const FLyraAnimLayerSelectionSet& SetToQuery = (bEquipped ? EquippedAnimSet : UneuippedAnimSet);
	return SetToQuery.SelectBestLayer(CosmeticTags);
}

// 返回所属 Pawn 的平台用户 ID；没有 Pawn 时返回无效用户。
const FPlatformUserId ULyraWeaponInstance::GetOwningUserId() const
{
	if (const APawn* Pawn = GetPawn())
	{
		return Pawn->GetPlatformUserId();
	}
	return PLATFORMUSERID_NONE;
}

// 为有效平台用户循环激活全部武器输入设备属性，并保存句柄供卸下时清理。
void ULyraWeaponInstance::ApplyDeviceProperties()
{
	const FPlatformUserId UserId = GetOwningUserId();

	if (UserId.IsValid())
	{
		if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
		{
			for (TObjectPtr<UInputDeviceProperty>& DeviceProp : ApplicableDeviceProperties)
			{
				FActivateDevicePropertyParams Params = {};
				Params.UserId = UserId;

				// 默认将设备属性应用到平台用户的主输入设备；如需指定其他设备，可设置 Params.DeviceId。
				// By default, the device property will be played on the Platform User's Primary Input Device.
				// If you want to override this and set a specific device, then you can set the DeviceId parameter.
				//Params.DeviceId = <some specific device id>;
				
				// 以循环模式保持属性持续生效，不在单次求值后自动移除；卸下武器时由 OnUnequipped 手动清理。
				// Don't remove this property it was evaluated. We want the properties to be applied as long as we are holding the 
				// weapon, and will remove them manually in OnUnequipped
				Params.bLooping = true;
			
				DevicePropertyHandles.Emplace(InputDeviceSubsystem->ActivateDeviceProperty(DeviceProp, Params));
			}
		}	
	}
}

// 通过保存的句柄批量移除武器设备属性，成功取得子系统后清空句柄列表。
void ULyraWeaponInstance::RemoveDeviceProperties()
{
	const FPlatformUserId UserId = GetOwningUserId();
	
	if (UserId.IsValid() && !DevicePropertyHandles.IsEmpty())
	{
		// 按保存的句柄一次性移除该武器已应用的所有设备属性。
		// Remove any device properties that have been applied
		if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
		{
			InputDeviceSubsystem->RemoveDevicePropertyHandles(DevicePropertyHandles);
			DevicePropertyHandles.Empty();
		}
	}
}

// 所属 Pawn 开始死亡时兜底移除仍在运行的输入设备属性。
void ULyraWeaponInstance::OnDeathStarted(AActor* OwningActor)
{
	// 所属 Pawn 死亡时兜底移除仍可能处于活动状态的设备属性，避免效果残留。
	// Remove any possibly active device properties when we die to make sure that there aren't any lingering around
	RemoveDeviceProperties();
}
