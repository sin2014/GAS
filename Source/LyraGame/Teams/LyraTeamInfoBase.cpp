// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTeamInfoBase.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTeamInfoBase)

class FLifetimeProperty;

// 构造始终相关且可复制的 TeamInfo Actor，并将 TeamId 初始化为 INDEX_NONE。
ALyraTeamInfoBase::ALyraTeamInfoBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TeamId(INDEX_NONE)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	NetPriority = 3.0f;
	SetReplicatingMovement(false);
}

// 登记 TeamTags 和 TeamId 的网络复制。
void ALyraTeamInfoBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TeamTags);
	DOREPLIFETIME_CONDITION(ThisClass, TeamId, COND_InitialOnly);
}

// TeamInfo 开始运行时尝试向当前 World 的 TeamSubsystem 注册。
void ALyraTeamInfoBase::BeginPlay()
{
	Super::BeginPlay();

	TryRegisterWithTeamSubsystem();
}

// Actor 结束时若 TeamSubsystem 仍存在则注销 TeamInfo，避免旧 World 记录残留。
void ALyraTeamInfoBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TeamId != INDEX_NONE)
	{
		ULyraTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
		if (TeamSubsystem)
		{
			// EndPlay 可能发生在 WorldSubsystem 已销毁之后，因此仅在子系统仍有效时注销。
			// EndPlay can happen at weird times where the subsystem has already been destroyed
			TeamSubsystem->UnregisterTeamInfo(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// 把当前 TeamInfo 注册到指定 TeamSubsystem。
void ALyraTeamInfoBase::RegisterWithTeamSubsystem(ULyraTeamSubsystem* Subsystem)
{
	Subsystem->RegisterTeamInfo(this);
}

// TeamId 有效且 WorldSubsystem 可用时注册；条件不足时等待复制或生命周期再次触发。
void ALyraTeamInfoBase::TryRegisterWithTeamSubsystem()
{
	if (TeamId != INDEX_NONE)
	{
		ULyraTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
		if (ensure(TeamSubsystem))
		{
			RegisterWithTeamSubsystem(TeamSubsystem);
		}
	}
}

// 仅允许权威端且仅设置一次 TeamId，随后立即尝试注册 TeamInfo。
void ALyraTeamInfoBase::SetTeamId(int32 NewTeamId)
{
	check(HasAuthority());
	check(TeamId == INDEX_NONE);
	check(NewTeamId != INDEX_NONE);

	TeamId = NewTeamId;

	TryRegisterWithTeamSubsystem();
}

// 客户端收到有效 TeamId 后向本地 TeamSubsystem 注册对应 TeamInfo。
void ALyraTeamInfoBase::OnRep_TeamId()
{
	TryRegisterWithTeamSubsystem();
}

