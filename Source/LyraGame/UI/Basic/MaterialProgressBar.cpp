// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialProgressBar.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialProgressBar)

// 同步 UMG 属性到进度条图像：选择材质、重建动态材质缓存，并应用设计时进度及各项覆盖参数。
void UMaterialProgressBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (Image_Bar)
	{
		Image_Bar->SetBrushFromMaterial(bUseStroke ? StrokeMaterial : NoStrokeMaterial);
		CachedMID = nullptr;
		CachedProgress = -1.0f;
		CachedStartProgress = -1.0f;

#if WITH_EDITORONLY_DATA
		if (IsDesignTime())
		{
			SetProgress_Internal(DesignTime_Progress);
		}
#endif

		if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
		{
			if (bOverrideDefaultSegmentEdge)
			{
				MID->SetScalarParameterValue(TEXT("SegmentEdge"), SegmentEdge);
			}

			if (bOverrideDefaultSegments)
			{
				MID->SetScalarParameterValue(TEXT("Segments"), (float)Segments);
			}

			if (bOverrideDefaultFillEdgeSoftness)
			{
				MID->SetScalarParameterValue(TEXT("FillEdgeSoftness"), FillEdgeSoftness);
			}

			if (bOverrideDefaultGlowEdge)
			{
				MID->SetScalarParameterValue(TEXT("GlowEdge"), GlowEdge);
			}

			if (bOverrideDefaultGlowSoftness)
			{
				MID->SetScalarParameterValue(TEXT("GlowSoftness"), GlowSoftness);
			}

			if (bOverrideDefaultOutlineScale)
			{
				MID->SetScalarParameterValue(TEXT("OutlineScale"), OutlineScale);
			}
		}

		if (bOverrideDefaultColorA)
		{
			SetColorA_Internal(CachedColorA);
		}

		if (bOverrideDefaultColorB)
		{
			SetColorB_Internal(CachedColorB);
		}

		if (bOverrideDefaultColorBackground)
		{
			SetColorBackground_Internal(CachedColorBackground);
		}
	}
}

#if WITH_EDITOR
// 编辑器重建控件后，从动态材质读取未被属性覆盖的默认颜色和标量参数，供设计器显示。
void UMaterialProgressBar::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	if (IsDesignTime() && Image_Bar)
	{
		if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
		{
			if (!bOverrideDefaultColorA)
			{
				MID->GetVectorParameterValue(TEXT("ColorA"), CachedColorA);
			}

			if (!bOverrideDefaultColorB)
			{
				MID->GetVectorParameterValue(TEXT("ColorB"), CachedColorB);
			}

			if (!bOverrideDefaultColorBackground)
			{
				MID->GetVectorParameterValue(TEXT("Unfilled Color"), CachedColorBackground);
			}

			if (!bOverrideDefaultSegmentEdge)
			{
				MID->GetScalarParameterValue(TEXT("SegmentEdge"), SegmentEdge);
			}

			if (!bOverrideDefaultSegments)
			{
				float SegmentsFloat;
				MID->GetScalarParameterValue(TEXT("Segments"), SegmentsFloat);
				Segments = FMath::TruncToInt(SegmentsFloat);
			}

			if (!bOverrideDefaultFillEdgeSoftness)
			{
				MID->GetScalarParameterValue(TEXT("FillEdgeSoftness"), FillEdgeSoftness);
			}

			if (!bOverrideDefaultGlowEdge)
			{
				MID->GetScalarParameterValue(TEXT("GlowEdge"), GlowEdge);
			}

			if (!bOverrideDefaultGlowSoftness)
			{
				MID->GetScalarParameterValue(TEXT("GlowSoftness"), GlowSoftness);
			}

			if (!bOverrideDefaultOutlineScale)
			{
				MID->GetScalarParameterValue(TEXT("OutlineScale"), OutlineScale);
			}
		}
	}
}
#endif

// 填充动画结束时广播进度条专用完成事件，其他动画仅交由基类处理。
void UMaterialProgressBar::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (BoundAnim_FillBar == Animation)
	{
		OnFillAnimationFinished.Broadcast();
	}
}

// 目标进度发生变化时写入动态材质的 Progress 参数。
void UMaterialProgressBar::SetProgress(float Progress)
{
	if (CachedProgress != Progress)
	{
		SetProgress_Internal(Progress);
	}
}

// 起始进度发生变化时写入动态材质的 StartProgress 参数。
void UMaterialProgressBar::SetStartProgress(float StartProgress)
{
	if (CachedStartProgress != StartProgress)
	{
		SetStartProgress_Internal(StartProgress);
	}
}

// 主填充颜色发生变化时写入动态材质的 ColorA 参数。
void UMaterialProgressBar::SetColorA(FLinearColor ColorA)
{
	if (CachedColorA != ColorA)
	{
		SetColorA_Internal(ColorA);
	}
}

// 次填充颜色发生变化时写入动态材质的 ColorB 参数。
void UMaterialProgressBar::SetColorB(FLinearColor ColorB)
{
	if (CachedColorB != ColorB)
	{
		SetColorB_Internal(ColorB);
	}
}

// 背景颜色发生变化时写入动态材质的 Unfilled Color 参数。
void UMaterialProgressBar::SetColorBackground(FLinearColor ColorBackground)
{
	if (CachedColorBackground != ColorBackground)
	{
		SetColorBackground_Internal(ColorBackground);
	}
}

// 设置动画起止进度，并按给定播放速率从头播放填充动画。
void UMaterialProgressBar::AnimateProgressFromStart(float Start, float End, float AnimSpeed)
{
	SetStartProgress(Start);
	SetProgress(End);
	PlayAnimation(BoundAnim_FillBar, 0.0f, 1, EUMGSequencePlayMode::Forward, AnimSpeed);
}

// 根据材质当前填充插值计算新的起点，再平滑动画到目标进度；材质不可用时不处理。
void UMaterialProgressBar::AnimateProgressFromCurrent(float End, float AnimSpeed)
{
	if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
	{
		const float CurrentStart = MID->K2_GetScalarParameterValue(TEXT("StartProgress"));
		const float CurrentEnd = MID->K2_GetScalarParameterValue(TEXT("Progress"));
		const float CurrentFill = MID->K2_GetScalarParameterValue(TEXT("FillAmount"));
		const float NewStart = FMath::Lerp(CurrentStart, CurrentEnd, CurrentFill);
		AnimateProgressFromStart(NewStart, End, AnimSpeed);
	}
}

// 动态材质可用时缓存目标进度并更新 Progress 标量参数。
void UMaterialProgressBar::SetProgress_Internal(float Progress)
{
	if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
	{
		CachedProgress = Progress;
		MID->SetScalarParameterValue(TEXT("Progress"), CachedProgress);
	}
}

// 动态材质可用时缓存起始进度并更新 StartProgress 标量参数。
void UMaterialProgressBar::SetStartProgress_Internal(float StartProgress)
{
	if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
	{
		CachedStartProgress = StartProgress;
		MID->SetScalarParameterValue(TEXT("StartProgress"), CachedStartProgress);
	}
}

// 动态材质可用时缓存并更新主填充颜色。
void UMaterialProgressBar::SetColorA_Internal(FLinearColor ColorA)
{
	if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
	{
		CachedColorA = ColorA;
		MID->SetVectorParameterValue(TEXT("ColorA"), CachedColorA);
	}
}

// 动态材质可用时缓存并更新次填充颜色。
void UMaterialProgressBar::SetColorB_Internal(FLinearColor ColorB)
{
	if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
	{
		CachedColorB = ColorB;
		MID->SetVectorParameterValue(TEXT("ColorB"), CachedColorB);
	}
}

// 动态材质可用时缓存并更新未填充区域颜色。
void UMaterialProgressBar::SetColorBackground_Internal(FLinearColor ColorBackground)
{
	if (UMaterialInstanceDynamic* MID = GetBarDynamicMaterial())
	{
		CachedColorBackground = ColorBackground;
		MID->SetVectorParameterValue(TEXT("Unfilled Color"), CachedColorBackground);
	}
}

// 延迟取得并缓存进度条图像的动态材质实例。
UMaterialInstanceDynamic* UMaterialProgressBar::GetBarDynamicMaterial() const
{
	if (!CachedMID)
	{
		CachedMID = Image_Bar->GetDynamicMaterial();
	}

	return CachedMID;
}

