// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeaponUserInterface.h"

#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "Weapons/LyraWeaponInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponUserInterface)

struct FGeometry;

// 构造跟踪所属 Pawn 当前武器实例的 UI 基类。
ULyraWeaponUserInterface::ULyraWeaponUserInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 仅转发基类 UMG 构造生命周期；当前不在构造阶段建立额外委托。
void ULyraWeaponUserInterface::NativeConstruct()
{
	Super::NativeConstruct();
}

// 仅转发基类 UMG 销毁生命周期；当前没有额外资源需要释放。
void ULyraWeaponUserInterface::NativeDestruct()
{
	Super::NativeDestruct();
}

// 每帧从 Pawn 装备管理器取得首个有效武器；实例变化时重建 UI 并广播新旧武器。
void ULyraWeaponUserInterface::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (ULyraEquipmentManagerComponent* EquipmentManager = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>())
		{
			if (ULyraWeaponInstance* NewInstance = EquipmentManager->GetFirstInstanceOfType<ULyraWeaponInstance>())
			{
				if (NewInstance != CurrentInstance && NewInstance->GetInstigator() != nullptr)
				{
					ULyraWeaponInstance* OldWeapon = CurrentInstance;
					CurrentInstance = NewInstance;
					RebuildWidgetFromWeapon();
					OnWeaponChanged(OldWeapon, CurrentInstance);
				}
			}
		}
	}
}

// 预留给派生类根据当前武器重建显示内容；基础实现为空。
void ULyraWeaponUserInterface::RebuildWidgetFromWeapon()
{
	
}

