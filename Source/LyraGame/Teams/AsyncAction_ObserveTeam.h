// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CancellableAsyncAction.h"
#include "UObject/ScriptInterface.h"
#include "UObject/WeakInterfacePtr.h"

#include "AsyncAction_ObserveTeam.generated.h"

class ILyraTeamAgentInterface;
struct FFrame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTeamObservedAsyncDelegate, bool, bTeamSet, int32, TeamId);

/**
 * 异步观察指定对象的队伍归属，并在归属变化时持续向蓝图广播。
 */
/**
 * Watches for team changes in the specified object
 */
UCLASS()
class UAsyncAction_ObserveTeam : public UCancellableAsyncAction
{
	GENERATED_UCLASS_BODY()

public:
	// 开始观察指定 TeamAgent：激活后立即广播一次当前队伍；
	// 若对象实现 ILyraTeamAgentInterface，则继续监听后续队伍变化。
	// Watches for team changes on the specified team agent
	//  - It will will fire once immediately to give the current team assignment
	//  - For anything that can ever belong to a team (implements ILyraTeamAgentInterface),
	//    it will also listen for team assignment changes in the future
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", Keywords="Watch"))
	static UAsyncAction_ObserveTeam* ObserveTeam(UObject* TeamAgent);

	//~UBlueprintAsyncActionBase interface
	virtual void Activate() override;
	virtual void SetReadyToDestroy() override;
	//~End of UBlueprintAsyncActionBase interface

public:
	// 初始队伍解析完成或之后队伍发生变化时广播。
	// Called when the team is set or changed
	UPROPERTY(BlueprintAssignable)
	FTeamObservedAsyncDelegate OnTeamChanged;

private:
	// 使用弱接口引用创建实际观察任务，避免异步动作延长目标对象生命周期。
	// Watches for team changes on the specified team actor
	static UAsyncAction_ObserveTeam* InternalObserveTeamChanges(TScriptInterface<ILyraTeamAgentInterface> TeamActor);

private:
	UFUNCTION()
	void OnWatchedAgentChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

	TWeakInterfacePtr<ILyraTeamAgentInterface> TeamInterfacePtr;
};
