// Copyright Epic Games, Inc. All Rights Reserved.

#include "Teams/LyraTeamSubsystem.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "LyraLogChannels.h"
#include "LyraTeamAgentInterface.h"
#include "LyraTeamCheats.h"
#include "LyraTeamPrivateInfo.h"
#include "LyraTeamPublicInfo.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamInfoBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTeamSubsystem)

class FSubsystemCollectionBase;

//////////////////////////////////////////////////////////////////////
// FLyraTeamTrackingInfo

// 按具体类型保存 PublicInfo 或 PrivateInfo；PublicInfo 同时更新 DisplayAsset 并广播变化。
void FLyraTeamTrackingInfo::SetTeamInfo(ALyraTeamInfoBase* Info)
{
	if (ALyraTeamPublicInfo* NewPublicInfo = Cast<ALyraTeamPublicInfo>(Info))
	{
		ensure((PublicInfo == nullptr) || (PublicInfo == NewPublicInfo));
		PublicInfo = NewPublicInfo;

		ULyraTeamDisplayAsset* OldDisplayAsset = DisplayAsset;
		DisplayAsset = NewPublicInfo->GetTeamDisplayAsset();

		if (OldDisplayAsset != DisplayAsset)
		{
			OnTeamDisplayAssetChanged.Broadcast(DisplayAsset);
		}
	}
	else if (ALyraTeamPrivateInfo* NewPrivateInfo = Cast<ALyraTeamPrivateInfo>(Info))
	{
		ensure((PrivateInfo == nullptr) || (PrivateInfo == NewPrivateInfo));
		PrivateInfo = NewPrivateInfo;
	}
	else
	{
		checkf(false, TEXT("Expected a public or private team info but got %s"), *GetPathNameSafe(Info))
	}
}

// 仅移除与传入 Actor 相同的公开或私有信息；移除 PublicInfo 时清空 DisplayAsset 并广播。
void FLyraTeamTrackingInfo::RemoveTeamInfo(ALyraTeamInfoBase* Info)
{
	if (PublicInfo == Info)
	{
		PublicInfo = nullptr;
	}
	else if (PrivateInfo == Info)
	{
		PrivateInfo = nullptr;
	}
	else
	{
		ensureMsgf(false, TEXT("Expected a previously registered team info but got %s"), *GetPathNameSafe(Info));
	}
}

//////////////////////////////////////////////////////////////////////
// ULyraTeamSubsystem

// 构造 WorldSubsystem，并初始化空 TeamMap 与 CheatManager 注册句柄。
ULyraTeamSubsystem::ULyraTeamSubsystem()
{
}

// 初始化队伍子系统，并为新创建的 CheatManager 注册 LyraTeamCheats 扩展。
void ULyraTeamSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	auto AddTeamCheats = [](UCheatManager* CheatManager)
	{
		CheatManager->AddCheatManagerExtension(NewObject<ULyraTeamCheats>(CheatManager));
	};

	CheatManagerRegistrationHandle = UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(AddTeamCheats));
}

// 注销 CheatManager 创建回调，再执行 WorldSubsystem 清理。
void ULyraTeamSubsystem::Deinitialize()
{
	UCheatManager::UnregisterFromOnCheatManagerCreated(CheatManagerRegistrationHandle);

	Super::Deinitialize();
}

// 以有效 TeamId 创建或取得跟踪条目，并写入 PublicInfo 或 PrivateInfo；参数无效时返回 false。
bool ULyraTeamSubsystem::RegisterTeamInfo(ALyraTeamInfoBase* TeamInfo)
{
	if (!ensure(TeamInfo))
	{
		return false;
	}

	const int32 TeamId = TeamInfo->GetTeamId();
	if (ensure(TeamId != INDEX_NONE))
	{
		FLyraTeamTrackingInfo& Entry = TeamMap.FindOrAdd(TeamId);
		Entry.SetTeamInfo(TeamInfo);

		return true;
	}

	return false;
}

// 从对应 TeamId 条目移除 TeamInfo；找不到条目时视为旧 World 残留并返回 false。
bool ULyraTeamSubsystem::UnregisterTeamInfo(ALyraTeamInfoBase* TeamInfo)
{
	if (!ensure(TeamInfo))
	{
		return false;
	}

	const int32 TeamId = TeamInfo->GetTeamId();
	if (ensure(TeamId != INDEX_NONE))
	{
		FLyraTeamTrackingInfo* Entry = TeamMap.Find(TeamId);

		// 找不到记录通常表示 Actor 来自已切换的旧 World，直接忽略其注销请求。
		// If it couldn't find the entry, this is probably a leftover actor from a previous world, ignore it
		if (Entry)
		{
			Entry->RemoveTeamInfo(TeamInfo);

			return true;
		}
	}

	return false;
}

// 优先修改目标关联的 LyraPlayerState，否则调用 Actor 自身队伍接口；两者都不存在时返回 false。
bool ULyraTeamSubsystem::ChangeTeamForActor(AActor* ActorToChange, int32 NewTeamIndex)
{
	const FGenericTeamId NewTeamID = IntegerToGenericTeamId(NewTeamIndex);
	if (ALyraPlayerState* LyraPS = const_cast<ALyraPlayerState*>(FindPlayerStateFromActor(ActorToChange)))
	{
		LyraPS->SetGenericTeamId(NewTeamID);
		return true;
	}
	else if (ILyraTeamAgentInterface* TeamActor = Cast<ILyraTeamAgentInterface>(ActorToChange))
	{
		TeamActor->SetGenericTeamId(NewTeamID);
		return true;
	}
	else
	{
		return false;
	}
}

// 依次从对象队伍接口、Actor Instigator、TeamInfo 或关联 PlayerState 解析 TeamId；均失败时返回 INDEX_NONE。
int32 ULyraTeamSubsystem::FindTeamFromObject(const UObject* TestObject) const
{
	// 优先从对象自身实现的队伍接口读取 TeamId。
	// See if it's directly a team agent
	if (const ILyraTeamAgentInterface* ObjectWithTeamInterface = Cast<ILyraTeamAgentInterface>(TestObject))
	{
		return GenericTeamIdToInteger(ObjectWithTeamInterface->GetGenericTeamId());
	}

	if (const AActor* TestActor = Cast<const AActor>(TestObject))
	{
		// 对普通 Actor，尝试继承其 Instigator 的队伍归属。
		// See if the instigator is a team actor
		if (const ILyraTeamAgentInterface* InstigatorWithTeamInterface = Cast<ILyraTeamAgentInterface>(TestActor->GetInstigator()))
		{
			return GenericTeamIdToInteger(InstigatorWithTeamInterface->GetGenericTeamId());
		}

		// TeamInfo Actor 本身不实现队伍接口，需要直接读取其 TeamId。
		// TeamInfo actors don't actually have the team interface, so they need a special case
		if (const ALyraTeamInfoBase* TeamInfo = Cast<ALyraTeamInfoBase>(TestActor))
		{
			return TeamInfo->GetTeamId();
		}

		// 最后回退到 Pawn 或 Controller 关联的 PlayerState。
		// Fall back to finding the associated player state
		if (const ALyraPlayerState* LyraPS = FindPlayerStateFromActor(TestActor))
		{
			return LyraPS->GetTeamId();
		}
	}

	return INDEX_NONE;
}

// 从 Pawn、Controller 或 PlayerState 本身取得 LyraPlayerState；不支持的 Actor 类型返回 nullptr。
const ALyraPlayerState* ULyraTeamSubsystem::FindPlayerStateFromActor(const AActor* PossibleTeamActor) const
{
	if (PossibleTeamActor != nullptr)
	{
		if (const APawn* Pawn = Cast<const APawn>(PossibleTeamActor))
		{
			//@TODO：考虑改用接口，或让队伍 Actor 向子系统注册并维护映射。
			//@TODO: Consider an interface instead or have team actors register with the subsystem and have it maintain a map? (or LWC style)
			if (ALyraPlayerState* LyraPS = Pawn->GetPlayerState<ALyraPlayerState>())
			{
				return LyraPS;
			}
		}
		else if (const AController* PC = Cast<const AController>(PossibleTeamActor))
		{
			if (ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(PC->PlayerState))
			{
				return LyraPS;
			}
		}
		else if (const ALyraPlayerState* LyraPS = Cast<const ALyraPlayerState>(PossibleTeamActor))
		{
			return LyraPS; 
		}

		// Try the instigator
// 		if (AActor* Instigator = PossibleTeamActor->GetInstigator())
// 		{
// 			if (ensure(Instigator != PossibleTeamActor))
// 			{
// 				return FindPlayerStateFromActor(Instigator);
// 			}
// 		}
	}

	return nullptr;
}

// 解析两个对象 TeamId；任一未分配则返回 InvalidArgument，否则按相等关系返回同队或异队。
ELyraTeamComparison ULyraTeamSubsystem::CompareTeams(const UObject* A, const UObject* B, int32& TeamIdA, int32& TeamIdB) const
{
	TeamIdA = FindTeamFromObject(Cast<const AActor>(A));
	TeamIdB = FindTeamFromObject(Cast<const AActor>(B));

	if ((TeamIdA == INDEX_NONE) || (TeamIdB == INDEX_NONE))
	{
		return ELyraTeamComparison::InvalidArgument;
	}
	else
	{
		return (TeamIdA == TeamIdB) ? ELyraTeamComparison::OnSameTeam : ELyraTeamComparison::DifferentTeams;
	}
}

// 解析两个对象 TeamId；任一未分配则返回 InvalidArgument，否则按相等关系返回同队或异队。
ELyraTeamComparison ULyraTeamSubsystem::CompareTeams(const UObject* A, const UObject* B) const
{
	int32 TeamIdA;
	int32 TeamIdB;
	return CompareTeams(A, B, /*out*/ TeamIdA, /*out*/ TeamIdB);
}

// 为蓝图输出对象是否属于有效队伍及解析出的 TeamId。
void ULyraTeamSubsystem::FindTeamFromActor(const UObject* TestObject, bool& bIsPartOfTeam, int32& TeamId) const
{
	TeamId = FindTeamFromObject(TestObject);
	bIsPartOfTeam = TeamId != INDEX_NONE;
}

// 仅在已注册队伍的权威 PublicInfo 上增加标签层数；客户端、过早调用或未知 TeamId 会记录错误。
void ULyraTeamSubsystem::AddTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount)
{
	auto FailureHandler = [&](const FString& ErrorMessage)
	{
		UE_LOG(LogLyraTeams, Error, TEXT("AddTeamTagStack(TeamId: %d, Tag: %s, StackCount: %d) %s"), TeamId, *Tag.ToString(), StackCount, *ErrorMessage);
	};

	if (FLyraTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		if (Entry->PublicInfo)
		{
			if (Entry->PublicInfo->HasAuthority())
			{
				Entry->PublicInfo->TeamTags.AddStack(Tag, StackCount);
			}
			else
			{
				FailureHandler(TEXT("failed because it was called on a client"));
			}
		}
		else
		{
			FailureHandler(TEXT("failed because there is no team info spawned yet (called too early, before the experience was ready)"));
		}
	}
	else
	{
		FailureHandler(TEXT("failed because it was passed an unknown team id"));
	}
}

// 仅在已注册队伍的权威 PublicInfo 上移除标签层数；客户端、过早调用或未知 TeamId 会记录错误。
void ULyraTeamSubsystem::RemoveTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount)
{
	auto FailureHandler = [&](const FString& ErrorMessage)
	{
		UE_LOG(LogLyraTeams, Error, TEXT("RemoveTeamTagStack(TeamId: %d, Tag: %s, StackCount: %d) %s"), TeamId, *Tag.ToString(), StackCount, *ErrorMessage);
	};

	if (FLyraTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		if (Entry->PublicInfo)
		{
			if (Entry->PublicInfo->HasAuthority())
			{
				Entry->PublicInfo->TeamTags.RemoveStack(Tag, StackCount);
			}
			else
			{
				FailureHandler(TEXT("failed because it was called on a client"));
			}
		}
		else
		{
			FailureHandler(TEXT("failed because there is no team info spawned yet (called too early, before the experience was ready)"));
		}
	}
	else
	{
		FailureHandler(TEXT("failed because it was passed an unknown team id"));
	}
}

// 返回 PublicInfo 与 PrivateInfo 中指定标签层数之和；未知 TeamId 记录日志并返回 0。
int32 ULyraTeamSubsystem::GetTeamTagStackCount(int32 TeamId, FGameplayTag Tag) const
{
	if (const FLyraTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		const int32 PublicStackCount = (Entry->PublicInfo != nullptr) ? Entry->PublicInfo->TeamTags.GetStackCount(Tag) : 0;
		const int32 PrivateStackCount = (Entry->PrivateInfo != nullptr) ? Entry->PrivateInfo->TeamTags.GetStackCount(Tag) : 0;
		return PublicStackCount + PrivateStackCount;
	}
	else
	{
		UE_LOG(LogLyraTeams, Verbose, TEXT("GetTeamTagStackCount(TeamId: %d, Tag: %s) failed because it was passed an unknown team id"), TeamId, *Tag.ToString());
		return 0;
	}
}

// 队伍公开和私有标签总层数大于 0 时返回 true。
bool ULyraTeamSubsystem::TeamHasTag(int32 TeamId, FGameplayTag Tag) const
{
	return GetTeamTagStackCount(TeamId, Tag) > 0;
}

// TeamMap 中存在指定 TeamId 条目时返回 true。
bool ULyraTeamSubsystem::DoesTeamExist(int32 TeamId) const
{
	return TeamMap.Contains(TeamId);
}

// 提取 TeamMap 全部键并按升序返回。
TArray<int32> ULyraTeamSubsystem::GetTeamIDs() const
{
	TArray<int32> Result;
	TeamMap.GenerateKeyArray(Result);
	Result.Sort();
	return Result;
}

// 允许配置的自伤和异队伤害，拒绝同队伤害；临时允许有 ASC 的无队伍练习目标受伤。
bool ULyraTeamSubsystem::CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowDamageToSelf) const
{
	if (bAllowDamageToSelf)
	{
		if ((Instigator == Target) || (FindPlayerStateFromActor(Cast<AActor>(Instigator)) == FindPlayerStateFromActor(Cast<AActor>(Target))))
		{
			return true;
		}
	}

	int32 InstigatorTeamId;
	int32 TargetTeamId;
	const ELyraTeamComparison Relationship = CompareTeams(Instigator, Target, /*out*/ InstigatorTeamId, /*out*/ TargetTeamId);
	if (Relationship == ELyraTeamComparison::DifferentTeams)
	{
		return true;
	}
	else if ((Relationship == ELyraTeamComparison::InvalidArgument) && (InstigatorTeamId != INDEX_NONE))
	{
		// 临时允许有 ASC 的无队伍目标受伤，以支持尚未分配队伍的练习靶。
		//@TODO：练习靶获得正式队伍归属后移除此例外。
		// Allow damaging non-team actors for now, as long as they have an ability system component
		//@TODO: This is temporary until the target practice dummy has a team assignment
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Cast<const AActor>(Target)) != nullptr;
	}

	return false;
}

// 返回指定 TeamId 当前公开 DisplayAsset；未知队伍返回 nullptr，ViewerTeamId 暂未参与重映射。
ULyraTeamDisplayAsset* ULyraTeamSubsystem::GetTeamDisplayAsset(int32 TeamId, int32 ViewerTeamId)
{
	// 当前尚未按 ViewerTeamId 重映射显示资产，直接返回目标队伍资产。
	// Currently ignoring ViewerTeamId

	if (FLyraTeamTrackingInfo* Entry = TeamMap.Find(TeamId))
	{
		return Entry->DisplayAsset;
	}

	return nullptr;
}

// 先解析观察者对象 TeamId，再调用显示资产查询接口取得目标队伍表现。
ULyraTeamDisplayAsset* ULyraTeamSubsystem::GetEffectiveTeamDisplayAsset(int32 TeamId, UObject* ViewerTeamAgent)
{
	return GetTeamDisplayAsset(TeamId, FindTeamFromObject(ViewerTeamAgent));
}

// 编辑器修改显示资产时向所有已注册队伍广播当前 DisplayAsset，强制颜色观察者刷新。
void ULyraTeamSubsystem::NotifyTeamDisplayAssetModified(ULyraTeamDisplayAsset* /*ModifiedAsset*/)
{
	// 当前编辑任一显示资产都会通知所有队伍观察者，而非仅通知被编辑资产对应的队伍。
	// Broadcasting to all observers when a display asset is edited right now, instead of only the edited one
	for (const auto& KVP : TeamMap)
	{
		const int32 TeamId = KVP.Key;
		const FLyraTeamTrackingInfo& TrackingInfo = KVP.Value;

		TrackingInfo.OnTeamDisplayAssetChanged.Broadcast(TrackingInfo.DisplayAsset);
	}
}

// 返回指定 TeamId 的显示资产变化委托；TeamId 尚无条目时创建空跟踪记录。
FOnLyraTeamDisplayAssetChangedDelegate& ULyraTeamSubsystem::GetTeamDisplayAssetChangedDelegate(int32 TeamId)
{
	return TeamMap.FindOrAdd(TeamId).OnTeamDisplayAssetChanged;
}

