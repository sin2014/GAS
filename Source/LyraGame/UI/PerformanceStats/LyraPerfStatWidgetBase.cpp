// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerfStatWidgetBase.h"

#include "Engine/GameInstance.h"
#include "Performance/LyraPerformanceStatSubsystem.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerfStatWidgetBase)

// 从 Slate 参数保存期望尺寸、Y 轴延迟上限、折线颜色和背景颜色。
void SLyraLatencyGraph::Construct(const FArguments& InArgs)
{
	DesiredSize = InArgs._DesiredSize;
	MaxYAxisOfGraph = InArgs._MaxLatencyToGraph;
	LineColor = InArgs._LineColor;
	BackgroundColor = InArgs._BackgroundColor;
}

// 先绘制背景，再在更高图层绘制采样折线，并返回下一个可用图层。
int32 SLyraLatencyGraph::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = LayerId;

	// 先在当前图层绘制覆盖控件几何范围的背景。
	// Draw the background
	FSlateDrawElement::MakeRotatedBox(
		OutDrawElements,
		MaxLayerId,
		AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("BlackBrush"),
		ESlateDrawEffect::NoPixelSnapping,
		0,
		TOptional<FVector2D>(),
		FSlateDrawElement::RelativeToElement,
		BackgroundColor);

	// 折线必须覆盖在背景之上，因此绘制前提升图层 ID。
	// We need to actually draw the graph plot on top of the background
	// so increment the layer
	MaxLayerId++;

	// 在新图层绘制当前采样窗口的折线。
	// Actually draw the graph plot
	DrawTotalLatency(AllottedGeometry, OutDrawElements, MaxLayerId);
	
	MaxLayerId++;

	return MaxLayerId;
}

// 返回构造参数指定的图表期望尺寸。
FVector2D SLyraLatencyGraph::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DesiredSize;
}

// 把采样窗口按控件宽度均匀映射到 X 轴，将缩放并裁剪后的延迟值映射到反向 Y 轴后绘制折线。
void SLyraLatencyGraph::DrawTotalLatency(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (!GraphData)
	{
		return;
	}
	
	static TArray<FVector2D> Points;
	Points.Reset(GraphData->GetSampleSize() + 1);
	
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	const float LineThickness = 1.0f;
	const double XSlice = WidgetSize.X / static_cast<double>(GraphData->GetSampleSize());
	const double Border = 1.0;

	int32 i = 0;
	
	GraphData->ForEachCurrentSample([&](const double Stat)
	{
		double Y = WidgetSize.Y - FMath::Clamp((Stat * ScaleFactor), 0.0, MaxYAxisOfGraph) / MaxYAxisOfGraph * WidgetSize.Y;
		Y = FMath::Clamp(Y, Border, WidgetSize.Y - Border);

		Points.Emplace(XSlice * double(++i), Y);
	});

	// 按样本顺序生成折线点；Y 值经过缩放、Y 轴上限裁剪并反转为 Slate 向下为正的坐标。
	// Why does this not just draw a straight line?? 
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::NoPixelSnapping,
		LineColor,
		false,
		LineThickness);
}

//////////////////////////////////////////////////////////////////////
// ULyraPerfStatGraph

// 构造裁剪到自身边界的 UMG 性能统计图表包装控件。
ULyraPerfStatGraph::ULyraPerfStatGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetClipping(EWidgetClipping::ClipToBounds);
}

// 创建并保存底层 SLyraLatencyGraph Slate 控件。
TSharedRef<SWidget> ULyraPerfStatGraph::RebuildWidget()
{
	return SAssignNew(SlateLatencyGraph, SLyraLatencyGraph);
}

// 释放 UMG Slate 资源时清空底层图表共享引用。
void ULyraPerfStatGraph::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateLatencyGraph.Reset();
}

// 把折线颜色同步到底层 Slate 图表。
void ULyraPerfStatGraph::SetLineColor(const FColor& InColor)
{
	SlateLatencyGraph->SetLineColor(InColor);
}

// 把 Y 轴最大值同步到底层 Slate 图表。
void ULyraPerfStatGraph::SetMaxYValue(const float InValue)
{
	SlateLatencyGraph->SetMaxYValue(InValue);
}

// 把背景颜色同步到底层 Slate 图表。
void ULyraPerfStatGraph::SetBackgroundColor(const FColor& InValue)
{
	SlateLatencyGraph->SetBackgroundColor(InValue);
}

// 把统计采样缓存指针和缩放倍率传给底层 Slate 图表。
void ULyraPerfStatGraph::UpdateGraphData(const FSampledStatCache* StatData, const float ScaleFactor)
{
	SlateLatencyGraph->UpdateGraphData(StatData, ScaleFactor);
}

//////////////////////////////////////////////////////////////////////
// ULyraPerfStatWidgetBase

// 从缓存的性能统计子系统读取当前统计值；子系统不可用时返回 0。
double ULyraPerfStatWidgetBase::FetchStatValue()
{
	if (ULyraPerformanceStatSubsystem* Subsystem = GetStatSubsystem())
	{
		return CachedStatSubsystem->GetCachedStat(StatToDisplay);
	}
	else
	{
		return 0.0;
	}
}

// 图表和采样缓存均有效时，把当前统计类型的数据及缩放倍率传给图表。
void ULyraPerfStatWidgetBase::UpdateGraphData(const float ScaleFactor)
{
	// 图表控件存在时，把当前统计类型的采样缓存指针和显示缩放倍率传给底层 Slate 控件。
	// When we cache the subsystem also update the graph data pointer if we have a graph widget
	if (PerfStatGraph)
	{
		if (const FSampledStatCache* GraphData = CachedStatSubsystem->GetCachedStatData(StatToDisplay))
		{
			PerfStatGraph->UpdateGraphData(GraphData, ScaleFactor);
		}	
	}
}

// 构造时缓存性能统计子系统，并配置可选图表的线色、Y 轴上限和背景色。
void ULyraPerfStatWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 构造时缓存性能统计子系统，并配置可选图表的颜色和 Y 轴范围。
	// Cache the subsystem on construct, which will also make sure the graph is up to date
	GetStatSubsystem();
	
	if (PerfStatGraph)
	{
		PerfStatGraph->SetLineColor(GraphLineColor);
		PerfStatGraph->SetMaxYValue(GraphMaxYValue);
		PerfStatGraph->SetBackgroundColor(GraphBackgroundColor);
	}
}

// 延迟从当前世界的 GameInstance 获取并缓存性能统计子系统。
ULyraPerformanceStatSubsystem* ULyraPerfStatWidgetBase::GetStatSubsystem()
{
	if (CachedStatSubsystem == nullptr)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				CachedStatSubsystem = GameInstance->GetSubsystem<ULyraPerformanceStatSubsystem>();
			}
		}
	}

	return CachedStatSubsystem;
}
