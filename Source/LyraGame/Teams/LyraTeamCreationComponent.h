// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"

#include "LyraTeamCreationComponent.generated.h"

class ULyraExperienceDefinition;
class ALyraTeamPublicInfo;
class ALyraTeamPrivateInfo;
class ALyraPlayerState;
class AGameModeBase;
class APlayerController;
class ULyraTeamDisplayAsset;

UCLASS(Blueprintable)
class ULyraTeamCreationComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	ULyraTeamCreationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface

	//~UActorComponent interface
	virtual void BeginPlay() override;
	//~End of UActorComponent interface

private:
	void OnExperienceLoaded(const ULyraExperienceDefinition* Experience);

protected:
	// Experience 加载后由服务器创建的 TeamId 到显示资产映射；显示资产可为空。
	// List of teams to create (id to display asset mapping, the display asset can be left unset if desired)
	UPROPERTY(EditDefaultsOnly, Category = Teams)
	TMap<uint8, TObjectPtr<ULyraTeamDisplayAsset>> TeamsToCreate;

	UPROPERTY(EditDefaultsOnly, Category=Teams)
	TSubclassOf<ALyraTeamPublicInfo> PublicTeamInfoClass;

	UPROPERTY(EditDefaultsOnly, Category=Teams)
	TSubclassOf<ALyraTeamPrivateInfo> PrivateTeamInfoClass;

#if WITH_SERVER_CODE
protected:
	virtual void ServerCreateTeams();
	virtual void ServerAssignPlayersToTeams();

	/** 为指定 PlayerState 选择队伍；纯观战者会被清除所有队伍归属。 */
	/** Sets the team ID for the given player state. Spectator-only player states will be stripped of any team association. */
	virtual void ServerChooseTeamForPlayer(ALyraPlayerState* PS);

private:
	void OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);
	void ServerCreateTeam(int32 TeamId, ULyraTeamDisplayAsset* DisplayAsset);

	/** 返回活动玩家最少的 TeamId；没有有效队伍时返回 INDEX_NONE。 */
	/** returns the Team ID with the fewest active players, or INDEX_NONE if there are no valid teams */
	int32 GetLeastPopulatedTeamID() const;
#endif
};
