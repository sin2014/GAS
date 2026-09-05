// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AsyncMixin.h"
#include "Blueprint/UserWidgetPool.h"
#include "Widgets/SPanel.h"

class FActiveTimerHandle;
class FArrangedChildren;
class FChildren;
class FPaintArgs;
class FReferenceCollector;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class UIndicatorDescriptor;
class ULyraIndicatorManagerComponent;
struct FSlateBrush;

class SActorCanvas : public SPanel, public FAsyncMixin, public FGCObject
{
public:
	/** Actor Canvas 专用槽位，缓存投影位置、深度、优先级、可见性和上一帧钳制状态。 */
	/** ActorCanvas-specific slot class */
	class FSlot : public TSlotBase<FSlot>
	{
	public: 

		FSlot(UIndicatorDescriptor* InIndicator)
			: TSlotBase<FSlot>()
			, Indicator(InIndicator)
			, ScreenPosition(FVector2D::ZeroVector)
			, Depth(0)
			, Priority(0.f)
			, bIsIndicatorVisible(true)
			, bInFrontOfCamera(true)
			, bHasValidScreenPosition(false)
			, bDirty(true)
			, bWasIndicatorClamped(false)
			, bWasIndicatorClampedStatusChanged(false)
		{
		}

		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
		SLATE_SLOT_END_ARGS()
		using TSlotBase<FSlot>::Construct;

		bool GetIsIndicatorVisible() const { return bIsIndicatorVisible; }
		void SetIsIndicatorVisible(bool bVisible)
		{
			if (bIsIndicatorVisible != bVisible)
			{
				bIsIndicatorVisible = bVisible;
				bDirty = true;
			}

			RefreshVisibility();
		}

		FVector2D GetScreenPosition() const { return ScreenPosition; }
		void SetScreenPosition(FVector2D InScreenPosition)
		{
			if (ScreenPosition != InScreenPosition)
			{
				ScreenPosition = InScreenPosition;
				bDirty = true;
			}
		}

		double GetDepth() const { return Depth; }
		void SetDepth(double InDepth)
		{
			if (Depth != InDepth)
			{
				Depth = InDepth;
				bDirty = true;
			}
		}

		int32 GetPriority() const { return Priority; }
		void SetPriority(int32 InPriority)
		{
			if (Priority != InPriority)
			{
				Priority = InPriority;
				bDirty = true;
			}
		}

		bool GetInFrontOfCamera() const { return bInFrontOfCamera; }
		void SetInFrontOfCamera(bool bInFront)
		{
			if (bInFrontOfCamera != bInFront)
			{
				bInFrontOfCamera = bInFront;
				bDirty = true;
			}

			RefreshVisibility();
		}

		bool HasValidScreenPosition() const { return bHasValidScreenPosition; }
		void SetHasValidScreenPosition(bool bValidScreenPosition)
		{
			if (bHasValidScreenPosition != bValidScreenPosition)
			{
				bHasValidScreenPosition = bValidScreenPosition;
				bDirty = true;
			}

			RefreshVisibility();
		}

		bool bIsDirty() const { return bDirty; }

		void ClearDirtyFlag()
		{
			bDirty = false;
		}

		bool WasIndicatorClamped() const { return bWasIndicatorClamped; }
		void SetWasIndicatorClamped(bool bWasClamped) const
		{
			if (bWasClamped != bWasIndicatorClamped)
			{
				bWasIndicatorClamped = bWasClamped;
				bWasIndicatorClampedStatusChanged = true;
			}
		}

		bool WasIndicatorClampedStatusChanged() const { return bWasIndicatorClampedStatusChanged; }
		void ClearIndicatorClampedStatusChangedFlag()
		{
			bWasIndicatorClampedStatusChanged = false;
		}

	private:
		void RefreshVisibility()
		{
			const bool bIsVisible = bIsIndicatorVisible && bHasValidScreenPosition;
			GetWidget()->SetVisibility(bIsVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
		}

		// 由 SActorCanvas::AddReferencedObjects 向 GC 显式报告并保持存活。
		//Kept Alive by SActorCanvas::AddReferencedObjects
		UIndicatorDescriptor* Indicator;
		FVector2D ScreenPosition;
		double Depth;
		int32 Priority;

		uint8 bIsIndicatorVisible : 1;
		uint8 bInFrontOfCamera : 1;
		uint8 bHasValidScreenPosition : 1;
		uint8 bDirty : 1;
		
		// 在 const 排列阶段缓存上一帧是否发生屏幕边缘钳制，并延迟到更新阶段报告状态变化。
		/** 
		 * Cached & frame-deferred value of whether the indicator was visually screen clamped last frame or not; 
		 * Semi-hacky mutable implementation as it is cached during a const paint operation
		 */
		mutable uint8 bWasIndicatorClamped : 1;
		mutable uint8 bWasIndicatorClampedStatusChanged : 1;

		friend class SActorCanvas;
	};

	/** 屏幕边缘方向箭头使用的独立槽位类型。 */
	/** ActorCanvas-specific slot class */
	class FArrowSlot : public TSlotBase<FArrowSlot>
	{
	};

	/** SActorCanvas 的 Slate 构造参数。 */
	/** Begin the arguments for this slate widget */
	SLATE_BEGIN_ARGS(SActorCanvas) {
		_Visibility = EVisibility::HitTestInvisible;
	}

		/** 声明此面板接受 FSlot 子项。 */
		/** Indicates that we have a slot that this widget supports */
		SLATE_SLOT_ARGUMENT(SActorCanvas::FSlot, Slots)
	
	/** Slate 参数声明结束标记。 */
	/** This always goes at the end */
	SLATE_END_ARGS()

	SActorCanvas()
		: CanvasChildren(this)
		, ArrowChildren(this)
		, AllChildren(this)
	{
		AllChildren.AddChildren(CanvasChildren);
		AllChildren.AddChildren(ArrowChildren);
	}

	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx, const FSlateBrush* ActorCanvasArrowBrush);

	// SWidget Interface
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual FChildren* GetChildren() override { return &AllChildren; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	// End SWidget

	void SetDrawElementsInOrder(bool bInDrawElementsInOrder) { bDrawElementsInOrder = bInDrawElementsInOrder; }

	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	
private:
	void OnIndicatorAdded(UIndicatorDescriptor* Indicator);
	void OnIndicatorRemoved(UIndicatorDescriptor* Indicator);

	void AddIndicatorForEntry(UIndicatorDescriptor* Indicator);
	void RemoveIndicatorForEntry(UIndicatorDescriptor* Indicator);

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddActorSlot(UIndicatorDescriptor* Indicator);
	int32 RemoveActorSlot(const TSharedRef<SWidget>& SlotWidget);

	void SetShowAnyIndicators(bool bIndicators);
	EActiveTimerReturnType UpdateCanvas(double InCurrentTime, float InDeltaTime);

	/** 根据指示器水平和垂直对齐方式计算控件尺寸、锚点偏移及屏幕边缘钳制所需留白。 */
	/** Helper function for calculating the offset */
	void GetOffsetAndSize(const UIndicatorDescriptor* Indicator,
		FVector2D& OutSize, 
		FVector2D& OutOffset,
		FVector2D& OutPaddingMin,
		FVector2D& OutPaddingMax) const;

	void UpdateActiveTimer();

private:
	TArray<TObjectPtr<UIndicatorDescriptor>> AllIndicators;
	TArray<UIndicatorDescriptor*> InactiveIndicators;
	
	FLocalPlayerContext LocalPlayerContext;
	TWeakObjectPtr<ULyraIndicatorManagerComponent> IndicatorComponentPtr;

	/** 世界指示器内容槽位。 */
	/** All the slots in this canvas */
	TPanelChildren<FSlot> CanvasChildren;
	mutable TPanelChildren<FArrowSlot> ArrowChildren;
	FCombinedChildren AllChildren;

	FUserWidgetPool IndicatorPool;

	const FSlateBrush* ActorCanvasArrowBrush = nullptr;

	mutable int32 NextArrowIndex = 0;
	mutable int32 ArrowIndexLastUpdate = 0;

	/** 是否严格按加入顺序分配绘制图层；启用后会破坏批处理并增加 Draw Call。 */
	/** Whether to draw elements in the order they were added to canvas. Note: Enabling this will disable batching and will cause a greater number of drawcalls */
	bool bDrawElementsInOrder = false;

	bool bShowAnyIndicators = false;

	mutable TOptional<FGeometry> OptionalPaintGeometry;

	TSharedPtr<FActiveTimerHandle> TickHandle;
};
