// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"

#include "LyraPawnExtensionComponent.generated.h"

#define UE_API LYRAGAME_API

namespace EEndPlayReason { enum Type : int; }

class UGameFrameworkComponentManager;
class ULyraAbilitySystemComponent;
class ULyraPawnData;
class UObject;
struct FActorInitStateChangedParams;
struct FFrame;
struct FGameplayTag;

/**
 * 为角色、载具等所有 Pawn 类型提供通用扩展，并协调依赖组件的分阶段初始化。
 */
/**
 * Component that adds functionality to all Pawn classes so it can be used for characters/vehicles/etc.
 * This coordinates the initialization of other components.
 */
UCLASS(MinimalAPI)
class ULyraPawnExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:

	UE_API ULyraPawnExtensionComponent(const FObjectInitializer& ObjectInitializer);

	/** 该总控初始化特性的名称；它会等待同一 Actor 上其他具名特性达到所需状态。 */
	/** The name of this overall feature, this one depends on the other named component features */
	static UE_API const FName NAME_ActorFeatureName;

	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	UE_API virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	UE_API virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	UE_API virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	UE_API virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface

	/** 返回指定 Actor 上的 PawnExtensionComponent；不存在时返回 nullptr。 */
	/** Returns the pawn extension component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	static ULyraPawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<ULyraPawnExtensionComponent>() : nullptr); }

	/** 以指定类型取得描述该 Pawn 构造与玩法配置的 PawnData。 */
	/** Gets the pawn data, which is used to specify pawn properties in data */
	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** 设置当前 PawnData，并推动依赖该数据的初始化流程。 */
	/** Sets the current pawn data */
	UE_API void SetPawnData(const ULyraPawnData* InPawnData);

	/** 返回当前关联的 ASC；该组件的 OwnerActor 可能不是本 Pawn。 */
	/** Gets the current ability system component, which may be owned by a different actor */
	UFUNCTION(BlueprintPure, Category = "Lyra|Pawn")
	ULyraAbilitySystemComponent* GetLyraAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 由所属 Pawn 调用，使该 Pawn 成为指定 ASC 的 AvatarActor，同时保留 InOwnerActor 作为持久 OwnerActor。 */
	/** Should be called by the owning pawn to become the avatar of the ability system. */
	UE_API void InitializeAbilitySystem(ULyraAbilitySystemComponent* InASC, AActor* InOwnerActor);

	/** 由所属 Pawn 调用，在其仍是当前 AvatarActor 时解除与 ASC 的关联。 */
	/** Should be called by the owning pawn to remove itself as the avatar of the ability system. */
	UE_API void UninitializeAbilitySystem();

	/** Pawn 的 Controller 变化后调用，用于重新评估数据可用性和初始化状态。 */
	/** Should be called by the owning pawn when the pawn's controller changes. */
	UE_API void HandleControllerChanged();

	/** PlayerState 复制到客户端后调用，使依赖 PlayerState/ASC 的初始化可以继续。 */
	/** Should be called by the owning pawn when the player state has been replicated. */
	UE_API void HandlePlayerStateReplicated();

	/** Pawn 完成 InputComponent 创建与挂接后调用，使本地输入初始化可以继续。 */
	/** Should be called by the owning pawn when the input component is setup. */
	UE_API void SetupPlayerInputComponent();

	/** 注册 ASC 初始化回调；若本 Pawn 已是 ASC 的 AvatarActor，则立即执行一次。 */
	/** Register with the OnAbilitySystemInitialized delegate and broadcast if our pawn has been registered with the ability system component */
	UE_API void OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate);

	/** 注册 ASC 解除初始化回调；本 Pawn 不再作为 ASC 的 AvatarActor 时触发。 */
	/** Register with the OnAbilitySystemUninitialized delegate fired when our pawn is removed as the ability system's avatar actor */
	UE_API void OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate);

protected:

	UE_API virtual void OnRegister() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	UE_API void OnRep_PawnData();

	/** 本 Pawn 成为 ASC 的 AvatarActor 后触发。 */
	/** Delegate fired when our pawn becomes the ability system's avatar actor */
	FSimpleMulticastDelegate OnAbilitySystemInitialized;

	/** 本 Pawn 不再是 ASC 的 AvatarActor 后触发。 */
	/** Delegate fired when our pawn is removed as the ability system's avatar actor */
	FSimpleMulticastDelegate OnAbilitySystemUninitialized;

	/** 用于构造和配置该 Pawn 的数据；可由生成流程设置，也可在关卡实例上指定。 */
	/** Pawn data used to create the pawn. Specified from a spawn function or on a placed instance. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "Lyra|Pawn")
	TObjectPtr<const ULyraPawnData> PawnData;

	/** 为便于访问而缓存的 ASC；其生命周期通常由 PlayerState 或 Pawn 自身管理。 */
	/** Pointer to the ability system component that is cached for convenience. */
	UPROPERTY(Transient)
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;
};

#undef UE_API
