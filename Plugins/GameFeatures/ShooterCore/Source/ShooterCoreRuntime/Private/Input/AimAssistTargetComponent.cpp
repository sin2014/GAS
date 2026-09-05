// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/AimAssistTargetComponent.h"

#include "Input/IAimAssistTargetInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimAssistTargetComponent)

// 输出组件配置的目标选项；未显式指定 ShapeComponent 时，从 Owner 上查找首个形状组件作为命中体积。
void UAimAssistTargetComponent::GatherTargetOptions(FAimAssistTargetOptions& OutTargetData)
{
	if (!TargetData.TargetShapeComponent.IsValid())
	{
		if (AActor* Owner = GetOwner())
		{
			TargetData.TargetShapeComponent = Owner->FindComponentByClass<UShapeComponent>();	
		}
	}
	OutTargetData = TargetData;
}

