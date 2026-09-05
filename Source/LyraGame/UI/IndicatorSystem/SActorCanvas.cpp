// Copyright Epic Games, Inc. All Rights Reserved.

#include "SActorCanvas.h"

#include "Engine/GameViewportClient.h"
#include "IActorIndicatorWidget.h"
#include "Layout/ArrangedChildren.h"
#include "LyraIndicatorManagerComponent.h"
#include "SceneView.h"
#include "UI/IndicatorSystem/IndicatorDescriptor.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"

class FSlateRect;

namespace EArrowDirection
{
	enum Type
	{
		Left,
		Top,
		Right,
		Bottom,
		MAX
	};
}

// 屏幕四条边对应的方向箭头旋转角。
// Angles for the direction of the arrow to display
const float ArrowRotations[EArrowDirection::MAX] =
{
	270.0f,
	0.0f,
	90.0f,
	180.0f
};

// 屏幕四条边对应的箭头外移方向向量。
// Offsets for the each direction that the arrow can point
const FVector2D ArrowOffsets[EArrowDirection::MAX] =
{
	FVector2D(-1.0f, 0.0f),
	FVector2D(0.0f, -1.0f),
	FVector2D(1.0f, 0.0f),
	FVector2D(0.0f, 1.0f)
};


class SActorCanvasArrowWidget : public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SActorCanvasArrowWidget)
	{}
	/** Slate 参数声明结束标记。 */
	/** always goes at the end */
	SLATE_END_ARGS()

	/** 初始化箭头旋转角和画刷指针。 */
	/** Ctor */
	SActorCanvasArrowWidget()
	: Rotation(0.0f)
	, Arrow(nullptr)
	{

	}

	/** 缓存箭头画刷并关闭每帧 Tick。 */
	/** Every widget needs one of these */
	void Construct(const FArguments& InArgs, const FSlateBrush* ActorCanvasArrowBrush)
	{
		Arrow = ActorCanvasArrowBrush;
		SetCanTick(false);
	}

	// 使用当前旋转角在分配几何中绘制箭头画刷，并返回占用后的最大图层。
	virtual int32 OnPaint(const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		int32 MaxLayerId = LayerId;

		if (Arrow)
		{
			const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
			const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
			const FColor FinalColorAndOpacity = (InWidgetStyle.GetColorAndOpacityTint() * Arrow->GetTint(InWidgetStyle)).ToFColor(true);

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				MaxLayerId++,
				AllottedGeometry.ToPaintGeometry(Arrow->ImageSize, FSlateLayoutTransform()),
				Arrow,
				DrawEffects,
				FMath::DegreesToRadians(GetRotation()),
				TOptional<FVector2D>(),
				FSlateDrawElement::RelativeToElement,
				FinalColorAndOpacity
			);
		}

		return MaxLayerId;
	}

	// 将箭头旋转角保存为 360 度周期内的余数。
	FORCEINLINE void SetRotation(float InRotation)
	{
		Rotation = FMath::Fmod(InRotation, 360.0f);
	}

	// 返回当前箭头旋转角。
	FORCEINLINE float GetRotation() const
	{
		return Rotation;
	}

	// 返回箭头画刷尺寸；未配置画刷时返回零尺寸。
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		if (Arrow)
		{
			return Arrow->ImageSize;
		}
		else
		{
			return FVector2D::ZeroVector;
		}
	}

private:
	float Rotation;
	
	const FSlateBrush* Arrow;
};

// 初始化本地玩家上下文和控件池，预创建十个边缘箭头，并按指示器状态启动活动计时器。
void SActorCanvas::Construct(const FArguments& InArgs, const FLocalPlayerContext& InLocalPlayerContext, const FSlateBrush* InActorCanvasArrowBrush)
{
	LocalPlayerContext = InLocalPlayerContext;
	ActorCanvasArrowBrush = InActorCanvasArrowBrush;

	IndicatorPool.SetWorld(LocalPlayerContext.GetWorld());

	SetCanTick(false);
	SetVisibility(EVisibility::SelfHitTestInvisible);

	// 预创建 10 个方向箭头槽位，排列时按需复用并隐藏未使用项。
	// Create 10 arrows for starters
	for (int32 i = 0; i < 10; ++i)
	{
		TSharedRef<SActorCanvasArrowWidget> ArrowWidget = SNew(SActorCanvasArrowWidget, ActorCanvasArrowBrush);
		ArrowWidget->SetVisibility(EVisibility::Collapsed);
		
		ArrowChildren.AddSlot(MoveTemp(
			FArrowSlot::FSlotArguments(MakeUnique<FArrowSlot>())
			[
				ArrowWidget
			]
		));
	}

	UpdateActiveTimer();
}

// 活动计时器中绑定管理组件、异步维护描述控件、更新投影可见性和排序状态；无指示器时停止计时。
EActiveTimerReturnType SActorCanvas::UpdateCanvas(double InCurrentTime, float InDeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SActorCanvas_UpdateCanvas);

	if (!OptionalPaintGeometry.IsSet())
	{
		return EActiveTimerReturnType::Continue;
	}

	// 取得本地玩家以及挂在其控制器上的指示器管理组件。
	// Grab the local player
	ULocalPlayer* LocalPlayer = LocalPlayerContext.GetLocalPlayer();
	ULyraIndicatorManagerComponent* IndicatorComponent = IndicatorComponentPtr.Get();
	if (IndicatorComponent == nullptr)
	{
		IndicatorComponent = ULyraIndicatorManagerComponent::GetComponent(LocalPlayerContext.GetPlayerController());
		if (IndicatorComponent)
		{
			// 地图切换后 World 可能变化，重新设置 UUserWidget 池的 World 并绑定新管理组件事件。
			// World may have changed
			IndicatorPool.SetWorld(LocalPlayerContext.GetWorld());

			IndicatorComponentPtr = IndicatorComponent;
			IndicatorComponent->OnIndicatorAdded.AddSP(this, &SActorCanvas::OnIndicatorAdded);
			IndicatorComponent->OnIndicatorRemoved.AddSP(this, &SActorCanvas::OnIndicatorRemoved);
			for (UIndicatorDescriptor* Indicator : IndicatorComponent->GetIndicators())
			{
				OnIndicatorAdded(Indicator);
			}
		}
		else
		{
			// TODO：管理组件不存在时应立即隐藏现有指示器；当前仅继续等待后续 Tick。
			//TODO HIDE EVERYTHING
			return EActiveTimerReturnType::Continue;
		}
	}

	// 没有本地玩家就无法取得视图投影数据，隐藏全部指示器。
	//Make sure we have a player. If we don't, we can't project anything
	if (LocalPlayer)
	{
		const FGeometry PaintGeometry = OptionalPaintGeometry.GetValue();

		FSceneViewProjectionData ProjectionData;
		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, /*out*/ ProjectionData))
		{
			SetShowAnyIndicators(true);

			bool IndicatorsChanged = false;

			for (int32 ChildIndex = 0; ChildIndex < CanvasChildren.Num(); ++ChildIndex)
			{
				SActorCanvas::FSlot& CurChild = CanvasChildren[ChildIndex];
				UIndicatorDescriptor* Indicator = CurChild.Indicator;

				// 描述对象允许自动移除且其场景组件已失效时，回收控件并删除槽位。
				// If the slot content is invalid and we have permission to remove it
				if (Indicator->CanAutomaticallyRemove())
				{
					IndicatorsChanged = true;

					RemoveIndicatorForEntry(Indicator);
					// 删除当前槽位后递减索引，避免跳过紧随其后的元素。
					// Decrement the current index to account for the removal 
					--ChildIndex;
					continue;
				}

				CurChild.SetIsIndicatorVisible(Indicator->GetIsVisible());

				if (!CurChild.GetIsIndicatorVisible())
				{
					IndicatorsChanged |= CurChild.bIsDirty();
					CurChild.ClearDirtyFlag();
					continue;
				}

				// 上一排列帧的钳制状态发生变化时标记需要重绘，并清除延迟状态标记。
				// If the indicator changed clamp status between updates, alert the indicator and mark the indicators as changed
				if (CurChild.WasIndicatorClampedStatusChanged())
				{
					//Indicator->OnIndicatorClampedStatusChanged(CurChild.WasIndicatorClamped());
					CurChild.ClearIndicatorClampedStatusChangedFlag();
					IndicatorsChanged = true;
				}

				FVector ScreenPositionWithDepth;

				FIndicatorProjection Projector;
				const bool Success = Projector.Project(*Indicator, ProjectionData, PaintGeometry.Size, OUT ScreenPositionWithDepth);

				if (!Success)
				{
					CurChild.SetHasValidScreenPosition(false);
					CurChild.SetInFrontOfCamera(false);

					IndicatorsChanged |= CurChild.bIsDirty();
					CurChild.ClearDirtyFlag();
					continue;
				}

				CurChild.SetInFrontOfCamera(Success);
				CurChild.SetHasValidScreenPosition(CurChild.GetInFrontOfCamera() || Indicator->GetClampToScreen());

				if (CurChild.HasValidScreenPosition())
				{
					// 只有指示器可显示或允许屏幕边缘钳制时才更新缓存的屏幕位置和深度。
					// Only dirty the screen position if we can actually show this indicator.
					CurChild.SetScreenPosition(FVector2D(ScreenPositionWithDepth));
					CurChild.SetDepth(ScreenPositionWithDepth.X);
				}

				CurChild.SetPriority(Indicator->GetPriority());

				IndicatorsChanged |= CurChild.bIsDirty();
				CurChild.ClearDirtyFlag();
			}

			if (IndicatorsChanged)
			{
				Invalidate(EInvalidateWidget::Paint);
			}
		}
		else
		{
			SetShowAnyIndicators(false);
		}
	}
	else
	{
		SetShowAnyIndicators(false);
	}

	if (AllIndicators.Num() == 0)
	{
		TickHandle.Reset();
		return EActiveTimerReturnType::Stop;
	}
	else
	{
		return EActiveTimerReturnType::Continue;
	}
}

// 更新画布总显示状态；关闭时立即折叠全部普通指示器和箭头子控件。
void SActorCanvas::SetShowAnyIndicators(bool bIndicators)
{
	if (bShowAnyIndicators != bIndicators)
	{
		bShowAnyIndicators = bIndicators;

		if (!bShowAnyIndicators)
		{
			for (int32 ChildIndex = 0; ChildIndex < AllChildren.Num(); ChildIndex++)
			{
				AllChildren.GetChildAt(ChildIndex)->SetVisibility(EVisibility::Collapsed);
			}
		}
	}
}

// 按优先级与深度稳定排序指示器，计算对齐和屏幕边缘裁剪位置，并从箭头池排列所需方向箭头。
void SActorCanvas::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SActorCanvas_OnArrangeChildren);

	NextArrowIndex = 0;

	// 仅在更新阶段确认存在有效投影数据后才排列指示器。
	//Make sure we have a player. If we don't, we can't project anything
	if (bShowAnyIndicators)
	{
		const FVector2D ArrowWidgetSize = ActorCanvasArrowBrush->GetImageSize();
		const FIntPoint FixedPadding = FIntPoint(10.0f, 10.0f) + FIntPoint(ArrowWidgetSize.X, ArrowWidgetSize.Y);
		const FVector Center = FVector(AllottedGeometry.Size * 0.5f, 0.0f);

		// 稳定排序指示器：先按 Priority 分组，同组内再按缓存深度排序。
		// Sort the children
		TArray<const SActorCanvas::FSlot*> SortedSlots;
		for (int32 ChildIndex = 0; ChildIndex < CanvasChildren.Num(); ++ChildIndex)
		{
			SortedSlots.Add(&CanvasChildren[ChildIndex]);
		}

		SortedSlots.StableSort([](const SActorCanvas::FSlot& A, const SActorCanvas::FSlot& B)
		{
			return A.GetPriority() == B.GetPriority() ? A.GetDepth() > B.GetDepth() : A.GetPriority() < B.GetPriority();
		});

		// 按排序结果依次计算每个指示器及其可选边缘箭头的几何。
		// Go through all the sorted children
		for (int32 ChildIndex = 0; ChildIndex < SortedSlots.Num(); ++ChildIndex)
		{
			// 取得当前排序后的指示器槽位。
			//grab a child
			const SActorCanvas::FSlot& CurChild = *SortedSlots[ChildIndex];
			const UIndicatorDescriptor* Indicator = CurChild.Indicator;

			// Slate 可见性过滤不接受此子控件时跳过，并清除其钳制状态。
			// Skip this indicator if it's invalid or has an invalid world position
			if (!ArrangedChildren.Accepts(CurChild.GetWidget()->GetVisibility()))
			{
				CurChild.SetWasIndicatorClamped(false);
				continue;
			}

			FVector2D ScreenPosition = CurChild.GetScreenPosition();
			const bool bInFrontOfCamera = CurChild.GetInFrontOfCamera();

			// 是否执行屏幕边缘钳制由描述对象决定。
			// Don't bother if we can't project the position and the indicator doesn't want to be clamped
			const bool bShouldClamp = Indicator->GetClampToScreen();

			// 根据内容期望尺寸和锚点对齐计算槽位尺寸、偏移及边缘留白。
			//get the offset and final size of the slot
			FVector2D SlotSize, SlotOffset, SlotPaddingMin, SlotPaddingMax;
			GetOffsetAndSize(Indicator, SlotSize, SlotOffset, SlotPaddingMin, SlotPaddingMax);

			bool bWasIndicatorClamped = false;

			// 仅要求钳制时计算内缩屏幕矩形、边缘交点和方向箭头。
			// If we don't have to clamp this thing, we can skip a lot of work
			if (bShouldClamp)
			{
				// 记录最终钳制到的屏幕边缘方向。
				//figure out if we clamped to any edge of the screen
				EArrowDirection::Type ClampDir = EArrowDirection::MAX;

				// 按控件对齐留白、固定边距和箭头尺寸构造可用的内层钳制矩形。
				// Determine the size of inner screen rect to clamp within
				const FIntPoint RectMin = FIntPoint(SlotPaddingMin.X, SlotPaddingMin.Y) + FixedPadding;
				const FIntPoint RectMax = FIntPoint(AllottedGeometry.Size.X - SlotPaddingMax.X, AllottedGeometry.Size.Y - SlotPaddingMax.Y) - FixedPadding;
				const FIntRect ClampRect(RectMin, RectMax);

				// 投影点位于矩形外时，求屏幕中心到投影点线段与四边的交点作为钳制位置。
				// Make sure the screen position is within the clamp rect
				if (!ClampRect.Contains(FIntPoint(ScreenPosition.X, ScreenPosition.Y)))
				{
					const FPlane Planes[] =
					{
						FPlane(FVector(1.0f, 0.0f, 0.0f), ClampRect.Min.X),	// Left
						FPlane(FVector(0.0f, 1.0f, 0.0f), ClampRect.Min.Y),	// Top
						FPlane(FVector(-1.0f, 0.0f, 0.0f), -ClampRect.Max.X),	// Right
						FPlane(FVector(0.0f, -1.0f, 0.0f), -ClampRect.Max.Y)	// Bottom
					};

					for (int32 i = 0; i < EArrowDirection::MAX; ++i)
					{
						FVector NewPoint;
						if (FMath::SegmentPlaneIntersection(Center, FVector(ScreenPosition, 0.0f), Planes[i], NewPoint))
						{
							ClampDir = (EArrowDirection::Type)i;
							ScreenPosition = FVector2D(NewPoint);
						}
					}
				}
				else if (!bInFrontOfCamera)
				{
					const float ScreenXNorm = ScreenPosition.X / (RectMax.X - RectMin.X);
					const float ScreenYNorm = ScreenPosition.Y / (RectMax.Y - RectMin.Y);
					// 相机后方的投影点即使落在矩形内，也按相对中心方向固定到最近屏幕边缘。
					//we need to pin this thing to the side of the screen
					if (ScreenXNorm < ScreenYNorm)
					{
						if (ScreenXNorm < (-ScreenYNorm + 1.0f))
						{
							ClampDir = EArrowDirection::Left;
							ScreenPosition.X = ClampRect.Min.X;
						}
						else
						{
							ClampDir = EArrowDirection::Bottom;
							ScreenPosition.Y = ClampRect.Max.Y;
						}
					}
					else
					{
						if (ScreenXNorm < (-ScreenYNorm + 1.0f))
						{
							ClampDir = EArrowDirection::Top;
							ScreenPosition.Y = ClampRect.Min.Y;
						}
						else
						{
							ClampDir = EArrowDirection::Right;
							ScreenPosition.X = ClampRect.Max.X;
						}
					}
				}

				bWasIndicatorClamped = (ClampDir != EArrowDirection::MAX);

				// 描述对象要求箭头、实际发生钳制且箭头池有可用项时才排列方向箭头。
				// should we show an arrow
				if (Indicator->GetShowClampToScreenArrow() &&
					bWasIndicatorClamped &&
					ArrowChildren.IsValidIndex(NextArrowIndex))
				{
					const FVector2D ArrowOffsetDirection = ArrowOffsets[ClampDir];
					const float ArrowRotation = ArrowRotations[ClampDir];

					// 从预创建箭头槽位中取下一个实例复用。
					//grab an arrow widget
					TSharedRef<SActorCanvasArrowWidget> ArrowWidgetToUse = StaticCastSharedRef<SActorCanvasArrowWidget>(ArrowChildren.GetChildAt(NextArrowIndex));
					NextArrowIndex++;

					// 按钳制边缘设置箭头朝向。
					//set the rotation of the arrow
					ArrowWidgetToUse->SetRotation(ArrowRotation);

					// 用指示器和箭头尺寸的一半计算箭头离开锚点的距离。
					//figure out the magnitude of the offset
					const FVector2D OffsetMagnitude = (SlotSize + ArrowWidgetSize) * 0.5f;

					// 减去箭头半尺寸，使计算位置对应箭头中心。
					//used to center the arrow on the position
					const FVector2D ArrowCenteringOffset = -(ArrowWidgetSize * 0.5f);

					FVector2D ArrowAlignmentOffset = FVector2D::ZeroVector;
					switch (Indicator->VAlignment)
					{
					case VAlign_Top:
						ArrowAlignmentOffset = SlotSize * FVector2D(0.0f, 0.5f);
						break;
					case VAlign_Bottom:
						ArrowAlignmentOffset = SlotSize * FVector2D(0.0f, -0.5f);
						break;
					}

					// 沿钳制边缘法线计算箭头相对指示器的偏移。
					//figure out the offset for the arrow
					const FVector2D WidgetOffset = (OffsetMagnitude * ArrowOffsetDirection);

					const FVector2D FinalOffset = (WidgetOffset + ArrowAlignmentOffset + ArrowCenteringOffset);

					// 将屏幕钳制点、方向偏移、对齐补偿和中心补偿合成为箭头最终位置。
					//get the final position
					const FVector2D FinalPosition = (ScreenPosition + FinalOffset);

					ArrowWidgetToUse->SetVisibility(EVisibility::HitTestInvisible);

					// 将箭头作为独立子项加入排列结果，与钳制后的指示器一同绘制。
					// Inject the arrow on top of the indicator
					ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
						ArrowWidgetToUse,			/* 要加入排列结果的箭头子控件。 */ // The child widget being arranged
						FinalPosition,				/* 箭头在父控件局部空间中的位置。 */ // Child's local position (i.e. position within parent)
						ArrowWidgetSize,			/* 箭头子控件的期望尺寸。 */ // Child's size
						1.f							/* 箭头子控件使用原始缩放比例。 */ // Child's scale
					));
				}
			}

			CurChild.SetWasIndicatorClamped(bWasIndicatorClamped);

			// 将最终屏幕位置加对齐偏移后加入 Slate 排列结果。
			// Add the information about this child to the output list (ArrangedChildren)
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
				CurChild.GetWidget(),
				ScreenPosition + SlotOffset,
				SlotSize,
				1.f
			));
		}
	}

	if (NextArrowIndex < ArrowIndexLastUpdate)
	{
		for (int32 ArrowRemovedIndex = NextArrowIndex; ArrowRemovedIndex < ArrowIndexLastUpdate; ArrowRemovedIndex++)
		{
			ArrowChildren.GetChildAt(ArrowRemovedIndex)->SetVisibility(EVisibility::Collapsed);
		}
	}

	ArrowIndexLastUpdate = NextArrowIndex;
}

// 缓存当前绘制几何，排列子控件并跳过裁剪区外项目，按配置共享或递增图层完成绘制。
int32 SActorCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SActorCanvas_OnPaint);

	OptionalPaintGeometry = AllottedGeometry;

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	int32 MaxLayerId = LayerId;

	const FPaintArgs NewArgs = Args.WithNewParent(this);
	const bool bShouldBeEnabled = ShouldBeEnabled(bParentEnabled);

	for (const FArrangedWidget& CurWidget : ArrangedChildren.GetInternalArray())
	{
		if (!IsChildWidgetCulled(MyCullingRect, CurWidget))
		{
			SWidget* MutableWidget = const_cast<SWidget*>(&CurWidget.Widget.Get());

			const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, MyCullingRect, OutDrawElements, bDrawElementsInOrder ? MaxLayerId : LayerId, InWidgetStyle, bShouldBeEnabled);
			MaxLayerId = FMath::Max(MaxLayerId, CurWidgetsMaxLayerId);
		}
		else
		{
			//SlateGI - RemoveContent
		}
	}

	return MaxLayerId;
}

// 返回供垃圾收集调试识别的 Slate 引用器名称。
FString SActorCanvas::GetReferencerName() const
{
	return TEXT("SActorCanvas");
}

// 向垃圾收集器报告画布持有的全部指示器描述对象。
void SActorCanvas::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObjects(AllIndicators);
}

// 把新描述加入活动与待创建集合，并启动其异步控件创建流程。
void SActorCanvas::OnIndicatorAdded(UIndicatorDescriptor* Indicator)
{
	AllIndicators.Add(Indicator);
	InactiveIndicators.Add(Indicator);
	
	AddIndicatorForEntry(Indicator);
}

// 解绑并回收描述对应控件和槽位，再从活动与待创建集合删除。
void SActorCanvas::OnIndicatorRemoved(UIndicatorDescriptor* Indicator)
{
	RemoveIndicatorForEntry(Indicator);
	
	AllIndicators.Remove(Indicator);
	InactiveIndicators.Remove(Indicator);
}

// 异步加载描述指定的 UUserWidget 类，确认描述仍有效后从控件池取得实例、绑定数据并加入画布槽位。
void SActorCanvas::AddIndicatorForEntry(UIndicatorDescriptor* Indicator)
{
	// 异步加载指示器控件类型，并通过 UUserWidget 池创建和复用实例。
	// Async load the indicator, and pool the results so that it's easy to use and reuse the widgets.
	TSoftClassPtr<UUserWidget> IndicatorClass = Indicator->GetIndicatorClass();
	if (!IndicatorClass.IsNull())
	{
		TWeakObjectPtr<UIndicatorDescriptor> IndicatorPtr(Indicator);
		AsyncLoad(IndicatorClass, [this, IndicatorPtr, IndicatorClass]() {
			if (UIndicatorDescriptor* Indicator = IndicatorPtr.Get())
			{
				// 异步加载期间描述对象可能已被移除，创建控件前再次确认仍在活动列表中。
				// While async loading this indicator widget we could have removed it.
				if (!AllIndicators.Contains(Indicator))
				{
					return;
				}

				// 从池中取得或创建 UUserWidget，绑定描述数据后放入 Actor Canvas 槽位。
				// Create the widget from the pool.
				if (UUserWidget* IndicatorWidget = IndicatorPool.GetOrCreateInstance(TSubclassOf<UUserWidget>(IndicatorClass.Get())))
				{
					if (IndicatorWidget->GetClass()->ImplementsInterface(UIndicatorWidgetInterface::StaticClass()))
					{
						IIndicatorWidgetInterface::Execute_BindIndicator(IndicatorWidget, Indicator);
					}

					Indicator->IndicatorWidget = IndicatorWidget;

					InactiveIndicators.Remove(Indicator);

					AddActorSlot(Indicator)
					[
						SAssignNew(Indicator->CanvasHost, SBox)
						[
							IndicatorWidget->TakeWidget()
						]
					];
				}
			}
		});
		StartAsyncLoading();
	}
}

// 解除指示器接口绑定，将 UUserWidget 归还控件池，并移除对应 Slate 宿主槽位。
void SActorCanvas::RemoveIndicatorForEntry(UIndicatorDescriptor* Indicator)
{
	if (UUserWidget* IndicatorWidget = Indicator->IndicatorWidget.Get())
	{
		if (IndicatorWidget->GetClass()->ImplementsInterface(UIndicatorWidgetInterface::StaticClass()))
		{
			IIndicatorWidgetInterface::Execute_UnbindIndicator(IndicatorWidget, Indicator);
		}

		Indicator->IndicatorWidget = nullptr;
		
		IndicatorPool.Release(IndicatorWidget);
	}

	TSharedPtr<SWidget> CanvasHost = Indicator->CanvasHost.Pin();
	if (CanvasHost.IsValid())
	{
		RemoveActorSlot(CanvasHost.ToSharedRef());
		Indicator->CanvasHost.Reset();
	}
}

// 创建绑定描述对象的画布槽位，并在槽位提交后确保活动计时器运行。
SActorCanvas::FScopedWidgetSlotArguments SActorCanvas::AddActorSlot(UIndicatorDescriptor* Indicator)
{
	TWeakPtr<SActorCanvas> WeakCanvas = SharedThis(this);
	return FScopedWidgetSlotArguments{ MakeUnique<FSlot>(Indicator), this->CanvasChildren, INDEX_NONE
		, [WeakCanvas](const FSlot*, int32)
		{
			if (TSharedPtr<SActorCanvas> Canvas = WeakCanvas.Pin())
			{
				Canvas->UpdateActiveTimer();
			}
		}};
}

// 按宿主 Slate 控件查找并删除画布槽位，刷新活动计时器并返回原索引；未找到时返回 -1。
int32 SActorCanvas::RemoveActorSlot(const TSharedRef<SWidget>& SlotWidget)
{
	for (int32 SlotIdx = 0; SlotIdx < CanvasChildren.Num(); ++SlotIdx)
	{
		if ( SlotWidget == CanvasChildren[SlotIdx].GetWidget() )
		{
			CanvasChildren.RemoveAt(SlotIdx);

			UpdateActiveTimer();

			return SlotIdx;
		}
	}

	return -1;
}

// 根据宿主控件期望尺寸及水平、垂直对齐计算投影锚点偏移和四边裁剪留白。
void SActorCanvas::GetOffsetAndSize(const UIndicatorDescriptor* Indicator,
	FVector2D& OutSize, 
	FVector2D& OutOffset,
	FVector2D& OutPaddingMin,
	FVector2D& OutPaddingMax) const
{
	// 当前没有外部 AllottedSize 输入，使用零尺寸作为对齐计算的锚点基准。
	//This might get used one day
	FVector2D AllottedSize = FVector2D::ZeroVector;

	// 从指示器宿主控件取得期望尺寸。
	//grab the desired size of the child widget
	TSharedPtr<SWidget> CanvasHost = Indicator->CanvasHost.Pin();
	if (CanvasHost.IsValid())
	{
		OutSize = CanvasHost->GetDesiredSize();
	}

	// 根据水平对齐计算投影锚点到控件左上角的偏移，以及左右边缘留白。
	//handle horizontal alignment
	switch(Indicator->GetHAlign())
	{
		case HAlign_Left: // same as Align_Top
			OutOffset.X = 0.0f;
			OutPaddingMin.X = 0.0f;
			OutPaddingMax.X = OutSize.X;
			break;
		
		case HAlign_Center:
			OutOffset.X = (AllottedSize.X - OutSize.X) / 2.0f;
			OutPaddingMin.X = OutSize.X / 2.0f;
			OutPaddingMax.X = OutPaddingMin.X;
			break;
		
		case HAlign_Right: // same as Align_Bottom
			OutOffset.X = AllottedSize.X - OutSize.X;
			OutPaddingMin.X = OutSize.X;
			OutPaddingMax.X = 0.0f;
			break;
	}

	// 根据垂直对齐计算投影锚点到控件左上角的偏移，以及上下边缘留白。
	//Now, handle vertical alignment
	switch(Indicator->GetVAlign())
	{
		case VAlign_Top:
			OutOffset.Y = 0.0f;
			OutPaddingMin.Y = 0.0f;
			OutPaddingMax.Y = OutSize.Y;
			break;
		
		case VAlign_Center:
			OutOffset.Y = (AllottedSize.Y - OutSize.Y) / 2.0f;
			OutPaddingMin.Y = OutSize.Y / 2.0f;
			OutPaddingMax.Y = OutPaddingMin.Y;
			break;
		
		case VAlign_Bottom:
			OutOffset.Y = AllottedSize.Y - OutSize.Y;
			OutPaddingMin.Y = OutSize.Y;
			OutPaddingMax.Y = 0.0f;
			break;
	}
}

// 存在指示器或尚未找到管理组件时注册活动计时器，避免重复注册。
void SActorCanvas::UpdateActiveTimer()
{
	const bool NeedsTicks = AllIndicators.Num() > 0 || !IndicatorComponentPtr.IsValid();

	if (NeedsTicks && !TickHandle.IsValid())
	{
		TickHandle = RegisterActiveTimer(0, FWidgetActiveTimerDelegate::CreateSP(this, &SActorCanvas::UpdateCanvas));
	}
}
