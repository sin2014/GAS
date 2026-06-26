// ZYZ

#include "AbilitySystem/Abilities/GASDamageGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GASGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"

void UGASDamageGameplayAbility::CauseDamage(AActor* TargetActor)
{
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, 1.f);
	for (TTuple<FGameplayTag, FScalableFloat> pair : DamageTypes)
	{
		const float ScaledDamage = pair.Value.GetValueAtLevel(GetAbilityLevel());
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(DamageSpecHandle, pair.Key, ScaledDamage);
	}
	const float ScaledDamageMultiplier = DamageMultiplier.GetValueAtLevel(GetAbilityLevel());
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(DamageSpecHandle, FGASGameplayTags::Get().Damage_Multiplier, ScaledDamageMultiplier);
	GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor));
}
