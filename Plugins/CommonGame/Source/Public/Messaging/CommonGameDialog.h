// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "CommonMessagingSubsystem.h"

#include "CommonGameDialog.generated.h"

#define UE_API COMMONGAME_API

USTRUCT(BlueprintType)
struct FConfirmationDialogAction
{
	GENERATED_BODY()

public:
	// 必填：该按钮返回的对话框结果。
	/** Required: The dialog option to provide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECommonMessagingResult Result = ECommonMessagingResult::Unknown;

	// 可选：覆盖结果所关联输入动作的默认显示文本。
	/** Optional: Display Text to use instead of the action name associated with the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText OptionalDisplayText;

	bool operator==(const FConfirmationDialogAction& Other) const
	{
		return Result == Other.Result &&
			OptionalDisplayText.EqualTo(Other.OptionalDisplayText);
	}
};

UCLASS(MinimalAPI)
class UCommonGameDialogDescriptor : public UObject
{
	GENERATED_BODY()
	
public:
	static UE_API UCommonGameDialogDescriptor* CreateConfirmationOk(const FText& Header, const FText& Body);
	static UE_API UCommonGameDialogDescriptor* CreateConfirmationOkCancel(const FText& Header, const FText& Body);
	static UE_API UCommonGameDialogDescriptor* CreateConfirmationYesNo(const FText& Header, const FText& Body);
	static UE_API UCommonGameDialogDescriptor* CreateConfirmationYesNoCancel(const FText& Header, const FText& Body);

public:
	// 对话框要显示的标题。
	/** The header of the message to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Header;
	
	// 对话框要显示的正文。
	/** The body of the message to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Body;

	// 确认按钮使用的输入动作。
	/** The confirm button's input action to use. */
	UPROPERTY(BlueprintReadWrite)
	TArray<FConfirmationDialogAction> ButtonActions;
};


UCLASS(MinimalAPI, Abstract)
class UCommonGameDialog : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	UE_API UCommonGameDialog();
	
	UE_API virtual void SetupDialog(UCommonGameDialogDescriptor* Descriptor, FCommonMessagingResultDelegate ResultCallback);

	UE_API virtual void KillDialog();
};

#undef UE_API
