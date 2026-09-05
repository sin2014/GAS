// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWidgetFactory.h"
#include "Templates/SubclassOf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWidgetFactory)

class UUserWidget;

// 基础工厂不提供匹配规则，返回空控件类供派生工厂实现选择逻辑。
TSubclassOf<UUserWidget> ULyraWidgetFactory::FindWidgetClassForData_Implementation(const UObject* Data) const
{
	return TSubclassOf<UUserWidget>();
}
