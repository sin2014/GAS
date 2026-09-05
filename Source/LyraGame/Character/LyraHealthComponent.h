// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameFrameworkComponent.h"

#include "LyraHealthComponent.generated.h"

#define UE_API LYRAGAME_API

class ULyraHealthComponent;

class ULyraAbilitySystemComponent;
class ULyraHealthSet;
class UObject;
struct FFrame;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLyraHealth_DeathEvent, AActor*, OwningActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FLyraHealth_AttributeChanged, ULyraHealthComponent*, HealthComponent, float, OldValue, float, NewValue, AActor*, Instigator);

/**
 * 表示角色死亡流程从存活到开始死亡、再到完成死亡的复制状态。
 */
/**
 * ELyraDeathState
 *
 *	Defines current state of death.
 */
UENUM(BlueprintType)
enum class ELyraDeathState : uint8
{
	NotDead = 0,
	DeathStarted,
	DeathFinished
};


/**
 * 将 ASC 中的生命属性与标准死亡流程封装为可复用的 Actor 组件。
 */
/**
 * ULyraHealthComponent
 *
 *	An actor component used to handle anything related to health.
 */
UCLASS(MinimalAPI, Blueprintable, Meta=(BlueprintSpawnableComponent))
class ULyraHealthComponent : public UGameFrameworkComponent
{
	GENERATED_BODY()

public:

	UE_API ULyraHealthComponent(const FObjectInitializer& ObjectInitializer);

	// 返回指定 Actor 上的 HealthComponent；不存在时返回 nullptr。
	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Lyra|Health")
	static ULyraHealthComponent* FindHealthComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<ULyraHealthComponent>() : nullptr); }

	// 使用指定 ASC 绑定生命 AttributeSet、属性变化委托和死亡事件。
	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Health")
	UE_API void InitializeWithAbilitySystem(ULyraAbilitySystemComponent* InASC);

	// 解除属性委托并清除对 ASC 和生命 AttributeSet 的引用。
	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Health")
	UE_API void UninitializeFromAbilitySystem();

	// 返回当前生命值。
	// Returns the current health value.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Health")
	UE_API float GetHealth() const;

	// 返回当前最大生命值。
	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Health")
	UE_API float GetMaxHealth() const;

	// 返回当前生命值占最大生命值的比例，范围为 [0.0, 1.0]。
	// Returns the current health in the range [0.0, 1.0].
	UFUNCTION(BlueprintCallable, Category = "Lyra|Health")
	UE_API float GetHealthNormalized() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Health")
	ELyraDeathState GetDeathState() const { return DeathState; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Lyra|Health", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool IsDeadOrDying() const { return (DeathState > ELyraDeathState::NotDead); }

	// 将死亡状态推进到 DeathStarted，并广播死亡开始事件。
	// Begins the death sequence for the owner.
	UE_API virtual void StartDeath();

	// 将死亡状态推进到 DeathFinished，并广播死亡完成事件。
	// Ends the death sequence for the owner.
	UE_API virtual void FinishDeath();

	// 通过自毁伤害向拥有者施加足以致死的伤害。
	// Applies enough damage to kill the owner.
	UE_API virtual void DamageSelfDestruct(bool bFellOutOfWorld = false);

public:

	// 生命值变化时触发；客户端也会调用，但 Instigator 可能无效。
	// Delegate fired when the health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FLyraHealth_AttributeChanged OnHealthChanged;

	// 最大生命值变化时触发；客户端也会调用，但 Instigator 可能无效。
	// Delegate fired when the max health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FLyraHealth_AttributeChanged OnMaxHealthChanged;

	// 死亡流程开始时触发。
	// Delegate fired when the death sequence has started.
	UPROPERTY(BlueprintAssignable)
	FLyraHealth_DeathEvent OnDeathStarted;

	// 死亡流程完成时触发。
	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FLyraHealth_DeathEvent OnDeathFinished;

protected:

	UE_API virtual void OnUnregister() override;

	UE_API void ClearGameplayTags();

	UE_API virtual void HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	UE_API virtual void HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	UE_API virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);

	UFUNCTION()
	UE_API virtual void OnRep_DeathState(ELyraDeathState OldDeathState);

protected:

	// 本组件绑定的 ASC。
	// Ability system used by this component.
	UPROPERTY()
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	// 从 ASC 取得并缓存的生命 AttributeSet。
	// Health set used by this component.
	UPROPERTY()
	TObjectPtr<const ULyraHealthSet> HealthSet;

	// 用于在网络两端同步并驱动死亡流程的复制状态。
	// Replicated state used to handle dying.
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	ELyraDeathState DeathState;
};

#undef UE_API
