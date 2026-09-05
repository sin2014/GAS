// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonPlayerInputKey.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "CommonLocalPlayer.h"
#include "CommonPlayerController.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/SlateRenderer.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonPlayerInputKey)

class FPaintArgs;
class FSlateRect;

#define LOCTEXT_NAMESPACE "CommonKeybindWidget"

DECLARE_LOG_CATEGORY_EXTERN(LogCommonPlayerInput, Log, All);
// 定义按键显示、动作映射和长按进度诊断使用的日志分类。
DEFINE_LOG_CATEGORY(LogCommonPlayerInput);

struct FSlateDrawUtil
{
	// 在不放大原图的前提下等比缩放画刷，并将其居中绘制到分配区域。
	static void DrawBrushCenterFit(
		FSlateWindowElementList& ElementList,
		uint32 InLayer,
		const FGeometry& InAllottedGeometry,
		const FSlateBrush* InBrush,
		const FLinearColor& InTint = FLinearColor::White)
	{
		DrawBrushCenterFitWithOffset
		(
			ElementList,
			InLayer,
			InAllottedGeometry,
			InBrush,
			InTint,
			FVector2D(0, 0)
		);
	}

	// 等比缩放并居中绘制画刷，再叠加指定局部偏移；空画刷直接跳过。
	static void DrawBrushCenterFitWithOffset(
		FSlateWindowElementList& ElementList,
		uint32 InLayer,
		const FGeometry& InAllottedGeometry,
		const FSlateBrush* InBrush,
		const FLinearColor& InTint,
		const FVector2D InOffset)
	{
		if (!InBrush)
		{
			return;
		}

		const FVector2D AreaSize = InAllottedGeometry.GetLocalSize();
		const FVector2D ProgressSize = InBrush->GetImageSize();
		const float FitScale = FMath::Min(FMath::Min(AreaSize.X / ProgressSize.X, AreaSize.Y / ProgressSize.Y), 1.0f);
		const FVector2D FinalSize = FitScale * ProgressSize;

		const FVector2D Offset = (InAllottedGeometry.GetLocalSize() * 0.5f) - (FinalSize * 0.5f) + InOffset;

		FSlateDrawElement::MakeBox
		(
			ElementList,
			InLayer,
			InAllottedGeometry.ToPaintGeometry(FinalSize, FSlateLayoutTransform(Offset)),
			InBrush,
			ESlateDrawEffect::None,
			InTint
		);
	}
};



// 更新待测量文本并将尺寸缓存标记为失效。
void FMeasuredText::SetText(const FText& InText)
{
	CachedText = InText;
	bTextDirty = true;
}

// 仅在文本变化时调用 Slate 字体测量服务，并返回缓存的文本尺寸。
FVector2D FMeasuredText::UpdateTextSize(const FSlateFontInfo &InFontInfo, float FontScale) const
{
	if (bTextDirty)
	{
		bTextDirty = false;
		CachedTextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(CachedText, InFontInfo, FontScale);
	}

	return CachedTextSize;
}

// 创建按键显示控件，初始化无效回退按键、无输入类型覆盖和零尺寸边框。
UCommonPlayerInputKey::UCommonPlayerInputKey(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BoundKeyFallback(EKeys::Invalid)
	, InputTypeOverride(ECommonInputType::Count)
{
	FrameSize = FVector2D(0, 0);
}

// 在运行时及设计时预构造阶段刷新按键显示；设计器中额外展示长按背板并计算尺寸。
void UCommonPlayerInputKey::NativePreConstruct()
{
	Super::NativePreConstruct();

	UpdateKeybindWidget();

	if (IsDesignTime())
	{
		ShowHoldBackPlate();
		RecalculateDesiredSize();
	}
}

// 构造运行时控件，订阅输入方式与玩家控制器变化，并同步长按进度。
void UCommonPlayerInputKey::NativeConstruct()
{
	Super::NativeConstruct();
}

// 销毁控件前解除输入和长按委托，停止计时器并清理运行时状态。
void UCommonPlayerInputKey::NativeDestruct()
{
	if (ProgressPercentageMID)
	{
		// 销毁动态材质实例前先把画刷恢复为原始材质，避免画刷继续引用即将失效的 MID。
		// Need to restore the material on the brush before we kill off the MID.
		HoldProgressBrush.SetResourceObject(ProgressPercentageMID->GetMaterial());

		ProgressPercentageMID->MarkAsGarbage();
		ProgressPercentageMID = nullptr;
	}

	Super::NativeDestruct();
}

// 按当前视觉模式绘制按键背景、图标、文本阴影与长按进度，并返回最终绘制层级。
int32 UCommonPlayerInputKey::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (bDrawProgress)
	{
		FSlateDrawUtil::DrawBrushCenterFit
		(
			OutDrawElements,
			++MaxLayer,
			AllottedGeometry,
			&HoldProgressBrush,
			FLinearColor(InWidgetStyle.GetColorAndOpacityTint() * HoldProgressBrush.GetTint(InWidgetStyle))
		);
	}

	if (bDrawCountdownText)
	{
		const FVector2D CountdownTextOffset = (AllottedGeometry.GetLocalSize() - CountdownText.GetTextSize()) * 0.5f;

		FSlateDrawElement::MakeText
		(
			OutDrawElements,
			++MaxLayer,
			AllottedGeometry.ToOffsetPaintGeometry(CountdownTextOffset),
			CountdownText.GetText(),
			CountdownTextFont,
			ESlateDrawEffect::None,
			FLinearColor(InWidgetStyle.GetColorAndOpacityTint())
		);
	}
	else if (bDrawBrushForKey)
	{
		// 在按键文本后绘制阴影，提高图标上的文字可读性。
		// Draw Shadow
		FSlateDrawUtil::DrawBrushCenterFitWithOffset
		(
			OutDrawElements,
			++MaxLayer,
			AllottedGeometry,
			&CachedKeyBrush,
			FLinearColor(InWidgetStyle.GetColorAndOpacityTint() * FLinearColor::Black),
			FVector2D(1, 1)
		);

		FSlateDrawUtil::DrawBrushCenterFit
		(
			OutDrawElements,
			++MaxLayer,
			AllottedGeometry,
			&CachedKeyBrush,
			FLinearColor(InWidgetStyle.GetColorAndOpacityTint() * CachedKeyBrush.GetTint(InWidgetStyle))
		);
	}
	else if (KeybindText.GetTextSize().X > 0)
	{
		const FVector2D FrameOffset = (AllottedGeometry.GetLocalSize() - FrameSize) * 0.5f;

		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			++MaxLayer,
			AllottedGeometry.ToPaintGeometry(FrameSize, FSlateLayoutTransform(FrameOffset)),
			&KeyBindTextBorder,
			ESlateDrawEffect::None,
			FLinearColor(InWidgetStyle.GetColorAndOpacityTint() * KeyBindTextBorder.GetTint(InWidgetStyle))
		);

		const FVector2D ActionTextOffset = (AllottedGeometry.GetLocalSize() - KeybindText.GetTextSize()) * 0.5f;

		FSlateDrawElement::MakeText
		(
			OutDrawElements,
			++MaxLayer,
			AllottedGeometry.ToOffsetPaintGeometry(ActionTextOffset),
			KeybindText.GetText(),
			KeyBindTextFont,
			ESlateDrawEffect::None,
			FLinearColor(InWidgetStyle.GetColorAndOpacityTint())
		);
	}

	return MaxLayer;
}

// 记录长按起始时间与持续时长，启动逐 Tick 进度同步。
void UCommonPlayerInputKey::StartHoldProgress(FName HoldActionName, float HoldDuration)
{
	if (HoldActionName == BoundAction && ensureMsgf(HoldDuration > 0.0f, TEXT("Trying to perform hold action \"%s\" with no HoldDuration"), *BoundAction.ToString()))
	{
		HoldKeybindDuration = HoldDuration;
		HoldKeybindStartTime = GetWorld()->GetRealTimeSeconds();

		UpdateHoldProgress();
	}
}

// 停止长按进度更新、清除时间状态并把材质恢复到初始进度。
void UCommonPlayerInputKey::StopHoldProgress(FName HoldActionName, bool bCompletedSuccessfully)
{
	if (HoldActionName == BoundAction)
	{
		HoldKeybindStartTime = 0.f;
		HoldKeybindDuration = 0.f;

		if (ensure(ProgressPercentageMID))
		{
			ProgressPercentageMID->SetScalarParameterValue(PercentageMaterialParameterName, 0.f);
		}

		if (bDrawCountdownText)
		{
			bDrawCountdownText = false;
			Invalidate(EInvalidateWidget::Paint);
			RecalculateDesiredSize();
		}
	}
}

// 从玩家控制器正在进行的长按动作取得时间信息，决定启动或停止本控件进度。
void UCommonPlayerInputKey::SyncHoldProgress()
{
	// 若此前正在跟踪长按动作，先停止旧动作并清除进度状态。
	// If we had an active hold action, stop it
	if (HoldKeybindStartTime > 0.f)
	{
		StopHoldProgress(BoundAction, false);
	}
}

// 计算当前长按百分比并写入动态材质，然后安排下一 Tick 继续更新。
void UCommonPlayerInputKey::UpdateHoldProgress()
{
	if (HoldKeybindStartTime != 0.f && HoldKeybindDuration > 0.f)
	{
		const float CurrentTime = GetWorld()->GetRealTimeSeconds();
		const float ElapsedTime = FMath::Min(CurrentTime - HoldKeybindStartTime, HoldKeybindDuration);
		const float RemainingTime = FMath::Max(0.0f, HoldKeybindDuration - ElapsedTime);

		if (ElapsedTime < HoldKeybindDuration && ensure(ProgressPercentageMID))
		{
			const float HoldKeybindPercentage = ElapsedTime / HoldKeybindDuration;
			ProgressPercentageMID->SetScalarParameterValue(PercentageMaterialParameterName, HoldKeybindPercentage);

			// 安排下一 Tick 再次同步长按进度，避免在当前输入回调内连续递归更新。
			// Schedule a callback for next tick to update the hold progress again.
			GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::UpdateHoldProgress);		
		}

		if (bShowTimeCountDown)
		{
			FNumberFormattingOptions Options;
			Options.MinimumFractionalDigits = 1;
			Options.MaximumFractionalDigits = 1;
			CountdownText.SetText(FText::AsNumber(RemainingTime, &Options));

			bDrawCountdownText = true;
			Invalidate(EInvalidateWidget::Paint);
			RecalculateDesiredSize();
		}
	}
}

// 解析动作映射、输入类型和图标资源，更新按键画刷、文本、长按状态与期望尺寸。
void UCommonPlayerInputKey::UpdateKeybindWidget()
{
	if (!GetOwningPlayer<ACommonPlayerController>())
	{
		bWaitingForPlayerController = true;
		return;
	}

	UCommonInputSubsystem* CommonInputSubsystem = GetInputSubsystem();

	if (CommonInputSubsystem && !CommonInputSubsystem->ShouldShowInputKeys())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bIsUsingGamepad = (InputTypeOverride == ECommonInputType::Gamepad) || ((CommonInputSubsystem != nullptr) && (CommonInputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad)) ;

	if (!BoundKey.IsValid())
	{
		BoundKey = BoundKeyFallback;
	}
	UE_LOG(LogCommonPlayerInput, Verbose, TEXT("UCommonKeybindWidget::UpdateKeybindWidget: Action: %s Key: %s"), *(BoundAction.ToString()), *(BoundKey.ToString()));

	// 必须先创建 Update 所需的进度动态材质实例，再刷新按键显示。
	// Must be called before Update, due to the creation of ProgressPercentageMID which will be used in Update
	SetupHoldKeybind();

	bool NewDrawBrushForKey = false;
	bool NeedToRecalcSize = false;

	if (BoundKey.IsValid())
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);

		ShowHoldBackPlate();

		NeedToRecalcSize = true;
	}
	else
	{
		if (bShowUnboundStatus)
		{
			SetVisibility(ESlateVisibility::HitTestInvisible);
			NewDrawBrushForKey = false;

			KeybindText.SetText(LOCTEXT("Unbound", "Unbound"));

			NeedToRecalcSize = true;
		}
		else
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (bDrawBrushForKey != NewDrawBrushForKey)
	{
		bDrawBrushForKey = NewDrawBrushForKey;
		Invalidate(EInvalidateWidget::Paint);
	}

	// RecalculateDesiredSize 依赖 bDrawBrushForKey，因此要在本轮是否绘制按键画刷确定后再调用。
	// As RecalculateDesiredSize relies on the bDrawBrushForKey 
	// we shouldn't call it until that value has been finalized
	// for the update
	if (NeedToRecalcSize)
	{
		RecalculateDesiredSize();
	}
}

// 改为直接显示指定按键，清除动作派生结果并刷新控件。
void UCommonPlayerInputKey::SetBoundKey(FKey NewKey)
{
	if (NewKey != BoundKey)
	{
		BoundKeyFallback = NewKey;
		BoundAction = NAME_None;
		UpdateKeybindWidget();
	}
}

// 保存输入动作并重新解析当前输入类型下的实际按键和长按配置。
void UCommonPlayerInputKey::SetBoundAction(FName NewBoundAction)
{
	bool bUpdateWidget = true;

	if (BoundAction != NewBoundAction)
	{
		BoundAction = NewBoundAction;
	}

	if (bUpdateWidget)
	{
		UpdateKeybindWidget();
	}
}

// 初始化控件并缓存所属本地玩家，以便查询输入子系统和动作映射。
void UCommonPlayerInputKey::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UCommonLocalPlayer* CommonLocalPlayer = GetOwningLocalPlayer<UCommonLocalPlayer>())
	{
		CommonLocalPlayer->OnPlayerControllerSet.AddUObject(this, &ThisClass::HandlePlayerControllerSet);
	}
}

// 更新长按显示覆盖策略并重建按键视觉。
void UCommonPlayerInputKey::SetForcedHoldKeybindStatus(ECommonKeybindForcedHoldStatus InForcedHoldKeybindStatus)
{
	ForcedHoldKeybindStatus = InForcedHoldKeybindStatus;

	UpdateKeybindWidget();
}

// 控制长按材质显示已完成进度还是剩余进度，并立即同步参数。
void UCommonPlayerInputKey::SetShowProgressCountDown(bool bShow)
{
	bShowTimeCountDown = bShow;
}

// 按需要创建长按动态材质、替换画刷材质并初始化进度参数。
void UCommonPlayerInputKey::SetupHoldKeybind()
{
	ACommonPlayerController* OwningCommonPlayer = Cast<ACommonPlayerController>(GetOwningPlayer());

	// 根据绑定动作和覆盖选项确定是否显示长按交互。
	// Setup the hold
	if (ForcedHoldKeybindStatus == ECommonKeybindForcedHoldStatus::ForcedHold)
	{
		bIsHoldKeybind = true;
	}
	else if (ForcedHoldKeybindStatus == ECommonKeybindForcedHoldStatus::NeverShowHold)
	{
		bIsHoldKeybind = false;
	}

	if (ensure(OwningCommonPlayer))
	{
		if (bIsHoldKeybind)
		{
			// 为长按进度材质创建并配置动态实例。
			// Setup the ProgressPercentageMID
			if (ProgressPercentageMID == nullptr)
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(HoldProgressBrush.GetResourceObject()))
				{
					ProgressPercentageMID = UMaterialInstanceDynamic::Create(Material, this);
					HoldProgressBrush.SetResourceObject(ProgressPercentageMID);
				}
			}
			SyncHoldProgress();
		}
	}
}

// 设置长按背景与轮廓材质参数，使长按按键具备可辨识背板。
void UCommonPlayerInputKey::ShowHoldBackPlate()
{
	bool bDirty = false;

	if (IsHoldKeybind())
	{
		float BrushSizeAsValue = 32.0f;
		
		float DesiredBoxSize = BrushSizeAsValue + 10.0f;
		if (!bDrawBrushForKey)
		{
			DesiredBoxSize += 14.0f;
		}

		const FVector2D NewDesiredBrushSize(DesiredBoxSize, DesiredBoxSize);
		if (HoldProgressBrush.GetImageSize() != NewDesiredBrushSize)
		{
			HoldProgressBrush.SetImageSize(NewDesiredBrushSize);
			bDirty = true;
		}

		if (!bDrawProgress)
		{
			bDrawProgress = true;
			bDirty = true;
		}

		static const FName BackAlphaName = TEXT("BackAlpha");
		static const FName OutlineAlphaName = TEXT("OutlineAlpha");

		if (ProgressPercentageMID)
		{
			ProgressPercentageMID->SetScalarParameterValue(BackAlphaName, 0.2f);
			ProgressPercentageMID->SetScalarParameterValue(OutlineAlphaName, 0.4f);
		}
	}
	else
	{
		if (bDrawProgress)
		{
			bDrawProgress = false;
			bDirty = true;
		}
	}

	if (bDirty)
	{
		Invalidate(EInvalidateWidget::Paint);
	}
}

// 玩家控制器可用后重新绑定长按进度委托并刷新当前动作状态。
void UCommonPlayerInputKey::HandlePlayerControllerSet(UCommonLocalPlayer* LocalPlayer, APlayerController* PlayerController)
{
	if (bWaitingForPlayerController && GetOwningPlayer<ACommonPlayerController>())
	{
		UpdateKeybindWidget();
		bWaitingForPlayerController = false;
	}
}

// 根据图标、文本、边框和长按背板计算控件稳定期望尺寸。
void UCommonPlayerInputKey::RecalculateDesiredSize()
{
	FVector2D MaximumDesiredSize(0, 0);
	float LayoutScale = 1;

	if (bDrawProgress)
	{
		MaximumDesiredSize = FVector2D::Max(MaximumDesiredSize, HoldProgressBrush.GetImageSize());
	}

	if (bDrawCountdownText)
	{
		MaximumDesiredSize = FVector2D::Max(MaximumDesiredSize, CountdownText.UpdateTextSize(CountdownTextFont, LayoutScale));
	}
	else if (bDrawBrushForKey)
	{
		MaximumDesiredSize = FVector2D::Max(MaximumDesiredSize, CachedKeyBrush.GetImageSize());
	}
	else
	{
		const FVector2D KeybindTextSize = KeybindText.UpdateTextSize(KeyBindTextFont, LayoutScale);
		FrameSize = FVector2D::Max(KeybindTextSize, KeybindFrameMinimumSize) + KeybindTextPadding.GetDesiredSize();
		MaximumDesiredSize = FVector2D::Max(MaximumDesiredSize, FrameSize);
	}

	SetMinimumDesiredSize(MaximumDesiredSize);
}

#undef LOCTEXT_NAMESPACE

