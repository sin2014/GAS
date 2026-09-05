// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularPawn.h"
#include "Teams/LyraTeamAgentInterface.h"

#include "LyraPawn.generated.h"

#define UE_API LYRAGAME_API

class AController;
class UObject;
struct FFrame;

/**
 * Lyra 的通用模块化 Pawn 基类，提供团队归属与接管生命周期处理。
 */
/**
 * ALyraPawn
 */
UCLASS(MinimalAPI)
class ALyraPawn : public AModularPawn, public ILyraTeamAgentInterface
{
	GENERATED_BODY()

public:

	UE_API ALyraPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~APawn interface
	UE_API virtual void PossessedBy(AController* NewController) override;
	UE_API virtual void UnPossessed() override;
	//~End of APawn interface

	//~ILyraTeamAgentInterface interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnLyraTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	//~End of ILyraTeamAgentInterface interface

protected:
	// 决定 Pawn 失去 Controller 后应保留、清除还是改为其他 TeamId。
	// Called to determine what happens to the team ID when possession ends
	virtual FGenericTeamId DetermineNewTeamAfterPossessionEnds(FGenericTeamId OldTeamID) const
	{
		// 可改为返回 OldTeamID 以保留原队伍，也可返回中立阵营等自定义 TeamId。
		// This could be changed to return, e.g., OldTeamID if you want to keep it assigned afterwards, or return an ID for some neutral faction, or etc...
		return FGenericTeamId::NoTeam;
	}

private:
	UFUNCTION()
	UE_API void OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

private:
	UPROPERTY(ReplicatedUsing = OnRep_MyTeamID)
	FGenericTeamId MyTeamID;

	UPROPERTY()
	FOnLyraTeamIndexChangedDelegate OnTeamChangedDelegate;

private:
	UFUNCTION()
	UE_API void OnRep_MyTeamID(FGenericTeamId OldTeamID);
};

#undef UE_API
