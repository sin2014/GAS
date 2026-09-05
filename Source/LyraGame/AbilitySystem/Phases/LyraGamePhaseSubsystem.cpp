// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "LyraGamePhaseAbility.h"
#include "LyraGamePhaseLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGamePhaseSubsystem)

class ULyraGameplayAbility;
class UObject;

DEFINE_LOG_CATEGORY(LogLyraGamePhase);

//////////////////////////////////////////////////////////////////////
// ULyraGamePhaseSubsystem

// 构造 World 级阶段子系统，活动阶段与观察者集合初始为空。
ULyraGamePhaseSubsystem::ULyraGamePhaseSubsystem()
{
}

// WorldSubsystem 初始化完成后的扩展点，当前仅执行父类逻辑。
void ULyraGamePhaseSubsystem::PostInitialize()
{
	Super::PostInitialize();
}

// 在父类允许的 World 中创建阶段子系统；当前未额外限制为仅权威 World。
bool ULyraGamePhaseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (Super::ShouldCreateSubsystem(Outer))
	{
		//UWorld* World = Cast<UWorld>(Outer);
		//check(World);

		//return World->GetAuthGameMode() != nullptr;
		//return nullptr;
		return true;
	}

	return false;
}

// 仅在 Game、PIE 和受支持的编辑器预览 World 中启用阶段管理。
bool ULyraGamePhaseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// 在 GameState ASC 上授予并激活阶段技能，并保存一次性结束回调；仅权威端应调用。
void ULyraGamePhaseSubsystem::StartPhase(TSubclassOf<ULyraGamePhaseAbility> PhaseAbility, FLyraGamePhaseDelegate PhaseEndedCallback)
{
	UWorld* World = GetWorld();
	ULyraAbilitySystemComponent* GameState_ASC = World->GetGameState()->FindComponentByClass<ULyraAbilitySystemComponent>();
	if (ensure(GameState_ASC))
	{
		FGameplayAbilitySpec PhaseSpec(PhaseAbility, 1, 0, this);
		FGameplayAbilitySpecHandle SpecHandle = GameState_ASC->GiveAbilityAndActivateOnce(PhaseSpec);
		FGameplayAbilitySpec* FoundSpec = GameState_ASC->FindAbilitySpecFromHandle(SpecHandle);
		
		if (FoundSpec && FoundSpec->IsActive())
		{
			FLyraGamePhaseEntry& Entry = ActivePhaseMap.FindOrAdd(SpecHandle);
			Entry.PhaseEndedCallback = PhaseEndedCallback;
		}
		else
		{
			PhaseEndedCallback.ExecuteIfBound(nullptr);
		}
	}
}

// 蓝图入口：桥接动态结束委托后启动指定阶段技能。
void ULyraGamePhaseSubsystem::K2_StartPhase(TSubclassOf<ULyraGamePhaseAbility> PhaseAbility, const FLyraGamePhaseDynamicDelegate& PhaseEndedDelegate)
{
	const FLyraGamePhaseDelegate EndedDelegate = FLyraGamePhaseDelegate::CreateWeakLambda(const_cast<UObject*>(PhaseEndedDelegate.GetUObject()), [PhaseEndedDelegate](const ULyraGamePhaseAbility* PhaseAbility) {
		PhaseEndedDelegate.ExecuteIfBound(PhaseAbility);
	});

	StartPhase(PhaseAbility, EndedDelegate);
}

// 蓝图入口：注册阶段开始观察者，并在目标阶段已活动时立即回调。
void ULyraGamePhaseSubsystem::K2_WhenPhaseStartsOrIsActive(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, FLyraGamePhaseTagDynamicDelegate WhenPhaseActive)
{
	const FLyraGamePhaseTagDelegate ActiveDelegate = FLyraGamePhaseTagDelegate::CreateWeakLambda(WhenPhaseActive.GetUObject(), [WhenPhaseActive](const FGameplayTag& PhaseTag) {
		WhenPhaseActive.ExecuteIfBound(PhaseTag);
	});

	WhenPhaseStartsOrIsActive(PhaseTag, MatchType, ActiveDelegate);
}

// 蓝图入口：注册指定标签匹配规则的阶段结束观察者。
void ULyraGamePhaseSubsystem::K2_WhenPhaseEnds(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, FLyraGamePhaseTagDynamicDelegate WhenPhaseEnd)
{
	const FLyraGamePhaseTagDelegate EndedDelegate = FLyraGamePhaseTagDelegate::CreateWeakLambda(WhenPhaseEnd.GetUObject(), [WhenPhaseEnd](const FGameplayTag& PhaseTag) {
		WhenPhaseEnd.ExecuteIfBound(PhaseTag);
	});

	WhenPhaseEnds(PhaseTag, MatchType, EndedDelegate);
}

// 保存原生阶段开始观察者，并对当前已匹配的活动阶段立即执行一次回调。
void ULyraGamePhaseSubsystem::WhenPhaseStartsOrIsActive(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, const FLyraGamePhaseTagDelegate& WhenPhaseActive)
{
	FPhaseObserver Observer;
	Observer.PhaseTag = PhaseTag;
	Observer.MatchType = MatchType;
	Observer.PhaseCallback = WhenPhaseActive;
	PhaseStartObservers.Add(Observer);

	if (IsPhaseActive(PhaseTag))
	{
		WhenPhaseActive.ExecuteIfBound(PhaseTag);
	}
}

// 保存原生阶段结束观察者，供阶段退出时按匹配规则通知。
void ULyraGamePhaseSubsystem::WhenPhaseEnds(FGameplayTag PhaseTag, EPhaseTagMatchType MatchType, const FLyraGamePhaseTagDelegate& WhenPhaseEnd)
{
	FPhaseObserver Observer;
	Observer.PhaseTag = PhaseTag;
	Observer.MatchType = MatchType;
	Observer.PhaseCallback = WhenPhaseEnd;
	PhaseEndObservers.Add(Observer);
}

// 若任一活动阶段等于或派生自 PhaseTag，则视为该阶段处于活动状态。
bool ULyraGamePhaseSubsystem::IsPhaseActive(const FGameplayTag& PhaseTag) const
{
	for (const auto& KVP : ActivePhaseMap)
	{
		const FLyraGamePhaseEntry& PhaseEntry = KVP.Value;
		if (PhaseEntry.PhaseTag.MatchesTag(PhaseTag))
		{
			return true;
		}
	}

	return false;
}

// 阶段技能开始时结束非祖先活动阶段、登记新阶段，并通知所有匹配的开始观察者。
void ULyraGamePhaseSubsystem::OnBeginPhase(const ULyraGamePhaseAbility* PhaseAbility, const FGameplayAbilitySpecHandle PhaseAbilityHandle)
{
	const FGameplayTag IncomingPhaseTag = PhaseAbility->GetGamePhaseTag();

	UE_LOG(LogLyraGamePhase, Log, TEXT("Beginning Phase '%s' (%s)"), *IncomingPhaseTag.ToString(), *GetNameSafe(PhaseAbility));

	const UWorld* World = GetWorld();
	ULyraAbilitySystemComponent* GameState_ASC = World->GetGameState()->FindComponentByClass<ULyraAbilitySystemComponent>();
	if (ensure(GameState_ASC))
	{
		TArray<FGameplayAbilitySpec*> ActivePhases;
		for (const auto& KVP : ActivePhaseMap)
		{
			const FGameplayAbilitySpecHandle ActiveAbilityHandle = KVP.Key;
			if (FGameplayAbilitySpec* Spec = GameState_ASC->FindAbilitySpecFromHandle(ActiveAbilityHandle))
			{
				ActivePhases.Add(Spec);
			}
		}

		for (const FGameplayAbilitySpec* ActivePhase : ActivePhases)
		{
			const ULyraGamePhaseAbility* ActivePhaseAbility = CastChecked<ULyraGamePhaseAbility>(ActivePhase->Ability);
			const FGameplayTag ActivePhaseTag = ActivePhaseAbility->GetGamePhaseTag();
			
			// 若新阶段等于当前阶段或位于当前阶段之下，则保留当前阶段；同一阶段标签也允许由多个技能共同表示。
			// 例如 Game.Playing 活动时可启动 Game.Playing.SuddenDeath，并继续保留父阶段 Game.Playing。
			// 随后启动同级的 Game.Playing.ActualSuddenDeath 会结束 SuddenDeath，但仍保留 Game.Playing；
			// 若改为启动 Game.GameOver，则所有 Game.Playing.* 阶段及 Game.Playing 本身都会结束。
			// So if the active phase currently matches the incoming phase tag, we allow it.
			// i.e. multiple gameplay abilities can all be associated with the same phase tag.
			// For example,
			// You can be in the, Game.Playing, phase, and then start a sub-phase, like Game.Playing.SuddenDeath
			// Game.Playing phase will still be active, and if someone were to push another one, like,
			// Game.Playing.ActualSuddenDeath, it would end Game.Playing.SuddenDeath phase, but Game.Playing would
			// continue.  Similarly if we activated Game.GameOver, all the Game.Playing* phases would end.
			if (!IncomingPhaseTag.MatchesTag(ActivePhaseTag))
			{
				UE_LOG(LogLyraGamePhase, Log, TEXT("\tEnding Phase '%s' (%s)"), *ActivePhaseTag.ToString(), *GetNameSafe(ActivePhaseAbility));

				FGameplayAbilitySpecHandle HandleToEnd = ActivePhase->Handle;
				GameState_ASC->CancelAbilitiesByFunc([HandleToEnd](const ULyraGameplayAbility* LyraAbility, FGameplayAbilitySpecHandle Handle) {
					return Handle == HandleToEnd;
				}, true);
			}
		}

		FLyraGamePhaseEntry& Entry = ActivePhaseMap.FindOrAdd(PhaseAbilityHandle);
		Entry.PhaseTag = IncomingPhaseTag;

		// 阶段登记为活动后，通知所有与新阶段标签匹配的开始观察者。
		// Notify all observers of this phase that it has started.
		for (const FPhaseObserver& Observer : PhaseStartObservers)
		{
			if (Observer.IsMatch(IncomingPhaseTag))
			{
				Observer.PhaseCallback.ExecuteIfBound(IncomingPhaseTag);
			}
		}
	}
}

// 阶段技能结束时执行对应结束回调、移除活动记录，并通知匹配的结束观察者。
void ULyraGamePhaseSubsystem::OnEndPhase(const ULyraGamePhaseAbility* PhaseAbility, const FGameplayAbilitySpecHandle PhaseAbilityHandle)
{
	const FGameplayTag EndedPhaseTag = PhaseAbility->GetGamePhaseTag();
	UE_LOG(LogLyraGamePhase, Log, TEXT("Ended Phase '%s' (%s)"), *EndedPhaseTag.ToString(), *GetNameSafe(PhaseAbility));

	const FLyraGamePhaseEntry& Entry = ActivePhaseMap.FindChecked(PhaseAbilityHandle);
	Entry.PhaseEndedCallback.ExecuteIfBound(PhaseAbility);

	ActivePhaseMap.Remove(PhaseAbilityHandle);

	// 从活动阶段表移除后，通知所有与该阶段标签匹配的结束观察者。
	// Notify all observers of this phase that it has ended.
	for (const FPhaseObserver& Observer : PhaseEndObservers)
	{
		if (Observer.IsMatch(EndedPhaseTag))
		{
			Observer.PhaseCallback.ExecuteIfBound(EndedPhaseTag);
		}
	}
}

// 按 ExactMatch 或父标签 PartialMatch 规则判断观察者是否接收该阶段事件。
bool ULyraGamePhaseSubsystem::FPhaseObserver::IsMatch(const FGameplayTag& ComparePhaseTag) const
{
	switch(MatchType)
	{
	case EPhaseTagMatchType::ExactMatch:
		return ComparePhaseTag == PhaseTag;
	case EPhaseTagMatchType::PartialMatch:
		return ComparePhaseTag.MatchesTag(PhaseTag);
	}

	return false;
}
