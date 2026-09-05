// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/LyraPlayerMappableKeyProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerMappableKeyProfile)

// 先启用 Enhanced Input 按键配置档，再保留项目级装备后初始化入口。
void ULyraPlayerMappableKeyProfile::EquipProfile()
{
	Super::EquipProfile();

	// 可在新按键配置档启用时执行项目特定初始化。
	// Do anything you may want to when a new key profile is equipped
}

// 先停用 Enhanced Input 按键配置档，再保留与装备流程对称的项目级清理入口。
void ULyraPlayerMappableKeyProfile::UnEquipProfile()
{
	Super::UnEquipProfile();
	
	// 可在按键配置档停用时执行对称清理。
	// Do anything you may want to when a new key profile is unequipped
}
