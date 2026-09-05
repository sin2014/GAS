// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/GA_Tornado.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitCancel.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GAS/CAbilitySystemStatics.h"

void UGA_Tornado::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}

	if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		UAbilityTask_PlayMontageAndWait* PlayTornadoMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TornadoMontage);
		PlayTornadoMontage->OnBlendOut.AddDynamic(this, &UGA_Tornado::K2_EndAbility);
		PlayTornadoMontage->OnCancelled.AddDynamic(this, &UGA_Tornado::K2_EndAbility);
		PlayTornadoMontage->OnInterrupted.AddDynamic(this, &UGA_Tornado::K2_EndAbility);
		PlayTornadoMontage->OnCompleted.AddDynamic(this, &UGA_Tornado::K2_EndAbility);
		PlayTornadoMontage->ReadyForActivation();

		if (K2_HasAuthority())
		{
			UAbilityTask_WaitGameplayEvent* WaitDamageEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, UCAbilitySystemStatics::GetGenericDamagePointTag(), nullptr, false);
			WaitDamageEvent->EventReceived.AddDynamic(this, &UGA_Tornado::TornadoDamageEventReceived);
			WaitDamageEvent->ReadyForActivation();
		}

		UAbilityTask_WaitCancel* WaitCancel = UAbilityTask_WaitCancel::WaitCancel(this);
		WaitCancel->OnCancel.AddDynamic(this, &UGA_Tornado::K2_EndAbility);
		WaitCancel->ReadyForActivation();

		UAbilityTask_WaitDelay* WaitTornadoTimeout = UAbilityTask_WaitDelay::WaitDelay(this, TornadoDuration);
		WaitTornadoTimeout->OnFinish.AddDynamic(this, &UGA_Tornado::K2_EndAbility);
		WaitTornadoTimeout->ReadyForActivation();
	}
}

void UGA_Tornado::TornadoDamageEventReceived(FGameplayEventData Payload)
{
	if (K2_HasAuthority())
	{
		FGameplayAbilityTargetDataHandle TargetDataHandle = Payload.TargetData;
		BP_ApplyGameplayEffectToTarget(TargetDataHandle, HitDamageEffect, GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
		PushTargetsFromOwnerLocation(TargetDataHandle, HitPushSpeed);
	}
}



