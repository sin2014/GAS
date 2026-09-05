// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPawnExtensionComponent.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Components/GameFrameworkComponentDelegates.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "LyraGameplayTags.h"
#include "LyraLogChannels.h"
#include "LyraPawnData.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPawnExtensionComponent)

class FLifetimeProperty;
class UActorComponent;

// PawnExtension 向 GameFramework 初始化状态系统注册的特性名称，其他组件用它声明和监听初始化依赖。
const FName ULyraPawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

// 构造 PawnExtensionComponent，启用复制并关闭 Tick，等待 PawnData 和 ASC 初始化。
ULyraPawnExtensionComponent::ULyraPawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	PawnData = nullptr;
	AbilitySystemComponent = nullptr;
}

// 登记 ULyraPawnExtensionComponent 需要通过网络复制的属性及复制条件。
void ULyraPawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULyraPawnExtensionComponent, PawnData);
}

// 在 ULyraPawnExtensionComponent 注册到所属 Actor 时校验拥有者并准备组件状态。
void ULyraPawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf((Pawn != nullptr), TEXT("LyraPawnExtensionComponent on [%s] can only be added to Pawn actors."), *GetNameSafe(GetOwner()));

	TArray<UActorComponent*> PawnExtensionComponents;
	Pawn->GetComponents(ULyraPawnExtensionComponent::StaticClass(), PawnExtensionComponents);
	ensureAlwaysMsgf((PawnExtensionComponents.Num() == 1), TEXT("Only one LyraPawnExtensionComponent should exist on [%s]."), *GetNameSafe(GetOwner()));

	// 尽早向组件初始化状态系统注册；只有游戏世界中的 Actor 才会实际注册成功。
	// Register with the init state system early, this will only work if this is a game world
	RegisterInitStateFeature();
}

// 在 BeginPlay 阶段启动 ULyraPawnExtensionComponent 的运行时监听和初始化流程。
void ULyraPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 监听该 Actor 上所有初始化特性的状态变化，以便依赖满足后继续推进。
	// Listen for changes to all features
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	
	// 通知状态管理器本组件已进入 Spawned，并立即尝试推进后续默认初始化阶段。
	// Notifies state manager that we have spawned, then try rest of default initialization
	ensure(TryToChangeInitState(LyraGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

// 在 EndPlay 阶段解除 ULyraPawnExtensionComponent 的委托、状态注册和外部引用。
void ULyraPawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeAbilitySystem();
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

// 仅由权威端设置 PawnData，拒绝重复赋值，并通过复制与初始化状态链向其他组件提供配置。
void ULyraPawnExtensionComponent::SetPawnData(const ULyraPawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();

	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogLyra, Error, TEXT("Trying to set PawnData [%s] on pawn [%s] that already has valid PawnData [%s]."), *GetNameSafe(InPawnData), *GetNameSafe(Pawn), *GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;

	Pawn->ForceNetUpdate();

	CheckDefaultInitialization();
}

// PawnData 复制到客户端后重新检查初始化链，使等待配置数据的组件继续推进。
void ULyraPawnExtensionComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
}

// 将指定 ASC 的 OwnerActor 设为持久拥有者、当前 Pawn 设为 AvatarActor；若旧 Pawn 仍占用该 ASC，则先解除旧 Avatar。
void ULyraPawnExtensionComponent::InitializeAbilitySystem(ULyraAbilitySystemComponent* InASC, AActor* InOwnerActor)
{
	check(InASC);
	check(InOwnerActor);

	if (AbilitySystemComponent == InASC)
	{
		// ASC 未发生变化，无需重复初始化。
		// The ability system component hasn't changed.
		return;
	}

	if (AbilitySystemComponent)
	{
		// 先解除旧 ASC 与本 Pawn 的 AvatarActor 关系及相关绑定。
		// Clean up the old ability system component.
		UninitializeAbilitySystem();
	}

	APawn* Pawn = GetPawnChecked<APawn>();
	AActor* ExistingAvatar = InASC->GetAvatarActor();

	UE_LOG(LogLyra, Verbose, TEXT("Setting up ASC [%s] on pawn [%s] owner [%s], existing [%s] "), *GetNameSafe(InASC), *GetNameSafe(Pawn), *GetNameSafe(InOwnerActor), *GetNameSafe(ExistingAvatar));

	if ((ExistingAvatar != nullptr) && (ExistingAvatar != Pawn))
	{
		UE_LOG(LogLyra, Log, TEXT("Existing avatar (authority=%d)"), ExistingAvatar->HasAuthority() ? 1 : 0);

		// ASC 已绑定另一个 Pawn 作为 AvatarActor，需要先让旧 Pawn 退出。
		// 客户端延迟时可能先生成并接管新 Pawn，随后才移除死亡的旧 Pawn，因此会出现这种情况。
		// There is already a pawn acting as the ASC's avatar, so we need to kick it out
		// This can happen on clients if they're lagged: their new pawn is spawned + possessed before the dead one is removed
		ensure(!ExistingAvatar->HasAuthority());

		if (ULyraPawnExtensionComponent* OtherExtensionComponent = FindPawnExtensionComponent(ExistingAvatar))
		{
			OtherExtensionComponent->UninitializeAbilitySystem();
		}
	}

	AbilitySystemComponent = InASC;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, Pawn);

	if (ensure(PawnData))
	{
		InASC->SetTagRelationshipMapping(PawnData->TagRelationshipMapping);
	}

	OnAbilitySystemInitialized.Broadcast();
}

// 仅在所属 Pawn 仍是 ASC 的 AvatarActor 时取消非跨死亡技能、清理输入和 GameplayCue，并解除 Avatar 关系。
void ULyraPawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 仅当本 Pawn 仍是 ASC 的 AvatarActor 时解除初始化；若新 Pawn 已接管，则它已处理旧绑定。
	// Uninitialize the ASC if we're still the avatar actor (otherwise another pawn already did it when they became the avatar actor)
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		FGameplayTagContainer AbilityTypesToIgnore;
		AbilityTypesToIgnore.AddTag(LyraGameplayTags::Ability_Behavior_SurvivesDeath);

		AbilitySystemComponent->CancelAbilities(nullptr, &AbilityTypesToIgnore);
		AbilitySystemComponent->ClearAbilityInput();
		AbilitySystemComponent->RemoveAllGameplayCues();

		if (AbilitySystemComponent->GetOwnerActor() != nullptr)
		{
			AbilitySystemComponent->SetAvatarActor(nullptr);
		}
		else
		{
			// 若 ASC 已无有效 OwnerActor，必须清空完整 ActorInfo，而不只是解除 AvatarActor。
			// If the ASC doesn't have a valid owner, we need to clear *all* actor info, not just the avatar pairing
			AbilitySystemComponent->ClearActorInfo();
		}

		OnAbilitySystemUninitialized.Broadcast();
	}

	AbilitySystemComponent = nullptr;
}

// Controller 变化时刷新有效 ASC 的 ActorInfo；若 ASC 已失去 OwnerActor 则解除初始化，最后重新检查状态链。
void ULyraPawnExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && (AbilitySystemComponent->GetAvatarActor() == GetPawnChecked<APawn>()))
	{
		ensure(AbilitySystemComponent->AbilityActorInfo->OwnerActor == AbilitySystemComponent->GetOwnerActor());
		if (AbilitySystemComponent->GetOwnerActor() == nullptr)
		{
			UninitializeAbilitySystem();
		}
		else
		{
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}
	}

	CheckDefaultInitialization();
}

// PlayerState 复制完成后重新检查初始化链，使依赖 PlayerState 和其 ASC 的组件继续推进。
void ULyraPawnExtensionComponent::HandlePlayerStateReplicated()
{
	CheckDefaultInitialization();
}

// InputComponent 创建完成后重新检查依赖，使本地输入相关特性可以进入下一初始化阶段。
void ULyraPawnExtensionComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
}

// 先推动依赖特性，再沿默认初始化状态链尝试把 Pawn 推进到 GameplayReady。
void ULyraPawnExtensionComponent::CheckDefaultInitialization()
{
	// 检查自身状态前，先推动可能被本组件依赖的其他初始化特性。
	// Before checking our progress, try progressing any other features we might depend on
	CheckDefaultInitializationForImplementers();

	static const TArray<FGameplayTag> StateChain = { LyraGameplayTags::InitState_Spawned, LyraGameplayTags::InitState_DataAvailable, LyraGameplayTags::InitState_DataInitialized, LyraGameplayTags::InitState_GameplayReady };

	// 沿既定状态链从 BeginPlay 设置的 Spawned 开始，依次尝试推进到 GameplayReady。
	// This will try to progress from spawned (which is only set in BeginPlay) through the data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}

// 按 Spawned、DataAvailable、DataInitialized、GameplayReady 状态链检查 PawnData、Controller 及其他组件依赖是否满足。
bool ULyraPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();
	if (!CurrentState.IsValid() && DesiredState == LyraGameplayTags::InitState_Spawned)
	{
		// 只要组件挂在有效 Pawn 上，就允许进入 Spawned。
		// As long as we are on a valid pawn, we count as spawned
		if (Pawn)
		{
			return true;
		}
	}
	if (CurrentState == LyraGameplayTags::InitState_Spawned && DesiredState == LyraGameplayTags::InitState_DataAvailable)
	{
		// 进入 DataAvailable 前必须已经取得 PawnData。
		// Pawn data is required.
		if (!PawnData)
		{
			return false;
		}

		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();

		if (bHasAuthority || bIsLocallyControlled)
		{
			// 权威端或自主代理还必须已被 Controller 接管。
			// Check for being possessed by a controller.
			if (!GetController<AController>())
			{
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == LyraGameplayTags::InitState_DataAvailable && DesiredState == LyraGameplayTags::InitState_DataInitialized)
	{
		// 只有同一 Pawn 上所有初始化特性都达到 DataAvailable，才能进入 DataInitialized。
		// Transition to initialize if all features have their data available
		return Manager->HaveAllFeaturesReachedInitState(Pawn, LyraGameplayTags::InitState_DataAvailable);
	}
	else if (CurrentState == LyraGameplayTags::InitState_DataInitialized && DesiredState == LyraGameplayTags::InitState_GameplayReady)
	{
		return true;
	}

	return false;
}

// 响应 PawnExtension 状态迁移；具体初始化工作由监听该状态的其他 Pawn 组件完成。
void ULyraPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (DesiredState == LyraGameplayTags::InitState_DataInitialized)
	{
		// 进入 DataInitialized 后的具体工作由监听该状态变化的其他组件完成。
		// This is currently all handled by other components listening to this state change
	}
}

// 其他组件初始化状态变化时重新运行默认初始化检查，以推进 PawnExtension 的依赖链。
void ULyraPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	// 其他特性刚进入 DataAvailable 时，重新判断本组件能否推进到 DataInitialized。
	// If another feature is now in DataAvailable, see if we should transition to DataInitialized
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		if (Params.FeatureState == LyraGameplayTags::InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}

// 注册 ASC 初始化委托；若当前 Pawn 已是 AvatarActor，则立即执行一次回调。
void ULyraPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemInitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemInitialized.Add(Delegate);
	}

	if (AbilitySystemComponent)
	{
		Delegate.Execute();
	}
}

// 注册 Pawn 不再作为 ASC AvatarActor 时触发的委托。
void ULyraPawnExtensionComponent::OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemUninitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemUninitialized.Add(Delegate);
	}
}

