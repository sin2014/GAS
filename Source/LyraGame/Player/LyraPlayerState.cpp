// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerState.h"

#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraPawnData.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameModes/LyraExperienceManagerComponent.h"
//@TODO：进一步解耦 PawnData 获取流程，避免 PlayerState 直接依赖 GameMode 等模块。
//@TODO: Would like to isolate this a bit better to get the pawn data in here without this having to know about other stuff
#include "GameModes/LyraGameMode.h"
#include "LyraLogChannels.h"
#include "LyraPlayerController.h"
#include "Messages/LyraVerbMessage.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerState)

class AController;
class APlayerState;
class FLifetimeProperty;

// PlayerState 完成 PawnData 技能授予和 ASC 配置后发送的组件扩展事件名称。
const FName ALyraPlayerState::NAME_LyraAbilityReady("LyraAbilitiesReady");

// 创建并复制持久 ASC、HealthSet 和 CombatSet，初始化连接类型、队伍、小队及较高网络更新频率。
ALyraPlayerState::ALyraPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MyPlayerConnectionType(ELyraPlayerConnectionType::Player)
{
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ASC 会在 InitializeComponent 时发现这些 AttributeSet；成员引用确保它们在此之前不会被垃圾回收。
	// These attribute sets will be detected by AbilitySystemComponent::InitializeComponent. Keeping a reference so that the sets don't get garbage collected before that.
	HealthSet = CreateDefaultSubobject<ULyraHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<ULyraCombatSet>(TEXT("CombatSet"));

	// PlayerState 持有的 ASC 需要较高频率网络更新，因此提高 NetUpdateFrequency。
	// AbilitySystemComponent needs to be updated at a high frequency.
	SetNetUpdateFrequency(100.0f);

	MyTeamID = FGenericTeamId::NoTeam;
	MySquadID = INDEX_NONE;
}

// 在组件初始化前完成 ALyraPlayerState 的早期注册和依赖准备。
void ALyraPlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

// 重置 ALyraPlayerState 的瞬时玩法状态，并保留需要跨重置存在的数据。
void ALyraPlayerState::Reset()
{
	Super::Reset();
}

// 客户端完成 PlayerState 初始化后通知当前 PawnExtension 重新处理 PlayerState 复制依赖。
void ALyraPlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);

	if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
	{
		PawnExtComp->CheckDefaultInitialization();
	}
}

// 向替代 PlayerState 复制连接类型，并保留后续复制统计数据的扩展点。
void ALyraPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	//@TODO：在 PlayerState 迁移时复制需要保留的统计数据。
	//@TODO: Copy stats
}

// 根据连接类型决定断线 PlayerState 是否立即销毁，纯断线玩家默认清理以避免长期累积。
void ALyraPlayerState::OnDeactivated()
{
	bool bDestroyDeactivatedPlayerState = false;

	switch (GetPlayerConnectionType())
	{
		case ELyraPlayerConnectionType::Player:
		case ELyraPlayerConnectionType::InactivePlayer:
			//@TODO：由 Experience 决定断线 PlayerState 应立即销毁还是暂时保留。
			// 长时间运行的服务器若频繁有玩家进出，持续保留会造成对象累积。
			//@TODO: Ask the experience if we should destroy disconnecting players immediately or leave them around
			// (e.g., for long running servers where they might build up if lots of players cycle through)
			bDestroyDeactivatedPlayerState = true;
			break;
		default:
			bDestroyDeactivatedPlayerState = true;
			break;
	}
	
	SetPlayerConnectionType(ELyraPlayerConnectionType::InactivePlayer);

	if (bDestroyDeactivatedPlayerState)
	{
		Destroy();
	}
}

// PlayerState 从非活动状态恢复时重新标记为活动玩家。
void ALyraPlayerState::OnReactivated()
{
	if (GetPlayerConnectionType() == ELyraPlayerConnectionType::InactivePlayer)
	{
		SetPlayerConnectionType(ELyraPlayerConnectionType::Player);
	}
}

// 服务器在 Experience 加载完成后从 GameMode 获取 PawnData，并设置到持久 PlayerState。
void ALyraPlayerState::OnExperienceLoaded(const ULyraExperienceDefinition* /*CurrentExperience*/)
{
	if (ALyraGameMode* LyraGameMode = GetWorld()->GetAuthGameMode<ALyraGameMode>())
	{
		if (const ULyraPawnData* NewPawnData = LyraGameMode->GetPawnDataForController(GetOwningController()))
		{
			SetPawnData(NewPawnData);
		}
		else
		{
			UE_LOG(LogLyra, Error, TEXT("ALyraPlayerState::OnExperienceLoaded(): Unable to find PawnData to initialize player state [%s]!"), *GetNameSafe(this));
		}
	}
}

// 登记 ALyraPlayerState 需要通过网络复制的属性及复制条件。
void ALyraPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PawnData, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyPlayerConnectionType, SharedParams)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyTeamID, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MySquadID, SharedParams);

	SharedParams.Condition = ELifetimeCondition::COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedViewRotation, SharedParams);

	DOREPLIFETIME(ThisClass, StatTags);	
}

// 返回服务器复制的观战视角旋转。
FRotator ALyraPlayerState::GetReplicatedViewRotation() const
{
	// 当前直接返回复制属性，后续可改为自定义压缩复制。
	// Could replace this with custom replication
	return ReplicatedViewRotation;
}

// 仅在权威端更新观战视角旋转，供远端观战和回放复制。
void ALyraPlayerState::SetReplicatedViewRotation(const FRotator& NewRotation)
{
	if (NewRotation != ReplicatedViewRotation)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedViewRotation, this);
		ReplicatedViewRotation = NewRotation;
	}
}

// 将 Owner Controller 转换为 ALyraPlayerController；未关联或类型不匹配时返回 nullptr。
ALyraPlayerController* ALyraPlayerState::GetLyraPlayerController() const
{
	return Cast<ALyraPlayerController>(GetOwner());
}

// 返回由 PlayerState 持有、可跨 Pawn 更换保留的 Lyra ASC。
UAbilitySystemComponent* ALyraPlayerState::GetAbilitySystemComponent() const
{
	return GetLyraAbilitySystemComponent();
}

// 初始化 PlayerState 持有的 ASC OwnerActor，并在权威端等待 Experience 决定 PawnData。
void ALyraPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());

	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && World->GetNetMode() != NM_Client)
	{
		AGameStateBase* GameState = GetWorld()->GetGameState();
		check(GameState);
		ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
		check(ExperienceComponent);
		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
	}
}

// 权威端一次性设置 PawnData，授予 AbilitySet、配置标签关系映射并广播技能就绪扩展事件。
void ALyraPlayerState::SetPawnData(const ULyraPawnData* InPawnData)
{
	check(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogLyra, Error, TEXT("Trying to set PawnData [%s] on player state [%s] that already has valid PawnData [%s]."), *GetNameSafe(InPawnData), *GetNameSafe(this), *GetNameSafe(PawnData));
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PawnData, this);
	PawnData = InPawnData;

	for (const ULyraAbilitySet* AbilitySet : PawnData->AbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr);
		}
	}

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, NAME_LyraAbilityReady);
	
	ForceNetUpdate();
}

// PawnData 复制回调当前不执行附加操作，保留为客户端派生行为扩展点。
void ALyraPlayerState::OnRep_PawnData()
{
}

// 更新活动玩家、在线观战、回放观战或断线状态。
void ALyraPlayerState::SetPlayerConnectionType(ELyraPlayerConnectionType NewType)
{
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MyPlayerConnectionType, this);
	MyPlayerConnectionType = NewType;
}

// 仅在权威端更新 SquadId，并手动触发本地小队变化处理。
void ALyraPlayerState::SetSquadID(int32 NewSquadId)
{
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MySquadID, this);

		MySquadID = NewSquadId;
	}
}

// 仅在权威端更新 TeamId，并广播旧队伍到新队伍的变化。
void ALyraPlayerState::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (HasAuthority())
	{
		const FGenericTeamId OldTeamID = MyTeamID;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MyTeamID, this);
		MyTeamID = NewTeamID;
		ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);
	}
	else
	{
		UE_LOG(LogLyraTeams, Error, TEXT("Cannot set team for %s on non-authority"), *GetPathName(this));
	}
}

// 返回 PlayerState 复制的 MyTeamID。
FGenericTeamId ALyraPlayerState::GetGenericTeamId() const
{
	return MyTeamID;
}

// 返回 PlayerState 自身的队伍变化多播委托地址。
FOnLyraTeamIndexChangedDelegate* ALyraPlayerState::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

// MyTeamID 复制变化时向 PlayerState 的队伍监听者广播旧值和新值。
void ALyraPlayerState::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

// MySquadID 复制回调预留给后续 SquadSubsystem 通知，目前不执行附加操作。
void ALyraPlayerState::OnRep_MySquadID()
{
	//@TODO：小队子系统实现后，在此通知 SquadId 变化。
	//@TODO: Let the squad subsystem know (once that exists)
}

// 在权威端向复制的 StatTags 容器增加指定标签层数。
void ALyraPlayerState::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.AddStack(Tag, StackCount);
}

// 在权威端从复制的 StatTags 容器移除指定标签层数。
void ALyraPlayerState::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.RemoveStack(Tag, StackCount);
}

// 返回 StatTags 中指定标签的当前层数，不存在时为 0。
int32 ALyraPlayerState::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

// StatTags 中指定标签至少有一层时返回 true。
bool ALyraPlayerState::HasStatTag(FGameplayTag Tag) const
{
	return StatTags.ContainsTag(Tag);
}

// 在目标客户端通过 GameplayMessageSubsystem 广播不可靠 VerbMessage。
void ALyraPlayerState::ClientBroadcastMessage_Implementation(const FLyraVerbMessage Message)
{
	// 只在网络客户端转发该消息，避免 Standalone 模式重复执行本地动作。
	// This check is needed to prevent running the action when in standalone mode
	if (GetNetMode() == NM_Client)
	{
		UGameplayMessageSubsystem::Get(this).BroadcastMessage(Message.Verb, Message);
	}
}

