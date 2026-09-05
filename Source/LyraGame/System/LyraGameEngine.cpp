// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameEngine)

class IEngineLoop;


// 构造 Lyra GameEngine，当前仅沿用 UGameEngine 的对象初始化流程。
ULyraGameEngine::ULyraGameEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 将引擎初始化完整交给 UGameEngine，保留项目级启动扩展入口。
void ULyraGameEngine::Init(IEngineLoop* InEngineLoop)
{
	Super::Init(InEngineLoop);
}

