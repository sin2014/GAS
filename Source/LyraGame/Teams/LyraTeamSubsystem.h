// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "LyraTeamSubsystem.generated.h"

#define UE_API LYRAGAME_API

class AActor;
class ALyraPlayerState;
class ALyraTeamInfoBase;
class ALyraTeamPrivateInfo;
class ALyraTeamPublicInfo;
class FSubsystemCollectionBase;
class ULyraTeamDisplayAsset;
struct FFrame;
struct FGameplayTag;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLyraTeamDisplayAssetChangedDelegate, const ULyraTeamDisplayAsset*, DisplayAsset);

USTRUCT()
struct FLyraTeamTrackingInfo
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<ALyraTeamPublicInfo> PublicInfo = nullptr;

	UPROPERTY()
	TObjectPtr<ALyraTeamPrivateInfo> PrivateInfo = nullptr;

	UPROPERTY()
	TObjectPtr<ULyraTeamDisplayAsset> DisplayAsset = nullptr;

	UPROPERTY()
	FOnLyraTeamDisplayAssetChangedDelegate OnTeamDisplayAssetChanged;

public:
	void SetTeamInfo(ALyraTeamInfoBase* Info);
	void RemoveTeamInfo(ALyraTeamInfoBase* Info);
};

// 两个对象队伍关系的比较结果。
// Result of comparing the team affiliation for two actors
UENUM(BlueprintType)
enum class ELyraTeamComparison : uint8
{
	// 两个对象属于同一有效队伍。
	// Both actors are members of the same team
	OnSameTeam,

	// 两个对象属于不同的有效队伍。
	// The actors are members of opposing teams
	DifferentTeams,

	// 至少一个对象无效或没有队伍归属。
	// One (or both) of the actors was invalid or not part of any team
	InvalidArgument
};

/** World 级队伍注册表，统一解析队伍归属、权限关系、标签和显示资产。 */
/** A subsystem for easy access to team information for team-based actors (e.g., pawns or player states) */
UCLASS(MinimalAPI)
class ULyraTeamSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UE_API ULyraTeamSubsystem();

	//~USubsystem interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End of USubsystem interface

	// 注册 Public/Private TeamInfo，并更新对应 TeamId 的跟踪记录。
	// Tries to registers a new team
	UE_API bool RegisterTeamInfo(ALyraTeamInfoBase* TeamInfo);

	// 注销指定 TeamInfo；找不到对应队伍记录时返回 false。
	// Tries to unregister a team, will return false if it didn't work
	UE_API bool UnregisterTeamInfo(ALyraTeamInfoBase* TeamInfo);

	// 在权威端更改 Actor 或其关联 PlayerState 的 TeamId；目标不支持队伍接口时失败。
	// Changes the team associated with this actor if possible
	// Note: This function can only be called on the authority
	UE_API bool ChangeTeamForActor(AActor* ActorToChange, int32 NewTeamId);

	// 依次从对象自身、Instigator、TeamInfo 或关联 PlayerState 解析 TeamId。
	// Returns the team this object belongs to, or INDEX_NONE if it is not part of a team
	UE_API int32 FindTeamFromObject(const UObject* TestObject) const;

	// 返回 Pawn、Controller 或 PlayerState 对应的 LyraPlayerState；无关联时返回 nullptr。
	// Returns the associated player state for this actor, or INDEX_NONE if it is not associated with a player
	UE_API const ALyraPlayerState* FindPlayerStateFromActor(const AActor* PossibleTeamActor) const;

	// 为蓝图返回对象是否属于队伍及其 TeamId。
	// Returns the team this object belongs to, or INDEX_NONE if it is not part of a team
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Teams, meta=(Keywords="Get"))
	UE_API void FindTeamFromActor(const UObject* TestActor, bool& bIsPartOfTeam, int32& TeamId) const;

	// 比较两个对象的队伍，返回同队、异队或参数无效，并输出双方 TeamId。
	// Compare the teams of two actors and returns a value indicating if they are on same teams, different teams, or one/both are invalid
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Teams, meta=(ExpandEnumAsExecs=ReturnValue))
	UE_API ELyraTeamComparison CompareTeams(const UObject* A, const UObject* B, int32& TeamIdA, int32& TeamIdB) const;

	// 比较两个对象的队伍关系，不返回具体 TeamId。
	// Compare the teams of two actors and returns a value indicating if they are on same teams, different teams, or one/both are invalid
	UE_API ELyraTeamComparison CompareTeams(const UObject* A, const UObject* B) const;

	// 根据自伤许可、同队/异队关系和目标 ASC 状态判断是否允许造成伤害。
	// Returns true if the instigator can damage the target, taking into account the friendly fire settings
	UE_API bool CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowDamageToSelf = true) const;

	// 权威端向队伍公开 TeamInfo 的标签增加指定层数。
	// Adds a specified number of stacks to the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Teams)
	UE_API void AddTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount);

	// 权威端从队伍公开 TeamInfo 的标签移除指定层数。
	// Removes a specified number of stacks from the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Teams)
	UE_API void RemoveTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount);

	// 返回公开与私有 TeamInfo 中该标签层数之和；不存在时返回 0。
	// Returns the stack count of the specified tag (or 0 if the tag is not present)
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API int32 GetTeamTagStackCount(int32 TeamId, FGameplayTag Tag) const;

	// 指定队伍至少拥有一层该标签时返回 true。
	// Returns true if there is at least one stack of the specified tag
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API bool TeamHasTag(int32 TeamId, FGameplayTag Tag) const;

	// 指定 TeamId 已在 TeamMap 中注册时返回 true。
	// Returns true if the specified team exists
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API bool DoesTeamExist(int32 TeamId) const;

	// 从指定观察者队伍的视角取得目标队伍显示资产，支持“本地玩家永远显示为蓝队”等重映射规则。
	// Gets the team display asset for the specified team, from the perspective of the specified team
	// (You have to specify a viewer too, in case the game mode is in a 'local player is always blue team' sort of situation)
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API ULyraTeamDisplayAsset* GetTeamDisplayAsset(int32 TeamId, int32 ViewerTeamId);

	// 先解析 ViewerTeamAgent 的队伍，再取得该视角下目标 TeamId 的有效显示资产。
	// Gets the team display asset for the specified team, from the perspective of the specified team
	// (You have to specify a viewer too, in case the game mode is in a 'local player is always blue team' sort of situation)
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API ULyraTeamDisplayAsset* GetEffectiveTeamDisplayAsset(int32 TeamId, UObject* ViewerTeamAgent);

	// 返回已注册 TeamId 的升序列表。
	// Gets the list of teams
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Teams)
	UE_API TArray<int32> GetTeamIDs() const;

	// 编辑器中显示资产被修改后通知观察者刷新队伍颜色等表现。
	// Called when a team display asset has been edited, causes all team color observers to update
	UE_API void NotifyTeamDisplayAssetModified(ULyraTeamDisplayAsset* ModifiedAsset);

	// 返回指定 TeamId 的显示资产变化委托，供异步观察动作注册。
	// Register for a team display asset notification for the specified team ID
	UE_API FOnLyraTeamDisplayAssetChangedDelegate& GetTeamDisplayAssetChangedDelegate(int32 TeamId);

private:
	UPROPERTY()
	TMap<int32, FLyraTeamTrackingInfo> TeamMap;

	FDelegateHandle CheatManagerRegistrationHandle;
};

#undef UE_API
