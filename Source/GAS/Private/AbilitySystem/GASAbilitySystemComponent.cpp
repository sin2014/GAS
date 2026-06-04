// ZYZ

#include "AbilitySystem/GASAbilitySystemComponent.h"

void UGASAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UGASAbilitySystemComponent::EffectApplid);
}

void UGASAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (auto Abilityclass : StartupAbilities)
	{
		if (Abilityclass)
		{
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Abilityclass, 1);
			//GiveAbility(AbilitySpec);
			GiveAbilityAndActivateOnce(AbilitySpec);
		}
	}
}

void UGASAbilitySystemComponent::EffectApplid(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);
	
	EffectAssetTags.Broadcast(TagContainer);
}
