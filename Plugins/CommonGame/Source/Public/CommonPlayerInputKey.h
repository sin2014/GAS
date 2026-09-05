// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "Fonts/SlateFontInfo.h"

#include "CommonPlayerInputKey.generated.h"

#define UE_API COMMONGAME_API

enum class ECommonInputType : uint8;

class APlayerController;
class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class UCommonLocalPlayer;
class UMaterialInstanceDynamic;
class UObject;
struct FFrame;
struct FGeometry;

UENUM(BlueprintType)
enum class ECommonKeybindForcedHoldStatus : uint8
{
	NoForcedHold,
	ForcedHold,
	NeverShowHold
};

USTRUCT()
struct FMeasuredText
{
	GENERATED_BODY()

public:
	FText GetText() const { return CachedText; }
	void SetText(const FText& InText);

	FVector2D GetTextSize() const { return CachedTextSize; }
	FVector2D UpdateTextSize(const FSlateFontInfo &InFontInfo, float FontScale = 1.0f) const;

private:

	FText CachedText;
	mutable FVector2D CachedTextSize;
	mutable bool bTextDirty = true;
};

UCLASS(MinimalAPI, Abstract, BlueprintType, Blueprintable, meta = (DisableNativeTick))
class UCommonPlayerInputKey : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UE_API UCommonPlayerInputKey(const FObjectInitializer& ObjectInitializer);

	// 根据当前绑定动作重新解析按键，并刷新图标、文本与长按显示。
	/** Update the key and associated display based on our current Boundaction */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	UE_API void UpdateKeybindWidget();

	// 直接设置该控件显示的绑定按键。
	/** Set the bound key for our keybind */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	UE_API void SetBoundKey(FKey NewBoundAction);

	// 设置需要解析和显示的输入动作。
	/** Set the bound action for our keybind */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	UE_API void SetBoundAction(FName NewBoundAction);

	// 强制把该按键显示为长按交互。
	/** Force this keybind to be a hold keybind */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	UE_API void SetForcedHoldKeybindStatus(ECommonKeybindForcedHoldStatus InForcedHoldKeybindStatus);

	// 强制把该按键显示为长按交互。
	/** Force this keybind to be a hold keybind */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	UE_API void SetShowProgressCountDown(bool bShow);

	// 设置轴映射筛选所使用的缩放值。
	/** Set the axis scale value for this keybind */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	void SetAxisScale(const float NewValue) { AxisScale = NewValue; }

	// 设置按键图标查询使用的预设名称覆盖。
	/** Set the preset name override value for this keybind. */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	void SetPresetNameOverride(const FName NewValue) { PresetNameOverride = NewValue; }

	// 当前显示所对应的输入动作。
	/** Our current BoundAction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keybind Widget")
	FName BoundAction;

	// 解析轴映射时要匹配的缩放值。
	/** Scale to read when using an axis Mapping */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keybind Widget")
	float AxisScale;

	// 可在蓝图中直接指定的按键，用于显示特定按键而非输入动作。
	/** Key this widget is bound to set directly in blueprint. Used when we want to reference a specific key instead of an action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keybind Widget")
	FKey BoundKeyFallback;

	// 允许为按键控件显式指定键鼠、手柄或触摸输入类型。
	/** Allows us to set the input type explicitly for the keybind widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybind Widget")
	ECommonInputType InputTypeOverride;

	// 允许为按键控件显式指定图标预设名称。
	/** Allows us to set the preset name explicitly for the keybind widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Keybind Widget")
	FName PresetNameOverride;

	// 控制是否按真实动作显示长按，或强制显示/隐藏长按效果。
	/** Setting that can show this keybind as a hold or never show it as a hold (even if it is) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keybind Widget")
	ECommonKeybindForcedHoldStatus ForcedHoldKeybindStatus;

	// 长按开始时由委托调用。
	/** Called through a delegate when we start hold progress */
	UFUNCTION()
	UE_API void StartHoldProgress(FName HoldActionName, float HoldDuration);

	// 长按停止时由委托调用。
	/** Called through a delegate when we stop hold progress */
	UFUNCTION()
	UE_API void StopHoldProgress(FName HoldActionName, bool bCompletedSuccessfully);

	// 返回该绑定当前是否应按长按动作显示。
	/** Get whether this keybind is a hold action. */
	UFUNCTION(BlueprintCallable, Category = "Keybind Widget")
	bool IsHoldKeybind() const { return bIsHoldKeybind; }

	UFUNCTION()
	bool IsBoundKeyValid() const { return BoundKey.IsValid(); }

protected:
	UE_API virtual void NativePreConstruct() override;
	UE_API virtual void NativeConstruct() override;
	UE_API virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	UE_API void RecalculateDesiredSize();

	// 释放 Slate 资源时同时销毁长按进度动态材质实例。
	/** Overridden to destroy our MID */
	UE_API virtual void NativeDestruct() override;

	// 该按键控件当前是否按长按交互显示。
	/** Whether or not this keybind widget is currently set to be a hold keybind */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Keybind Widget", meta=(ScriptName = "IsHoldKeybindValue"))
	bool bIsHoldKeybind;

	/**  */
	UPROPERTY(Transient)
	bool bShowKeybindBorder;

	UPROPERTY(Transient)
	FVector2D FrameSize;

	UPROPERTY(BlueprintReadOnly, Category = "Keybind Widget")
	bool bShowTimeCountDown;

	// 根据动作映射或直接配置解析出的实际按键。
	/** Derived Key this widget is bound to */
	UPROPERTY(BlueprintReadOnly, Category = "Keybind Widget")
	FKey BoundKey;

	// 用于显示长按进度的材质。
	/** Material for showing Progress */
	UPROPERTY(EditDefaultsOnly, Category = "Keybind Widget")
	FSlateBrush HoldProgressBrush;

	// 承载按键文本和背景的边框控件。
	/** The key bind text border. */
	UPROPERTY(EditDefaultsOnly, Category = "Keybind Widget")
	FSlateBrush KeyBindTextBorder;

	// 未绑定任何按键时是否显示未绑定提示。
	/** Should this keybinding widget display information that it is currently unbound? */
	UPROPERTY(EditAnywhere, Category = "Keybind Widget")
	bool bShowUnboundStatus = false;

	// 不同显示尺寸对应的按键文本字体。
	/** The font to apply at each size */
	UPROPERTY(EditDefaultsOnly, Category = "Font")
	FSlateFontInfo KeyBindTextFont;

	// 不同显示尺寸对应的按键文本字体。
	/** The font to apply at each size */
	UPROPERTY(EditDefaultsOnly, Category = "Font")
	FSlateFontInfo CountdownTextFont;

	UPROPERTY(Transient)
	FMeasuredText CountdownText;

	UPROPERTY(Transient)
	FMeasuredText KeybindText;

	UPROPERTY(Transient)
	FMargin KeybindTextPadding;

	UPROPERTY(Transient)
	FVector2D KeybindFrameMinimumSize;

	// 长按图像材质中表示完成百分比的参数名。
	/** The material parameter name for hold percentage in the HoldKeybindImage */
	UPROPERTY(EditDefaultsOnly, Category = "Keybind Widget")
	FName PercentageMaterialParameterName;	

	// 用于实时写入长按进度的动态材质实例。
	/** MID for the progress percentage */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProgressPercentageMID;

	UE_API virtual void NativeOnInitialized() override;

private:
	// 从所属玩家控制器同步当前长按动作的起始时间与持续时间。
	/**
	 * Synchronizes the hold progress to whatever is currently set in the
	 * owning player controller.
	 */
	UE_API void SyncHoldProgress();

	// 长按期间更新长按图像材质中的进度值。
	/** Called for updating the HoldKeybindImage during a hold keybind */
	UE_API void UpdateHoldProgress();

	// 需要显示长按交互时创建并配置进度材质。
	/** Called when we want to set up this keybind widget as a hold keybind */
	UE_API void SetupHoldKeybind();

	UE_API void ShowHoldBackPlate();

	UE_API void HandlePlayerControllerSet(UCommonLocalPlayer* LocalPlayer, APlayerController* PlayerController);

	// 本次长按动作开始的时间。
	/** Time when we started using a hold keybind */
	float HoldKeybindStartTime = 0;

	// 完成本次长按动作所需的秒数。
	/** How long, in seconds, we will be doing a hold keybind */
	float HoldKeybindDuration = 0;

	bool bDrawProgress = false;
	bool bDrawBrushForKey = false;
	bool bDrawCountdownText = false;
	bool bWaitingForPlayerController = false;

	UPROPERTY(Transient)
	FSlateBrush CachedKeyBrush;
};

#undef UE_API
