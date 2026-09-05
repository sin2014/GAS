// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraReticleWidgetBase.h"

#include "Inventory/LyraInventoryItemInstance.h"
#include "Weapons/LyraRangedWeaponInstance.h"
#include "Weapons/LyraWeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraReticleWidgetBase)

// 构造可由武器实例初始化的准星 UMG 基类。
ULyraReticleWidgetBase::ULyraReticleWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 保存武器及其背包物品实例，并触发派生控件的武器初始化回调。
void ULyraReticleWidgetBase::InitializeFromWeapon(ULyraWeaponInstance* InWeapon)
{
	WeaponInstance = InWeapon;
	InventoryInstance = nullptr;
	if (WeaponInstance)
	{
		InventoryInstance = Cast<ULyraInventoryItemInstance>(WeaponInstance->GetInstigator());
	}
	OnWeaponInitialized();
}


// 远程武器返回基础散布角与倍率的乘积，其他武器返回 0。
float ULyraReticleWidgetBase::ComputeSpreadAngle() const
{
	if (const ULyraRangedWeaponInstance* RangedWeapon = Cast<const ULyraRangedWeaponInstance>(WeaponInstance))
	{
		const float BaseSpreadAngle = RangedWeapon->GetCalculatedSpreadAngle();
		const float SpreadAngleMultiplier = RangedWeapon->GetCalculatedSpreadAngleMultiplier();
		const float ActualSpreadAngle = BaseSpreadAngle * SpreadAngleMultiplier;

		return ActualSpreadAngle;
	}
	else
	{
		return 0.0f;
	}
}

// 远程武器返回当前首发精准状态，其他武器返回 false。
bool ULyraReticleWidgetBase::HasFirstShotAccuracy() const
{
	if (const ULyraRangedWeaponInstance* RangedWeapon = Cast<const ULyraRangedWeaponInstance>(WeaponInstance))
	{
		return RangedWeapon->HasFirstShotAccuracy();
	}
	else
	{
		return false;
	}
}

// 把远距离散布圆锥边缘点投影到屏幕，返回其到视口中心的像素距离；缺少相机或投影失败时返回 0。
float ULyraReticleWidgetBase::ComputeMaxScreenspaceSpreadRadius() const
{
	const float LongShotDistance = 10000.f;

	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->PlayerCameraManager)
	{
		// 将武器散布视为圆锥：在远距离构造锥边缘点并投影到屏幕，其与屏幕中心的距离即准星散布半径。
		// 该近似以相机为锥顶，没有完全补偿相机与枪口之间的空间偏移。
		// A weapon's spread can be thought of as a cone shape. To find the screenspace spread for reticle visualization,
		// we create a line on the edge of the cone at a long distance. The end of that point is on the edge of the cone's circle.
		// We then project it back onto the screen. Its distance from screen center is the spread radius.

		// This isn't perfect, due to there being some distance between the camera location and the gun muzzle.

		const float SpreadRadiusRads = FMath::DegreesToRadians(ComputeSpreadAngle() * 0.5f);
		const float SpreadRadiusAtDistance = FMath::Tan(SpreadRadiusRads) * LongShotDistance;

		FVector CamPos;
		FRotator CamOrient;
		PC->PlayerCameraManager->GetCameraViewPoint(CamPos, CamOrient);

		FVector CamForwDir = CamOrient.RotateVector(FVector::ForwardVector);
		FVector CamUpDir   = CamOrient.RotateVector(FVector::UpVector);

		FVector OffsetTargetAtDistance = CamPos + (CamForwDir * LongShotDistance) + (CamUpDir * SpreadRadiusAtDistance);

		FVector2D OffsetTargetInScreenspace;

		if (PC->ProjectWorldLocationToScreen(OffsetTargetAtDistance, OffsetTargetInScreenspace, true))
		{
			int32 ViewportSizeX(0), ViewportSizeY(0);
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			const FVector2D ScreenSpaceCenter(FVector::FReal(ViewportSizeX) * 0.5f, FVector::FReal(ViewportSizeY) * 0.5f);

			return (OffsetTargetInScreenspace - ScreenSpaceCenter).Length();
		}
	}
	
	return 0.0f;
}
