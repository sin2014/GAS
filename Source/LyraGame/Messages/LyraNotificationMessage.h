// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

#include "LyraNotificationMessage.generated.h"

class UObject;

LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Lyra_AddNotification_Message);

class APlayerState;

// 发往临时通知流的消息，例如淘汰播报或物品拾取记录；由 TargetChannel 决定具体展示位置和解释方式。
// A message destined for a transient log (e.g., an elimination feed or inventory pickup stream)
USTRUCT(BlueprintType)
struct FLyraNotificationMessage
{
	GENERATED_BODY()

	// 接收此通知的 GameplayTag 频道。
	// The destination channel
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	FGameplayTag TargetChannel;

	// 指定接收通知的玩家；为空时向所有本地玩家显示。
	// The target player (if none is set then it will display for all local players)
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	TObjectPtr<APlayerState> TargetPlayer = nullptr;

	// 实际显示给用户的文本。
	// The message to display
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	FText PayloadMessage;

	// 由目标频道自行解释的附加 GameplayTag，例如样式或定义标识。
	// Extra payload specific to the target channel (e.g., a style or definition asset)
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	FGameplayTag PayloadTag;

	// 由目标频道自行解释的附加对象，例如样式或定义资产。
	// Extra payload specific to the target channel (e.g., a style or definition asset)
	UPROPERTY(BlueprintReadWrite, Category=Notification)
	TObjectPtr<UObject> PayloadObject = nullptr;
};
