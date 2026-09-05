// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "LyraSimulatedInputWidget.generated.h"

#define UE_API LYRAGAME_API

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UCommonHardwareVisibilityBorder;
class UEnhancedPlayerInput;

// 可从 UMG 注入 Enhanced Input Action 或传统按键值的基础控件，并随玩家控制映射变化更新实际模拟按键。
/**
 *  A UMG widget with base functionality to inject input (keys or input actions)
 *  to the enhanced input subsystem.
 */
UCLASS(MinimalAPI, meta=( DisplayName="Lyra Simulated Input Widget" ))
class ULyraSimulatedInputWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	
	UE_API ULyraSimulatedInputWidget(const FObjectInitializer& ObjectInitializer);
	
	//~ Begin UWidget
#if WITH_EDITOR
	UE_API virtual const FText GetPaletteCategory() override;
#endif
	//~ End UWidget interface

	//~ Begin UUserWidget
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	UE_API virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	//~ End UUserWidget interface
	
	/** 返回所属本地玩家的 Enhanced Input 子系统；没有所属玩家时返回 nullptr。 */
	/** Get the enhanced input subsystem based on the owning local player of this widget. Will return null if there is no owning player */
	UFUNCTION(BlueprintCallable)
	UE_API UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

	/** 返回当前 Enhanced Player Input；输入子系统或 Player Input 不存在时返回 nullptr。 */
	/** Get the current player input from the current input subsystem */
	UE_API UEnhancedPlayerInput* GetPlayerInput() const;

	/**  */
	UFUNCTION(BlueprintCallable)
	const UInputAction* GetAssociatedAction() const { return AssociatedAction; }

	/** 返回通过 UPlayerInput::InputKey 注入数值时使用的当前按键。 */
	/** Returns the current key that will be used to input any values. */
	UFUNCTION(BlueprintCallable)
	FKey GetSimulatedKey() const { return KeyToSimulate; }

	// 将向量作为当前模拟按键的输入值注入 UPlayerInput。
	/**
	 * Injects the given vector as an input to the current simulated key.
	 * This calls "InputKey" on the current player.
	 */
	UFUNCTION(BlueprintCallable)
	UE_API void InputKeyValue(const FVector& Value);

	// 将向量直接注入关联 Enhanced Input Action，不执行该 Action 自带的 Modifier 和 Trigger。
	/**
	 * Injects the given vector as an input to the current simulated key.
	 * This calls "InputKey" on the current player.
	 */
	UFUNCTION(BlueprintCallable)
	UE_API void InputKeyValue2D(const FVector2D& Value);

	UFUNCTION(BlueprintCallable)
	UE_API void FlushSimulatedInput();
	
protected:

	/** 查询关联 Action 当前映射的按键，并更新 KeyToSimulate；无映射时使用后备按键。 */
	/** Set the KeyToSimulate based on a query from enhanced input about what keys are mapped to the associated action */
	UE_API void QueryKeyToSimulate();

	/** 玩家控制映射重建后调用，使模拟按键跟随用户重绑定结果。 */
	/** Called whenever control mappings change, so we have a chance to adapt our own keys */
	UFUNCTION()
	UE_API void OnControlMappingsRebuilt();

	/** Common Visibility Border，可按平台特征控制此模拟输入控件是否显示。 */
	/** The common visibility border will allow you to specify UI for only specific platforms if desired */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonHardwareVisibilityBorder> CommonVisibilityBorder = nullptr;
	
	/** 优先注入的 Enhanced Input Action。 */
	/** The associated input action that we should simulate input for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<const UInputAction> AssociatedAction = nullptr;

	/** 关联 Action 当前没有按键映射时采用的后备按键。 */
	/** The Key to simulate input for in the case where none are currently bound to the associated action */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FKey FallbackBindingKey = EKeys::Gamepad_Right2D;

	/** 当前实际传给 UPlayerInput::InputKey 的按键。 */
	/** The key that should be input via InputKey on the player input */
	FKey KeyToSimulate;
};

#undef UE_API
