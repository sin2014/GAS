// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "GenericTeamAgentInterface.h"
#include "TargetActor_Around.generated.h"

/**
 * 
 */
UCLASS()
class ATargetActor_Around : public AGameplayAbilityTargetActor, public IGenericTeamAgentInterface
{
	GENERATED_BODY()
public:
	ATargetActor_Around();
	void ConfigureDetection(float DetectionRadius, const FGenericTeamId& InTeamId, const FGameplayTag& InLocalGameplayCueTag);

	/** Assigns Team Agent to given TeamID */
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID);
	
	/** Retrieve team identifier in form of FGenericTeamId */
	FORCEINLINE virtual FGenericTeamId GetGenericTeamId() const { return TeamId; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UPROPERTY(Replicated)
	FGenericTeamId TeamId;

	UPROPERTY(VisibleDefaultsOnly, Category = "Comp")
	class USceneComponent* RootComp;

	UPROPERTY(VisibleDefaultsOnly, Category = "Targeting")
	class USphereComponent* DetectionSphere;

	UPROPERTY(ReplicatedUsing = OnRep_TargetDetectionRadiusReplicated)
	float TargetDetectionRadius;

	UFUNCTION()
	void OnRep_TargetDetectionRadiusReplicated();

	UPROPERTY(Replicated)
	FGameplayTag LocalGameplayCueTag;

	UFUNCTION()
	void ActorInDetectionRange(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);
};

