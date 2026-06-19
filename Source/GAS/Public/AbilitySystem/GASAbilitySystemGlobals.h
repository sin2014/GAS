// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "GASAbilitySystemGlobals.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API UGASAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
	
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
