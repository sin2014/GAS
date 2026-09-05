// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"

#include "UObject/ObjectPtr.h"
#include "AsyncAction_QueryReplays.generated.h"

class APlayerController;
class INetworkReplayStreamer;
class ULyraReplayList;
class UObject;
struct FEnumerateStreamsResult;
struct FFrame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQueryReplayAsyncDelegate, ULyraReplayList*, Results);

// 蓝图异步操作：通过回放流服务枚举与当前网络回放版本兼容的回放，并按录制时间从新到旧返回。
/**
 * Watches for team changes in the specified player controller
 */
UCLASS()
class UAsyncAction_QueryReplays : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UAsyncAction_QueryReplays(const FObjectInitializer& ObjectInitializer);

	// 为指定玩家控制器创建回放查询操作；玩家控制器用于保持调用上下文有效。
	// Watches for team changes in the specified player controller
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UAsyncAction_QueryReplays* QueryReplays(APlayerController* PlayerController);

	virtual void Activate() override;

public:
	// 回放枚举完成或无法创建回放流时广播结果列表。
	// Called when the replay query completes
	UPROPERTY(BlueprintAssignable)
	FQueryReplayAsyncDelegate QueryComplete;

private:
	void OnEnumerateStreamsComplete(const FEnumerateStreamsResult& Result);

private:
	UPROPERTY()
	TObjectPtr<ULyraReplayList> ResultList;

	TWeakObjectPtr<APlayerController> PlayerController;

	TSharedPtr<INetworkReplayStreamer> ReplayStreamer;
};
