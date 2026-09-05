// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"

#include "LyraGamePhaseSubsystem.generated.h"

template <typename T> class TSubclassOf;

class ULyraGamePhaseAbility;
class UObject;
struct FFrame;
struct FGameplayAbilitySpecHandle;

DECLARE_DYNAMIC_DELEGATE_OneParam(FLyraGamePhaseDynamicDelegate, const ULyraGamePhaseAbility*, Phase);
DECLARE_DELEGATE_OneParam(FLyraGamePhaseDelegate, const ULyraGamePhaseAbility* Phase);

DECLARE_DYNAMIC_DELEGATE_OneParam(FLyraGamePhaseTagDynamicDelegate, const FGameplayTag&, PhaseTag);
DECLARE_DELEGATE_OneParam(FLyraGamePhaseTagDelegate, const FGameplayTag& PhaseTag);

// 阶段观察者匹配已开始或已结束阶段标签的规则。
// Match rule for message receivers
UENUM(BlueprintType)
enum class EPhaseTagMatchType : uint8
{
	// 仅匹配完全相同的阶段标签；例如监听 A.B 可收到 A.B，但不会收到 A.B.C。
	// An exact match will only receive messages with exactly the same channel
	// (e.g., registering for "A.B" will match a broadcast of A.B but not A.B.C)
	ExactMatch,

	// 匹配该标签及其所有子标签；例如监听 A.B 可同时收到 A.B 和 A.B.C。
	// A partial match will receive any messages rooted in the same channel
	// (e.g., registering for "A.B" will match a broadcast of A.B as well as A.B.C)
	PartialMatch
};


/**
 * 使用层级 GameplayTag 管理 Lyra 游戏阶段的 WorldSubsystem。
 * 父阶段与其子阶段可同时活动，但同级阶段不能共存。启动新阶段时，会结束所有不是其祖先的活动阶段。
 * 例如 Game.Playing 与 Game.Playing.WarmUp 可共存，而 Game.Playing 与 Game.ShowingScore 不可共存；
 * 当 Game.Playing 和 Game.Playing.CaptureTheFlag 活动时启动 Game.Playing.PostGame，父阶段 Game.Playing 保留，
 * 原有子阶段 Game.Playing.CaptureTheFlag 则结束。
 */
/** Subsystem for managing Lyra's game phases using gameplay tags in a nested manner, which allows parent and child 
 * phases to be active at the same time, but not sibling phases.
 * Example:  Game.Playing and Game.Playing.WarmUp can coexist, but Game.Playing and Game.ShowingScore cannot. 
 * When a new phase is started, any active phases that are not ancestors will be ended.
 * Example: if Game.Playing and Game.Playing.CaptureTheFlag are active when Game.Playing.PostGame is started, 
 *     Game.Playing will remain active, while Game.Playing.CaptureTheFlag will end.
 */
UCLASS()
class ULyraGamePhaseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	ULyraGamePhaseSubsystem();

	virtual void PostInitialize() override;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	void StartPhase(TSubclassOf<ULyraGamePhaseAbility> PhaseAbility, FLyraGamePhaseDelegate PhaseEndedCallback = FLyraGamePhaseDelegate());

	//TODO：应返回观察者句柄以支持主动注销，否则观察者数组会持续增长到 World 重置。
	//TODO：也可考虑周期性清理失效观察者，因为即使提供句柄，调用方也未必总会正确解绑。
	//TODO Return a handle so folks can delete these.  They will just grow until the world resets.
	//TODO Should we just occasionally clean these observers up?  It's not as if everyone will properly unhook them even if there is a handle.
	void WhenPhaseStartsOrIsActive(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, const FLyraGamePhaseTagDelegate& WhenPhaseActive);
	void WhenPhaseEnds(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, const FLyraGamePhaseTagDelegate& WhenPhaseEnd);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, BlueprintPure = false, meta = (AutoCreateRefTerm = "PhaseTag"))
	bool IsPhaseActive(const FGameplayTag& PhaseTag) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Phase", meta = (DisplayName="Start Phase", AutoCreateRefTerm = "PhaseEnded"))
	void K2_StartPhase(TSubclassOf<ULyraGamePhaseAbility> Phase, const FLyraGamePhaseDynamicDelegate& PhaseEnded);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Phase", meta = (DisplayName = "When Phase Starts or Is Active", AutoCreateRefTerm = "WhenPhaseActive"))
	void K2_WhenPhaseStartsOrIsActive(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, FLyraGamePhaseTagDynamicDelegate WhenPhaseActive);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Phase", meta = (DisplayName = "When Phase Ends", AutoCreateRefTerm = "WhenPhaseEnd"))
	void K2_WhenPhaseEnds(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, FLyraGamePhaseTagDynamicDelegate WhenPhaseEnd);

	void OnBeginPhase(const ULyraGamePhaseAbility* PhaseAbility, const FGameplayAbilitySpecHandle PhaseAbilityHandle);
	void OnEndPhase(const ULyraGamePhaseAbility* PhaseAbility, const FGameplayAbilitySpecHandle PhaseAbilityHandle);

private:
	struct FLyraGamePhaseEntry
	{
	public:
		FGameplayTag PhaseTag;
		FLyraGamePhaseDelegate PhaseEndedCallback;
	};

	TMap<FGameplayAbilitySpecHandle, FLyraGamePhaseEntry> ActivePhaseMap;

	struct FPhaseObserver
	{
	public:
		bool IsMatch(const FGameplayTag& ComparePhaseTag) const;
	
		FGameplayTag PhaseTag;
		EPhaseTagMatchType MatchType = EPhaseTagMatchType::ExactMatch;
		FLyraGamePhaseTagDelegate PhaseCallback;
	};

	TArray<FPhaseObserver> PhaseStartObservers;
	TArray<FPhaseObserver> PhaseEndObservers;

	friend class ULyraGamePhaseAbility;
};
