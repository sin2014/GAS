// Copyright Epic Games, Inc. All Rights Reserved.

#include "PocketCaptureSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "PocketCapture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PocketCaptureSubsystem)

class FSubsystemCollectionBase;

// UPocketCaptureSubsystem
//---------------------------------------------------------------------------------

// 构造世界级 Pocket Capture 管理器；Ticker 在 Initialize 时注册。
UPocketCaptureSubsystem::UPocketCaptureSubsystem()
{
}

// 随世界子系统初始化注册核心 Ticker，用于在下一帧撤销临时纹理强制流送标记。
void UPocketCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
}

// 注销 Ticker，逐个停用仍存活的渲染器并清空弱引用槽位。
void UPocketCaptureSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);

	for (int32 RendererIndex = 0; RendererIndex < ThumbnailRenderers.Num(); RendererIndex++)
	{
		if (UPocketCapture* Renderer = ThumbnailRenderers[RendererIndex].Get())
		{
			Renderer->Deinitialize();
		}
	}

	ThumbnailRenderers.Reset();
}

// 创建指定类型的 Pocket Capture，复用渲染器数组中的空槽并以稳定索引初始化其场景捕获组件。
UPocketCapture* UPocketCaptureSubsystem::CreateThumbnailRenderer(TSubclassOf<UPocketCapture> ThumbnailRendererClass)
{
	UPocketCapture* Renderer = NewObject<UPocketCapture>(this, ThumbnailRendererClass);

	int32 RendererEmptyIndex = ThumbnailRenderers.IndexOfByKey(nullptr);
	if (RendererEmptyIndex == INDEX_NONE)
	{
		RendererEmptyIndex = ThumbnailRenderers.Add(Renderer);
	}
	else
	{
		ThumbnailRenderers[RendererEmptyIndex] = Renderer;
	}

	Renderer->Initialize(GetWorld(), RendererEmptyIndex);

	return Renderer;
}

// 从管理数组移除匹配的渲染器并执行反初始化；空指针或不属于本子系统的对象会被忽略。
void UPocketCaptureSubsystem::DestroyThumbnailRenderer(UPocketCapture* ThumbnailRenderer)
{
	if (ThumbnailRenderer)
	{
		const int32 ThumbnailIndex = ThumbnailRenderers.IndexOfByKey(ThumbnailRenderer);
		if (ThumbnailIndex != INDEX_NONE)
		{
			ThumbnailRenderers[ThumbnailIndex] = nullptr;
			ThumbnailRenderer->Deinitialize();
		}
	}
}

// 为本次离屏捕获涉及的组件开启强制 Mip 流送，并登记到下一 Tick 再撤销，保证缩略图使用足够清晰的纹理。
void UPocketCaptureSubsystem::StreamThisFrame(TArray<UPrimitiveComponent*>& PrimitiveComponents)
{
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		PrimitiveComponent->bForceMipStreaming = true;
		StreamedLastFrameButNotNext.Remove(PrimitiveComponent);
	}

	StreamNextFrame.Append(PrimitiveComponents);
}

// 撤销上一帧未再次请求的强制流送标记，再轮换本帧集合；返回 true 使 Ticker 持续运行。
bool UPocketCaptureSubsystem::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_URealTimeThumbnailSubsystem_Tick);

	for (TWeakObjectPtr<UPrimitiveComponent> PrimitiveComponent : StreamedLastFrameButNotNext)
	{
		if (PrimitiveComponent.IsValid())
		{
			PrimitiveComponent->bForceMipStreaming = false;
		}
	}

	StreamedLastFrameButNotNext = MoveTemp(StreamNextFrame);

	return true;
}

