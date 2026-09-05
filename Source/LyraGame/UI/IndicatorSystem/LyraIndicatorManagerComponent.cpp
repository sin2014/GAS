// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraIndicatorManagerComponent.h"

#include "IndicatorDescriptor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraIndicatorManagerComponent)

// 构造自动注册并自动激活的控制器指示器管理组件。
ULyraIndicatorManagerComponent::ULyraIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRegister = true;
	bAutoActivate = true;
}

// 从指定控制器查找指示器管理组件；控制器为空时返回空指针。
/*static*/ ULyraIndicatorManagerComponent* ULyraIndicatorManagerComponent::GetComponent(AController* Controller)
{
	if (Controller)
	{
		return Controller->FindComponentByClass<ULyraIndicatorManagerComponent>();
	}

	return nullptr;
}

// 把描述对象关联到当前管理器，先广播新增事件，再加入受管列表。
void ULyraIndicatorManagerComponent::AddIndicator(UIndicatorDescriptor* IndicatorDescriptor)
{
	IndicatorDescriptor->SetIndicatorManagerComponent(this);
	OnIndicatorAdded.Broadcast(IndicatorDescriptor);
	Indicators.Add(IndicatorDescriptor);
}

// 验证描述对象属于当前管理器，广播移除事件并从受管列表删除；空指针不处理。
void ULyraIndicatorManagerComponent::RemoveIndicator(UIndicatorDescriptor* IndicatorDescriptor)
{
	if (IndicatorDescriptor)
	{
		ensure(IndicatorDescriptor->GetIndicatorManagerComponent() == this);
	
		OnIndicatorRemoved.Broadcast(IndicatorDescriptor);
		Indicators.Remove(IndicatorDescriptor);
	}
}
