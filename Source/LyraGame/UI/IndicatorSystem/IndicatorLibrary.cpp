// Copyright Epic Games, Inc. All Rights Reserved.

#include "IndicatorLibrary.h"

#include "LyraIndicatorManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IndicatorLibrary)

class AController;

// 构造指示器蓝图库对象。
UIndicatorLibrary::UIndicatorLibrary()
{
}

// 返回指定控制器上的 Lyra 指示器管理组件；控制器为空或未挂载时返回空指针。
ULyraIndicatorManagerComponent* UIndicatorLibrary::GetIndicatorManagerComponent(AController* Controller)
{
	return ULyraIndicatorManagerComponent::GetComponent(Controller);
}

