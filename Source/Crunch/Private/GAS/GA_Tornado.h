// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/CGameplayAbility.h"
#include "GA_Tornado.generated.h"

/**
 * 
 */
UCLASS()
class UGA_Tornado : public UCGameplayAbility
{
	GENERATED_BODY()
public:	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;	
	
private:
	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> HitDamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float HitPushSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Anim")
	UAnimMontage* TornadoMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Effect")
	float TornadoDuration = 4.f;

	UFUNCTION()
	void TornadoDamageEventReceived(FGameplayEventData Payload);
};

