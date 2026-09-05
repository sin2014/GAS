// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"

#include "CommonMessagingSubsystem.generated.h"

#define UE_API COMMONGAME_API

class FSubsystemCollectionBase;
class UCommonGameDialogDescriptor;
class UObject;

// 对话框可能返回的结果。
/** Possible results from a dialog */
UENUM(BlueprintType)
enum class ECommonMessagingResult : uint8
{
	// 用户按下“是”按钮。
	/** The "yes" button was pressed */
	Confirmed,
	// 用户按下“否”按钮。
	/** The "no" button was pressed */
	Declined,
	// 用户按下忽略或取消按钮。
	/** The "ignore/cancel" button was pressed */
	Cancelled,
	// 对话框在没有用户输入的情况下被显式关闭。
	/** The dialog was explicitly killed (no user input) */
	Killed,
	Unknown UMETA(Hidden)
};

DECLARE_DELEGATE_OneParam(FCommonMessagingResultDelegate, ECommonMessagingResult /* Result */);

/**
 * 
 */
UCLASS(MinimalAPI, config = Game)
class UCommonMessagingSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UCommonMessagingSubsystem() { }

	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	UE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UE_API virtual void ShowConfirmation(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback = FCommonMessagingResultDelegate());
	UE_API virtual void ShowError(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback = FCommonMessagingResultDelegate());

private:

};

#undef UE_API
