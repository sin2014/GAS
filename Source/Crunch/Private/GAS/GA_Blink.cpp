// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/GA_Blink.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GAS/TargetActor_GroundPick.h"
#include "GAS/CAbilitySystemStatics.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"

UGA_Blink::UGA_Blink()
{
	ActivationOwnedTags.AddTag(UCAbilitySystemStatics::GetAimStatTag());
}

void UGA_Blink::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		K2_EndAbility();
		return;
	}

	UAbilityTask_PlayMontageAndWait* PlayTargetingMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TargetingMontage);
	PlayTargetingMontage->ReadyForActivation();

	UAbilityTask_WaitTargetData* WaitBlinkLocationTargetData = UAbilityTask_WaitTargetData::WaitTargetData(this, NAME_None, EGameplayTargetingConfirmation::UserConfirmed, GroundPickTargetActorClass);
	WaitBlinkLocationTargetData->ValidData.AddDynamic(this, &UGA_Blink::GroundPickTargetReceived);
	WaitBlinkLocationTargetData->Cancelled.AddDynamic(this, &UGA_Blink::GroundPickCancelled);
	WaitBlinkLocationTargetData->ReadyForActivation();

	AGameplayAbilityTargetActor* TargetActor;
	WaitBlinkLocationTargetData->BeginSpawningActor(this, GroundPickTargetActorClass, TargetActor);
	ATargetActor_GroundPick* GroundPickTargetActor = Cast<ATargetActor_GroundPick>(TargetActor);
	if (GroundPickTargetActor)
	{
		GroundPickTargetActor->SetShouldDrawDebug(ShouldDrawDebug());
		GroundPickTargetActor->SetTargetAreaRadius(TargetAreaRadius);
		GroundPickTargetActor->SetTargetTraceRange(BlinkCastRange);
	}

	WaitBlinkLocationTargetData->FinishSpawningActor(this, TargetActor);
}

FGameplayTag UGA_Blink::GetTeleportationTag()
{
	return FGameplayTag::RequestGameplayTag("ability.blink.teleport");
}

void UGA_Blink::GroundPickTargetReceived(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	if (!K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}

	BlinkTargetDataHandle = TargetDataHandle;

	if (HasAuthorityOrPredictionKey(CurrentActorInfo, &CurrentActivationInfo))
	{
		UAbilityTask_PlayMontageAndWait* PlayTeleportMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TeleportMontage);
		PlayTeleportMontage->OnBlendOut.AddDynamic(this, &UGA_Blink::K2_EndAbility);
		PlayTeleportMontage->OnCancelled.AddDynamic(this, &UGA_Blink::K2_EndAbility);
		PlayTeleportMontage->OnInterrupted.AddDynamic(this, &UGA_Blink::K2_EndAbility);
		PlayTeleportMontage->OnCompleted.AddDynamic(this, &UGA_Blink::K2_EndAbility);
		PlayTeleportMontage->ReadyForActivation();

		if (K2_HasAuthority())
		{
			UAbilityTask_WaitGameplayEvent* WaitTeleportTimepoint = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GetTeleportationTag());
			WaitTeleportTimepoint->EventReceived.AddDynamic(this, &UGA_Blink::Teleport);
			WaitTeleportTimepoint->ReadyForActivation();
		}
	}
}

void UGA_Blink::GroundPickCancelled(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	K2_EndAbility();
}

void UGA_Blink::Teleport(FGameplayEventData Payload)
{
	if (K2_HasAuthority())
	{
		FHitResult PickedLocationHitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(BlinkTargetDataHandle, 1);
		FVector PickedTeleportLocation = PickedLocationHitResult.ImpactPoint;

		GetAvatarActorFromActorInfo()->SetActorLocation(PickedTeleportLocation);
		BP_ApplyGameplayEffectToTarget(BlinkTargetDataHandle, DamageEffect, GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
		PushTargetsFromOwnerLocation(BlinkTargetDataHandle, BlinkLandTargetPushSpeed);
	}
}

