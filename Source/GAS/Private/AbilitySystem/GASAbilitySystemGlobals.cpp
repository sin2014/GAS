// ZYZ

#include "AbilitySystem/GASAbilitySystemGlobals.h"
#include "GASGameplayEffectTypes.h"

FGameplayEffectContext* UGASAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FGASGameplayEffectContext();
}
