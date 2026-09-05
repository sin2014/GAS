// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "Widgets/SLeafWidget.h"

#include "LyraPerfStatWidgetBase.generated.h"

enum class ELyraDisplayablePerformanceStat : uint8;

class ULyraPerformanceStatSubsystem;
class UObject;
struct FFrame;
class FSampledStatCache;

class SLyraLatencyGraph : public SLeafWidget
{
public:
	/** SLyraLatencyGraph 的 Slate 构造参数。 */
	/** Begin the arguments for this slate widget */
	SLATE_BEGIN_ARGS(SLyraLatencyGraph)
		: _DesiredSize(150, 50),
		_MaxLatencyToGraph(33.0),
		_LineColor(255, 255, 255, 255),
		_BackgroundColor(0, 0, 0, 128)
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

	SLATE_ARGUMENT(FVector2D, DesiredSize)
	SLATE_ARGUMENT(double, MaxLatencyToGraph)
	SLATE_ARGUMENT(FColor, LineColor)
	SLATE_ARGUMENT(FColor, BackgroundColor)
	SLATE_END_ARGS()

	/** 从 Slate 参数缓存尺寸、Y 轴上限和颜色。 */
	/** Contruct function needed for every Widget */
	void Construct(const FArguments& InArgs);

	/** 依次绘制背景和统计折线，并返回使用后的最高图层 ID。 */
	/** Called with the elements to be drawn */
	virtual int32 OnPaint(const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual bool ComputeVolatility() const override { return true; }
	
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	inline void SetLineColor(const FColor& InColor)
	{
		LineColor = InColor;	
	}

	inline void SetMaxYValue(const double InValue)
	{
		MaxYAxisOfGraph = InValue;
	}

	inline void SetBackgroundColor(const FColor& InColor)
	{
		BackgroundColor = InColor;
	}

	inline void UpdateGraphData(const FSampledStatCache* StatData, const float InScaleFactor)
	{
		GraphData = StatData;
		ScaleFactor = InScaleFactor;
	}
	
private:
	
	void DrawTotalLatency(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	// 图表期望布局尺寸，不随采样数量自动变化。
	/**
	 * The size of the graph to draw
	 */
	FVector2D DesiredSize = { 150.0, 50.0 };

	// Y 轴最大显示值；超出此值的样本会裁剪到图表顶部。
	/**
	 * Max Y value of the graph. The values drawn will be clamped to this
	 */
	double MaxYAxisOfGraph = 33.0;

	float ScaleFactor = 1.0f;

	// 统计折线颜色。
	/**
	 * Color of the line to draw on the graph
	 */
	FColor LineColor = FColor(255, 255, 255, 255);

	// 图表背景颜色和透明度。
	/**
	 * The background color to draw when drawing the graph
	 */
	FColor BackgroundColor = FColor(0, 0, 0, 128);

	// 指向性能子系统拥有的采样缓存；控件不取得所有权，绘制时只读。
	/**
	 * The cache of data that this graph widget needs to draw
	 */
	const FSampledStatCache* GraphData = nullptr;
};

// UMG 包装控件，负责创建和释放实际绘制历史曲线的 SLyraLatencyGraph。
/**
 * ULyraPerfStatGraph
 *
 * Base class for a widget that displays the graph of a stat over time.
 */
UCLASS(meta = (DisableNativeTick))
class ULyraPerfStatGraph : public UUserWidget
{
	GENERATED_BODY()

public:
	ULyraPerfStatGraph(const FObjectInitializer& ObjectInitializer);
	
	void SetLineColor(const FColor& InColor);
	
	void SetMaxYValue(const float InValue);
	
	void SetBackgroundColor(const FColor& InValue);

	void UpdateGraphData(const FSampledStatCache* StatData, const float ScaleFactor);
	
protected:
	// Begin UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End UWidget interface

	// 实际绘图的 Slate 控件，在 RebuildWidget 中创建，在 ReleaseSlateResources 中释放。
	// The actual slate widget which will draw the graph. Created in RebuildWidget and
	// destroyed in ReleaseSlateResources.
	TSharedPtr<SLyraLatencyGraph> SlateLatencyGraph;
};

// 单个性能统计项的基础控件，可读取最新值并选择性地把历史采样绑定到图表控件。
/**
 * ULyraPerfStatWidgetBase
 *
 * Base class for a widget that displays a single stat, e.g., FPS, ping, etc...
 */
 UCLASS(Abstract)
class ULyraPerfStatWidgetBase : public UCommonUserWidget
{
public:
	GENERATED_BODY()

public:
	// 返回此控件配置要显示的性能统计类型。
	// Returns the stat this widget is supposed to display
	UFUNCTION(BlueprintPure)
	ELyraDisplayablePerformanceStat GetStatToDisplay() const
	{
		return StatToDisplay;
	}

	// 从性能统计子系统读取该统计项最近一次未缩放的原始值。
	// Polls for the value of this stat (unscaled)
	UFUNCTION(BlueprintPure)
	double FetchStatValue();

	UFUNCTION(BlueprintCallable)
	void UpdateGraphData(const float ScaleFactor = 1.0f);

protected:

 	virtual void NativeConstruct() override;

 	ULyraPerformanceStatSubsystem* GetStatSubsystem();

	// 可选的历史曲线控件；存在时绑定与 StatToDisplay 对应的采样缓存。
	/**
	 * An optional stat graph widget to display this stat's value over time.
	 */
	UPROPERTY(BlueprintReadWrite, meta=(BindWidget, OptionalWidget=true))
 	TObjectPtr<ULyraPerfStatGraph> PerfStatGraph;
 	
	// 首次访问后缓存的游戏实例性能统计子系统。
	// Cached subsystem pointer
	UPROPERTY(Transient)
	TObjectPtr<ULyraPerformanceStatSubsystem> CachedStatSubsystem;

 	UPROPERTY(EditAnywhere, Category = Display)
 	FColor GraphLineColor = FColor(255, 255, 255, 255);
	
 	UPROPERTY(EditAnywhere, Category = Display)
 	FColor GraphBackgroundColor = FColor(0, 0, 0, 128);

	// 图表 Y 轴上限，样本绘制时会裁剪到此值。
	/**
	  * The max value of the Y axis to clamp the graph to. 
	  */
 	UPROPERTY(EditAnywhere, Category = Display)
 	double GraphMaxYValue = 33.0;

	// 此控件读取和显示的性能统计类型。
	// The stat to display
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Display)
	ELyraDisplayablePerformanceStat StatToDisplay;
 };
