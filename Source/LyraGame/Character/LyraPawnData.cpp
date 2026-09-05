// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPawnData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPawnData)

// 构造不可变 PawnData 资产，不创建运行时对象，具体 Pawn、技能、输入和相机配置由资产属性提供。
ULyraPawnData::ULyraPawnData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PawnClass = nullptr;
	InputConfig = nullptr;
	DefaultCameraMode = nullptr;
}

