// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/LyraGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "NativeGameplayTags.h"

#include "LyraAbilitySystemComponent.generated.h"

#define UE_API LYRAGAME_API

class AActor;
class UGameplayAbility;
class ULyraAbilityTagRelationshipMapping;
class UObject;
struct FFrame;
struct FGameplayAbilityTargetDataHandle;

LYRAGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_AbilityInputBlocked);

/**
 * Lyra 项目的技能系统组件（ASC）基类，负责输入路由、激活组互斥、标签关系和技能失败反馈。
 */
/**
 * ULyraAbilitySystemComponent
 *
 *	Base ability system component class used by this project.
 */
UCLASS(MinimalAPI)
class ULyraAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UE_API ULyraAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	UE_API virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	typedef TFunctionRef<bool(const ULyraGameplayAbility* LyraAbility, FGameplayAbilitySpecHandle Handle)> TShouldCancelAbilityFunc;
	UE_API void CancelAbilitiesByFunc(TShouldCancelAbilityFunc ShouldCancelFunc, bool bReplicateCancelAbility);

	UE_API void CancelInputActivatedAbilities(bool bReplicateCancelAbility);

	UE_API void AbilityInputTagPressed(const FGameplayTag& InputTag);
	UE_API void AbilityInputTagReleased(const FGameplayTag& InputTag);

	UE_API void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	UE_API void ClearAbilityInput();

	UE_API bool IsActivationGroupBlocked(ELyraAbilityActivationGroup Group) const;
	UE_API void AddAbilityToActivationGroup(ELyraAbilityActivationGroup Group, ULyraGameplayAbility* LyraAbility);
	UE_API void RemoveAbilityFromActivationGroup(ELyraAbilityActivationGroup Group, ULyraGameplayAbility* LyraAbility);
	UE_API void CancelActivationGroupAbilities(ELyraAbilityActivationGroup Group, ULyraGameplayAbility* IgnoreLyraAbility, bool bReplicateCancelAbility);

	// 通过应用动态创建的 GameplayEffect，向 ASC 持续授予指定 GameplayTag。
	// Uses a gameplay effect to add the specified dynamic granted tag.
	UE_API void AddDynamicTagGameplayEffect(const FGameplayTag& Tag);

	// 移除所有用于授予指定动态标签的激活 GameplayEffect 实例，从而撤销该标签。
	// Removes all active instances of the gameplay effect that was used to add the specified dynamic granted tag.
	UE_API void RemoveDynamicTagGameplayEffect(const FGameplayTag& Tag);

	/** 读取指定 GameplayAbilitySpecHandle 与激活 PredictionKey 对应的已复制 TargetData。 */
	/** Gets the ability target data associated with the given ability handle and activation info */
	UE_API void GetAbilityTargetData(const FGameplayAbilitySpecHandle AbilityHandle, FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle& OutTargetDataHandle);

	/** 设置当前技能标签关系映射；传入 nullptr 时清除映射。 */
	/** Sets the current tag relationship mapping, if null it will clear it out */
	UE_API void SetTagRelationshipMapping(ULyraAbilityTagRelationshipMapping* NewMapping);
	
	/** 根据技能标签关系映射，汇总额外的激活所需标签与激活阻塞标签。 */
	/** Looks at ability tags and gathers additional required and blocking tags */
	UE_API void GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const;

	UE_API void TryActivateAbilitiesOnSpawn();

protected:

	UE_API virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	UE_API virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	UE_API virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	UE_API virtual void NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason) override;
	UE_API virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	UE_API virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags) override;
	UE_API virtual void HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled) override;

	/** 以不可靠 Client RPC 通知所属客户端某技能激活失败及其原因标签。 */
	/** Notify client that an ability failed to activate */
	UFUNCTION(Client, Unreliable)
	UE_API void ClientNotifyAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

	UE_API void HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);
protected:

	// 非空时，用该数据资产查询技能之间的激活条件、阻塞与取消关系。
	// If set, this table is used to look up tag relationships for activate and cancel
	UPROPERTY()
	TObjectPtr<ULyraAbilityTagRelationshipMapping> TagRelationshipMapping;

	// 本帧刚按下输入的 GameplayAbilitySpec 句柄。
	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// 本帧刚释放输入的 GameplayAbilitySpec 句柄。
	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// 当前输入仍保持按下状态的 GameplayAbilitySpec 句柄。
	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	// 各激活分组中当前正在运行的技能实例数量。
	// Number of abilities running in each activation group.
	int32 ActivationGroupCounts[(uint8)ELyraAbilityActivationGroup::MAX];
};

#undef UE_API
