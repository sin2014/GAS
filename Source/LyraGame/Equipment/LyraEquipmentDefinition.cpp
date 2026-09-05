// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraEquipmentDefinition.h"
#include "LyraEquipmentInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraEquipmentDefinition)

// 将运行时装备实例类型默认设为 ULyraEquipmentInstance。
ULyraEquipmentDefinition::ULyraEquipmentDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = ULyraEquipmentInstance::StaticClass();
}

