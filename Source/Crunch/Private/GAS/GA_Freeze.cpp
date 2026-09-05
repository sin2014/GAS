// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/GA_Freeze.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/CAbilitySystemStatics.h"
#include "GAS/TargetActor_GroundPick.h"

UGA_Freeze::UGA_Freeze()
{
	ActivationOwnedTags.AddTag(UCAbilitySystemStatics::GetAimStatTag());
}

void UGA_Freeze::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!HasAuthorityOrPredictionKey(CurrentActorInfo, &CurrentActivationInfo))
	{
		return;
	}
	
	UAbilityTask_PlayMontageAndWait* PlayTargetingMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FreezeTargetingMontage);
	PlayTargetingMontage->OnBlendOut.AddDynamic(this, &UGA_Freeze::K2_EndAbility);
	PlayTargetingMontage->OnCancelled.AddDynamic(this, &UGA_Freeze::K2_EndAbility);
	PlayTargetingMontage->OnCompleted.AddDynamic(this, &UGA_Freeze::K2_EndAbility);
	PlayTargetingMontage->OnInterrupted.AddDynamic(this, &UGA_Freeze::K2_EndAbility);
	PlayTargetingMontage->ReadyForActivation();

	UAbilityTask_WaitTargetData* WaitTargetData = UAbilityTask_WaitTargetData::WaitTargetData(this, NAME_None, EGameplayTargetingConfirmation::UserConfirmed, TargetActorClass);
	WaitTargetData->ValidData.AddDynamic(this, &UGA_Freeze::TargetReceived);
	WaitTargetData->Cancelled.AddDynamic(this, &UGA_Freeze::TargetingCancelled);
	WaitTargetData->ReadyForActivation();
	
	AGameplayAbilityTargetActor* TargetActor;
	WaitTargetData->BeginSpawningActor(this, TargetActorClass, TargetActor);
	ATargetActor_GroundPick* GroundPickActor = Cast<ATargetActor_GroundPick>(TargetActor);
	if (GroundPickActor)
	{
		GroundPickActor->SetShouldDrawDebug(ShouldDrawDebug());
		GroundPickActor->SetTargetAreaRadius(TargetingRadius);
		GroundPickActor->SetTargetTraceRange(TargetRange);
	}

	WaitTargetData->FinishSpawningActor(this, TargetActor);
}

void UGA_Freeze::TargetReceived(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	if (!K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}

	if (K2_HasAuthority())
	{
		//FGameplayEffectSpecHandle EffectSpec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
		BP_ApplyGameplayEffectToTarget(TargetDataHandle, DamageEffect, GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
	}

	FGameplayCueParameters FreezeCueParameters;
	FreezeCueParameters.Location = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(TargetDataHandle, 1).ImpactPoint;
	FreezeCueParameters.RawMagnitude = TargetingRadius;

	GetAbilitySystemComponentFromActorInfo()->ExecuteGameplayCue(FreezingGameplayCueTag, FreezeCueParameters);

	PlayMontageLocally(FreezeCastMontage);
	K2_EndAbility();
}

void UGA_Freeze::TargetingCancelled(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	K2_EndAbility();
}
