// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActionWidget.h"
#include "LyraActionWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;

/** 显示 Common Input 操作当前实际绑定按键图标的控件，并优先反映 Enhanced Input 的用户重绑定结果。 */
/** An action widget that will get the icon of key that is currently assigned to the common input action on this widget */
UCLASS(BlueprintType, Blueprintable)
class ULyraActionWidget : public UCommonActionWidget
{
	GENERATED_BODY()

public:

	//~ Begin UCommonActionWidget interface
	virtual FSlateBrush GetIcon() const override;
	//~ End of UCommonActionWidget interface

	/** 与此 Common Input 操作对应的 Enhanced Input Action。 */
	/** The Enhanced Input Action that is associated with this Common Input action. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	const TObjectPtr<UInputAction> AssociatedInputAction;

private:

	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
	
};
