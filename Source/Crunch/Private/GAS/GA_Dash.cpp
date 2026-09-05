// Fill out your copyright notice in the Description page of Project Settings.  

#include "GAS/GA_Dash.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/TargetActor_Around.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"

void UGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!K2_CommitAbility() || !DashMontage)
	{
		K2_EndAbility();
		return;
	}

	if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		UAbilityTask_PlayMontageAndWait* PlayDashMontage = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DashMontage);
		PlayDashMontage->OnBlendOut.AddDynamic(this, &UGA_Dash::K2_EndAbility);
		PlayDashMontage->OnCancelled.AddDynamic(this, &UGA_Dash::K2_EndAbility);
		PlayDashMontage->OnInterrupted.AddDynamic(this, &UGA_Dash::K2_EndAbility);
		PlayDashMontage->OnCompleted.AddDynamic(this, &UGA_Dash::K2_EndAbility);
		PlayDashMontage->ReadyForActivation();

		UAbilityTask_WaitGameplayEvent* WaitDashStartEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, GetDashStartTag());
		WaitDashStartEvent->EventReceived.AddDynamic(this, &UGA_Dash::StartDash);
		WaitDashStartEvent->ReadyForActivation();
	}
}

void UGA_Dash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UAbilitySystemComponent* OwnerAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	if (OwnerAbilitySystemComponent && DashEffectHandle.IsValid())
	{
		OwnerAbilitySystemComponent->RemoveActiveGameplayEffect(DashEffectHandle);
	}

	if (PushForwardInputTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(PushForwardInputTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FGameplayTag UGA_Dash::GetDashStartTag()
{
	return FGameplayTag::RequestGameplayTag("ability.dash.start");
}

void UGA_Dash::PushForward()
{
	if (OwnerCharacterMovementComponent)
	{
		FVector ForwardActor = GetAvatarActorFromActorInfo()->GetActorForwardVector();
		OwnerCharacterMovementComponent->AddInputVector(ForwardActor);
		PushForwardInputTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGA_Dash::PushForward);
	}
}

void UGA_Dash::StartDash(FGameplayEventData Payload)
{

	if (K2_HasAuthority())
	{
		if (DashEffect)
		{
			DashEffectHandle = BP_ApplyGameplayEffectToOwner(DashEffect, GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
		}
	}

	if (IsLocallyControlled())
	{
		PushForwardInputTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGA_Dash::PushForward);
		OwnerCharacterMovementComponent = GetAvatarActorFromActorInfo()->GetComponentByClass<UCharacterMovementComponent>();
	}

	UAbilityTask_WaitTargetData* WaitTargetData = UAbilityTask_WaitTargetData::WaitTargetData(this, NAME_None, EGameplayTargetingConfirmation::CustomMulti, TargetActorClass);
	WaitTargetData->ValidData.AddDynamic(this, &UGA_Dash::TargetReceived);
	WaitTargetData->ReadyForActivation();

	AGameplayAbilityTargetActor* TargetActor;
	WaitTargetData->BeginSpawningActor(this, TargetActorClass, TargetActor);
	ATargetActor_Around* TargetActorAround = Cast<ATargetActor_Around>(TargetActor);
	if (TargetActorAround)
	{
		TargetActorAround->ConfigureDetection(TargetDetectionRadius, GetOwnerTeamId(), LocalGameplayCueTag);
	}

	WaitTargetData->FinishSpawningActor(this, TargetActor);
	if (TargetActorAround)
	{
		TargetActorAround->AttachToComponent(GetOwningComponentFromActorInfo(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TargetActorAttachSocketName);
	}
}


void UGA_Dash::TargetReceived(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	if (K2_HasAuthority())
	{
		BP_ApplyGameplayEffectToTarget(TargetDataHandle, DamageEffect, GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo));
		PushTargetsFromOwnerLocation(TargetDataHandle, TargetHitPushSpeed);
	}
}
