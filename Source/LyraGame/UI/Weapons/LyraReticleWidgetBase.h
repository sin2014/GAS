// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"

#include "LyraReticleWidgetBase.generated.h"

class ULyraInventoryItemInstance;
class ULyraWeaponInstance;
class UObject;
struct FFrame;

UCLASS(Abstract)
class ULyraReticleWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	ULyraReticleWidgetBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintImplementableEvent)
	void OnWeaponInitialized();

	UFUNCTION(BlueprintCallable)
	void InitializeFromWeapon(ULyraWeaponInstance* InWeapon);

	/** 返回当前武器乘入姿态倍率后的完整散布锥角，单位为度。 */
	/** Returns the current weapon's diametrical spread angle, in degrees */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float ComputeSpreadAngle() const;

	/** 将当前散布锥投影到屏幕，返回准星中心到锥边缘的最大像素半径。 */
	/** Returns the current weapon's maximum spread radius in screenspace units (pixels) */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float ComputeMaxScreenspaceSpreadRadius() const;

	// 当前远程武器允许首发精度且基础散布和状态倍率均处于最小值时返回 true。
	/**
	 * Returns true if the current weapon is at 'first shot accuracy'
	 * (the weapon allows it and it is at min spread)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool HasFirstShotAccuracy() const;

protected:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULyraWeaponInstance> WeaponInstance;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<ULyraInventoryItemInstance> InventoryInstance;
};
