// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Accolades/LyraAccoladeDefinition.h"
#include "AsyncMixin.h"
#include "CommonUserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "LyraAccoladeHostWidget.generated.h"

class UObject;
class USoundBase;
class UUserWidget;
struct FDataRegistryAcquireResult;
struct FLyraNotificationMessage;

USTRUCT(BlueprintType)
struct FPendingAccoladeEntry
{
	GENERATED_BODY();

	UPROPERTY(BlueprintReadOnly)
	FLyraAccoladeDefinitionRow Row; 

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> Icon = nullptr;

	UPROPERTY()
	TObjectPtr<UUserWidget> AllocatedWidget = nullptr;

	int32 SequenceID = 0;

	bool bFinishedLoading = false;

	void CancelDisplay();
};

/** 监听嘉奖通知、按接收顺序异步加载 Data Registry 行及资源，并逐个定时展示符合位置标签的嘉奖。 */
/**
 * 
 */
UCLASS(BlueprintType)
class ULyraAccoladeHostWidget : public UCommonUserWidget, public FAsyncMixin
{
	GENERATED_BODY()

public:
	// 本 Host Widget 负责的展示位置标签，用于过滤异步加载完成的嘉奖。
	// The location tag (used to filter incoming messages to only display the appropriate accolades in a given location)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag LocationName;

	// UUserWidget 接口开始。
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// UUserWidget 接口结束。
	//~End of UUserWidget interface

	UFUNCTION(BlueprintImplementableEvent)
	void DestroyAccoladeWidget(UUserWidget* Widget);

	UFUNCTION(BlueprintImplementableEvent)
	UUserWidget* CreateAccoladeWidget(const FPendingAccoladeEntry& Entry);
private:
	FGameplayMessageListenerHandle ListenerHandle;

	int32 NextDisplaySequenceID = 0;
	int32 AllocatedSequenceID = 0;

	FTimerHandle NextTimeToReconsiderHandle;

	// 正在异步读取 Registry 行或加载资源的嘉奖；完成顺序可能与通知顺序不同。
	// List of async pending load accolades (which might come in the wrong order due to the row read)
	UPROPERTY(Transient)
	TArray<FPendingAccoladeEntry> PendingAccoladeLoads;

	// 已加载且等待逐个展示的嘉奖；首项是当前可见项，其余项等待展示时长结束。
	// List of pending accolades (due to one at a time display duration; the first one in the list is the current visible one)
	UPROPERTY(Transient)
	TArray<FPendingAccoladeEntry> PendingAccoladeDisplays;


	void OnNotificationMessage(FGameplayTag Channel, const FLyraNotificationMessage& Notification);
	void OnRegistryLoadCompleted(const FDataRegistryAcquireResult& AccoladeHandle, int32 SequenceID);

	void ConsiderLoadedAccolades();
	void PopDisplayedAccolade();
	void ProcessLoadedAccolade(const FPendingAccoladeEntry& Entry);
	void DisplayNextAccolade();
};
