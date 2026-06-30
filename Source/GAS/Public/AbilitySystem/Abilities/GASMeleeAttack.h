// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/GASDamageGameplayAbility.h"
#include "Interaction/CombatInterface.h"
#include "GASMeleeAttack.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API UGASMeleeAttack : public UGASDamageGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Melee", meta = (DisplayName = "Pick Random Tagged Montage"))
	bool PickRandomTaggedMontage(const TArray<FTaggedMontage>& TaggedMontages, FTaggedMontage& OutTaggedMontage) const;
};
